#include "JPboxgroup.h"
#include "jp_box_factory.h"
#include <algorithm>

// Cue drafts, apply/cancel and synchronization of nested presets.
// The owning graph remains JPboxgroup: draft links borrow live FBOs and must
// be detached before graph deletion. Ownership is unchanged by this extraction.
namespace
{
    // These boxes can render staged parameters independently. Cameras acquire
    // the existing shared capture; images reuse the asynchronous decode cache.
    bool rendersCueDraft(int type)
    {
        return type == JPbox::SHADERBOX || type == JPbox::FRAMEDIFFERENCEBOX ||
            type == JPbox::PAINTBOX || type == JPbox::IMAGEBOX || type == JPbox::CAMBOX;
    }
	void copyFboStraight(ofFbo &source, ofFbo &destination)
	{
		if (!source.isAllocated() || !destination.isAllocated()) return;
		ofPushStyle();
		destination.begin();
		ofClear(0, 0, 0, 0);
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		ofSetColor(255);
		ofSetRectMode(OF_RECTMODE_CORNER);
		source.draw(0, 0, destination.getWidth(), destination.getHeight());
		destination.end();
		ofEnableAlphaBlending();
		ofPopStyle();
	}
}

bool JPboxgroup::setCueFromSelected()
{
	return setCueByIndex(openguinumber);
}

bool JPboxgroup::setCueByIndex(int index)
{
	// The caller (setCueBoxByIndex / toggleCueBoxByIndex) sets the intended target
	// graph. clearCue() below resets targetPreset to nullptr, so capture and restore
	// it — otherwise a group-box cue would wrongly build its draft from the main graph.
	JPbox_preset *intendedTarget = cueState.targetPreset;
	if (index < 0 || index >= getCueTargetBoxSize())
	{
		clearCue();
		return false;
	}

	bool wasInspectorTarget = isCueDraftMode() && cueSelectedIndex() == cueState.sourceIndex;
	clearCue();
	cueState.targetPreset = intendedTarget;
	if (wasInspectorTarget && cueSelectedIndex() >= 0 && cueSelectedIndex() < getCueTargetBoxSize())
	{
		setControllers();
	}
	if (getCueTargetActiveRender() >= 0 && getCueTargetActiveRender() < getCueTargetBoxSize())
	{
		if (cueApplySnapshotFbo.getWidth() != getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getWidth() ||
			cueApplySnapshotFbo.getHeight() != getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getHeight())
		{
			cueApplySnapshotFbo.allocate(getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getWidth(), getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getHeight());
		}
		copyFboStraight(getCueTargetBoxAt(getCueTargetActiveRender())->fbo,
			cueApplySnapshotFbo);
	}

	if (beginCueDraftForBoxIndex(index))
	{
		return true;
	}

	return false;
}

bool JPboxgroup::toggleCueByIndex(int index)
{
	cout << "toggleCueByIndex(" << index << ") targetBoxSize=" << getCueTargetBoxSize() << endl;
	if (index < 0 || index >= getCueTargetBoxSize())
	{
		cout << "  -> index out of range, clearCue" << endl;
		clearCue();
		return true;
	}
	cout << "  hasCue=" << hasCue() << " sourceIndex=" << cueState.sourceIndex << endl;
	if (hasCue() && cueState.sourceIndex == index)
	{
		cout << "  -> same index, clearing" << endl;
		clearCue();
		return true;
	}
	cout << "  -> calling setCueByIndex" << endl;
	return setCueByIndex(index);
}

void JPboxgroup::clearCue()
{
	bool wasInspectorTarget = isCueDraftMode() && cueSelectedIndex() == cueState.sourceIndex;
	removeCueAddedBoxesFromRealGraph();
	clearCueDraft();
	cueState.mode = CUE_NONE;
	cueState.sourceIndex = -1;
	cueState.previewIndex = -1;
	cueState.stagedActiveRenderIndex = -1;
	cueState.targetPreset = nullptr;
	cueFullscreenPreview = false;
	cueMonitorMode = CUE_MONITOR_FINAL_OUTPUT;
	cuePanelApplyArmed = false;
	pendingCueApply = false;
	pendingCueRebuild = false;
	if (wasInspectorTarget && cueSelectedIndex() >= 0)
	{
		setControllers();
	}
}

bool JPboxgroup::applyCue()
{
	if (isCueDraftMode())
	{
		return applyCueDraftToSource();
	}
	if (isCueNormalPreviewMode())
	{
		if (cueState.stagedActiveRenderIndex >= 0 &&
			cueState.stagedActiveRenderIndex < getCueTargetBoxSize() &&
			cueState.stagedActiveRenderIndex != getCueTargetActiveRender())
		{
			updateTransition(cueState.stagedActiveRenderIndex);
		}
		return true;
	}
	return false;
}

bool JPboxgroup::hasCue() const
{
	return cueState.mode != CUE_NONE;
}

bool JPboxgroup::setCueBoxByIndex(int index)
{
	// Global cue: always target the main-graph tree.
	cueState.targetPreset = nullptr;
	return setCueByIndex(index);
}

bool JPboxgroup::setCueBoxByName(string boxName)
{
	cueState.targetPreset = nullptr;
	return setCueByIndex(findCueTargetBoxIndexByName(boxName));
}

bool JPboxgroup::toggleCueBoxByIndex(int index)
{
	cout << "toggleCueBoxByIndex(" << index << ") called" << endl;
	// The cue is a GLOBAL staging session over the whole main-graph tree, so it
	// always targets the main graph (nullptr). Edits inside groups stage into the
	// corresponding draft-tree sub-box (see getDraftBoxForCurrentInspector).
	cueState.targetPreset = nullptr;
	return toggleCueByIndex(index);
}

int JPboxgroup::getCueEntryIndexForCurrentView() const
{
	// The cue is global (main-graph tree). Return a MAIN-graph source index: inside
	// a group that is the main box containing the group; in the main graph the
	// selected box, else the active render.
	if (isGroupViewActive())
	{
		if (!activeGroupPath.empty())
		{
			return activeGroupPath[0];
		}
		return activerender != nullptr ? *activerender : -1;
	}
	if (openguinumber >= 0 && openguinumber < (int)boxes.size())
	{
		return openguinumber;
	}
	return activerender != nullptr ? *activerender : -1;
}

bool JPboxgroup::hasCueBox() const
{
	return hasCue();
}

// --- CUE target helpers ---
vector<JPbox *>& JPboxgroup::getCueTargetBoxes()
{
	return (cueState.targetPreset != nullptr) ? cueState.targetPreset->boxes : boxes;
}

int JPboxgroup::getCueTargetBoxSize() const
{
	return (cueState.targetPreset != nullptr) ? (int)cueState.targetPreset->boxes.size() : (int)boxes.size();
}

JPbox *JPboxgroup::getCueTargetBoxAt(int index) const
{
	if (index < 0) return nullptr;
	if (cueState.targetPreset != nullptr)
	{
		return (index < (int)cueState.targetPreset->boxes.size()) ? cueState.targetPreset->boxes[index] : nullptr;
	}
	return (index < (int)boxes.size()) ? boxes[index] : nullptr;
}

int &JPboxgroup::getCueTargetActiveRender()
{
	return (cueState.targetPreset != nullptr) ? cueState.targetPreset->activeRender : *activerender;
}

bool JPboxgroup::cueTargetsCurrentView() const
{
	if (!hasCue())
	{
		return false;
	}
	return isGroupViewActive() ? (cueState.targetPreset == getActivePreset())
							   : (cueState.targetPreset == nullptr);
}

int JPboxgroup::cueSelectedIndex() const
{
	// The cue is a GLOBAL staging session over the whole main-graph tree. This
	// returns the TOP-LEVEL main-graph box index for the current context: inside a
	// group that is the main box that contains the group (activeGroupPath[0]), so
	// dirty-marking and draft lookups stay at the main-graph level. The exact box
	// being edited (a sub-box deep in the tree) is resolved by
	// getDraftBoxForCurrentInspector().
	if (isGroupViewActive())
	{
		return activeGroupPath.empty() ? -1 : activeGroupPath[0];
	}
	return openguinumber;
}
JPbox *JPboxgroup::getDraftBoxForCurrentInspector()
{
	// Resolve the draft-tree box that the inspector is editing. The draft graph
	// clones the MAIN boxes (and, for presets, their internal sub-boxes via
	// copyPresetInternalState), so we navigate that draft tree by activeGroupPath
	// and finally the selected sub-box.
	if (!isCueDraftMode())
	{
		return nullptr;
	}
	if (!isGroupViewActive())
	{
		return getCueDraftBoxForRealIndex(openguinumber);
	}
	if (activeGroupPath.empty())
	{
		return nullptr;
	}
	JPbox_preset *dp = dynamic_cast<JPbox_preset *>(getCueDraftBoxForRealIndex(activeGroupPath[0]));
	if (dp == nullptr)
	{
		return nullptr;
	}
	for (size_t depth = 1; depth < activeGroupPath.size(); depth++)
	{
		int idx = activeGroupPath[depth];
		if (idx < 0 || idx >= (int)dp->boxes.size() || dp->boxes[idx] == nullptr)
		{
			return nullptr;
		}
		dp = dynamic_cast<JPbox_preset *>(dp->boxes[idx]);
		if (dp == nullptr)
		{
			return nullptr;
		}
	}
	if (groupInspectorIndex < 0 || groupInspectorIndex >= (int)dp->boxes.size())
	{
		return nullptr;
	}
	return dp->boxes[groupInspectorIndex];
}

int JPboxgroup::findCueTargetBoxIndexByName(const string &boxName) const
{
	const vector<JPbox *> &target = (cueState.targetPreset != nullptr) ? cueState.targetPreset->boxes : boxes;
	for (int i = 0; i < (int)target.size(); i++)
	{
		if (target[i] != nullptr && target[i]->name == boxName)
		{
			return i;
		}
	}
	return -1;
}

bool JPboxgroup::promoteCueToActive()
{
	return requestCueApply();
}

bool JPboxgroup::requestCueApply()
{
	if (!hasCue())
	{
		return false;
	}
	pendingCueApply = true;
	return true;
}

void JPboxgroup::processPendingCueApply()
{
	if (!pendingCueApply)
	{
		return;
	}
	pendingCueApply = false;
	applyCue();
}

void JPboxgroup::requestCueRebuild()
{
	if (hasCue())
	{
		pendingCueRebuild = true;
	}
}

void JPboxgroup::processPendingCueRebuild()
{
	if (!pendingCueRebuild)
	{
		return;
	}
	pendingCueRebuild = false;
	rebuildCueAfterGraphChange();
}

bool JPboxgroup::rebuildCueAfterGraphChange()
{
	if (!hasCue())
	{
		return false;
	}

	if (isCueNormalPreviewMode())
	{
		if (cueState.sourceIndex < 0 || cueState.sourceIndex >= getCueTargetBoxSize() ||
			cueState.previewIndex < 0 || cueState.previewIndex >= getCueTargetBoxSize() ||
			getCueTargetBoxAt(cueState.sourceIndex) == nullptr || getCueTargetBoxAt(cueState.previewIndex) == nullptr)
		{
			clearCue();
			return false;
		}
		return true;
	}

	if (!isCueDraftMode())
	{
		return false;
	}
	// Rebuild the draft graph — check all draft real indices are still valid
	for (int i = 0; i < (int)cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex < 0 || realIndex >= getCueTargetBoxSize() ||
			getCueTargetBoxAt(realIndex) == nullptr || cueState.draftBoxes[i] == nullptr)
		{
			clearCue();
			return false;
		}
	}

	if (!isCueDraftMode())
	{
		return false;
	}

	struct PresetExposedInputSnapshot
	{
		vector<string> presetPath;
		vector<JPbox_preset::ExposedTextureInput> inputs;
	};
	std::function<void(
		JPbox_preset *,
		const vector<string> &,
		vector<PresetExposedInputSnapshot> &)>
		snapshotExposedInputs =
		[&snapshotExposedInputs](
			JPbox_preset *preset,
			const vector<string> &path,
			vector<PresetExposedInputSnapshot> &result) {
			if (preset == nullptr)
			{
				return;
			}
			PresetExposedInputSnapshot item;
			item.presetPath = path;
			item.inputs = preset->exposedTextureInputs;
			result.push_back(item);
			for (JPbox *box : preset->boxes)
			{
				if (box != nullptr &&
					box->getTipo() == JPbox::PRESETBOX)
				{
					vector<string> childPath = path;
					childPath.push_back(box->name);
					snapshotExposedInputs(
						dynamic_cast<JPbox_preset *>(box),
						childPath, result);
				}
			}
		};
	auto restoreExposedInputs =
		[](JPbox_preset *root,
		   const vector<PresetExposedInputSnapshot> &items) {
			for (const PresetExposedInputSnapshot &item :
				items)
			{
				JPbox_preset *preset = root;
				for (const string &name : item.presetPath)
				{
					JPbox_preset *next = nullptr;
					if (preset != nullptr)
					{
						for (JPbox *box : preset->boxes)
						{
							if (box != nullptr &&
								box->name == name &&
								box->getTipo() ==
									JPbox::PRESETBOX)
							{
								next = dynamic_cast<
									JPbox_preset *>(box);
								break;
							}
						}
					}
					preset = next;
				}
				if (preset != nullptr)
				{
					preset->setExposedTextureInputs(
						item.inputs);
				}
			}
		};

	struct DraftSnapshot
	{
		int realIndex = -1;
		string name;
		JPParameterGroup parameters;
		bool onoff = true;
		bool bypass = false;
		vector<string> linkNames;
		vector<bool> linkSet;
		vector<int> presetActiveRenders;
		vector<PresetLinkAssignment> presetLinks;
		vector<PresetExposedInputSnapshot>
			presetExposedInputs;
		unsigned int dirtyFlags = CUE_DIRTY_NONE;
		bool hasMedia = false;
		JPMediaState media;
	};

	vector<DraftSnapshot> snapshots;
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex < 0 || realIndex >= getCueTargetBoxSize() ||
			i < 0 || i >= cueState.draftBoxes.size() ||
			getCueTargetBoxAt(realIndex) == nullptr || cueState.draftBoxes[i] == nullptr)
		{
			continue;
		}
		DraftSnapshot snapshot;
		snapshot.realIndex = realIndex;
		snapshot.name = getCueTargetBoxAt(realIndex)->name;
		snapshot.parameters = cueState.draftBoxes[i]->parameters;
		snapshot.onoff = cueState.draftBoxes[i]->getonoff();
		snapshot.bypass = cueState.draftBoxes[i]->getBypass();
		if (auto *media = dynamic_cast<JPMediaInspectable *>(cueState.draftBoxes[i]))
		{
			snapshot.hasMedia = true;
			snapshot.media = media->mediaState();
		}
		if (cueState.draftBoxes[i]->getTipo() == JPbox::PRESETBOX)
		{
			JPbox_preset *draftPreset =
				dynamic_cast<JPbox_preset *>(cueState.draftBoxes[i]);
			snapshotPresetActiveRenders(
				draftPreset, snapshot.presetActiveRenders);
			snapshotPresetLinks(
				draftPreset, snapshot.presetLinks);
			snapshotExposedInputs(
				draftPreset, {},
				snapshot.presetExposedInputs);
		}
		for (int linkIndex = 0; linkIndex < cueState.draftBoxes[i]->fbohandlergroup.getSize(); linkIndex++)
		{
			bool isSet = cueState.draftBoxes[i]->fbohandlergroup.getisPointerSet(linkIndex);
			snapshot.linkSet.push_back(isSet);
			snapshot.linkNames.push_back(isSet ? cueState.draftBoxes[i]->fbohandlergroup.getFboName(linkIndex) : "");
		}
		snapshot.dirtyFlags = getCueDraftDirtyFlags(realIndex);
		if (snapshot.dirtyFlags != CUE_DIRTY_NONE)
		{
			snapshots.push_back(snapshot);
		}
	}

	int sourceIndex = cueState.sourceIndex;
	int keepStagedActiveRenderIndex = cueState.stagedActiveRenderIndex;
	bool keepFullscreenPreview = cueFullscreenPreview;
	CueMonitorMode keepMonitorMode = cueMonitorMode;

	if (sourceIndex < 0 || sourceIndex >= getCueTargetBoxSize() ||
		getCueTargetBoxAt(sourceIndex) == nullptr)
	{
		clearCue();
		return false;
	}

	if (!buildCueDraftGraph(sourceIndex))
	{
		return false;
	}

	cueFullscreenPreview = keepFullscreenPreview;
	cueMonitorMode = keepMonitorMode;
	if (keepStagedActiveRenderIndex >= 0 && keepStagedActiveRenderIndex < getCueTargetBoxSize())
	{
		setCueStagedActiveRenderIndex(keepStagedActiveRenderIndex);
	}
	cueState.dirtyDraftRealIndices.clear();
	for (int i = 0; i < cueState.draftDirtyFlags.size(); i++)
	{
		cueState.draftDirtyFlags[i] = CUE_DIRTY_NONE;
	}

	for (int i = 0; i < snapshots.size(); i++)
	{
		int realIndex = snapshots[i].realIndex;
		if (realIndex < 0 || realIndex >= getCueTargetBoxSize() ||
			getCueTargetBoxAt(realIndex) == nullptr ||
			getCueTargetBoxAt(realIndex)->name != snapshots[i].name)
		{
			realIndex = findCueTargetBoxIndexByName(snapshots[i].name);
		}

		JPbox *draftBox = getCueDraftBoxForRealIndex(realIndex);
		if (draftBox == nullptr)
		{
			continue;
		}

		copyParametersByNameOrIndex(draftBox->parameters, snapshots[i].parameters);
		if (snapshots[i].hasMedia)
			if (auto *media = dynamic_cast<JPMediaInspectable *>(draftBox))
				media->mediaState() = snapshots[i].media;
		draftBox->setonoff(snapshots[i].onoff);
		draftBox->setBypass(snapshots[i].bypass);
		if (!snapshots[i].presetActiveRenders.empty() &&
			draftBox->getTipo() == JPbox::PRESETBOX)
		{
			int valueIndex = 0;
			restorePresetActiveRenders(dynamic_cast<JPbox_preset *>(draftBox),
								 snapshots[i].presetActiveRenders, valueIndex);
		}
		if (!snapshots[i].presetExposedInputs.empty() &&
			draftBox->getTipo() == JPbox::PRESETBOX)
		{
			restoreExposedInputs(
				dynamic_cast<JPbox_preset *>(draftBox),
				snapshots[i].presetExposedInputs);
		}
		if (!snapshots[i].presetLinks.empty() &&
			draftBox->getTipo() == JPbox::PRESETBOX)
		{
			restorePresetLinks(
				dynamic_cast<JPbox_preset *>(draftBox),
				snapshots[i].presetLinks);
		}
		for (int linkIndex = 0; linkIndex < snapshots[i].linkSet.size() &&
								   linkIndex < draftBox->fbohandlergroup.getSize(); linkIndex++)
		{
			if (!snapshots[i].linkSet[linkIndex])
			{
				draftBox->fbohandlergroup.deleteFboPointer(linkIndex);
				continue;
			}
			int linkedRealIndex = findCueTargetBoxIndexByName(snapshots[i].linkNames[linkIndex]);
			JPbox *linkedDraft = getCueDraftBoxForRealIndex(linkedRealIndex);
			if (linkedDraft != nullptr)
			{
				draftBox->fbohandlergroup.setFboPointer(&linkedDraft->fbo, &linkedDraft->name, linkIndex);
			}
			else if (linkedRealIndex >= 0 && linkedRealIndex < getCueTargetBoxSize() && getCueTargetBoxAt(linkedRealIndex) != nullptr)
			{
				JPbox *linkedReal = getCueTargetBoxAt(linkedRealIndex);
				draftBox->fbohandlergroup.setFboPointer(&linkedReal->fbo, &linkedReal->name, linkIndex);
			}
		}
		markCueDraftDirty(realIndex, snapshots[i].dirtyFlags);
	}
	for (int i = 0; i < cueState.cueAddedRealIndices.size(); i++)
	{
		markCueDraftDirty(cueState.cueAddedRealIndices[i], CUE_DIRTY_ADDED);
	}

	// Rebuild the inspector controllers if the current selection maps to a draft
	// (selection index is preserved across the rebuild for both main and group).
	if (getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr)
	{
		setControllers();
	}
	rewireCueDraftGraph();
	updateCueDraftGraph();
	return true;
}

bool JPboxgroup::beginCueDraftForActiveShader()
{
	return beginCueDraftForBoxIndex(getCueTargetActiveRender());
}

void JPboxgroup::clearCueDraft()
{
	for (int i = 0; i < cueState.draftBoxes.size(); i++)
	{
		if (cueState.draftBoxes[i] != nullptr)
		{
			cueState.draftBoxes[i]->clear();
			delete cueState.draftBoxes[i];
			cueState.draftBoxes[i] = nullptr;
		}
	}
	cueState.draftBoxes.clear();
	// The draft's stack addresses boxes that are being destroyed right here.
	cueDraftHistory.clear();
	cueState.draftRealIndices.clear();
	cueState.dirtyDraftRealIndices.clear();
	cueState.draftDirtyFlags.clear();
	cueState.draftBaselineParameters.clear();
	cueState.draftBaselineOnOff.clear();
	cueState.draftBaselineBypass.clear();
	cueState.draftInspectorRealIndex = -1;
	cueState.draftSourceBox = nullptr;
	cueState.draftOutputBox = nullptr;
	cueState.draftOutputRealIndex = -1;
	cueState.stagedActiveRenderIndex = -1;
}

std::unique_ptr<JPboxgroup::RetainedScene> JPboxgroup::cloneRenderScene()
{
    auto scene=std::make_unique<RetainedScene>();
    scene->active=activerender?*activerender:0;
    scene->finalLayers=finalQuickImages;
    for(auto *source:boxes) {
        auto *clone=jp_box_factory::create(source->dir,jp_box_factory::Context::Interactive);
        if(!clone) return nullptr;
        scene->nodes.push_back(clone);
        clone->setup(source->dir,source->name);
        copyEditableBoxState(clone,source); clone->uid=source->uid;
        if(auto *group=dynamic_cast<JPbox_preset *>(clone)) {
            auto *original=dynamic_cast<JPbox_preset *>(source);
            if(!synchronizeCuePresetStructure(group,original)) return nullptr;
            copyPresetInternalState(group,original);
        }
        copyFboStraight(source->fbo,clone->fbo);
        if(auto *shader=dynamic_cast<JPbox_shader *>(clone)) shader->seedTransitionFeedback(source->fbo);
    }
    for(size_t i=0;i<boxes.size();++i) copyBoxLinksByName(scene->nodes[i],boxes[i],scene->nodes);
    return scene;
}

bool JPboxgroup::applyCueDraftToSource()
{
	if (!isCueDraftMode())
	{
		return false;
	}
	int sourceIndex = cueState.sourceIndex;
	bool draftWasInspectorTarget = getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr;
	int stagedActiveIndex = cueState.stagedActiveRenderIndex;
	int targetSize = getCueTargetBoxSize();
	int targetActiveRender = getCueTargetActiveRender();
	if (stagedActiveIndex < 0 || stagedActiveIndex >= targetSize)
	{
		stagedActiveIndex = targetActiveRender;
	}
	bool activeRenderChanged = stagedActiveIndex != targetActiveRender;
	if (cueState.dirtyDraftRealIndices.empty() && !activeRenderChanged)
	{
		return true;
	}
    // Preserve the whole outgoing presentation, including FINAL, before applying
    // draft edits. Interrupted changes deliberately start from the displayed mix.
    ofFbo visibleBeforeApply=captureSessionOutput();
    std::unique_ptr<RetainedScene> liveBeforeApply;
    if(!sessionFadeActive && mainTransitionState().progress()>=1.f && TransitionSR::preferences().quality!=jp_transition::Quality::Capture)
        liveBeforeApply=cloneRenderScene();
    if (targetActiveRender >= 0 && targetActiveRender < targetSize &&
        (cueApplySnapshotFbo.getWidth() != getCueTargetBoxAt(targetActiveRender)->fbo.getWidth() ||
		 cueApplySnapshotFbo.getHeight() != getCueTargetBoxAt(targetActiveRender)->fbo.getHeight()))
	{
		cueApplySnapshotFbo.allocate(getCueTargetBoxAt(targetActiveRender)->fbo.getWidth(), getCueTargetBoxAt(targetActiveRender)->fbo.getHeight());
	}
	if (targetActiveRender >= 0 && targetActiveRender < targetSize)
	{
		copyFboStraight(getCueTargetBoxAt(targetActiveRender)->fbo,
			cueApplySnapshotFbo);
	}

	vector<int> dirtyIndices = cueState.dirtyDraftRealIndices;
	vector<int> deletedIndices = getCueDirtyIndices(CUE_DIRTY_DELETED);
	std::sort(deletedIndices.begin(), deletedIndices.end(), std::greater<int>());
	vector<JPbox*> &targetBoxes = getCueTargetBoxes();
	for (int i = 0; i < dirtyIndices.size(); i++)
	{
		int realIndex = dirtyIndices[i];
		unsigned int flags = getCueDraftDirtyFlags(realIndex);
		if ((flags & CUE_DIRTY_DELETED) != 0)
		{
			continue;
		}
		int draftIndex = findCueDraftCloneIndexForRealIndex(realIndex);
		if (realIndex < 0 || realIndex >= targetSize ||
			draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
			targetBoxes[realIndex] == nullptr || cueState.draftBoxes[draftIndex] == nullptr)
		{
			continue;
		}
		if ((flags & (CUE_DIRTY_PARAMS | CUE_DIRTY_LINKS |
					  CUE_DIRTY_ADDED | CUE_DIRTY_PRESET_ACTIVE |
					  CUE_DIRTY_BYPASS_PAUSE)) != 0)
		{
			copyParametersByNameOrIndex(targetBoxes[realIndex]->parameters, cueState.draftBoxes[draftIndex]->parameters);
			targetBoxes[realIndex]->copyCustomStateFrom(
				cueState.draftBoxes[draftIndex]);
			// For a preset/group, the editable state (incl. exposed params) lives
			// in its internal sub-boxes; commit those staged edits back to the live
			// preset too.
			if (targetBoxes[realIndex]->getTipo() == JPbox::PRESETBOX &&
				cueState.draftBoxes[draftIndex]->getTipo() == JPbox::PRESETBOX)
			{
				copyPresetInternalState(dynamic_cast<JPbox_preset *>(targetBoxes[realIndex]),
										dynamic_cast<JPbox_preset *>(cueState.draftBoxes[draftIndex]));
			}
		}
		if ((flags & (CUE_DIRTY_BYPASS_PAUSE | CUE_DIRTY_ADDED)) != 0)
		{
			targetBoxes[realIndex]->setonoff(cueState.draftBoxes[draftIndex]->getonoff());
			targetBoxes[realIndex]->setBypass(cueState.draftBoxes[draftIndex]->getBypass());
		}
		if ((flags & (CUE_DIRTY_LINKS | CUE_DIRTY_ADDED)) != 0)
		{
			copyCueDraftLinksToReal(realIndex);
		}
	}
	cueApplyingCommit = true;
	for (int i = 0; i < deletedIndices.size(); i++)
	{
		int realIndex = deletedIndices[i];
		if (realIndex >= 0 && realIndex < targetSize && !isCueAddedRealIndex(realIndex))
		{
			if (cueState.targetPreset != nullptr)
			{
				// Delete from the target preset's own box vector (deleteBoxAtIndex
				// only handles the main graph).
				vector<JPbox *> &pb = cueState.targetPreset->boxes;
				if (realIndex < (int)pb.size() && pb[realIndex] != nullptr)
				{
					// Applying a cue is a commit, not an edit the user walks
					// back with Ctrl+Z - the cue has its own cancel. So the box
					// is detached through the shared helper for consistency and
					// then destroyed straight away.
					JPGraphDetachedBox detached = detachBoxFromView(pb,
						realIndex, cueState.targetPreset);
					destroyDetachedBox(detached);
				}
			}
			else
			{
				deleteBoxAtIndex(realIndex);
			}
			if (stagedActiveIndex == realIndex)
			{
				stagedActiveIndex = targetActiveRender;
			}
			else if (stagedActiveIndex > realIndex)
			{
				stagedActiveIndex--;
			}
		}
	}
	cueApplyingCommit = false;
	cueState.cueAddedRealIndices.clear();
	// Group-internal boxes added during the cue are already in their presets;
	// keep them (just drop the staging tracking) since we are committing.
	cueAddedGroupBoxes.clear();
	updateRealBoxesForCueApply();
	if (stagedActiveIndex >= 0 && stagedActiveIndex < targetSize)
	{
		getCueTargetActiveRender() = stagedActiveIndex;
		// The shared crossfader drives MAIN only. Group presets own their local
		// child crossfade, so do not hijack MAIN with a sub-box FBO.
		if (cueState.targetPreset == nullptr)
		{
            transition.setLerpValue(1.f);
            transition.setFboPointer1(nullptr);
            transition.setFboPointer2(&targetBoxes[stagedActiveIndex]->fbo);
		}
	}

    // The scene compositor owns this CUE apply. Do not also animate a group's
    // child selection underneath it, which would apply the transition twice.
    std::function<void(const vector<JPbox *> &)> settleGroups = [&](const vector<JPbox *> &nodes) {
        for (auto *node : nodes) if (auto *group = dynamic_cast<JPbox_preset *>(node)) {
            settleGroups(group->boxes);
            group->clearActiveRenderMorph();
            group->activeRenderTransition.setLerpValue(1.f);
            group->activeRenderTransitionRunning = false;
            group->activeTransitionSnapshot.clear();
            group->lastCompositedActiveRender = group->activeRender;
            group->activeRenderTransitionTarget = group->activeRender;
        }
    };
    settleGroups(boxes);

    outgoingScene=std::move(liveBeforeApply);
    sessionFadeSnapshot=std::move(visibleBeforeApply);
    sessionFadeActive=sessionFadeSnapshot.isAllocated(); sessionFadeStarted=false;
    configureSceneTransition();

	bool keepFullscreenPreview = cueFullscreenPreview;
	CueMonitorMode keepMonitorMode = cueMonitorMode;
	int rebuildSourceIndex = sourceIndex;
	if (rebuildSourceIndex < 0 || rebuildSourceIndex >= targetSize || targetBoxes[rebuildSourceIndex] == nullptr)
	{
		rebuildSourceIndex = stagedActiveIndex;
	}
	if (rebuildSourceIndex < 0 || rebuildSourceIndex >= targetSize || targetBoxes[rebuildSourceIndex] == nullptr)
	{
		rebuildSourceIndex = targetActiveRender;
	}
	if (rebuildSourceIndex >= 0 && rebuildSourceIndex < targetSize && buildCueDraftGraph(rebuildSourceIndex))
	{
		cueFullscreenPreview = keepFullscreenPreview;
		cueMonitorMode = keepMonitorMode;
		setCueStagedActiveRenderIndex(stagedActiveIndex);
	}
	else
	{
		clearCue();
		return true;
	}
	if (draftWasInspectorTarget && getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr)
	{
		setControllers();
	}
	return true;
}

JPbox *JPboxgroup::getInspectorBox()
{
	// While a (global) cue is active, edit the corresponding DRAFT-tree box so all
	// changes stage in the cue instead of the live graph — anywhere in the tree,
	// main graph or inside any group.
	if (isCueDraftMode())
	{
		JPbox *draftBox = getDraftBoxForCurrentInspector();
		if (draftBox != nullptr)
		{
			cueState.draftInspectorRealIndex = cueSelectedIndex();
			return draftBox;
		}
	}

	// Group view: real sub-box from the active preset (uses groupInspectorIndex).
	if (isGroupViewActive() && groupInspectorIndex >= 0)
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr && groupInspectorIndex < (int)preset->boxes.size())
		{
			return preset->boxes[groupInspectorIndex];
		}
	}
	// Main view: real box.
	if (openguinumber >= 0 && openguinumber < boxes.size())
	{
		return boxes[openguinumber];
	}
	return nullptr;
}

JPbox *JPboxgroup::getCuePreviewBox()
{
	if (cueMonitorMode == CUE_MONITOR_SELECTED_BOX &&
		cueSelectedIndex() >= 0 && cueSelectedIndex() < getCueTargetBoxSize())
	{
		JPbox *draftBox = getCueDraftBoxForRealIndex(cueSelectedIndex());
		if (draftBox != nullptr)
		{
			return draftBox;
		}
		return getCueTargetBoxAt(cueSelectedIndex());
	}
	if (isCueDraftMode())
	{
		// Show the STAGED draft output so all pending changes are visible in the
		// CUE window before Apply, while the live output stays untouched.
		if (cueState.draftOutputBox != nullptr)
		{
			return cueState.draftOutputBox;
		}
		return cueState.draftSourceBox;
	}
	if (isCueNormalPreviewMode() &&
		cueState.previewIndex >= 0 && cueState.previewIndex < getCueTargetBoxSize())
	{
		return getCueTargetBoxAt(cueState.previewIndex);
	}
	return nullptr;
}

JPbox *JPboxgroup::getCueDraftSourceBox()
{
	return cueState.draftSourceBox;
}

JPbox *JPboxgroup::getCueDraftBoxForRealIndex(int index) const
{
	if (!isCueDraftMode())
	{
		return nullptr;
	}
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex >= 0 && draftIndex < cueState.draftBoxes.size())
	{
		return cueState.draftBoxes[draftIndex];
	}
	return nullptr;
}

JPbox *JPboxgroup::getCueDraftBoxForCurrentViewIndex(int index) const
{
	if (!isCueDraftMode() || index < 0)
	{
		return nullptr;
	}
	if (!isGroupViewActive())
	{
		return getCueDraftBoxForRealIndex(index);
	}
	JPbox_preset *draftPreset = getDraftPresetForCurrentView();
	if (draftPreset == nullptr || index >= (int)draftPreset->boxes.size())
	{
		return nullptr;
	}
	return draftPreset->boxes[index];
}

JPbox *JPboxgroup::getEditableBoxForRealIndex(int index)
{
	if (index < 0 || index >= boxes.size())
	{
		return nullptr;
	}
	JPbox *draftBox = getCueDraftBoxForRealIndex(index);
	if (draftBox != nullptr)
	{
		return draftBox;
	}
	return boxes[index];
}

bool JPboxgroup::beginCueDraftForBoxIndex(int index)
{
	if (index < 0 || index >= getCueTargetBoxSize() || getCueTargetBoxSize() == 0 ||
		getCueTargetActiveRender() < 0 || getCueTargetActiveRender() >= getCueTargetBoxSize())
	{
		return false;
	}
	if (getCueTargetBoxAt(index) == nullptr)
	{
		return false;
	}
	return buildCueDraftGraph(index);
}

bool JPboxgroup::buildCueDraftGraph(int sourceIndex)
{
	if (sourceIndex < 0 || sourceIndex >= getCueTargetBoxSize() ||
		getCueTargetBoxAt(sourceIndex) == nullptr)
	{
		clearCueDraft();
		cueState.mode = CUE_NONE;
		cueState.sourceIndex = -1;
		cueState.previewIndex = -1;
		return false;
	}

	bool draftWasInspectorTarget = isCueDraftMode() && cueSelectedIndex() == cueState.sourceIndex;
	clearCueDraft();

	vector<JPbox*> &targetBoxes = getCueTargetBoxes();
	for (int realIndex = 0; realIndex < (int)targetBoxes.size(); realIndex++)
	{
		if (targetBoxes[realIndex] == nullptr)
		{
			continue;
		}
		JPbox *draft = cloneBoxForCueDraft(realIndex);
		if (draft == nullptr)
		{
			clearCueDraft();
			cueState.mode = CUE_NONE;
			cueState.sourceIndex = -1;
			cueState.previewIndex = -1;
			if (draftWasInspectorTarget)
			{
				setControllers();
			}
			return false;
		}
		cueState.draftBoxes.push_back(draft);
		cueState.draftRealIndices.push_back(realIndex);
		cueState.draftDirtyFlags.push_back(CUE_DIRTY_NONE);
		cueState.draftBaselineParameters.push_back(draft->parameters);
		cueState.draftBaselineOnOff.push_back(draft->getonoff());
		cueState.draftBaselineBypass.push_back(draft->getBypass());
		if (realIndex == sourceIndex)
		{
			cueState.draftSourceBox = draft;
		}
	}

	cueState.mode = cueState.draftSourceBox != nullptr ? CUE_DRAFT_CHAIN : CUE_NONE;
	cueState.sourceIndex = sourceIndex;
	cueState.previewIndex = -1;
	cueState.stagedActiveRenderIndex = getCueTargetActiveRender();
	setCueStagedActiveRenderIndex(cueState.stagedActiveRenderIndex);
	cueFullscreenPreview = false;
	rewireCueDraftGraph();
	updateCueDraftGraph();
	// Rebuild the inspector so its sliders bind to the DRAFT box's parameters
	// (group-aware: cueSelectedIndex() is groupInspectorIndex in group view). Without
	// this, editing in a group cue would keep hitting the live box's parameters.
	if (getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr)
	{
		setControllers();
	}
	return isCueDraftMode();
}

bool JPboxgroup::collectCueDraftPath(int currentIndex, int activeIndex, vector<int> &path, vector<bool> &visiting)
{
	if (currentIndex < 0 || currentIndex >= boxes.size() || visiting[currentIndex])
	{
		return false;
	}
	if (currentIndex == activeIndex)
	{
		if (std::find(path.begin(), path.end(), currentIndex) == path.end())
		{
			path.insert(path.begin(), currentIndex);
		}
		return true;
	}

	visiting[currentIndex] = true;
	bool foundPath = false;
	string currentName = boxes[currentIndex]->name;
	for (int consumerIndex = 0; consumerIndex < boxes.size(); consumerIndex++)
	{
		if (consumerIndex == currentIndex || boxes[consumerIndex] == nullptr)
		{
			continue;
		}
		for (int linkIndex = 0; linkIndex < boxes[consumerIndex]->fbohandlergroup.getSize(); linkIndex++)
		{
			if (boxes[consumerIndex]->fbohandlergroup.getisPointerSet(linkIndex) &&
				boxes[consumerIndex]->fbohandlergroup.getFboName(linkIndex) == currentName &&
				collectCueDraftPath(consumerIndex, activeIndex, path, visiting))
			{
				foundPath = true;
			}
		}
	}
	visiting[currentIndex] = false;

	if (foundPath && std::find(path.begin(), path.end(), currentIndex) == path.end())
	{
		path.insert(path.begin(), currentIndex);
	}
	return foundPath;
}

JPbox *JPboxgroup::cloneBoxForCueDraft(int index)
{
	if (index < 0 || index >= getCueTargetBoxSize() || getCueTargetBoxAt(index) == nullptr)
	{
		return nullptr;
	}

	JPbox *source = getCueTargetBoxAt(index);
	JPbox *draft = nullptr;
	const int type = source->getTipo();
	if (rendersCueDraft(type) || type == source->PRESETBOX)
	{
		string draftName = source->name + "_cue_draft";
		draft = jp_box_factory::create(source->dir, jp_box_factory::Context::Interactive);
		if (draft == nullptr)
		{
			return nullptr;
		}
		draft->setup(source->dir, draftName);
	}
	else
	{
		draft = new JPbox();
		draft->setup(source->dir, source->name + "_cue_draft");
	}
	copyEditableBoxState(draft, source);
    // Preserve the held frame when the source is paused or its pixels are
    // still loading. Subsequent updates render the draft's own parameters.
    if (type == JPbox::IMAGEBOX || type == JPbox::CAMBOX)
        copyFboStraight(source->fbo, draft->fbo);
	// A preset draft is reloaded from disk by the factory/setup, so seed
	// its internal sub-box state from the LIVE preset. Without this the draft (and
	// its exposed-param sliders) would show stale on-disk values, and the CUE
	// preview would not match the live composite.
	if (type == source->PRESETBOX)
	{
		JPbox_preset *draftPreset =
			dynamic_cast<JPbox_preset *>(draft);
		JPbox_preset *sourcePreset =
			dynamic_cast<JPbox_preset *>(source);
		if (!synchronizeCuePresetStructure(
				draftPreset, sourcePreset))
		{
			draft->clear();
			delete draft;
			return nullptr;
		}
		copyPresetInternalState(draftPreset, sourcePreset);
	}
	draft->name = source->name;
	// The draft is a staging view of the SAME box, not a new one: the inspector
	// hands it out in place of the live box for the duration of the cue. Give it
	// a separate identity and anything asking "which box is this output bound
	// to" would see two identities alternate as the cue opens and closes.
	//
	// Note this is assigned HERE, by the caller. copyEditableBoxState and
	// copyPresetInternalState must never copy uid - doing so would write the
	// draft's identity onto the live box when the cue is applied.
	draft->uid = source->uid;
	return draft;
}

void JPboxgroup::copyEditableBoxState(JPbox *destination, JPbox *source)
{
	if (destination == nullptr || source == nullptr)
	{
		return;
	}
	destination->parameters = source->parameters;
	destination->setonoff(source->getonoff());
	destination->setBypass(source->getBypass());
	destination->setPos(source->x, source->y);
	destination->copyCustomStateFrom(source);
}

void JPboxgroup::copyBoxLinksByName(
	JPbox *destination,
	JPbox *source,
	const vector<JPbox *> &destinationSiblings)
{
	if (destination == nullptr || source == nullptr)
	{
		return;
	}
	for (int destinationIndex = 0;
		 destinationIndex < destination->fbohandlergroup.getSize();
		 destinationIndex++)
	{
		const string samplerName =
			destination->fbohandlergroup.getName(destinationIndex);
		const int sourceIndex =
			source->fbohandlergroup.findIndexByName(samplerName);
		if (sourceIndex < 0 ||
			!source->fbohandlergroup.getisPointerSet(sourceIndex))
		{
			destination->fbohandlergroup.deleteFboPointer(destinationIndex);
			continue;
		}

		const string linkedName =
			source->fbohandlergroup.getFboName(sourceIndex);
		JPbox *linkedDestination = nullptr;
		for (JPbox *candidate : destinationSiblings)
		{
			if (candidate != nullptr && candidate->name == linkedName)
			{
				linkedDestination = candidate;
				break;
			}
		}
		if (linkedDestination != nullptr)
		{
			destination->fbohandlergroup.setFboPointer(
				&linkedDestination->fbo,
				&linkedDestination->name,
				destinationIndex);
		}
		else
		{
			destination->fbohandlergroup.deleteFboPointer(destinationIndex);
		}
	}
}

void JPboxgroup::snapshotPresetLinks(
	JPbox_preset *preset,
	vector<PresetLinkAssignment> &assignments,
	const vector<string> &presetPath) const
{
	if (preset == nullptr)
	{
		return;
	}
	for (JPbox *box : preset->boxes)
	{
		if (box == nullptr)
		{
			continue;
		}
		for (int linkIndex = 0;
			 linkIndex < box->fbohandlergroup.getSize();
			 linkIndex++)
		{
			if (preset->isExposedTextureInputTarget(
					box->name,
					box->fbohandlergroup.getName(linkIndex)))
			{
				continue;
			}
			PresetLinkAssignment assignment;
			assignment.presetPath = presetPath;
			assignment.boxName = box->name;
			assignment.samplerName =
				box->fbohandlergroup.getName(linkIndex);
			assignment.connected =
				box->fbohandlergroup.getisPointerSet(linkIndex);
			if (assignment.connected)
			{
				assignment.sourceName =
					box->fbohandlergroup.getFboName(linkIndex);
			}
			assignments.push_back(assignment);
		}

		if (box->getTipo() == JPbox::PRESETBOX)
		{
			vector<string> childPath = presetPath;
			childPath.push_back(box->name);
			snapshotPresetLinks(
				dynamic_cast<JPbox_preset *>(box),
				assignments,
				childPath);
		}
	}
}

void JPboxgroup::restorePresetLinks(
	JPbox_preset *preset,
	const vector<PresetLinkAssignment> &assignments)
{
	if (preset == nullptr)
	{
		return;
	}

	for (const PresetLinkAssignment &assignment : assignments)
	{
		JPbox_preset *parentPreset = preset;
		for (const string &presetName : assignment.presetPath)
		{
			JPbox_preset *nextPreset = nullptr;
			for (JPbox *candidate : parentPreset->boxes)
			{
				if (candidate != nullptr &&
					candidate->name == presetName &&
					candidate->getTipo() == JPbox::PRESETBOX)
				{
					nextPreset = dynamic_cast<JPbox_preset *>(candidate);
					break;
				}
			}
			parentPreset = nextPreset;
			if (parentPreset == nullptr)
			{
				break;
			}
		}
		if (parentPreset == nullptr)
		{
			continue;
		}

		JPbox *targetBox = nullptr;
		JPbox *sourceBox = nullptr;
		for (JPbox *candidate : parentPreset->boxes)
		{
			if (candidate == nullptr)
			{
				continue;
			}
			if (candidate->name == assignment.boxName)
			{
				targetBox = candidate;
			}
			if (assignment.connected &&
				candidate->name == assignment.sourceName)
			{
				sourceBox = candidate;
			}
		}
		if (targetBox == nullptr)
		{
			continue;
		}

		const int linkIndex =
			targetBox->fbohandlergroup.findIndexByName(
				assignment.samplerName);
		if (linkIndex < 0)
		{
			continue;
		}
		if (assignment.connected && sourceBox != nullptr)
		{
			targetBox->fbohandlergroup.setFboPointer(
				&sourceBox->fbo, &sourceBox->name, linkIndex);
		}
		else
		{
			targetBox->fbohandlergroup.deleteFboPointer(linkIndex);
		}
	}
}

JPbox_preset *JPboxgroup::getDraftPresetForCurrentView() const
{
	if (!isCueDraftMode() || activeGroupPath.empty())
	{
		return nullptr;
	}

	JPbox_preset *draftPreset = dynamic_cast<JPbox_preset *>(
		getCueDraftBoxForRealIndex(activeGroupPath[0]));
	if (draftPreset == nullptr)
	{
		return nullptr;
	}

	for (size_t depth = 1; depth < activeGroupPath.size(); depth++)
	{
		int index = activeGroupPath[depth];
		if (index < 0 || index >= (int)draftPreset->boxes.size() ||
			draftPreset->boxes[index] == nullptr)
		{
			return nullptr;
		}
		draftPreset = dynamic_cast<JPbox_preset *>(draftPreset->boxes[index]);
		if (draftPreset == nullptr)
		{
			return nullptr;
		}
	}
	return draftPreset;
}

void JPboxgroup::copyPresetInternalState(JPbox_preset *destination, JPbox_preset *source)
{
	if (destination == nullptr || source == nullptr)
	{
		return;
	}
	destination->activeRender = source->boxes.empty() ? 0 :
		ofClamp(source->activeRender, 0, (int)source->boxes.size() - 1);
	destination->exposedParams = source->exposedParams;
	destination->exposedParamOriginalIndices =
		source->exposedParamOriginalIndices;
	destination->setExposedTextureInputs(
		source->exposedTextureInputs);
	int n = std::min((int)destination->boxes.size(), (int)source->boxes.size());
	for (int i = 0; i < n; i++)
	{
		if (destination->boxes[i] == nullptr || source->boxes[i] == nullptr)
		{
			continue;
		}
		destination->boxes[i]->parameters = source->boxes[i]->parameters; // deep copy
		destination->boxes[i]->setonoff(source->boxes[i]->getonoff());
		destination->boxes[i]->setBypass(source->boxes[i]->getBypass());
		destination->boxes[i]->copyCustomStateFrom(
			source->boxes[i]);
        if (source->boxes[i]->getTipo() == JPbox::IMAGEBOX || source->boxes[i]->getTipo() == JPbox::CAMBOX)
            copyFboStraight(source->boxes[i]->fbo, destination->boxes[i]->fbo);
		copyBoxLinksByName(
			destination->boxes[i],
			source->boxes[i],
			destination->boxes);
		if (destination->boxes[i]->getTipo() == JPbox::PRESETBOX &&
			source->boxes[i]->getTipo() == JPbox::PRESETBOX)
		{
			copyPresetInternalState(dynamic_cast<JPbox_preset *>(destination->boxes[i]),
									dynamic_cast<JPbox_preset *>(source->boxes[i]));
		}
	}
	destination->syncExposedTextureInputs();
}

bool JPboxgroup::synchronizeCuePresetStructure(
	JPbox_preset *destination,
	JPbox_preset *source)
{
	if (destination == nullptr || source == nullptr)
	{
		return false;
	}

	bool structureMatches =
		destination->boxes.size() == source->boxes.size();
	if (structureMatches)
	{
		for (int i = 0; i < (int)source->boxes.size(); i++)
		{
			JPbox *destinationBox = destination->boxes[i];
			JPbox *sourceBox = source->boxes[i];
			if (destinationBox == nullptr || sourceBox == nullptr ||
				destinationBox->name != sourceBox->name ||
				destinationBox->dir != sourceBox->dir ||
				destinationBox->getTipo() != sourceBox->getTipo())
			{
				structureMatches = false;
				break;
			}
		}
	}

	if (!structureMatches)
	{
		destination->clear();
		for (JPbox *sourceBox : source->boxes)
		{
			if (sourceBox == nullptr)
			{
				continue;
			}
			string cloneName = sourceBox->name;
			JPbox *clone =
				jp_box_factory::create(sourceBox->dir, jp_box_factory::Context::Interactive);
			if (clone == nullptr)
			{
				destination->clear();
				return false;
			}
			clone->setup(sourceBox->dir, cloneName);
			clone->name = sourceBox->name;
			// Mirrors the live child, so it carries the same identity - see the
			// note in the draft clone above.
			clone->uid = sourceBox->uid;
			copyEditableBoxState(clone, sourceBox);
			destination->boxes.push_back(clone);
		}
		destination->resizeExposedParams(
			(int)destination->boxes.size());
	}

	for (int i = 0;
		i < (int)source->boxes.size() &&
		i < (int)destination->boxes.size();
		i++)
	{
		JPbox *destinationBox = destination->boxes[i];
		JPbox *sourceBox = source->boxes[i];
		if (destinationBox == nullptr || sourceBox == nullptr)
		{
			continue;
		}
		if (sourceBox->getTipo() == JPbox::PRESETBOX)
		{
			if (destinationBox->getTipo() != JPbox::PRESETBOX ||
				!synchronizeCuePresetStructure(
					dynamic_cast<JPbox_preset *>(
						destinationBox),
					dynamic_cast<JPbox_preset *>(
						sourceBox)))
			{
				return false;
			}
		}
	}
	return true;
}

void JPboxgroup::snapshotPresetActiveRenders(JPbox_preset *source, vector<int> &values) const
{
	if (source == nullptr)
	{
		return;
	}
	values.push_back(source->activeRender);
	for (JPbox *box : source->boxes)
	{
		if (box != nullptr && box->getTipo() == JPbox::PRESETBOX)
		{
			snapshotPresetActiveRenders(dynamic_cast<JPbox_preset *>(box), values);
		}
	}
}

void JPboxgroup::restorePresetActiveRenders(JPbox_preset *destination, const vector<int> &values, int &valueIndex) const
{
	if (destination == nullptr || valueIndex >= (int)values.size())
	{
		return;
	}
	destination->activeRender = destination->boxes.empty() ? 0 :
		ofClamp(values[valueIndex], 0, (int)destination->boxes.size() - 1);
	valueIndex++;
	for (JPbox *box : destination->boxes)
	{
		if (box != nullptr && box->getTipo() == JPbox::PRESETBOX)
		{
			restorePresetActiveRenders(dynamic_cast<JPbox_preset *>(box), values, valueIndex);
		}
	}
}

int JPboxgroup::findCueDraftCloneIndexForRealIndex(int index) const
{
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		if (cueState.draftRealIndices[i] == index)
		{
			return i;
		}
	}
	return -1;
}

bool JPboxgroup::isCueSourceIndex(int index) const
{
	if (isCueDraftMode())
	{
		return isCueDraftRealIndex(index);
	}
	return hasCue() && cueState.sourceIndex == index;
}

bool JPboxgroup::isCueDraftRealIndex(int index) const
{
	return findCueDraftCloneIndexForRealIndex(index) >= 0;
}

bool JPboxgroup::isRealIndexDraftEditable(int index) const
{
	return isCueDraftMode() && isCueDraftRealIndex(index);
}

bool JPboxgroup::isCueDraftDirty(int index) const
{
	unsigned int flags = getCueDraftDirtyFlags(index);
	return (flags & ~CUE_DIRTY_STAGED_ACTIVE) != CUE_DIRTY_NONE;
}

unsigned int JPboxgroup::getCueDraftDirtyFlags(int index) const
{
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex < 0 || draftIndex >= cueState.draftDirtyFlags.size())
	{
		return CUE_DIRTY_NONE;
	}
	return cueState.draftDirtyFlags[draftIndex];
}

bool JPboxgroup::isCueDeletedRealIndex(int index) const
{
	return (getCueDraftDirtyFlags(index) & CUE_DIRTY_DELETED) != 0;
}

bool JPboxgroup::isCueDraftMode() const
{
	return cueState.mode == CUE_DRAFT_CHAIN;
}

bool JPboxgroup::isCueNormalPreviewMode() const
{
	return cueState.mode == CUE_NORMAL_PREVIEW;
}

bool JPboxgroup::setCueStagedActiveRenderIndex(int index)
{
	if (!hasCue() || index < 0 || index >= getCueTargetBoxSize() || getCueTargetBoxAt(index) == nullptr)
	{
		return false;
	}
	cueState.stagedActiveRenderIndex = index;
	// The CUE OUTPUT preview renders cueState.draftOutputBox. Point it at the
	// DRAFT clone of the staged active render (not the live box), and refresh it
	// every time the staged index changes, so the preview reflects staged edits.
	JPbox *draftOut = getCueDraftBoxForRealIndex(index);
	cueState.draftOutputBox = draftOut != nullptr ? draftOut : getCueTargetBoxAt(index);
	cueState.draftOutputRealIndex = index;
	markCueDraftDirty(cueState.sourceIndex, CUE_DIRTY_STAGED_ACTIVE);
	return true;
}

void JPboxgroup::markCueDraftDirty(int index, unsigned int flags)
{
	if (!isCueDraftMode() || !isCueDraftRealIndex(index))
	{
		return;
	}
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex >= 0 && draftIndex < cueState.draftDirtyFlags.size())
	{
		cueState.draftDirtyFlags[draftIndex] |= flags;
	}
	if (std::find(cueState.dirtyDraftRealIndices.begin(),
				  cueState.dirtyDraftRealIndices.end(),
				  index) == cueState.dirtyDraftRealIndices.end())
	{
		cueState.dirtyDraftRealIndices.push_back(index);
	}
}

void JPboxgroup::removeCueDraftDirty(int index, unsigned int flags)
{
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex >= 0 && draftIndex < cueState.draftDirtyFlags.size())
	{
		if (flags == 0)
		{
			cueState.draftDirtyFlags[draftIndex] = CUE_DIRTY_NONE;
		}
		else
		{
			cueState.draftDirtyFlags[draftIndex] &= ~flags;
		}
	}
	if (getCueDraftDirtyFlags(index) == CUE_DIRTY_NONE)
	{
		cueState.dirtyDraftRealIndices.erase(
			std::remove(cueState.dirtyDraftRealIndices.begin(),
						cueState.dirtyDraftRealIndices.end(),
						index),
			cueState.dirtyDraftRealIndices.end());
	}
}

bool JPboxgroup::isCueAddedRealIndex(int index) const
{
	return std::find(cueState.cueAddedRealIndices.begin(),
					 cueState.cueAddedRealIndices.end(),
					 index) != cueState.cueAddedRealIndices.end();
}

void JPboxgroup::addCueAddedRealIndex(int index)
{
	if (index < 0 || index >= boxes.size())
	{
		return;
	}
	if (!isCueAddedRealIndex(index))
	{
		cueState.cueAddedRealIndices.push_back(index);
	}
	markCueDraftDirty(index, CUE_DIRTY_ADDED);
}

vector<int> JPboxgroup::getCueDirtyIndices(unsigned int mask) const
{
	vector<int> result;
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		if (i >= cueState.draftDirtyFlags.size())
		{
			continue;
		}
		unsigned int flags = cueState.draftDirtyFlags[i];
		if (flags == CUE_DIRTY_NONE)
		{
			continue;
		}
		if (mask != 0 && (flags & mask) == 0)
		{
			continue;
		}
		result.push_back(cueState.draftRealIndices[i]);
	}
	return result;
}

string JPboxgroup::getCueDirtySummary() const
{
	int params = 0;
	int bypassPause = 0;
	int links = 0;
	int added = 0;
	int deleted = 0;
	bool stagedActive = false;
	bool presetActive = false;
	for (int i = 0; i < cueState.draftDirtyFlags.size(); i++)
	{
		unsigned int flags = cueState.draftDirtyFlags[i];
		if (flags & CUE_DIRTY_PARAMS) params++;
		if (flags & CUE_DIRTY_BYPASS_PAUSE) bypassPause++;
		if (flags & CUE_DIRTY_LINKS) links++;
		if (flags & CUE_DIRTY_ADDED) added++;
		if (flags & CUE_DIRTY_DELETED) deleted++;
		if (flags & CUE_DIRTY_STAGED_ACTIVE) stagedActive = true;
		if (flags & CUE_DIRTY_PRESET_ACTIVE) presetActive = true;
	}
	vector<string> parts;
	if (params > 0) parts.push_back("params " + ofToString(params));
	if (bypassPause > 0) parts.push_back("pause " + ofToString(bypassPause));
	if (links > 0) parts.push_back("links " + ofToString(links));
	if (added > 0) parts.push_back("new " + ofToString(added));
	if (deleted > 0) parts.push_back("delete " + ofToString(deleted));
	if (stagedActive) parts.push_back("active");
	if (presetActive) parts.push_back("group active");
	if (parts.empty())
	{
		return "";
	}
	string summary = " (";
	for (int i = 0; i < parts.size(); i++)
	{
		if (i > 0)
		{
			summary += ", ";
		}
		summary += parts[i];
	}
	summary += ")";
	return summary;
}

bool JPboxgroup::revertCueDraftBox(int index)
{
	// Operate on the cue's target graph (main or the active preset in group view).
	vector<JPbox *> &tboxes = getCueTargetBoxes();
	int &tActiveRender = getCueTargetActiveRender();
	int *selPtr = isGroupViewActive() ? &groupInspectorIndex : &openguinumber;

	if (!isCueDraftMode() || index < 0 || index >= (int)tboxes.size())
	{
		return false;
	}
	if (isCueAddedRealIndex(index))
	{
		string deletedName = tboxes[index]->name;
		for (int k = (int)tboxes.size() - 1; k >= 0; k--)
		{
			if (k == index || tboxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < tboxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (tboxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					tboxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		dropFinalLayersForDestroyedBox(tboxes[index]);
		tboxes[index]->clear();
		delete tboxes[index];
		tboxes[index] = nullptr;
		tboxes.erase(tboxes.begin() + index);
		cueState.cueAddedRealIndices.erase(
			std::remove(cueState.cueAddedRealIndices.begin(),
						cueState.cueAddedRealIndices.end(),
						index),
			cueState.cueAddedRealIndices.end());
		removeCueDraftDirty(index);
		for (int &addedIndex : cueState.cueAddedRealIndices)
		{
			if (addedIndex > index)
			{
				addedIndex--;
			}
		}
		for (int &dirtyIndex : cueState.dirtyDraftRealIndices)
		{
			if (dirtyIndex > index)
			{
				dirtyIndex--;
			}
		}
		if (*selPtr == index)
		{
			*selPtr = -1;
			for (int c = 0; c < controllers.size(); c++)
			{
				delete controllers[c];
				controllers[c] = nullptr;
			}
			controllers.clear();
		}
		else if (*selPtr > index)
		{
			(*selPtr)--;
		}
		if (tActiveRender > index)
		{
			tActiveRender--;
		}
		tActiveRender = tboxes.empty() ? 0 : ofClamp(tActiveRender, 0, int(tboxes.size()) - 1);
		requestCueRebuild();
		return true;
	}
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
		draftIndex >= cueState.draftBaselineParameters.size() ||
		tboxes[index] == nullptr || cueState.draftBoxes[draftIndex] == nullptr)
	{
		return false;
	}
	cueState.draftBoxes[draftIndex]->parameters = cueState.draftBaselineParameters[draftIndex];
	cueState.draftBoxes[draftIndex]->setonoff(cueState.draftBaselineOnOff[draftIndex]);
	cueState.draftBoxes[draftIndex]->setBypass(cueState.draftBaselineBypass[draftIndex]);
	cueState.draftBoxes[draftIndex]->copyCustomStateFrom(
		tboxes[index]);
	if (cueState.draftBoxes[draftIndex]->getTipo() == JPbox::PRESETBOX &&
		tboxes[index]->getTipo() == JPbox::PRESETBOX)
	{
		copyPresetInternalState(
			dynamic_cast<JPbox_preset *>(cueState.draftBoxes[draftIndex]),
			dynamic_cast<JPbox_preset *>(tboxes[index]));
	}
	removeCueDraftDirty(index);
	if (cueSelectedIndex() == index)
	{
		setControllers();
	}
	rewireCueDraftGraph();
	updateCueDraftGraph();
	return true;
}

void JPboxgroup::removeCueAddedBoxesFromRealGraph()
{
	if (cueState.cueAddedRealIndices.empty())
	{
		return;
	}
	// Operate on the cue's target graph (main boxes, or the target preset's
	// boxes in group view) and keep the matching selection index consistent.
	vector<JPbox *> &tboxes = getCueTargetBoxes();
	int &tActiveRender = getCueTargetActiveRender();
	int *selPtr = isGroupViewActive() ? &groupInspectorIndex : &openguinumber;

	vector<int> added = cueState.cueAddedRealIndices;
	std::sort(added.begin(), added.end(), std::greater<int>());
	added.erase(std::unique(added.begin(), added.end()), added.end());
	cueState.cueAddedRealIndices.clear();
	for (int i = 0; i < added.size(); i++)
	{
		int index = added[i];
		if (index < 0 || index >= (int)tboxes.size() || tboxes[index] == nullptr)
		{
			continue;
		}
		string deletedName = tboxes[index]->name;
		for (int k = (int)tboxes.size() - 1; k >= 0; k--)
		{
			if (k == index || tboxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < tboxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (tboxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					tboxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		dropFinalLayersForDestroyedBox(tboxes[index]);
		tboxes[index]->clear();
		delete tboxes[index];
		tboxes[index] = nullptr;
		tboxes.erase(tboxes.begin() + index);
		if (*selPtr == index)
		{
			*selPtr = -1;
			for (int c = 0; c < controllers.size(); c++)
			{
				delete controllers[c];
				controllers[c] = nullptr;
			}
			controllers.clear();
		}
		else if (*selPtr > index)
		{
			(*selPtr)--;
		}
		if (tActiveRender > index)
		{
			tActiveRender--;
		}
	}
	if (!tboxes.empty())
	{
		tActiveRender = ofClamp(tActiveRender, 0, int(tboxes.size()) - 1);
	}
	else
	{
		tActiveRender = 0;
	}
}
void JPboxgroup::removeCueAddedGroupBoxes()
{
	if (cueAddedGroupBoxes.empty())
	{
		return;
	}
	for (auto &entry : cueAddedGroupBoxes)
	{
		JPbox_preset *preset = entry.first;
		JPbox *box = entry.second;
		if (preset == nullptr || box == nullptr)
		{
			continue;
		}
		int idx = -1;
		for (int i = 0; i < (int)preset->boxes.size(); i++)
		{
			if (preset->boxes[i] == box)
			{
				idx = i;
				break;
			}
		}
		if (idx < 0)
		{
			continue; // already removed
		}
		// Drop any sibling links referencing this box.
		string deletedName = box->name;
		for (int k = 0; k < (int)preset->boxes.size(); k++)
		{
			if (k == idx || preset->boxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < preset->boxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (preset->boxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					preset->boxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		dropFinalLayersForDestroyedBox(box);
		box->clear();
		delete box;
		preset->boxes.erase(preset->boxes.begin() + idx);
		if (preset->activeRender > idx)
		{
			preset->activeRender--;
		}
		preset->activeRender = preset->boxes.empty() ? 0 : ofClamp(preset->activeRender, 0, (int)preset->boxes.size() - 1);
		// Fix the group-view selection if it pointed at/after the removed box.
		if (isGroupViewActive() && getActivePreset() == preset)
		{
			if (groupInspectorIndex == idx)
			{
				groupInspectorIndex = -1;
			}
			else if (groupInspectorIndex > idx)
			{
				groupInspectorIndex--;
			}
		}
	}
	cueAddedGroupBoxes.clear();
}

bool JPboxgroup::commitCueDraftLink(int targetRealIndex, int linkIndex, int sourceRealIndex)
{
	vector<JPbox *> &target = getCueTargetBoxes();
	if (!isCueDraftMode() ||
		targetRealIndex < 0 || targetRealIndex >= (int)target.size() ||
		sourceRealIndex < 0 || sourceRealIndex >= (int)target.size())
	{
		return false;
	}
	JPbox *targetDraft = getCueDraftBoxForRealIndex(targetRealIndex);
	JPbox *sourceDraft = getCueDraftBoxForRealIndex(sourceRealIndex);
	if (targetDraft == nullptr ||
		linkIndex < 0 ||
		linkIndex >= targetDraft->fbohandlergroup.getSize() ||
		target[sourceRealIndex] == nullptr)
	{
		return false;
	}
	if (sourceDraft != nullptr)
	{
		targetDraft->fbohandlergroup.setFboPointer(&sourceDraft->fbo, &sourceDraft->name, linkIndex);
	}
	else
	{
		targetDraft->fbohandlergroup.setFboPointer(&target[sourceRealIndex]->fbo, &target[sourceRealIndex]->name, linkIndex);
	}
	markCueDraftDirty(targetRealIndex, CUE_DIRTY_LINKS);
	updateCueDraftGraph();
	return true;
}

void JPboxgroup::copyCueDraftLinksToReal(int realIndex)
{
	vector<JPbox *> &target = getCueTargetBoxes();
	int draftIndex = findCueDraftCloneIndexForRealIndex(realIndex);
	if (draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
		realIndex < 0 || realIndex >= (int)target.size() ||
		cueState.draftBoxes[draftIndex] == nullptr ||
		target[realIndex] == nullptr)
	{
		return;
	}
	JPbox *draftBox = cueState.draftBoxes[draftIndex];
	int maxLinks = std::min(draftBox->fbohandlergroup.getSize(), target[realIndex]->fbohandlergroup.getSize());
	for (int linkIndex = 0; linkIndex < maxLinks; linkIndex++)
	{
		if (!draftBox->fbohandlergroup.getisPointerSet(linkIndex))
		{
			target[realIndex]->fbohandlergroup.deleteFboPointer(linkIndex);
			continue;
		}
		string linkedName = draftBox->fbohandlergroup.getFboName(linkIndex);
		int linkedRealIndex = findCueTargetBoxIndexByName(linkedName);
		if (linkedRealIndex >= 0 && linkedRealIndex < (int)target.size() && target[linkedRealIndex] != nullptr)
		{
			target[realIndex]->fbohandlergroup.setFboPointer(&target[linkedRealIndex]->fbo,
															&target[linkedRealIndex]->name,
															linkIndex);
		}
		else
		{
			target[realIndex]->fbohandlergroup.deleteFboPointer(linkIndex);
		}
	}
}

void JPboxgroup::rewireCueDraftGraph()
{
	vector<JPbox *> &target = getCueTargetBoxes();
	for (int draftIndex = 0; draftIndex < cueState.draftBoxes.size(); draftIndex++)
	{
		int realIndex = cueState.draftRealIndices[draftIndex];
		if (realIndex < 0 || realIndex >= (int)target.size() || cueState.draftBoxes[draftIndex] == nullptr)
		{
			continue;
		}
		if ((getCueDraftDirtyFlags(realIndex) & CUE_DIRTY_LINKS) != 0)
		{
			continue;
		}
		for (int linkIndex = 0; linkIndex < cueState.draftBoxes[draftIndex]->fbohandlergroup.getSize() &&
			 linkIndex < target[realIndex]->fbohandlergroup.getSize(); linkIndex++)
		{
			if (!target[realIndex]->fbohandlergroup.getisPointerSet(linkIndex))
			{
				continue;
			}
			string linkedName = target[realIndex]->fbohandlergroup.getFboName(linkIndex);
			int linkedRealIndex = findCueTargetBoxIndexByName(linkedName);
			int linkedDraftIndex = findCueDraftCloneIndexForRealIndex(linkedRealIndex);
			if (linkedDraftIndex >= 0 && linkedDraftIndex < cueState.draftBoxes.size())
			{
				cueState.draftBoxes[draftIndex]->fbohandlergroup.setFboPointer(&cueState.draftBoxes[linkedDraftIndex]->fbo,
																			   &cueState.draftBoxes[linkedDraftIndex]->name,
																			   linkIndex);
			}
			else if (linkedRealIndex >= 0 && linkedRealIndex < (int)target.size())
			{
				cueState.draftBoxes[draftIndex]->fbohandlergroup.setFboPointer(&target[linkedRealIndex]->fbo,
																			   &target[linkedRealIndex]->name,
																			   linkIndex);
			}
		}
	}
}

void JPboxgroup::updateCueDraftGraph()
{
	vector<JPbox *> &target = getCueTargetBoxes();
	vector<bool> required(cueState.draftBoxes.size(), false);
	std::function<void(int)> markDraftDependencies = [&](int index)
	{
		if (index < 0 || index >= (int)cueState.draftBoxes.size() ||
			required[index] || cueState.draftBoxes[index] == nullptr)
		{
			return;
		}
		required[index] = true;
		JPbox *consumer = cueState.draftBoxes[index];
		for (int inlet = 0; inlet < consumer->fbohandlergroup.getSize(); ++inlet)
		{
			if (!consumer->fbohandlergroup.getisPointerSet(inlet)) continue;
			ofFbo *input = consumer->fbohandlergroup.getFboPointerReference(inlet);
			for (int source = 0; source < (int)cueState.draftBoxes.size(); ++source)
			{
				JPbox *candidate = cueState.draftBoxes[source];
				if (candidate != nullptr && &candidate->fbo == input)
				{
					markDraftDependencies(source);
					break;
				}
			}
		}
	};
	auto markDraftBox = [&](JPbox *box)
	{
		for (int i = 0; box != nullptr && i < (int)cueState.draftBoxes.size(); ++i)
		{
			if (cueState.draftBoxes[i] == box)
			{
				markDraftDependencies(i);
				break;
			}
		}
	};
	markDraftBox(cueState.draftOutputBox);
	markDraftBox(getCuePreviewBox());
	for (int i = 0; i < (int)cueState.draftBoxes.size(); ++i)
	{
		if (cueState.draftBoxes[i] != nullptr)
		{
			cueState.draftBoxes[i]->setRenderThisFrame(required[i]);
		}
	}
	for (int i = 0; i < cueState.draftBoxes.size(); i++)
	{
		if (cueState.draftBoxes[i] == nullptr)
		{
			continue;
		}
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex >= 0 && realIndex < (int)target.size() && target[realIndex] != nullptr)
		{
			int type = target[realIndex]->getTipo();
			if (type == target[realIndex]->PRESETBOX)
			{
				if (isCueDraftDirty(realIndex))
				{
					// Staged edits: re-render the draft preset, re-rendering its
					// supported sub-boxes with their own parameters, sharing
                    // camera capture and mirroring unsupported sources.
					renderPresetDraftMirroringLive(dynamic_cast<JPbox_preset *>(cueState.draftBoxes[i]),
												   dynamic_cast<JPbox_preset *>(target[realIndex]));
				}
				else
				{
					// Clean passthrough: mirror the live composite.
					copyFboStraight(target[realIndex]->fbo,
						cueState.draftBoxes[i]->fbo);
				}
				continue;
			}
			if (!rendersCueDraft(type))
			{
				// Sources without an independent draft renderer mirror live output.
				cueState.draftBoxes[i]->update();
				copyFboStraight(target[realIndex]->fbo,
					cueState.draftBoxes[i]->fbo);
				continue;
			}
		}
		// Re-render supported boxes with staged parameters and draft graph inputs.
		if (cueState.draftBoxes[i] != nullptr)
		{
			cueState.draftBoxes[i]->update();
		}
	}
}
void JPboxgroup::renderPresetDraftMirroringLive(JPbox_preset *draftPreset, JPbox_preset *livePreset)
{
	if (draftPreset == nullptr || livePreset == nullptr)
	{
		return;
	}
	// This renderer intentionally bypasses JPbox_preset::update(), so resolve
	// staged public inlets here before any child shader samples its inputs.
	draftPreset->pruneInvalidExposedTextureInputs();
	draftPreset->syncExposedTextureInputs();
	int n = std::min((int)draftPreset->boxes.size(), (int)livePreset->boxes.size());
	// Render internal boxes back-to-front (dependency order, matching JPbox_preset::updateFBO).
	for (int i = n - 1; i >= 0; i--)
	{
		JPbox *d = draftPreset->boxes[i];
		JPbox *l = livePreset->boxes[i];
		if (d == nullptr || l == nullptr)
		{
			continue;
		}
		int t = l->getTipo();
		if (rendersCueDraft(t))
		{
			// Re-render with staged params, reading its (draft internal) inputs.
			d->update();
		}
		else if (t == JPbox::PRESETBOX)
		{
			renderPresetDraftMirroringLive(dynamic_cast<JPbox_preset *>(d),
										   dynamic_cast<JPbox_preset *>(l));
		}
		else if (d->fbo.isAllocated() && l->fbo.isAllocated())
		{
			// Other sources (video/spout/ndi): mirror the live
			// output instead of re-opening the device.
			copyFboStraight(l->fbo, d->fbo);
		}
	}
	// Composite through the same local crossfade used by live presets so a
	// staged group activation animates in the CUE preview as well.
	draftPreset->renderActiveRender();
}

void JPboxgroup::updateRealBoxesForCueApply()
{
	vector<JPbox *> &target = getCueTargetBoxes();
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex >= 0 && realIndex < (int)target.size() && target[realIndex] != nullptr)
		{
			target[realIndex]->update();
		}
	}
}

void JPboxgroup::copyParametersByNameOrIndex(JPParameterGroup &destination, JPParameterGroup &source)
{
	for (int srcIndex = 0; srcIndex < source.getSize(); srcIndex++)
	{
		int dstIndex = -1;
		string srcName = source.getName(srcIndex);
		for (int i = 0; i < destination.getSize(); i++)
		{
			if (destination.getName(i) == srcName)
			{
				dstIndex = i;
				break;
			}
		}
		if (dstIndex < 0 && srcIndex < destination.getSize())
		{
			dstIndex = srcIndex;
		}
		if (dstIndex < 0 || dstIndex >= destination.getSize() ||
			destination.getType(dstIndex) != source.getType(srcIndex))
		{
			continue;
		}
		if (source.getType(srcIndex) == source.FLOAT)
		{
			destination.setFloatValue(source.getFloatValue(srcIndex), dstIndex);
			destination.setFloatLerpValue(source.getLerpValue(srcIndex), dstIndex);
			destination.setRangeMin(source.getRangeMin(srcIndex), dstIndex);
			destination.setRangeMax(source.getRangeMax(srcIndex), dstIndex);
			destination.setRangeEnabled(
				source.getJParameter(srcIndex)->rangeEnabled, dstIndex);
			destination.setSpeed(source.getSpeed(srcIndex), dstIndex);
			destination.setBpmRate(source.getBpmRate(srcIndex), dstIndex);
			destination.setAudioSource(source.getAudioSource(srcIndex), dstIndex);
			destination.setAudioDiv(source.getAudioDiv(srcIndex), dstIndex);
			destination.setAudioBase(source.getAudioBase(srcIndex), dstIndex);
			destination.setAudioAmount(source.getAudioAmount(srcIndex), dstIndex);
			destination.setAudioInvert(source.getAudioInvert(srcIndex), dstIndex);
		destination.setAudioDrivesSpeed(source.getAudioDrivesSpeed(srcIndex), dstIndex);
		destination.setAudioSpeedDirection(source.getAudioSpeedDirection(srcIndex), dstIndex);
			destination.setAudioThreshold(source.getAudioThreshold(srcIndex), dstIndex);
			destination.setAudioCurve(source.getAudioCurve(srcIndex), dstIndex);
			destination.setAudioAttackMs(source.getAudioAttackMs(srcIndex), dstIndex);
			destination.setAudioReleaseMs(source.getAudioReleaseMs(srcIndex), dstIndex);
			destination.setmovetype(source.getMovType(srcIndex), dstIndex);
			destination.setlastmovetype(
				source.getLastMovType(srcIndex), dstIndex);
		}
		else if (source.getType(srcIndex) == source.BOOL)
		{
			destination.setBoolValue(source.getBoolValue(srcIndex), dstIndex);
		}
		JPParameter *destinationParameter = destination.getJParameter(dstIndex);
		JPParameter *sourceParameter = source.getJParameter(srcIndex);
		destinationParameter->randomLocked = sourceParameter->randomLocked;
		destinationParameter->defaultFloatValue =
			sourceParameter->defaultFloatValue;
		destinationParameter->defaultBoolValue =
			sourceParameter->defaultBoolValue;
	}
}
