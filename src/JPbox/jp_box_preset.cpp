#include "jp_box_preset.h"

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

		JPbox *bx;
		if (directory.getValue().find(".frag") != std::string::npos)
		{
			bx = new JPbox_shader();
		}
		else if (directory.getValue().find(".jpg") != std::string::npos ||
				 directory.getValue().find(".png") != std::string::npos ||
				 directory.getValue().find(".jpeg") != std::string::npos)
		{
			bx = new JPbox_image();
		}
		else if (directory.getValue().find(".mov") != std::string::npos ||
				 directory.getValue().find(".mkv") != std::string::npos ||
				 directory.getValue().find(".mp4") != std::string::npos ||
				 directory.getValue().find(".flv") != std::string::npos ||
				 directory.getValue().find(".vob") != std::string::npos ||
				 directory.getValue().find(".avi") != std::string::npos)
		{
			bx = new JPbox_video();
		}
		else if (directory.getValue().find("cam") != std::string::npos)
		{
			bx = new JPbox_cam();
		}
#ifdef NDI
		else if (directory.getValue().find("ndiReceiver") != std::string::npos) {
			bx = new JPbox_ndi();
		}
#endif
#ifdef SPOUT
		else if (directory.getValue().find("spoutReceiver") != std::string::npos)
		{
			bx = new JPbox_spout();
		}
#endif
		else if (directory.getValue().find(".xml") != std::string::npos)
		{
			bx = new JPbox_preset();
		}
		else if (directory.getValue().find("framedifference") != std::string::npos)
		{
			bx = new JPbox_framedifference();
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

		int index = 0;
		auto parameters = box.getChild("parameters").getChildren();
		// cout << "PARAMETER SIZE SB " << sb->parameters.getSize() << endl;
		for (auto &param : parameters)
		{
			/*cout << "............" << endl;
			cout << "nombre parametro:" << param.getChild("name").getValue() << endl;
			cout << "min parametro:" << param.getChild("min").getFloatValue() << endl;
			cout << "max parametro:" << param.getChild("max").getFloatValue() << endl;
			cout << "value parametro:" << param.getChild("value").getValue() << endl;
			cout << "

			parametro:" << param.getChild("movtype").getIntValue() << endl;
			cout << "speed parametro:" << param.getChild("speed").getFloatValue() << endl;*/

			if (bx->parameters.getType(index) == bx->parameters.FLOAT)
			{

				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setMin(param.getChild("min").getFloatValue(), index);
				bx->parameters.setMax(param.getChild("max").getFloatValue(), index);
				bx->parameters.setFloatLerpValue(param.getChild("value").getFloatValue(), index);
				bx->parameters.setFloatValue(param.getChild("value").getFloatValue(), index);
				bx->parameters.setmovetype(param.getChild("movtype").getIntValue(), index);
				bx->parameters.setSpeed(param.getChild("speed").getFloatValue(), index);
				auto bpmRate = param.getChild("bpmrate");
				if (bpmRate)
				{
					bx->parameters.setBpmRate(bpmRate.getIntValue(), index);
				}
			}
			else if (bx->parameters.getType(index) == bx->parameters.BOOL)
			{
				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setBoolValue(param.getChild("value").getBoolValue(), index);
			}
			index++;
		}
		bx->loadCustomState(box);
		boxes.push_back(bx);
	}

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
	activeRender = int(ofClamp(xml.getChild("activerender").getIntValue(), 0, boxes.size() - 1));

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
		for (int i = boxes.size() - 1; i >= 0; i--)
		{
			boxes[i]->update();
			// Do NOT force onoff - user can toggle PAUSE freely in group view.
			// Initial onoff state is loaded from XML (defaults to true if not found).
		}
		if (boxes.empty() || activeRender < 0 || activeRender >= (int)boxes.size())
		{
			onoff.boolValue = false;
			return;
		}
		renderActiveRender();
	}
	else
	{
		JPbox::updateFBO();
	}
}

void JPbox_preset::renderActiveRender()
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

	if (targetIndex != lastCompositedActiveRender &&
		(!activeRenderTransitionRunning || activeRenderTransitionTarget != targetIndex))
	{
		activeRenderTransitionInitialized = true;
		activeRenderTransition.setLerpValue(0);
		activeRenderTransitionTarget = targetIndex;
		activeRenderTransitionRunning = true;
	}

	ofPushStyle();
	ofEnableAlphaBlending();
	fbo.begin();
	ofClear(0, 0, 0, 0);
	if (activeRenderTransitionRunning)
	{
		activeRenderTransition.advance();
		float progress = activeRenderTransition.getLerpValue();
		float easedProgress = progress * progress * (3.0f - 2.0f * progress);
		ofSetColor(255, 255, 255, 255);
		boxes[lastCompositedActiveRender]->fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
		ofSetColor(255, 255, 255, (unsigned char)(255.0f * easedProgress));
		boxes[targetIndex]->fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
	}
	else
	{
		ofSetColor(255, 255, 255, 255);
		boxes[targetIndex]->fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
	}
	fbo.end();
	ofPopStyle();

	if (activeRenderTransitionRunning && activeRenderTransition.getLerpValue() >= 1.0f)
	{
		lastCompositedActiveRender = activeRenderTransitionTarget;
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

	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		ofNoFill();
		ofSetColor(0);
		ofDrawEllipse(fbohandlergroup.getPosX(i),
			fbohandlergroup.getPosY(i), inlet_size, inlet_size);
		ofFill();
		const bool linked =
			fbohandlergroup.getisPointerSet(i);
		const bool hovered = fbohandlergroup.mouseOver(i);
		ofSetColor(linked ?
			(hovered ? ofColor(100, 255, 0, 255) :
			 ofColor(0, 120, 0, 255)) :
			(hovered ? COL_ACCENT_RED :
			 ofColor(COL_ACCENT_RED, 190)));
		ofDrawEllipse(fbohandlergroup.getPosX(i),
			fbohandlergroup.getPosY(i), inlet_size, inlet_size);
	}
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

void JPbox_preset::save()
{
	// Save internal boxes back to this preset's XML file
	if (dir.empty()) return;

	ofXml xml;

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
		data.appendChild("onoff").set(boxes[i]->getonoff());
		data.appendChild("bypass").set(boxes[i]->getBypass());
		boxes[i]->saveCustomState(data);

		if (boxes[i]->parameters.getSize() > 0)
		{
			auto parameters = data.appendChild("parameters");
			for (int k = 0; k < boxes[i]->parameters.getSize(); k++)
			{
				if (boxes[i]->parameters.getType(k) == boxes[i]->parameters.BOOL)
				{
					auto param = parameters.appendChild("param");
					param.appendChild("name").set(boxes[i]->parameters.getName(k));
					param.appendChild("value").set(boxes[i]->parameters.getBoolValue(k));
				}
				else
				{
					auto param = parameters.appendChild("param");
					param.appendChild("name").set(boxes[i]->parameters.getName(k));
					param.appendChild("min").set(boxes[i]->parameters.getMin(k));
					param.appendChild("max").set(boxes[i]->parameters.getMax(k));
					param.appendChild("value").set(boxes[i]->parameters.getFloatValue(k));
					param.appendChild("movtype").set(boxes[i]->parameters.getMovType(k));
					param.appendChild("speed").set(boxes[i]->parameters.getSpeed(k));
					param.appendChild("bpmrate").set(boxes[i]->parameters.getBpmRate(k));
				}
			}
		}

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

		// Recursively save nested presets
		if (boxes[i]->getTipo() == JPbox::PRESETBOX)
		{
			JPbox_preset *childPreset = dynamic_cast<JPbox_preset *>(boxes[i]);
			if (childPreset != nullptr)
			{
				childPreset->save();
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

	ofFilePath::createEnclosingDirectory(dir);
	xml.save(dir);
}
