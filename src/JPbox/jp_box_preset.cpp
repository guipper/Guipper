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
	activeRender = xml.getChild("activerender").getIntValue();
}

void JPbox_preset::update()
{
	JPbox::update();
	updateFBO();
}

void JPbox_preset::updateFBO()
{
	// onoff.boolValue = true;
	if (onoff.boolValue)
	{
		for (int i = boxes.size() - 1; i >= 0; i--)
		{
			boxes[i]->update();
			boxes[i]->onoff.boolValue = true;
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

void JPbox_preset::clear()
{
	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		boxes[i]->clear();
		boxes[i] = nullptr;
		delete boxes.at(i);
	}

	boxes.clear();
}

void JPbox_preset::addBox(JPbox &_box)
{
}
