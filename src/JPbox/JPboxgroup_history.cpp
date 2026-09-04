#include "JPboxgroup.h"

// Undo/redo for the node graph.
//
// The ring itself is in ../JPutils/jp_graph_history.h and knows nothing about
// openFrameworks. This file is the half that does: it resolves an edit's uids
// against a live box list and performs the mutation in both directions.
//
// Two rules run through all of it.
//
// Boxes are addressed by uid, never by index. Indices shift the moment anything
// is deleted, and `name` is assigned bare in two places so it is not unique
// either; repairBoxUids is what makes uid the one identifier that holds still.
//
// Nothing is ever destroyed on the undo path. A deleted box stays alive inside
// its history entry, so bringing it back costs no shader recompile and no video
// reload, and its address stays valid - which is the only reason severed inlets
// can be restored by handing back the very same &box->fbo pointer.

// ---------------------------------------------------------------- view lookup

vector<JPbox *> *JPboxgroup::historyBoxesForEdit(const JPGraphEdit &edit)
{
	if (edit.cueDraft) return &cueState.draftBoxes;
	if (edit.viewUid.empty()) return &boxes;
	JPbox_preset *preset = historyPresetForEdit(edit);
	return preset != nullptr ? &preset->boxes : nullptr;
}

JPbox_preset *JPboxgroup::historyPresetForEdit(const JPGraphEdit &edit)
{
	if (edit.cueDraft || edit.viewUid.empty()) return nullptr;
	JPbox *box = findBoxByUid(edit.viewUid);
	if (box == nullptr || box->getTipo() != JPbox::PRESETBOX) return nullptr;
	return static_cast<JPbox_preset *>(box);
}

int *JPboxgroup::historyActiveRenderForEdit(const JPGraphEdit &edit)
{
	if (edit.cueDraft) return nullptr;
	if (edit.viewUid.empty()) return activerender;
	JPbox_preset *preset = historyPresetForEdit(edit);
	return preset != nullptr ? &preset->activeRender : nullptr;
}

// The uid to stamp on an entry being recorded right now.
string JPboxgroup::currentViewUid() const
{
	if (isCueDraftMode()) return string();
	JPbox_preset *preset = isGroupViewActive() ? getActivePreset() : nullptr;
	return preset != nullptr ? preset->uid : string();
}

JPbox *JPboxgroup::findBoxInView(const vector<JPbox *> &list,
	const string &boxUid) const
{
	if (boxUid.empty()) return nullptr;
	for (JPbox *box : list)
	{
		if (box != nullptr && box->uid == boxUid) return box;
	}
	return nullptr;
}

JPbox *JPboxgroup::findBoxAnywhere(const string &boxUid) const
{
	if (boxUid.empty()) return nullptr;
	// The cue draft first: its clones deliberately carry the same uid as the real
	// boxes they stand in for, so searching the real graph would silently hand
	// back the box the draft exists to leave alone.
	if (isCueDraftMode())
	{
		JPbox *draft = findBoxInView(cueState.draftBoxes, boxUid);
		if (draft != nullptr) return draft;
	}
	return findBoxByUid(boxUid);
}

// Boxes referenced by a non-structural edit - a parameter value, an on/off - need
// not live in the view's own list: an exposed slider on a group writes straight
// into a child's parameter. Structural edits never use this, because where a box
// sits in WHICH list is the whole point of them.
JPbox *JPboxgroup::findBoxForEdit(const vector<JPbox *> &list,
	const string &boxUid) const
{
	JPbox *box = findBoxInView(list, boxUid);
	if (box != nullptr) return box;
	return findBoxAnywhere(boxUid);
}

void JPboxgroup::bindHistories()
{
	// Two rings for the whole composition, and neither has to be re-bound when a
	// group appears: an entry names its own view. This used to walk the tree
	// binding every preset's own ring, which meant any path that produced a
	// preset - load, addBox, paste, grouping - had to remember to call it, and
	// forgetting made Ctrl+Z silently do nothing inside that group.
	graphHistory.bind(this);
	cueDraftHistory.bind(this);
}

JPGraphUndoRing &JPboxgroup::currentViewHistory()
{
	// The cue draft keeps its own, because its boxes are clones that die when
	// the cue is applied or dropped. Everything else - the main graph and every
	// group, at any depth - shares one timeline.
	return isCueDraftMode() ? cueDraftHistory : graphHistory;
}

// How long a stream of changes to one parameter still counts as the same
// gesture. Long enough to bridge the gaps in a knob sweep, short enough that
// coming back to the same knob later is its own step.
static const unsigned long long kParameterCoalesceMs = 600;

void JPboxgroup::pushEdit(JPGraphEdit &&edit)
{
	// The one place an entry is stamped with the view it belongs to. Doing it
	// here rather than at each recording site means a new kind of edit cannot
	// forget, and forgetting would silently file it against the main graph.
	edit.cueDraft = isCueDraftMode();
	edit.viewUid = currentViewUid();
	edit.stampMs = (unsigned long long)ofGetElapsedTimeMillis();

	// One gesture, one step. A MIDI knob reports every step of its travel, so
	// without this a single sweep would push dozens of entries and evict the
	// whole history. A mouse drag never gets here more than once - it commits on
	// release - so this only ever folds together streams that have no release to
	// wait for.
	if (edit.kind == JPGraphEdit::SetParameter)
	{
		JPGraphEdit *newest = currentViewHistory().amendableNewest();
		if (newest != nullptr &&
			newest->kind == JPGraphEdit::SetParameter &&
			newest->cueDraft == edit.cueDraft &&
			newest->viewUid == edit.viewUid &&
			newest->boxUid == edit.boxUid &&
			newest->paramIndex == edit.paramIndex &&
			edit.stampMs - newest->stampMs <= kParameterCoalesceMs)
		{
			// Keep the ORIGINAL before: undo has to land where the gesture
			// started, not one message back.
			newest->paramAfter = edit.paramAfter;
			newest->stampMs = edit.stampMs;
			return;
		}
	}

	currentViewHistory().push(std::move(edit));
}

void JPboxgroup::invalidateCurrentViewRedo()
{
	currentViewHistory().invalidateRedo();
}

void JPboxgroup::afterHistoryChange(const JPGraphEdit &edit, bool applying,
	bool structural, bool retargetTransition)
{
	// Controllers hold raw JPParameter pointers into the boxes they were built
	// from, so any restored or removed box leaves them dangling. Rebuilding is
	// the same thing every structural path in this class already does.
	// Only when boxes came or went. Selection and inspector indices are
	// positions in the box list, so after a structural edit they address the
	// wrong box even when they are still in range - and setControllers() below
	// would build the panel from it. A value edit moves nothing, and dropping
	// the user's selection on every Ctrl+Z would just be rude.
	if (structural)
	{
		clearSelection();
		vector<JPbox *> *current = getCurrentViewBoxes();
		const int currentSize = current != nullptr ? (int)current->size() : 0;
		if (openguinumber >= currentSize) openguinumber = -1;
		if (groupInspectorIndex >= currentSize) groupInspectorIndex = -1;
		if (groupPreviewBoxIndex >= currentSize) groupPreviewBoxIndex = -1;
	}

	// Point the inspector at the box the step was about, before the panel is
	// built from it. Without this an undo of a slider or a bypass changes
	// something the user cannot see, which is the same complaint as undoing in a
	// view you are not looking at.
	focusInspectorOnEditBox(edit, applying);

	setControllers();
	if (!edit.cueDraft)
	{
		requestCueRebuild();
		int *active = historyActiveRenderForEdit(edit);
		vector<JPbox *> *list = historyBoxesForEdit(edit);
		if (active != nullptr && list != nullptr)
		{
			*active = ofClamp(*active, 0, std::max(0, (int)list->size() - 1));
			// Skipped for an active-render edit: it has already run
			// updateTransition itself, and calling it again with the value it
			// just set would see "nothing changed" and clear the morph it armed.
			if (edit.viewUid.empty() && retargetTransition)
			{
				updateTransition(*activerender);
			}
		}
	}
	else if (isCueDraftMode())
	{
		updateCueDraftGraph();
	}
}

// Which single box a step is about, or empty when it is about several or none.
// Direction matters for the active render: undo and redo leave a DIFFERENT box
// active, and the one worth showing is whichever ends up active.
static string editFocusUid(const JPGraphEdit &edit, bool applying)
{
	switch (edit.kind)
	{
	case JPGraphEdit::SetParameter:
	case JPGraphEdit::SetBoxState:
	case JPGraphEdit::SetConnection:
	case JPGraphEdit::ReorderInput:
		return edit.boxUid;
	case JPGraphEdit::SetActiveRender:
		return applying ? edit.activeUidAfter : edit.activeUidBefore;
	default:
		// Moving, deleting and grouping are visible on the canvas, and the
		// structural ones deliberately drop the inspector index because after
		// them it addresses a different box.
		return string();
	}
}

void JPboxgroup::focusInspectorOnEditBox(const JPGraphEdit &edit, bool applying)
{
	const string uid = editFocusUid(edit, applying);
	if (uid.empty()) return;
	vector<JPbox *> *list = getCurrentViewBoxes();
	if (list == nullptr) return;
	for (int i = 0; i < (int)list->size(); i++)
	{
		if ((*list)[i] == nullptr || (*list)[i]->uid != uid) continue;
		if (isGroupViewActive()) groupInspectorIndex = i;
		else openguinumber = i;
		return;
	}
}

// --------------------------------------------------------- parameter capture

namespace
{
	JPGraphParamState readParamState(JPParameter *parameter)
	{
		JPGraphParamState state;
		if (parameter == nullptr) return state;
		state.floatValue = parameter->floatValue;
		state.lerpValue = parameter->floatLerpValue;
		state.boolValue = parameter->boolValue;
		state.movtype = parameter->movtype;
		state.lastMovtype = parameter->lastMovtype;
		state.bpmRate = parameter->bpmRate;
		state.speed = parameter->speed;
		state.min = parameter->min;
		state.max = parameter->max;
		state.rangeEnabled = parameter->rangeEnabled;
		state.randomLocked = parameter->randomLocked;
		state.defaultFloat = parameter->defaultFloatValue;
		state.defaultBool = parameter->defaultBoolValue;
		return state;
	}

	JPGraphParamState readParamState(JPbox *box, int index)
	{
		if (box == nullptr) return JPGraphParamState();
		return readParamState(box->parameters.getJParameter(index));
	}

	void writeParamState(JPbox *box, int index, const JPGraphParamState &state)
	{
		if (box == nullptr) return;
		JPParameter *parameter = box->parameters.getJParameter(index);
		if (parameter == nullptr) return;
		parameter->floatValue = state.floatValue;
		parameter->floatLerpValue = state.lerpValue;
		parameter->boolValue = state.boolValue;
		parameter->movtype = state.movtype;
		parameter->lastMovtype = state.lastMovtype;
		parameter->bpmRate = state.bpmRate;
		parameter->speed = state.speed;
		parameter->min = state.min;
		parameter->max = state.max;
		parameter->rangeEnabled = state.rangeEnabled;
		parameter->randomLocked = state.randomLocked;
		parameter->defaultFloatValue = state.defaultFloat;
		parameter->defaultBoolValue = state.defaultBool;
	}

	// Everything except the value itself: automation, range, lock, defaults.
	bool sameParamSettings(const JPGraphParamState &a, const JPGraphParamState &b)
	{
		return a.boolValue == b.boolValue &&
			a.movtype == b.movtype && a.lastMovtype == b.lastMovtype &&
			a.bpmRate == b.bpmRate && a.speed == b.speed &&
			a.min == b.min && a.max == b.max &&
			a.rangeEnabled == b.rangeEnabled &&
			a.randomLocked == b.randomLocked &&
			a.defaultFloat == b.defaultFloat && a.defaultBool == b.defaultBool;
	}

	// Did this gesture change something worth an undo step?
	//
	// A parameter under automation rewrites its own value on every frame, so its
	// value moving between press and release says nothing about what the user
	// did - recording it would fill the stack with steps nobody took. Its
	// SETTINGS are a different matter: switching automation off, arming a range
	// or locking it are deliberate clicks, and those are still recorded. The
	// value only counts when the parameter is standing still at one end or the
	// other, which is exactly when a human moved it.
	bool parameterEditIsMeaningful(const JPGraphParamState &before,
		const JPGraphParamState &after)
	{
		if (!sameParamSettings(before, after)) return true;
		const bool automatedThroughout =
			before.movtype != JPParameter::STANDART &&
			after.movtype != JPParameter::STANDART;
		if (automatedThroughout) return false;
		return before.floatValue != after.floatValue;
	}
}

bool JPboxgroup::resolveParameterOwner(JPParameter *parameter, string &uid,
	int &index) const
{
	if (parameter == nullptr) return false;
	bool found = false;
	std::function<void(const vector<JPbox *> &)> walk =
		[&](const vector<JPbox *> &list)
	{
		for (JPbox *box : list)
		{
			if (box == nullptr || found) continue;
			for (int i = 0; i < box->parameters.getSize(); i++)
			{
				if (box->parameters.getJParameter(i) != parameter) continue;
				uid = box->uid;
				index = i;
				found = true;
				return;
			}
			JPbox_preset *preset = dynamic_cast<JPbox_preset *>(box);
			if (preset != nullptr) walk(preset->boxes);
		}
	};
	// The draft first, for the same reason findBoxAnywhere checks it first.
	if (isCueDraftMode()) walk(cueState.draftBoxes);
	if (!found) walk(boxes);
	return found;
}

void JPboxgroup::beginParameterCapture()
{
	pendingParams.clear();
	for (JPcontroller *controller : controllers)
	{
		if (controller == nullptr || controller->parameters == nullptr) continue;

		PendingParam pending;
		pending.parameter = controller->parameters;
		if (!resolveParameterOwner(pending.parameter, pending.uid,
			pending.index))
		{
			continue;
		}
		JPbox *owner = findBoxAnywhere(pending.uid);
		if (owner == nullptr) continue;
		pending.before = readParamState(owner, pending.index);
		pendingParams.push_back(pending);
	}
}

void JPboxgroup::commitParameterCapture()
{
	if (pendingParams.empty()) return;
	vector<PendingParam> captured;
	captured.swap(pendingParams);

	for (const PendingParam &pending : captured)
	{
		JPbox *owner = findBoxAnywhere(pending.uid);
		if (owner == nullptr || pending.index >= owner->parameters.getSize())
		{
			continue;
		}
		const JPGraphParamState after = readParamState(owner, pending.index);
		if (!parameterEditIsMeaningful(pending.before, after)) continue;

		JPGraphEdit edit;
		edit.kind = JPGraphEdit::SetParameter;
		edit.boxUid = pending.uid;
		edit.paramIndex = pending.index;
		edit.paramBefore = pending.before;
		edit.paramAfter = after;
		pushEdit(std::move(edit));
	}
}

// -------------------------------------------------------------- move capture

void JPboxgroup::beginMoveCapture()
{
	pendingMoves.clear();
	vector<JPbox *> *list = getCurrentViewBoxes();
	if (list == nullptr) return;
	// Every box that could travel this gesture, not just the one under the
	// cursor: dragging one box of a multi-selection moves all of them, and that
	// has to come back as a single step.
	for (JPbox *box : *list)
	{
		if (box == nullptr) continue;
		PendingMove pending;
		pending.uid = box->uid;
		pending.fromX = box->x;
		pending.fromY = box->y;
		pendingMoves.push_back(pending);
	}
}

void JPboxgroup::commitMoveCapture()
{
	if (pendingMoves.empty()) return;
	vector<PendingMove> captured;
	captured.swap(pendingMoves);

	vector<JPbox *> *list = getCurrentViewBoxes();
	if (list == nullptr) return;

	JPGraphEdit edit;
	edit.kind = JPGraphEdit::MoveBoxes;
	for (const PendingMove &pending : captured)
	{
		JPbox *box = findBoxInView(*list, pending.uid);
		if (box == nullptr) continue;
		if (box->x == pending.fromX && box->y == pending.fromY) continue;
		JPGraphEdit::BoxMove move;
		move.uid = pending.uid;
		move.fromX = pending.fromX;
		move.fromY = pending.fromY;
		move.toX = box->x;
		move.toY = box->y;
		edit.moves.push_back(move);
	}
	// A click that grabbed a box but never moved it is not an edit.
	if (edit.moves.empty()) return;
	pushEdit(std::move(edit));
}

// ------------------------------------------------------------- other records

// Changes arriving from MIDI. They are edits like any other - the earlier
// reading, that a bind driven live was a performance rather than something to
// walk back, turned out to be the wrong call: it is exactly the surface where a
// knob knocked by accident needs an undo.
//
// What is still NOT recorded is the binding itself. Learning or clearing a MIDI
// bind changes the controller layout, not the composition.
void JPboxgroup::recordExternalParameterChange(JPParameter *parameter,
	const JPGraphParamState &before)
{
	if (parameter == nullptr) return;
	string uid;
	int index = -1;
	if (!resolveParameterOwner(parameter, uid, index)) return;

	const JPGraphParamState after = readParamState(parameter);
	if (!parameterEditIsMeaningful(before, after)) return;

	JPGraphEdit edit;
	edit.kind = JPGraphEdit::SetParameter;
	edit.boxUid = uid;
	edit.paramIndex = index;
	edit.paramBefore = before;
	edit.paramAfter = after;
	pushEdit(std::move(edit));
}

void JPboxgroup::recordBoxStateChange(JPbox *box, bool onoffBefore,
	bool bypassBefore)
{
	if (box == nullptr) return;
	const bool onoffAfter = box->getonoff();
	const bool bypassAfter = box->getBypass();
	if (onoffBefore == onoffAfter && bypassBefore == bypassAfter) return;

	JPGraphEdit edit;
	edit.kind = JPGraphEdit::SetBoxState;
	edit.boxUid = box->uid;
	edit.onoffBefore = onoffBefore;
	edit.onoffAfter = onoffAfter;
	edit.bypassBefore = bypassBefore;
	edit.bypassAfter = bypassAfter;
	pushEdit(std::move(edit));
}

JPGraphParamState JPboxgroup::captureParamState(JPParameter *parameter) const
{
	return readParamState(parameter);
}

void JPboxgroup::recordConnectionChange(JPbox *consumer, int inlet,
	const string &producerBefore, const string &producerAfter)
{
	if (consumer == nullptr || inlet < 0) return;
	if (producerBefore == producerAfter) return;
	JPGraphEdit edit;
	edit.kind = JPGraphEdit::SetConnection;
	edit.boxUid = consumer->uid;
	edit.inletIndex = inlet;
	edit.producerBefore = producerBefore;
	edit.producerAfter = producerAfter;
	pushEdit(std::move(edit));
}

int JPboxgroup::currentViewActiveRender()
{
	int *active = getCurrentViewActiveRenderPointer();
	return active != nullptr ? *active : -1;
}

bool JPboxgroup::editActiveRenderForCurrentView(int index)
{
	const int before = currentViewActiveRender();
	const bool changed = requestSetActiveRenderForCurrentView(index);
	recordActiveRenderChange(before);
	return changed;
}

void JPboxgroup::recordActiveRenderChange(int beforeIndex)
{
	// Not while a cue is up. There the change is STAGED - it lands on the draft
	// or on the cue's staged index, and the cue's own apply and cancel are what
	// resolve it. Recording it here would let Ctrl+Z rewrite the real graph's
	// active render from a change that never reached it.
	if (hasCue()) return;

	vector<JPbox *> *list = getCurrentViewBoxes();
	int *active = getCurrentViewActiveRenderPointer();
	if (list == nullptr || active == nullptr) return;
	const int afterIndex = *active;
	if (beforeIndex == afterIndex) return;

	JPGraphEdit edit;
	edit.kind = JPGraphEdit::SetActiveRender;
	edit.activeIndexBefore = beforeIndex;
	edit.activeIndexAfter = afterIndex;
	if (beforeIndex >= 0 && beforeIndex < (int)list->size() &&
		(*list)[beforeIndex] != nullptr)
	{
		edit.activeUidBefore = (*list)[beforeIndex]->uid;
	}
	if (afterIndex >= 0 && afterIndex < (int)list->size() &&
		(*list)[afterIndex] != nullptr)
	{
		edit.activeUidAfter = (*list)[afterIndex]->uid;
	}
	pushEdit(std::move(edit));
}

void JPboxgroup::recordInputReorder(JPbox *consumer, int first, int second)
{
	if (consumer == nullptr || first < 0 || second < 0 || first == second) return;
	JPGraphEdit edit;
	edit.kind = JPGraphEdit::ReorderInput;
	edit.boxUid = consumer->uid;
	edit.inletIndex = first;
	edit.inletSecond = second;
	pushEdit(std::move(edit));
}

void JPboxgroup::beginBoxStateCapture()
{
	pendingBoxStates.clear();
	vector<JPbox *> *list = getCurrentViewBoxes();
	if (list == nullptr) return;
	for (JPbox *box : *list)
	{
		if (box == nullptr) continue;
		PendingBoxState pending;
		pending.uid = box->uid;
		pending.onoff = box->getonoff();
		pending.bypass = box->getBypass();
		pendingBoxStates.push_back(pending);
	}
}

void JPboxgroup::commitBoxStateCapture()
{
	if (pendingBoxStates.empty()) return;
	vector<PendingBoxState> captured;
	captured.swap(pendingBoxStates);

	vector<JPbox *> *list = getCurrentViewBoxes();
	if (list == nullptr) return;
	for (const PendingBoxState &pending : captured)
	{
		JPbox *box = findBoxInView(*list, pending.uid);
		if (box == nullptr) continue;
		const bool onoffAfter = box->getonoff();
		const bool bypassAfter = box->getBypass();
		if (pending.onoff == onoffAfter && pending.bypass == bypassAfter)
		{
			continue;
		}
		JPGraphEdit edit;
		edit.kind = JPGraphEdit::SetBoxState;
		edit.boxUid = pending.uid;
		edit.onoffBefore = pending.onoff;
		edit.onoffAfter = onoffAfter;
		edit.bypassBefore = pending.bypass;
		edit.bypassAfter = bypassAfter;
		pushEdit(std::move(edit));
	}
}

// ------------------------------------------------------------- detach/attach

string JPboxgroup::producerUidForInlet(const vector<JPbox *> &list,
	JPbox *consumer, int inlet) const
{
	if (consumer == nullptr || inlet < 0 ||
		inlet >= consumer->fbohandlergroup.getSize())
	{
		return string();
	}
	if (!consumer->fbohandlergroup.getisPointerSet(inlet)) return string();
	// Matched on the FBO pointer, not on the name: two boxes can carry the same
	// name and the cable would then be restored to the wrong producer.
	ofFbo *target = consumer->fbohandlergroup.getFboPointerReference(inlet);
	if (target == nullptr) return string();
	for (JPbox *box : list)
	{
		if (box != nullptr && &box->fbo == target) return box->uid;
	}
	return string();
}

bool JPboxgroup::applyActiveRenderEdit(vector<JPbox *> &list,
	const JPGraphEdit &edit, const string &boxUid, int fallbackIndex)
{
	int *active = historyActiveRenderForEdit(edit);
	if (active == nullptr) return false;

	// The uid first, so the right box comes back even if the list was reordered
	// since; the index is the fallback, and it is what "no box was active" - a
	// real state, with no uid to name it - comes back as.
	int target = -1;
	JPbox *box = findBoxInView(list, boxUid);
	if (box != nullptr)
	{
		for (int i = 0; i < (int)list.size(); i++)
		{
			if (list[i] == box) target = i;
		}
	}
	if (target < 0)
	{
		target = ofClamp(fallbackIndex, 0, std::max(0, (int)list.size() - 1));
	}

	if (edit.viewUid.empty())
	{
		// Through updateTransition rather than by assignment, so undo crossfades
		// and arms the parameter morph exactly as double-clicking the box does.
		// Assigning directly would make undo snap while the original action
		// faded, which reads as two different features.
		updateTransition(target);
	}
	else
	{
		*active = target;
	}
	return true;
}

bool JPboxgroup::applyConnectionEdit(vector<JPbox *> &list,
	const JPGraphEdit &edit, const string &producerUid)
{
	JPbox *consumer = findBoxInView(list, edit.boxUid);
	if (consumer == nullptr) return false;
	if (edit.inletIndex < 0 ||
		edit.inletIndex >= consumer->fbohandlergroup.getSize())
	{
		return false;
	}
	if (producerUid.empty())
	{
		consumer->fbohandlergroup.deleteFboPointer(edit.inletIndex);
		return true;
	}
	JPbox *producer = findBoxInView(list, producerUid);
	if (producer == nullptr) return false;
	return consumer->fbohandlergroup.setFboPointer(&producer->fbo,
		&producer->name, edit.inletIndex);
}

JPGraphDetachedBox JPboxgroup::detachBoxFromView(vector<JPbox *> &list,
	int index, JPbox_preset *owner)
{
	JPGraphDetachedBox detached;
	if (index < 0 || index >= (int)list.size() || list[index] == nullptr)
	{
		return detached;
	}

	JPbox *box = list[index];
	detached.box = box;
	detached.uid = box->uid;
	detached.index = index;

	// Record and cut every cable pointing at this box before it leaves, so undo
	// can put them back. Matched on the FBO pointer for the same reason as above.
	for (int k = 0; k < (int)list.size(); k++)
	{
		if (k == index || list[k] == nullptr) continue;
		JPFbohandlerGroup &inlets = list[k]->fbohandlergroup;
		for (int l = 0; l < inlets.getSize(); l++)
		{
			if (!inlets.getisPointerSet(l)) continue;
			if (inlets.getFboPointerReference(l) != &box->fbo) continue;
			detached.severedInlets.push_back(
				std::make_pair(list[k]->uid, l));
			inlets.deleteFboPointer(l);
		}
	}

	// The FINAL stack names boxes by uid, so a layer left behind stops drawing
	// and cannot be told apart from a healthy one in the panel. Cutting it here
	// rather than in each caller is the same reasoning as the severed inlets
	// above: this is the one door every removal goes through.
	captureFinalLayersForBox(box, detached.finalLayers);

	list.erase(list.begin() + index);

	// The exposed-parameter arrays run parallel to the box list, so they have to
	// lose the same slot or every group child past this one shifts by one.
	if (owner != nullptr)
	{
		if (index < (int)owner->exposedParams.size())
		{
			owner->exposedParams.erase(owner->exposedParams.begin() + index);
		}
		if (index < (int)owner->exposedParamOriginalIndices.size())
		{
			owner->exposedParamOriginalIndices.erase(
				owner->exposedParamOriginalIndices.begin() + index);
		}
		// Captured before they are dropped: reattaching has to restore them.
		for (const JPbox_preset::ExposedTextureInput &input :
			owner->exposedTextureInputs)
		{
			if (input.targetBoxName != box->name) continue;
			JPGraphDetachedBox::ExposedInput saved;
			saved.publicName = input.publicName;
			saved.targetBoxName = input.targetBoxName;
			saved.targetSamplerName = input.targetSamplerName;
			detached.exposedInputs.push_back(saved);
		}
		owner->removeExposedTextureInputsForBox(box->name);
		if (owner->activeRender > index) owner->activeRender--;
		owner->activeRender = ofClamp(owner->activeRender, 0,
			std::max(0, (int)owner->boxes.size() - 1));
	}

	return detached;
}

void JPboxgroup::reattachBoxToView(vector<JPbox *> &list,
	JPGraphDetachedBox &detached, JPbox_preset *owner)
{
	JPbox *box = static_cast<JPbox *>(detached.box);
	if (box == nullptr) return;

	const int index = ofClamp(detached.index, 0, (int)list.size());
	list.insert(list.begin() + index, box);
	// The graph owns it again; the history must not free it.
	detached.box = nullptr;

	if (owner != nullptr)
	{
		if (index <= (int)owner->exposedParams.size())
		{
			owner->exposedParams.insert(
				owner->exposedParams.begin() + index,
				vector<bool>(box->parameters.getSize(), false));
		}
		if (index <= (int)owner->exposedParamOriginalIndices.size())
		{
			owner->exposedParamOriginalIndices.insert(
				owner->exposedParamOriginalIndices.begin() + index,
				vector<pair<int, int>>(box->parameters.getSize(),
					std::make_pair(-1, -1)));
		}
		if (owner->activeRender >= index) owner->activeRender++;
		for (const JPGraphDetachedBox::ExposedInput &saved :
			detached.exposedInputs)
		{
			JPbox_preset::ExposedTextureInput input;
			input.publicName = saved.publicName;
			input.targetBoxName = saved.targetBoxName;
			input.targetSamplerName = saved.targetSamplerName;
			owner->exposedTextureInputs.push_back(input);
		}
		if (!detached.exposedInputs.empty())
		{
			// Rebuild BEFORE sync, and both are needed. Rebuilding is what
			// re-creates the group's public inlets from the entries just
			// restored; sync only copies a pointer from an inlet that already
			// exists, so on its own it finds publicIndex < 0 and does nothing -
			// the inlet would stay missing until something else forced a
			// rebuild.
			owner->rebuildExposedTextureInputHandlers();
			owner->syncExposedTextureInputs();
		}
	}

	restoreFinalLayers(detached.finalLayers);

	// Restore the cables that were cut when it left. The box never moved in
	// memory, so this is the same pointer they held before.
	for (const pair<string, int> &severed : detached.severedInlets)
	{
		JPbox *consumer = findBoxInView(list, severed.first);
		if (consumer == nullptr) continue;
		if (severed.second < 0 ||
			severed.second >= consumer->fbohandlergroup.getSize())
		{
			continue;
		}
		consumer->fbohandlergroup.setFboPointer(&box->fbo, &box->name,
			severed.second);
	}
}

// ---------------------------------------------------------------- grouping
//
// Grouping is a move, not a rebuild: the members keep their identity and their
// memory address the whole way, so both directions are pointer surgery on two
// vectors plus the cables that crossed the boundary. The group's public inputs
// are created once, when the group is first made, and simply travel with it -
// undoing does not tear them down, so a redo has nothing to reconstruct.

void JPboxgroup::eraseParentSlots(JPbox_preset *owner, int index)
{
	if (owner == nullptr) return;
	if (index >= 0 && index < (int)owner->exposedParams.size())
	{
		owner->exposedParams.erase(owner->exposedParams.begin() + index);
	}
	if (index >= 0 && index < (int)owner->exposedParamOriginalIndices.size())
	{
		owner->exposedParamOriginalIndices.erase(
			owner->exposedParamOriginalIndices.begin() + index);
	}
}

void JPboxgroup::insertParentSlots(JPbox_preset *owner, int index,
	int parameterCount)
{
	if (owner == nullptr) return;
	if (index >= 0 && index <= (int)owner->exposedParams.size())
	{
		owner->exposedParams.insert(owner->exposedParams.begin() + index,
			vector<bool>((std::size_t)parameterCount, false));
	}
	if (index >= 0 && index <= (int)owner->exposedParamOriginalIndices.size())
	{
		owner->exposedParamOriginalIndices.insert(
			owner->exposedParamOriginalIndices.begin() + index,
			vector<pair<int, int>>((std::size_t)parameterCount,
				std::make_pair(-1, -1)));
	}
}

// Forms ONE group: pulls its members out of the parent list and puts the group
// box in their place.
bool JPboxgroup::applyGroupPayload(vector<JPbox *> &list,
	JPGraphEdit::GroupPayload &payload, JPbox_preset *owner)
{
	JPbox_preset *group = static_cast<JPbox_preset *>(payload.groupBox);
	if (group == nullptr) return false;

	// Take the members out of the parent. Descending by their CURRENT position,
	// so removing one does not shift the ones still to be found.
	vector<pair<int, JPbox *>> found;
	for (const JPGraphDetachedBox &entry : payload.members)
	{
		for (int k = 0; k < (int)list.size(); k++)
		{
			if (list[k] != nullptr && list[k]->uid == entry.uid)
			{
				found.push_back(std::make_pair(k, list[k]));
				break;
			}
		}
	}
	if (found.size() != payload.members.size()) return false;

	vector<pair<int, JPbox *>> descending = found;
	std::sort(descending.begin(), descending.end(),
		[](const pair<int, JPbox *> &a, const pair<int, JPbox *> &b) {
			return a.first > b.first;
		});
	for (const pair<int, JPbox *> &item : descending)
	{
		list.erase(list.begin() + item.first);
		eraseParentSlots(owner, item.first);
	}

	// Back into the group, in the order they were grouped in.
	group->boxes.clear();
	for (const JPGraphDetachedBox &entry : payload.members)
	{
		for (const pair<int, JPbox *> &item : found)
		{
			if (item.second->uid == entry.uid)
			{
				group->boxes.push_back(item.second);
				break;
			}
		}
	}
	group->activeRender = ofClamp(group->activeRender, 0,
		std::max(0, (int)group->boxes.size() - 1));

	const int groupIndex = ofClamp(payload.groupIndex, 0, (int)list.size());
	list.insert(list.begin() + groupIndex, group);
	insertParentSlots(owner, groupIndex, group->parameters.getSize());
	// The graph owns it again.

	// Consumers that stayed outside read the group once more.
	for (const JPGraphDetachedBox &entry : payload.members)
	{
		for (const pair<string, int> &severed : entry.severedInlets)
		{
			JPbox *consumer = findBoxInView(list, severed.first);
			if (consumer == nullptr) continue;
			if (severed.second < 0 ||
				severed.second >= consumer->fbohandlergroup.getSize())
			{
				continue;
			}
			consumer->fbohandlergroup.setFboPointer(&group->fbo,
				&group->name, severed.second);
		}
	}
	group->syncExposedTextureInputs();
	restoreFinalLayers(payload.finalLayers);
	payload.finalLayers.clear();
	return true;
}

// Dissolves ONE group: hands its children back to the parent list and takes the
// group box out. This is what a user-driven ungroup runs too.
bool JPboxgroup::revertGroupPayload(vector<JPbox *> &list,
	JPGraphEdit::GroupPayload &payload, JPbox_preset *owner)
{
	JPbox_preset *group = static_cast<JPbox_preset *>(payload.groupBox);
	if (group == nullptr) return false;

	int groupIndex = -1;
	for (int k = 0; k < (int)list.size(); k++)
	{
		if (list[k] == group) groupIndex = k;
	}
	if (groupIndex < 0) return false;

	// Collect the members while they are still inside the group.
	vector<JPbox *> members;
	for (const JPGraphDetachedBox &entry : payload.members)
	{
		JPbox *box = findBoxInView(group->boxes, entry.uid);
		if (box == nullptr) return false;
		members.push_back(box);
	}

	group->boxes.clear();
	// After the clear, so the children - which go straight back into the parent
	// list and keep their uids - are not swept up with the group.
	captureFinalLayersForBox(group, payload.finalLayers);
	list.erase(list.begin() + groupIndex);
	eraseParentSlots(owner, groupIndex);

	// Put each one back at the index it held before the grouping. Ascending, so
	// each insertion lands where it was rather than being pushed along.
	for (std::size_t i = 0; i < members.size(); i++)
	{
		const int index = ofClamp(payload.members[i].index, 0, (int)list.size());
		list.insert(list.begin() + index, members[i]);
		insertParentSlots(owner, index, members[i]->parameters.getSize());
	}

	// And the cables that had been re-pointed at the group go back to the member
	// they came from.
	for (std::size_t i = 0; i < members.size(); i++)
	{
		for (const pair<string, int> &severed : payload.members[i].severedInlets)
		{
			JPbox *consumer = findBoxInView(list, severed.first);
			if (consumer == nullptr) continue;
			if (severed.second < 0 ||
				severed.second >= consumer->fbohandlergroup.getSize())
			{
				continue;
			}
			consumer->fbohandlergroup.setFboPointer(&members[i]->fbo,
				&members[i]->name, severed.second);
		}
	}

	// The group is out of the graph but NOT destroyed: the history holds it so a
	// redo can put the very same object back, exposures and all.
	return true;
}

void JPboxgroup::destroyDetachedBox(JPGraphDetachedBox &detached)
{
	JPbox *box = static_cast<JPbox *>(detached.box);
	if (box == nullptr) return;
	detached.box = nullptr;
	box->clear();
	delete box;
}

bool JPboxgroup::applyGroupEdit(vector<JPbox *> &list, JPGraphEdit &edit,
	JPbox_preset *owner)
{
	// Backwards, because the payloads are stored in descending groupIndex order:
	// forming the lowest-indexed group first keeps every later index still
	// meaningful.
	for (std::size_t i = edit.groups.size(); i > 0; i--)
	{
		if (!applyGroupPayload(list, edit.groups[i - 1], owner)) return false;
	}
	return true;
}

bool JPboxgroup::revertGroupEdit(vector<JPbox *> &list, JPGraphEdit &edit,
	JPbox_preset *owner)
{
	// Forwards: highest index first, so dissolving one does not shift the next.
	for (JPGraphEdit::GroupPayload &payload : edit.groups)
	{
		if (!revertGroupPayload(list, payload, owner)) return false;
	}
	return true;
}

// ------------------------------------------------------------- view navigation

bool JPboxgroup::findViewPath(const string &viewUid, vector<int> &outPath) const
{
	outPath.clear();
	if (viewUid.empty()) return true; // the main graph

	// Indices, with getActivePreset()'s exact convention: path[0] indexes
	// `boxes`, path[k>0] indexes preset->boxes. Typed with getTipo() and
	// static_cast rather than dynamic_cast for the same reason - a path built
	// here has to survive getActivePreset()'s own validation.
	std::function<bool(const vector<JPbox *> &, vector<int> &)> walk =
		[&](const vector<JPbox *> &list, vector<int> &path) -> bool
	{
		for (int i = 0; i < (int)list.size(); i++)
		{
			JPbox *box = list[i];
			if (box == nullptr || box->getTipo() != JPbox::PRESETBOX) continue;
			JPbox_preset *preset = static_cast<JPbox_preset *>(box);
			path.push_back(i);
			if (preset->uid == viewUid) return true;
			if (walk(preset->boxes, path)) return true;
			path.pop_back();
		}
		return false;
	};
	return walk(boxes, outPath);
}

bool JPboxgroup::navigateToView(const vector<int> &path)
{
	if (path == activeGroupPath) return true;

	// Up to the deepest level the two paths agree on, then back down one step at
	// a time. Through the ordinary navigators, never by assigning
	// activeGroupPath: they are what save and restore each view's zoom and pan,
	// clear the selection and size the tab state. A raw assignment drops all of
	// that silently.
	std::size_t common = 0;
	while (common < path.size() && common < activeGroupPath.size() &&
		path[common] == activeGroupPath[common])
	{
		common++;
	}
	if (!navigateToBreadcrumbLevel((int)common)) return false;

	for (std::size_t level = common; level < path.size(); level++)
	{
		// navigateToChildPreset takes the position among the CHILD PRESETS of
		// the current view, not a box index.
		const vector<int> children = getDirectChildPresetIndices();
		int ordinal = -1;
		for (int k = 0; k < (int)children.size(); k++)
		{
			if (children[(std::size_t)k] == path[level]) ordinal = k;
		}
		if (ordinal < 0) return false;
		if (!navigateToChildPreset(ordinal)) return false;
	}
	return true;
}

bool JPboxgroup::navigateToEditView(const JPGraphEdit &edit)
{
	// The cue draft is not a place you can navigate to; it is drawn over
	// whatever view is current.
	if (edit.cueDraft) return isCueDraftMode();

	vector<int> path;
	// A group that is not in the tree cannot be shown, and its entry cannot be
	// replayed either. Refusing leaves the ring's cursor untouched, which is the
	// honest outcome.
	if (!findViewPath(edit.viewUid, path)) return false;
	return navigateToView(path);
}

// ------------------------------------------------------------ apply / revert

bool JPboxgroup::applyGraphEdit(JPGraphEdit &edit)
{
	// Show the user where this is about to happen. A step recorded inside a
	// group is undone inside that group, so the canvas has to be there first -
	// otherwise Ctrl+Z appears to do nothing while quietly changing something.
	if (!navigateToEditView(edit)) return false;

	vector<JPbox *> *list = historyBoxesForEdit(edit);
	if (list == nullptr) return false;
	JPbox_preset *owner = historyPresetForEdit(edit);
	bool retargetTransition = true;
	// Did boxes come or go? Only these two kinds change the shape of the list.
	const bool structural = edit.kind == JPGraphEdit::DeleteBoxes ||
		edit.kind == JPGraphEdit::GroupBoxes;

	switch (edit.kind)
	{
	case JPGraphEdit::MoveBoxes:
		for (const JPGraphEdit::BoxMove &move : edit.moves)
		{
			JPbox *box = findBoxInView(*list, move.uid);
			if (box != nullptr) box->setPos(move.toX, move.toY);
		}
		break;

	case JPGraphEdit::SetParameter:
		writeParamState(findBoxForEdit(*list, edit.boxUid), edit.paramIndex,
			edit.paramAfter);
		break;

	case JPGraphEdit::SetConnection:
		if (!applyConnectionEdit(*list, edit, edit.producerAfter)) return false;
		break;

	case JPGraphEdit::SetActiveRender:
		if (!applyActiveRenderEdit(*list, edit, edit.activeUidAfter,
			edit.activeIndexAfter))
		{
			return false;
		}
		retargetTransition = false;
		break;

	case JPGraphEdit::ReorderInput:
	{
		JPbox *consumer = findBoxInView(*list, edit.boxUid);
		if (consumer == nullptr) return false;
		consumer->fbohandlergroup.swapConnections(edit.inletIndex,
			edit.inletSecond);
		break;
	}

	case JPGraphEdit::SetBoxState:
	{
		JPbox *box = findBoxForEdit(*list, edit.boxUid);
		if (box == nullptr) return false;
		box->setonoff(edit.onoffAfter);
		box->setBypass(edit.bypassAfter);
		break;
	}

	case JPGraphEdit::DeleteBoxes:
		// Redoing a delete: take the boxes back out, highest index first so the
		// lower ones keep the positions they were recorded at.
		for (int i = (int)edit.detached.size() - 1; i >= 0; i--)
		{
			JPGraphDetachedBox &entry = edit.detached[(std::size_t)i];
			JPbox *box = findBoxInView(*list, entry.uid);
			if (box == nullptr) continue;
			int index = -1;
			for (int k = 0; k < (int)list->size(); k++)
				if ((*list)[k] == box) index = k;
			if (index < 0) continue;
			JPGraphDetachedBox fresh = detachBoxFromView(*list, index, owner);
			entry.box = fresh.box;
			entry.severedInlets = fresh.severedInlets;
			entry.finalLayers = fresh.finalLayers;
		}
		break;

	case JPGraphEdit::GroupBoxes:
		// Inverted means this entry IS an ungroup, so applying it dissolves.
		if (!(edit.inverted ? revertGroupEdit(*list, edit, owner)
							: applyGroupEdit(*list, edit, owner)))
		{
			return false;
		}
		break;

	default:
		return false;
	}

	afterHistoryChange(edit, true, structural, retargetTransition);
	return true;
}

bool JPboxgroup::revertGraphEdit(JPGraphEdit &edit)
{
	// Show the user where this is about to happen. A step recorded inside a
	// group is undone inside that group, so the canvas has to be there first -
	// otherwise Ctrl+Z appears to do nothing while quietly changing something.
	if (!navigateToEditView(edit)) return false;

	vector<JPbox *> *list = historyBoxesForEdit(edit);
	if (list == nullptr) return false;
	JPbox_preset *owner = historyPresetForEdit(edit);
	bool retargetTransition = true;
	// Did boxes come or go? Only these two kinds change the shape of the list.
	const bool structural = edit.kind == JPGraphEdit::DeleteBoxes ||
		edit.kind == JPGraphEdit::GroupBoxes;

	switch (edit.kind)
	{
	case JPGraphEdit::MoveBoxes:
		for (const JPGraphEdit::BoxMove &move : edit.moves)
		{
			JPbox *box = findBoxInView(*list, move.uid);
			if (box != nullptr) box->setPos(move.fromX, move.fromY);
		}
		break;

	case JPGraphEdit::SetParameter:
		writeParamState(findBoxForEdit(*list, edit.boxUid), edit.paramIndex,
			edit.paramBefore);
		break;

	case JPGraphEdit::SetConnection:
		if (!applyConnectionEdit(*list, edit, edit.producerBefore)) return false;
		break;

	case JPGraphEdit::SetActiveRender:
		if (!applyActiveRenderEdit(*list, edit, edit.activeUidBefore,
			edit.activeIndexBefore))
		{
			return false;
		}
		retargetTransition = false;
		break;

	case JPGraphEdit::ReorderInput:
	{
		JPbox *consumer = findBoxInView(*list, edit.boxUid);
		if (consumer == nullptr) return false;
		// A swap is its own inverse.
		consumer->fbohandlergroup.swapConnections(edit.inletIndex,
			edit.inletSecond);
		break;
	}

	case JPGraphEdit::SetBoxState:
	{
		JPbox *box = findBoxForEdit(*list, edit.boxUid);
		if (box == nullptr) return false;
		box->setonoff(edit.onoffBefore);
		box->setBypass(edit.bypassBefore);
		break;
	}

	case JPGraphEdit::DeleteBoxes:
		// Lowest index first, so each insertion lands at the index it was taken
		// from rather than being pushed along by the ones after it.
		for (std::size_t i = 0; i < edit.detached.size(); i++)
		{
			reattachBoxToView(*list, edit.detached[i], owner);
		}
		break;

	case JPGraphEdit::GroupBoxes:
		if (!(edit.inverted ? applyGroupEdit(*list, edit, owner)
							: revertGroupEdit(*list, edit, owner)))
		{
			return false;
		}
		break;

	default:
		return false;
	}

	afterHistoryChange(edit, false, structural, retargetTransition);
	return true;
}

void JPboxgroup::releaseGraphEdit(JPGraphEdit &edit, bool wasApplied)
{
	// Ownership follows which side of the cursor the entry sat on. A delete that
	// still stands owns its boxes; once undone the graph has them back. Grouping
	// is the mirror image: the group box is the history's only while the grouping
	// is undone.
	if (edit.kind == JPGraphEdit::DeleteBoxes && wasApplied)
	{
		for (JPGraphDetachedBox &entry : edit.detached) destroyDetachedBox(entry);
	}
	// Grouping and ungrouping are mirror images: the group box is out of the
	// graph, and so owned by the history, on opposite sides of the cursor.
	if (edit.kind == JPGraphEdit::GroupBoxes && wasApplied == edit.inverted)
	{
		for (JPGraphEdit::GroupPayload &payload : edit.groups)
		{
			JPbox *group = static_cast<JPbox *>(payload.groupBox);
			payload.groupBox = nullptr;
			if (group == nullptr) continue;
			// Its children are back in the parent by now, so boxes is empty and
			// clear() takes nothing with it. That ordering is load-bearing:
			// JPbox_preset::clear() deletes every box it still holds.
			group->clear();
			delete group;
		}
	}
}

// ------------------------------------------------------------------ shortcut

bool JPboxgroup::graphUndoShortcut(bool redo)
{
	JPGraphUndoRing &history = currentViewHistory();
	if (redo) history.redo();
	else history.undo();
	// Consumed either way. With the node screen focused Ctrl+Z means "undo a
	// graph edit" even when the stack is empty; falling through to whatever
	// handles the bare key next would be a surprise.
	return true;
}

// -------------------------------------------------------------- mapping undo

void JPboxgroup::beginMappingCapture()
{
	pendingMappingValid = false;
	pendingMappingBox = nullptr;
	// The advanced editor and the simple corner-pin tier are the same box seen
	// two ways, and a mapping box is always a shader box.
	JPbox_shader *box = dynamic_cast<JPbox_shader *>(getMappingEditBox());
	if (box == nullptr) return;
	pendingMappingBox = box;
	pendingMappingBefore = box->captureMappingSnapshot();
	pendingMappingValid = true;
}

void JPboxgroup::commitMappingCapture()
{
	if (!pendingMappingValid) return;
	pendingMappingValid = false;
	JPbox_shader *box = pendingMappingBox;
	pendingMappingBox = nullptr;
	if (box == nullptr) return;
	// Still the box the panel is editing? Closing the panel mid-gesture, or the
	// box being deleted under it, would otherwise push onto a stale object.
	if (dynamic_cast<JPbox_shader *>(getMappingEditBox()) != box) return;
	box->pushMappingSnapshot(pendingMappingBefore);
}

bool JPboxgroup::mappingUndoShortcut(bool redo)
{
	if (!mappingEditActive) return false;
	JPbox_shader *box = dynamic_cast<JPbox_shader *>(getMappingEditBox());
	if (box == nullptr) return false;
	const bool changed = redo ? box->mappingRedo() : box->mappingUndo();
	if (changed)
	{
		// A mapping value is a shader uniform like any other, so the cue draft
		// has to be told, exactly as a corner drag tells it.
		markMappingParameterChanged();
		if (isCueDraftMode()) updateCueDraftGraph();
	}
	// Consumed either way: with the mapping panel open Ctrl+Z means "undo a
	// mapping edit" even at the end of the stack. Falling through to the graph
	// would undo something the user cannot even see.
	return true;
}
