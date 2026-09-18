#include <set>
#include <chrono>
#include "../JPutils/jp_storage.h"
#include "jp_box_preset.h"
#include "jp_media.h"
#include "jp_box_factory.h"
#include "../JPutils/jp_parameter_xml.h"

JPbox_preset::JPbox_preset()
{
}

JPbox_preset::~JPbox_preset()
{
}

void JPbox_preset::setup(string _directory, string _name)
{

	// JPbox::setup(jp_constants::p_font);
	JPbox::setup(_directory, _name);
	tipo = PRESETBOX;
	activeRenderTransitionRunning = false;
	lastCompositedActiveRender = -1;
	activeRenderTransitionTarget = -1;

	clear();
	ofXml xml;
	xml.load(_directory);
	// Carga inicial de las cajitas :
	auto boxloader = xml.find("/box");

	cout << "******************************************************************" << endl;
	for (auto &box : boxloader)
	{

		auto nombre = box.getChild("nombre");
		auto x = box.getChild("x");
		auto y = box.getChild("y");
		auto directory = box.getChild("directory");

		// cout << "Nombre : " << nombre.getValue() << endl;
		// cout << "y : " << x.getValue() << endl;
		// cout << "x : " << y.getValue() << endl;
		// cout << "Directory : " << directory.getValue() << endl;

		JPbox *bx = jp_box_factory::create(directory.getValue(),
			jp_box_factory::Context::Stored);
		if (bx == nullptr)
		{
			// Nothing matched: a build without NDI/Spout, or a save that
			// references a box type this binary does not know about.
			ofLogWarning("JPbox_preset")
				<< "skipping box '" << nombre.getValue()
				<< "' with unsupported directory '"
				<< directory.getValue() << "'";
			continue;
		}

		bx->setup(jp_normalizePath(directory.getValue()), nombre.getValue());
		bx->setPos(x.getIntValue(), y.getIntValue());

		// Load onoff and bypass states
		auto onoffChild = box.getChild("onoff");
		if (onoffChild)
		{
			bx->setonoff(onoffChild.getBoolValue());
		}
		else
		{
			// Default to true for backward compatibility (older XMLs without onoff)
			bx->setonoff(true);
		}
		auto bypassChild = box.getChild("bypass");
		if (bypassChild)
		{
			bx->setBypass(bypassChild.getBoolValue());
		}
		// Same rule as the main graph: adopt a stored identity, otherwise keep
		// the constructor's. JPboxgroup::repairBoxUids re-mints afterwards if
		// this group file has already contributed these uids once - which it
		// has, whenever the same group is placed twice.
		auto uidChild = box.getChild("uid");
		if (uidChild && !uidChild.getValue().empty()) bx->uid = uidChild.getValue();
		auto toOutputChild = box.getChild("tooutput");
		bx->setOutputCandidate(
			toOutputChild ? toOutputChild.getBoolValue() : false);

		jp_parameter_xml::load(box, bx->parameters,
			jp_parameter_xml::LoadContext::Preset);
		bx->loadCustomState(box);
		boxes.push_back(bx);
	}

    loadPreparedState(xml);
}
void JPbox_preset::setupPrepared(string directory, string name, const ofXml &xml, vector<JPbox *> &children)
{
    JPbox::setup(directory,name);tipo=PRESETBOX;clear();boxes.swap(children);
    loadPreparedState(xml);
}
void JPbox_preset::loadPreparedState(const ofXml &xml)
{
    auto boxloader=xml.find("/box");
	// Initialize exposedParams based on loaded boxes
	resizeExposedParams((int)boxes.size());

	// Load exposedParams from XML
	auto exposedChild = xml.getChild("exposedParams");
	if (exposedChild)
	{
		auto boxNodes = exposedChild.getChildren();
		for (auto &boxNode : boxNodes)
		{
			int childIndex = boxNode.getIntValue();
			// Check for origBox/origParam (propagated expose)
			auto origBoxChild = boxNode.getChild("origBox");
			auto origParamChild = boxNode.getChild("origParam");
			auto paramChild = boxNode.getChild("param");
			if (paramChild)
			{
				int paramIndex = paramChild.getIntValue();
				if (childIndex >= 0 &&
					childIndex < (int)exposedParams.size() &&
					paramIndex >= 0)
				{
					if (paramIndex >=
						(int)exposedParams[childIndex].size())
					{
						exposedParams[childIndex].resize(
							paramIndex + 1, false);
						exposedParamOriginalIndices[childIndex]
							.resize(paramIndex + 1, {-1, -1});
					}
					exposedParams[childIndex][paramIndex] = true;
					// Load propagation indices for propagated exposes
					if (origBoxChild && origParamChild)
					{
						if (childIndex >= (int)exposedParamOriginalIndices.size())
						{
							exposedParamOriginalIndices.resize(childIndex + 1);
						}
						if (paramIndex >= (int)exposedParamOriginalIndices[childIndex].size())
						{
							exposedParamOriginalIndices[childIndex].resize(paramIndex + 1, {-1, -1});
						}
						exposedParamOriginalIndices[childIndex][paramIndex] = {
							origBoxChild.getIntValue(),
							origParamChild.getIntValue()
						};
					}
				}
			}
		}
	}

	auto exposedInputsChild = xml.getChild("exposedInputs");
	if (exposedInputsChild)
	{
		for (auto &inputNode : exposedInputsChild.getChildren("input"))
		{
			auto nameNode = inputNode.getChild("name");
			auto boxNode = inputNode.getChild("box");
			auto samplerNode = inputNode.getChild("sampler");
			if (!nameNode || !boxNode || !samplerNode)
			{
				continue;
			}
			ExposedTextureInput input;
			input.publicName = nameNode.getValue();
			input.targetBoxName = boxNode.getValue();
			input.targetSamplerName = samplerNode.getValue();
			if (input.publicName.empty() ||
				input.targetBoxName.empty() ||
				input.targetSamplerName.empty())
			{
				continue;
			}
			bool duplicate = false;
			for (const ExposedTextureInput &existing :
				exposedTextureInputs)
			{
				if (existing.publicName == input.publicName ||
					(existing.targetBoxName == input.targetBoxName &&
					 existing.targetSamplerName ==
						input.targetSamplerName))
				{
					duplicate = true;
					break;
				}
			}
			if (!duplicate)
			{
				exposedTextureInputs.push_back(input);
			}
		}
	}
	pruneInvalidExposedTextureInputs();
	rebuildExposedTextureInputHandlers();

	// Una vez que cargo todas las cajitas les cargamos los links :
	// Mira lo que esta este algoritmo para levantar los links entre cajitas papa !!!
	int index1 = 0;
	for (auto &box : boxloader)
	{
		if (index1 >= (int)boxes.size())
		{
			break;
		}
		auto fboslinks = box.getChild("fboslinks").getChildren();
		for (auto &fbolink : fboslinks)
		{
			int linkIndex = boxes[index1]->fbohandlergroup.findIndexByName(
				fbolink.getName());
			if (linkIndex < 0)
			{
				continue;
			}
			for (int i = 0; i < boxes.size(); i++)
			{
				if (boxes[i]->name == fbolink.getValue() && i != index1)
				{
					ofFbo *fbopointer = &boxes[i]->fbo;
					string *fbopointername = &boxes[i]->name;
					boxes[index1]->fbohandlergroup.setFboPointer(
						fbopointer, fbopointername, linkIndex);
				}
			}
		}
		index1++;
	}
	//activeRender = xml.getChild("activerender").getIntValue();
	activeRender = boxes.empty()?0:ofClamp(xml.getChild("activerender").getIntValue(),0,int(boxes.size())-1);

	// Load viewport zoom/pan
	auto zoomChild = xml.getChild("viewportZoom");
	if (zoomChild)
	{
		viewportZoom = zoomChild.getFloatValue();
	}
	auto panXChild = xml.getChild("viewportPanX");
	auto panYChild = xml.getChild("viewportPanY");
	if (panXChild && panYChild)
	{
		viewportPan.x = panXChild.getFloatValue();
		viewportPan.y = panYChild.getFloatValue();
	}
}

void JPbox_preset::update()
{
	JPbox::update();
	pruneInvalidExposedTextureInputs();
	updateExposedTextureInputNodePositions();
	syncExposedTextureInputs();
	updateFBO();
}

void JPbox_preset::updateFBO()
{
	// Check if this preset itself is bypassed (PAUSE) - pass input through instead of rendering
	if (tryPassThroughFBO())
	{
		return;
	}
	// onoff.boolValue = true;
	if (onoff.boolValue)
	{
		// Schedule the children the same way the top level schedules us.
		//
		// This loop used to update every child at full rate on every frame, so
		// dropping a heavy shader inside a group silently opted it out of ALL
		// throttling - the one place where the saving matters most, because a
		// group is how you park a branch you are not currently showing.
		//
        beginActiveRenderTransition();
        if(activeRenderTransitionRunning) activeRenderTransition.advance();
        updateActiveRenderMorph();
        const auto transitionStarted=std::chrono::steady_clock::now();
        const float parentScale=TransitionSR::renderScaleLimit();
        struct RestoreScale {float value;~RestoreScale(){TransitionSR::renderScaleLimit()=value;}} restoreScale{parentScale};
        const float localScale=activeRenderTransitionRunning && activeRenderTransition.getLerpValue()<1.f?activeRenderTransition.state().scale():1.f;
        const float scale=std::min(parentScale,localScale);
        if(scale!=childTransitionScale) {
            std::function<void(const vector<JPbox *> &)> resize=[&](const vector<JPbox *> &nodes) {
                for(auto *node:nodes) {if(auto *shader=dynamic_cast<JPbox_shader *>(node))shader->setTransitionRenderScale(scale);if(auto *group=dynamic_cast<JPbox_preset *>(node))resize(group->boxes);}
            };
            resize(boxes);childTransitionScale=scale;
        }
        TransitionSR::renderScaleLimit()=scale;
        if(activeRenderTransitionRunning && activeRenderTransition.state().capture() && !activeTransitionSnapshot.isAllocated() && lastCompositedActiveRender>=0 && lastCompositedActiveRender<int(boxes.size())) {
            auto &source=boxes[lastCompositedActiveRender]->fbo;
            activeTransitionSnapshot.allocate(source.getWidth(),source.getHeight(),GL_RGBA);
            activeTransitionSnapshot.begin();ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);
            ofEnableBlendMode(OF_BLENDMODE_DISABLED);ofSetColor(255);source.draw(0,0);ofPopStyle();activeTransitionSnapshot.end();
        }
		// Roots are collected only when this group is itself rendering. When
		// the group is off-frame its composite is skipped anyway, so keeping a
		// child at full rate would produce a frame nobody reads; leaving the
		// root list empty lets every child fall to its own staggered rate.
		{
			vector<int> roots;
			if (shouldRenderThisFrame())
			{
				roots.push_back(activeRender);
				// Mid-crossfade both ends have to stay live, exactly as the
				// top-level scheduler keeps both transition inputs.
				if (activeRenderTransitionRunning && !activeTransitionSnapshot.isAllocated())
					roots.push_back(lastCompositedActiveRender);
			}
			jp_renderschedule::apply(boxes, roots, ofGetFrameNum(), false);
		}

		// update() is called unconditionally, and only the RENDER is throttled.
		// Skipping the call would stall whatever the child owns beyond its FBO
		// - a video box advances playback here, a camera box pulls its frame -
		// so a paused-looking group would also be a stopped-clock group.
		for (int i = boxes.size() - 1; i >= 0; i--)
		{
			boxes[i]->isactiverender = isactiverender && i == activeRender;
			boxes[i]->update();
			// Do NOT force onoff - user can toggle PAUSE freely in group view.
			// Initial onoff state is loaded from XML (defaults to true if not found).
		}
		if (boxes.empty() || activeRender < 0 || activeRender >= (int)boxes.size())
		{
			onoff.boolValue = false;
			return;
		}
		// The composite is a full-resolution blit of the active child, so it
		// obeys the group's own rate.
		if (shouldRenderThisFrame()) renderActiveRender();
        TransitionSR::renderScaleLimit()=parentScale;
        activeRenderTransition.observeFrame(std::max(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-transitionStarted).count(),ofGetLastFrameTime()*1000.),
            ofGetTargetFrameRate()>0?ofGetTargetFrameRate():60.);

	}
	else
	{
		JPbox::updateFBO();
	}
}

void JPbox_preset::beginActiveRenderTransition()
{
	if (boxes.empty() || activeRender < 0 || activeRender >= (int)boxes.size() ||
		boxes[activeRender] == nullptr)
	{
		return;
	}

	int targetIndex = activeRender;
	if (lastCompositedActiveRender < 0 ||
		lastCompositedActiveRender >= (int)boxes.size() ||
		boxes[lastCompositedActiveRender] == nullptr)
	{
		lastCompositedActiveRender = targetIndex;
		activeRenderTransitionRunning = false;
	}

	if (activeRenderTransitionRunning ? targetIndex != activeRenderTransitionTarget :
        targetIndex != lastCompositedActiveRender)
	{
        if(activeRenderTransitionRunning && fbo.isAllocated()) {
            activeTransitionSnapshot.allocate(fbo.getWidth(),fbo.getHeight(),GL_RGBA);
            activeTransitionSnapshot.begin();ofPushStyle();ofSetRectMode(OF_RECTMODE_CORNER);
            ofEnableBlendMode(OF_BLENDMODE_DISABLED);ofSetColor(255);ofClear(0,0,0,0);
            fbo.draw(0,0);ofPopStyle();activeTransitionSnapshot.end();
        } else activeTransitionSnapshot.clear();
		activeRenderTransitionInitialized = true;
		activeRenderTransition.setType(TransitionSR::preferences().effect);
        activeRenderTransition.setDurationMs(TransitionSR::preferences().duration * 1000.);
        clearActiveRenderMorph();
        jp_transition::Capabilities caps;
        auto *source=dynamic_cast<JPbox_shader *>(boxes[lastCompositedActiveRender]);
        auto *target=dynamic_cast<JPbox_shader *>(boxes[targetIndex]);
        caps.morph=source && target && source!=target && !activeTransitionSnapshot.isAllocated() && source->transitionCompatibleWith(*target);
        if(caps.morph) for(int i=0;i<target->parameters.getSize();++i)
            caps.staged=caps.staged || jp_transition::category(target->shader.getShaderSource(GL_FRAGMENT_SHADER),target->parameters.getName(i))!=jp_transition::Category::None;
        // Resolve feedback capabilities through the incoming dependency graph,
        // including nested groups, while preserving every shared branch.
        vector<JPbox *> allNodes;
        std::function<void(const vector<JPbox *> &)> collect = [&](const vector<JPbox *> &nodes) {
            for (auto *node : nodes) {
                allNodes.push_back(node);
                if (auto *group = dynamic_cast<JPbox_preset *>(node)) collect(group->boxes);
            }
        };
        collect(boxes);
        auto dependencies = [&](JPbox *root) {
            std::set<JPbox *> seen;
            std::function<void(JPbox *)> visit = [&](JPbox *node) {
                if (!node || !seen.insert(node).second) return;
                if (auto *group = dynamic_cast<JPbox_preset *>(node)) {
                    if (group->activeRender >= 0 && group->activeRender < int(group->boxes.size()))
                        visit(group->boxes[group->activeRender]);
                }
                for (int i = 0; i < node->fbohandlergroup.getSize(); ++i)
                    if (node->fbohandlergroup.getisPointerSet(i))
                        for (auto *candidate : allNodes)
                            if (&candidate->fbo == node->fbohandlergroup.getFboPointerReference(i)) visit(candidate);
            };
            visit(root);
            return seen;
        };
        const auto shared = dependencies(boxes[lastCompositedActiveRender]);
        vector<JPbox_shader *> feedbackTargets;
        for (auto *node : dependencies(boxes[targetIndex]))
            if (!shared.count(node)) if (auto *shader = dynamic_cast<JPbox_shader *>(node))
                if (shader->shader.getUniformLocation("feedback") >= 0) feedbackTargets.push_back(shader);
        caps.feedback = !feedbackTargets.empty();
        activeRenderTransition.setCapabilities(caps);
        activeRenderTransition.setLerpValue(0);
        if (activeRenderTransition.state().effect() == jp_transition::Feedback)
            for (auto *shader : feedbackTargets)
                shader->seedTransitionFeedback(activeTransitionSnapshot.isAllocated() ? activeTransitionSnapshot : boxes[lastCompositedActiveRender]->fbo);
        if(activeRenderTransition.state().effect()==jp_transition::Morph || activeRenderTransition.state().effect()==jp_transition::StagedMorph) {
            localMorphSource=source;localMorphTarget=target;
            for(int i=0;i<target->parameters.getSize();++i) {
                auto *a=source->parameters.getJParameter(i),*b=target->parameters.getJParameter(i);
                if(b->variabletype==JPParameter::BOOL)b->setMorph(a->boolValue?1.f:0.f,1.f);
                else {a->setMorph(b->floatValue,0.f);b->setMorph(a->floatValue,1.f);}
            }
        }
		activeRenderTransitionTarget = targetIndex;
		activeRenderTransitionRunning = true;
	}

}
void JPbox_preset::clearActiveRenderMorph()
{
    for(auto *node:{localMorphSource,localMorphTarget})
        if(node && std::find(boxes.begin(),boxes.end(),node)!=boxes.end())
            for(int i=0;i<node->parameters.getSize();++i)node->parameters.getJParameter(i)->clearMorph();
    localMorphSource=nullptr;localMorphTarget=nullptr;
}
void JPbox_preset::updateActiveRenderMorph()
{
    if(!localMorphSource || !localMorphTarget)return;
    if(std::find(boxes.begin(),boxes.end(),localMorphSource)==boxes.end() ||
        std::find(boxes.begin(),boxes.end(),localMorphTarget)==boxes.end() || activeRenderTransition.getLerpValue()>=1.f) {clearActiveRenderMorph();return;}
    auto *target=dynamic_cast<JPbox_shader *>(localMorphTarget);
    for(int i=0;i<target->parameters.getSize();++i) {
        auto category=activeRenderTransition.state().effect()==jp_transition::StagedMorph?
            jp_transition::category(target->shader.getShaderSource(GL_FRAGMENT_SHADER),target->parameters.getName(i)):jp_transition::Category::None;
        const float amount=jp_transition::morphProgress(activeRenderTransition.getLerpValue(),category);
        auto *a=localMorphSource->parameters.getJParameter(i),*b=target->parameters.getJParameter(i);
        if(a->isMorphing())a->morphAmount=amount;
        if(b->isMorphing())b->morphAmount=1.f-amount;
    }
}
void JPbox_preset::renderActiveRender()
{
    if(boxes.empty() || activeRender<0 || activeRender>=int(boxes.size()))return;
    const int targetIndex=activeRender;

	ofPushStyle();
	// Rect mode is global and this runs during update(), so it inherits
	// whatever the last draw left behind. Under OF_RECTMODE_CENTER a quad drawn
	// at (0,0) spans (-w/2,-h/2) to (w/2,h/2), so only its bottom-right quarter
	// lands inside the FBO - the group ends up holding a full-scale crop in its
	// top-left corner and nothing else.
	//
	// That is invisible in the group's own thumbnail but ruins the whole screen
	// when the group is the ACTIVE RENDER: the node background is drawn from
	// this FBO, so a quarter-filled FBO paints a quarter-filled canvas. It
	// looked intermittent because it depends on what happened to draw last -
	// clicking any box changes the sequence and the next composite comes out
	// right.
	//
	// Every sibling that composites one FBO into another already does this:
	// JPbox::tryPassThroughFBO, drawSourceInto, drawNodeEditorBackground.
	ofSetRectMode(OF_RECTMODE_CORNER);
	// Set here rather than only in the else branch below, so the transition's
	// fallback draw cannot composite through a stale colour.
	ofSetColor(255, 255, 255, 255);
	fbo.begin();
	ofClear(0, 0, 0, 0);
	ofEnableBlendMode(OF_BLENDMODE_DISABLED);
	if (activeRenderTransitionRunning)
	{
		float progress = activeRenderTransition.getLerpValue();
		float easedProgress = progress * progress * (3.0f - 2.0f * progress);
		if (!activeRenderTransition.renderStraightMix(
			activeTransitionSnapshot.isAllocated()?&activeTransitionSnapshot:&boxes[lastCompositedActiveRender]->fbo,
			&boxes[targetIndex]->fbo, easedProgress,
			fbo.getWidth(), fbo.getHeight()))
		{
			boxes[targetIndex]->fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
		}
	}
	else
	{
		ofSetColor(255, 255, 255, 255);
		boxes[targetIndex]->fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
	}
	fbo.end();
	ofEnableAlphaBlending();
	ofPopStyle();

	if (activeRenderTransitionRunning && activeRenderTransition.getLerpValue() >= 1.0f)
	{
		lastCompositedActiveRender = activeRenderTransitionTarget;
        activeTransitionSnapshot.clear();
		activeRenderTransitionRunning = false;
	}
}

void JPbox_preset::draw()
{
	//	cout << "DRAW " << endl;
	ofSetRectMode(OF_RECTMODE_CORNER);
	// PARA QUE EL FBO FUNCIONE BIEN NECESITA OFRECTMODE CORNER CUANDO LEVANTA EL SHADER, AS� QUE LO PONEMOS ASI
	// shaderrender.fbo.draw(x- width/2, y-height/2, width, height);
	ofSetColor(255);
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();

	draw_inlets();
	ofSetColor(255);
}

void JPbox_preset::setExposedParam(int childIndex, int paramIndex, bool exposed)
{
	if (childIndex < 0 || childIndex >= (int)exposedParams.size())
		return;
	if (paramIndex < 0 || paramIndex >= (int)exposedParams[childIndex].size())
		return;
	exposedParams[childIndex][paramIndex] = exposed;
}

bool JPbox_preset::isParamExposed(int childIndex, int paramIndex) const
{
	if (childIndex < 0 || childIndex >= (int)exposedParams.size())
		return false;
	if (paramIndex < 0 || paramIndex >= (int)exposedParams[childIndex].size())
		return false;
	return exposedParams[childIndex][paramIndex];
}

void JPbox_preset::clearExposedParams()
{
	exposedParams.clear();
	exposedParamOriginalIndices.clear();
}

void JPbox_preset::resizeExposedParams(int numChildren)
{
	exposedParams.resize(numChildren);
	exposedParamOriginalIndices.resize(numChildren);
	for (int i = 0; i < numChildren; i++)
	{
		int numParams = 0;
		if (i >= 0 && i < (int)boxes.size())
		{
			numParams = boxes[i]->parameters.getSize();
		}
		exposedParams[i].assign(numParams, false);
		exposedParamOriginalIndices[i].assign(numParams, {-1, -1});
	}
}

JPbox *JPbox_preset::findDirectChildByName(
	const string &childName) const
{
	for (JPbox *box : boxes)
	{
		if (box != nullptr && box->name == childName)
		{
			return box;
		}
	}
	return nullptr;
}

string JPbox_preset::makeUniqueExposedTextureInputName(
	const string &samplerName) const
{
	string baseName = samplerName.empty() ? "input" : samplerName;
	string candidate = baseName;
	int suffix = 2;
	auto nameExists = [this](const string &name) {
		for (const ExposedTextureInput &input :
			exposedTextureInputs)
		{
			if (input.publicName == name)
			{
				return true;
			}
		}
		return false;
	};
	while (nameExists(candidate))
	{
		candidate = baseName + "_" + ofToString(suffix++);
	}
	return candidate;
}

bool JPbox_preset::exposeTextureInput(
	const string &targetBoxName,
	const string &targetSamplerName,
	string *publicName)
{
	for (const ExposedTextureInput &input :
		exposedTextureInputs)
	{
		if (input.targetBoxName == targetBoxName &&
			input.targetSamplerName == targetSamplerName)
		{
			if (publicName != nullptr)
			{
				*publicName = input.publicName;
			}
			return true;
		}
	}

	JPbox *target = findDirectChildByName(targetBoxName);
	if (target == nullptr)
	{
		return false;
	}
	const int samplerIndex =
		target->fbohandlergroup.findIndexByName(targetSamplerName);
	if (samplerIndex < 0 ||
		target->fbohandlergroup.getisPointerSet(samplerIndex))
	{
		return false;
	}

	ExposedTextureInput input;
	input.publicName =
		makeUniqueExposedTextureInputName(targetSamplerName);
	input.targetBoxName = targetBoxName;
	input.targetSamplerName = targetSamplerName;
	exposedTextureInputs.push_back(input);
	rebuildExposedTextureInputHandlers();
	updateExposedTextureInputNodePositions();
	if (publicName != nullptr)
	{
		*publicName = input.publicName;
	}
	return true;
}

bool JPbox_preset::removeExposedTextureInput(
	const string &targetBoxName,
	const string &targetSamplerName)
{
	for (auto input = exposedTextureInputs.begin();
		input != exposedTextureInputs.end(); ++input)
	{
		if (input->targetBoxName != targetBoxName ||
			input->targetSamplerName != targetSamplerName)
		{
			continue;
		}
		JPbox *target =
			findDirectChildByName(input->targetBoxName);
		if (target != nullptr)
		{
			const int samplerIndex =
				target->fbohandlergroup.findIndexByName(
					input->targetSamplerName);
			if (samplerIndex >= 0)
			{
				target->fbohandlergroup.deleteFboPointer(
					samplerIndex);
			}
		}
		exposedTextureInputs.erase(input);
		rebuildExposedTextureInputHandlers();
		updateExposedTextureInputNodePositions();
		return true;
	}
	return false;
}

bool JPbox_preset::removeExposedTextureInputsForBox(
	const string &targetBoxName)
{
	bool removed = false;
	for (auto input = exposedTextureInputs.begin();
		input != exposedTextureInputs.end();)
	{
		if (input->targetBoxName == targetBoxName)
		{
			input = exposedTextureInputs.erase(input);
			removed = true;
		}
		else
		{
			++input;
		}
	}
	if (removed)
	{
		rebuildExposedTextureInputHandlers();
		updateExposedTextureInputNodePositions();
	}
	return removed;
}

bool JPbox_preset::isTextureInputExposed(
	const string &targetBoxName,
	const string &targetSamplerName) const
{
	return isExposedTextureInputTarget(
		targetBoxName, targetSamplerName);
}

bool JPbox_preset::isExposedTextureInputTarget(
	const string &targetBoxName,
	const string &targetSamplerName) const
{
	for (const ExposedTextureInput &input :
		exposedTextureInputs)
	{
		if (input.targetBoxName == targetBoxName &&
			input.targetSamplerName == targetSamplerName)
		{
			return true;
		}
	}
	return false;
}

void JPbox_preset::renameExposedTextureInputTarget(
	const string &oldBoxName,
	const string &newBoxName)
{
	for (ExposedTextureInput &input :
		exposedTextureInputs)
	{
		if (input.targetBoxName == oldBoxName)
		{
			input.targetBoxName = newBoxName;
		}
	}
}

bool JPbox_preset::retargetExposedTextureInput(
	const string &publicName,
	const string &targetBoxName,
	const string &targetSamplerName)
{
	JPbox *target = findDirectChildByName(targetBoxName);
	if (target == nullptr ||
		target->fbohandlergroup.findIndexByName(
			targetSamplerName) < 0)
	{
		return false;
	}
	for (ExposedTextureInput &input :
		exposedTextureInputs)
	{
		if (input.publicName == publicName)
		{
			input.targetBoxName = targetBoxName;
			input.targetSamplerName =
				targetSamplerName;
			return true;
		}
	}
	return false;
}

void JPbox_preset::setExposedTextureInputs(
	const vector<ExposedTextureInput> &inputs)
{
	exposedTextureInputs = inputs;
	pruneInvalidExposedTextureInputs();
	rebuildExposedTextureInputHandlers();
	updateExposedTextureInputNodePositions();
}

void JPbox_preset::rebuildExposedTextureInputHandlers()
{
	JPFbohandlerGroup previousHandlers = fbohandlergroup;
	fbohandlergroup.clear();
	for (const ExposedTextureInput &input :
		exposedTextureInputs)
	{
		fbohandlergroup.addFbohandler(input.publicName);
		const int previousIndex =
			previousHandlers.findIndexByName(input.publicName);
		const int newIndex = fbohandlergroup.getSize() - 1;
		if (previousIndex >= 0 &&
			previousHandlers.getisPointerSet(previousIndex))
		{
			fbohandlergroup.setFboPointer(
				previousHandlers.getFboPointerReference(
					previousIndex),
				previousHandlers.getFboNameReference(
					previousIndex),
				newIndex);
		}
	}
	fbohandlergroup.setupdragobjects(
		x, y, outlet_size, outlet_size);
}

void JPbox_preset::syncExposedTextureInputs()
{
	for (const ExposedTextureInput &input :
		exposedTextureInputs)
	{
		JPbox *target =
			findDirectChildByName(input.targetBoxName);
		if (target == nullptr)
		{
			continue;
		}
		const int samplerIndex =
			target->fbohandlergroup.findIndexByName(
				input.targetSamplerName);
		const int publicIndex =
			fbohandlergroup.findIndexByName(input.publicName);
		if (samplerIndex < 0 || publicIndex < 0)
		{
			continue;
		}
		if (fbohandlergroup.getisPointerSet(publicIndex))
		{
			target->fbohandlergroup.setFboPointer(
				fbohandlergroup.getFboPointerReference(
					publicIndex),
				fbohandlergroup.getFboNameReference(
					publicIndex),
				samplerIndex);
		}
		else
		{
			target->fbohandlergroup.deleteFboPointer(
				samplerIndex);
		}
	}
}

void JPbox_preset::pruneInvalidExposedTextureInputs()
{
	bool removed = false;
	for (auto input = exposedTextureInputs.begin();
		input != exposedTextureInputs.end();)
	{
		JPbox *target =
			findDirectChildByName(input->targetBoxName);
		if (target == nullptr ||
			target->fbohandlergroup.findIndexByName(
				input->targetSamplerName) < 0)
		{
			input = exposedTextureInputs.erase(input);
			removed = true;
		}
		else
		{
			++input;
		}
	}
	if (removed)
	{
		rebuildExposedTextureInputHandlers();
	}
}

string JPbox_preset::getExposedTextureInputTargetLabel(
	const string &publicName) const
{
	for (const ExposedTextureInput &input :
		exposedTextureInputs)
	{
		if (input.publicName == publicName)
		{
			return input.targetBoxName + "." +
				input.targetSamplerName;
		}
	}
	return "";
}

void JPbox_preset::updateExposedTextureInputNodePositions()
{
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		float inletY = y;
		if (fbohandlergroup.getSize() > 1)
		{
			inletY = y + ofMap(
				i, 0, fbohandlergroup.getSize() - 1,
				-(height / 2) * 3 / 6,
				(height / 2) * 3 / 6);
		}
		fbohandlergroup.setPos(
			x - width / 2, inletY, i);
	}
}

void JPbox_preset::clear()
{
    childTransitionScale=1.f;
    clearActiveRenderMorph();activeTransitionSnapshot.clear();
	activeRenderTransitionRunning = false;
	lastCompositedActiveRender = -1;
	activeRenderTransitionTarget = -1;
	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		boxes[i]->clear();
		delete boxes[i];
		boxes[i] = nullptr;
	}

	boxes.clear();
	exposedParams.clear();
	exposedParamOriginalIndices.clear();
	exposedTextureInputs.clear();
	fbohandlergroup.clear();
}

void JPbox_preset::addBox(JPbox &_box)
{
}

ofXml JPbox_preset::snapshotXml()
{
	ofXml xml;
	xml.appendChild("guipper_format").set(1);

	// Save activerender
	auto activerender_save = xml.appendChild("activerender");
	activerender_save.set(activeRender);

	// Save viewport zoom/pan
	auto viewportZoom_save = xml.appendChild("viewportZoom");
	viewportZoom_save.set(viewportZoom);
	auto viewportPanX_save = xml.appendChild("viewportPanX");
	viewportPanX_save.set(viewportPan.x);
	auto viewportPanY_save = xml.appendChild("viewportPanY");
	viewportPanY_save.set(viewportPan.y);

	for (int i = 0; i < (int)boxes.size(); i++)
	{
		if (boxes[i] == nullptr) continue;

		auto data = xml.appendChild("box");
		data.appendChild("nombre").set(boxes[i]->name);
		data.appendChild("x").set(boxes[i]->x);
		data.appendChild("y").set(boxes[i]->y);
		data.appendChild("directory").set(boxes[i]->dir);
		data.appendChild("uid").set(boxes[i]->uid);
		data.appendChild("tooutput").set(boxes[i]->getOutputCandidate());
		data.appendChild("onoff").set(boxes[i]->getonoff());
		data.appendChild("bypass").set(boxes[i]->getBypass());
		boxes[i]->saveCustomState(data);

		jp_parameter_xml::save(data, boxes[i]->parameters);

		// Save FBO links
		if (boxes[i]->fbohandlergroup.getPointerSetsSize() > 0)
		{
			auto fboslinks = data.appendChild("fboslinks");
			for (int k = 0; k < boxes[i]->fbohandlergroup.getSize(); k++)
			{
				if (boxes[i]->fbohandlergroup.getisPointerSet(k) &&
					!isExposedTextureInputTarget(
						boxes[i]->name,
						boxes[i]->fbohandlergroup.getName(k)))
				{
					fboslinks.appendChild(boxes[i]->fbohandlergroup.getName(k))
						.set(boxes[i]->fbohandlergroup.getFboName(k));
				}
			}
		}


	}

	if (!exposedTextureInputs.empty())
	{
		auto exposedInputsNode =
			xml.appendChild("exposedInputs");
		for (const ExposedTextureInput &input :
			exposedTextureInputs)
		{
			auto inputNode =
				exposedInputsNode.appendChild("input");
			inputNode.appendChild("name")
				.set(input.publicName);
			inputNode.appendChild("box")
				.set(input.targetBoxName);
			inputNode.appendChild("sampler")
				.set(input.targetSamplerName);
		}
	}

	// Save exposedParams at root level (to match setup() load format: xml.getChild("exposedParams"))
	if (!exposedParams.empty())
	{
		auto exposedNode = xml.appendChild("exposedParams");
		for (int ci = 0; ci < (int)exposedParams.size(); ci++)
		{
			for (int pi = 0; pi < (int)exposedParams[ci].size(); pi++)
			{
				if (exposedParams[ci][pi])
				{
					auto boxNode = exposedNode.appendChild("box");
					boxNode.set(ci);
					auto paramNode = boxNode.appendChild("param");
					paramNode.set(pi);
					// For propagated exposes (beyond child's own params), save original indices
					if (ci < (int)boxes.size() && boxes[ci] != nullptr &&
						pi >= boxes[ci]->parameters.getSize() &&
						ci < (int)exposedParamOriginalIndices.size() &&
						pi < (int)exposedParamOriginalIndices[ci].size())
					{
						auto origBoxNode = boxNode.appendChild("origBox");
						origBoxNode.set(exposedParamOriginalIndices[ci][pi].first);
						auto origParamNode = boxNode.appendChild("origParam");
						origParamNode.set(exposedParamOriginalIndices[ci][pi].second);
					}
				}
			}
		}
	}

    return xml;
}
bool JPbox_preset::save()
{
    if (dir.empty()) return false;
    for (auto* child : boxes) {
        if (auto* preset = dynamic_cast<JPbox_preset*>(child))
            if (!preset->save()) return false;
    }
    return jp::saveXml(snapshotXml(), dir);
}
