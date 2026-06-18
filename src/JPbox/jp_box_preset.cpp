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
		bx->setup(directory.getValue(), nombre.getValue());
		bx->setPos(x.getIntValue(), y.getIntValue());

		// Load onoff and bypass states
		auto onoffChild = box.getChild("onoff");
		if (onoffChild)
		{
			bx->setonoff(onoffChild.getBoolValue());
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
			}
			else if (bx->parameters.getType(index) == bx->parameters.BOOL)
			{
				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setBoolValue(param.getChild("value").getBoolValue(), index);
			}
			index++;
		}
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
				if (childIndex >= 0 && childIndex < (int)exposedParams.size() &&
					paramIndex >= 0 && paramIndex < (int)exposedParams[childIndex].size())
				{
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

	// Una vez que cargo todas las cajitas les cargamos los links :
	// Mira lo que esta este algoritmo para levantar los links entre cajitas papa !!!
	int index1 = 0;
	int index2 = 0;
	for (auto &box : boxloader)
	{
		auto fboslinks = box.getChild("fboslinks").getChildren();
		index2 = 0;
		for (auto &fbolink : fboslinks)
		{
			for (int i = 0; i < boxes.size(); i++)
			{
				if (boxes[i]->name == fbolink.getValue() && i != index1)
				{
					ofFbo *fbopointer = &boxes[i]->fbo;
					string *fbopointername = &boxes[i]->name;
					boxes[index1]->fbohandlergroup.setFboPointer(fbopointer, fbopointername, index2);
				}
			}
			index2++;
		}
		index1++;
	}
	//activeRender = xml.getChild("activerender").getIntValue();
	activeRender = int(ofClamp(xml.getChild("activerender").getIntValue(), 0, boxes.size() - 1));
}

void JPbox_preset::update()
{
	JPbox::update();
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
			// Force children onoff to true so they render by default.
			// PAUSE (bypass) is NOT affected and can be toggled independently.
			boxes[i]->onoff.boolValue = true;
		}
		if (boxes.empty() || activeRender < 0 || activeRender >= (int)boxes.size())
		{
			onoff.boolValue = false;
			return;
		}
		ofSetColor(255, 255);
		fbo.begin();
		boxes[activeRender]->fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
		fbo.end();
	}
	else
	{
		JPbox::updateFBO();
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

void JPbox_preset::clear()
{
	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		boxes[i]->clear();
		delete boxes[i];
		boxes[i] = nullptr;
	}

	boxes.clear();
	exposedParams.clear();
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
				}
			}
		}

		// Save FBO links
		if (boxes[i]->fbohandlergroup.getPointerSetsSize() > 0)
		{
			auto fboslinks = data.appendChild("fboslinks");
			for (int k = 0; k < boxes[i]->fbohandlergroup.getSize(); k++)
			{
				if (boxes[i]->fbohandlergroup.getisPointerSet(k))
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
