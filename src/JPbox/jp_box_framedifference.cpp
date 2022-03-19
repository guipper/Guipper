#include "jp_box_framedifference.h"
#include "jp_box_cam.h"

JPbox_framedifference::JPbox_framedifference() {}
JPbox_framedifference::~JPbox_framedifference() {}
void JPbox_framedifference::reload()
{
	// img.loadImage(dir);
}
void JPbox_framedifference::setup(string _dir, string _name)
{
	JPbox::setup(_dir, _name);

	// name = "FRAMEDIFFERENCE";
	// dir = "framedifference";

	frameAnterior.allocate(jp_constants::renderWidth, jp_constants::renderHeight);

	parameters.addFloatValue(0.5, "limit");
	parameters.addFloatValue(0.5, "force");

	parameters.addBoolValue(false, "origcolor");

	tipo = FRAMEDIFFERENCEBOX;
	fbohandlergroup.addFbohandler("textura1");
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	shader.load("", "shaders/private/framedifference.frag");
	frameNum = 0;
}
void JPbox_framedifference::update()
{
	JPbox::update();
	updateFBO();
	setfbohandler_nodepos(); // MANEJAS LAS ENTRADAS
	frameNum++;
}
void JPbox_framedifference::updateFBO()
{
	if (onoff.boolValue)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		fbo.begin();
		// ofClear(0, 255);
		if (fbohandlergroup.getisPointerSet(0))
		{
			shader.begin();
			update_globalUniforms();
			update_NonglobalUniforms();
			ofRect(0, 0, fbo.getWidth(), fbo.getHeight());
			shader.end();
		}
		else
		{
			ofSetColor(255, 0, 0);
			ofDrawEllipse(fbo.getWidth() / 2, fbo.getHeight() / 2, 200, 200);
		}
		fbo.end();

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		if (fbohandlergroup.getisPointerSet(0))
		{
			frameAnterior.begin();
			fbohandlergroup.getFboPointer(0).draw(0, 0, fbo.getWidth(), fbo.getHeight());
			frameAnterior.end();
		}
	}
	else
	{
		JPbox::updateFBO();
	}
}
void JPbox_framedifference::update_NonglobalUniforms()
{
	for (int i = 0; i < parameters.getSize(); i++)
	{
		if (parameters.getType(i) == parameters.FLOAT)
		{
			shader.setUniform1f(parameters.getName(i), parameters.getFloatValue(i));
		}
		else if (parameters.getType(i) == parameters.BOOL)
		{
			shader.setUniform1f(parameters.getName(i), parameters.getBoolValue(i));
		}
	}
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		if (fbohandlergroup.getisPointerSet(i))
		{
			shader.setUniformTexture(fbohandlergroup.getName(i), fbohandlergroup.getFboPointer(i), i + 1);
			//
		}
	}

	if (fbohandlergroup.getisPointerSet(0))
	{
		shader.setUniformTexture("textura2", frameAnterior, 2);
	}
}
void JPbox_framedifference::update_globalUniforms()
{
	shader.setUniformTexture("feedback", fbo.getTexture(), 0);
	/// shader.setUniformTexture("camara", videograb->getTexture(),1);
	shader.setUniform2f("resolution", fbo.getWidth(), fbo.getHeight());
	shader.setUniform4f("mouse", ofMap(ofGetMouseX(), 0, ofGetWidth(), 0, 1),
						ofMap(ofGetMouseY(), 0, ofGetHeight(), 0, 1),
						ofMap(jp_constants::mousePressedPos.x, 0, ofGetWidth(), 0, 1),
						ofMap(jp_constants::mousePressedPos.y, 0, ofGetHeight(), 0, 1));

	shader.setUniform1i("globalframeNum", ofGetFrameNum());
	shader.setUniform1i("boxframeNum", frameNum);
	shader.setUniform2f("window_mouse", ofMap(jp_constants::window_mousex, 0, jp_constants::window_width, 0, 1),
						ofMap(jp_constants::window_mousey, 0, jp_constants::window_height, 0, 1));
	shader.setUniform1f("time", ofGetElapsedTimef());
}
void JPbox_framedifference::setfbohandler_nodepos()
{
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		float x_e = x - width / 2;
		float y_e = y;
		if (fbohandlergroup.getSize() > 1)
		{
			y_e = y + ofMap(i, 0, fbohandlergroup.getSize() - 1, -(height / 2) * 3 / 6, (height / 2) * 3 / 6);
		}
		fbohandlergroup.setPos(x_e, y_e, i);
	}
}
void JPbox_framedifference::draw()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	// PARA QUE EL FBO FUNCIONE BIEN NECESITA OFRECTMODE CORNER CUANDO LEVANTA EL SHADER, AS� QUE LO PONEMOS ASI
	// shaderrender.fbo.draw(x- width/2, y-height/2, width, height);
	ofSetColor(255);
	JPbox::draw();
	// fbo.draw(x - width / 2, y - height / 2, width, height);
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
	if (mouseOverOutlet())
	{
		ofSetColor(200, 200, 0, 150);
	}
	else
	{
		ofSetColor(0, 0, 200, 150);
	}
	// ofDrawRectangle(outlet_x, outlet_y, outlet_size, outlet_size);
	ofSetColor(255, 255);

	// DIBUJAR NODOS:
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		ofNoFill();
		ofSetColor(0);
		ofDrawEllipse(fbohandlergroup.getPosX(i), fbohandlergroup.getPosY(i), inlet_size, inlet_size);

		ofFill();
		if (fbohandlergroup.getisPointerSet(i))
		{
			if (fbohandlergroup.mouseOver(i))
			{
				ofSetColor(100, 255, 0, 255);
			}
			else
			{
				ofSetColor(0, 120, 0, 255);
			}
		}
		else
		{
			if (fbohandlergroup.mouseOver(i))
			{
				ofSetColor(200, 0, 0, 255);
			}
			else
			{
				ofSetColor(200, 0, 0, 255);
			}
		}
		ofDrawEllipse(fbohandlergroup.getPosX(i), fbohandlergroup.getPosY(i), inlet_size, inlet_size);
		// ofDrawRectangle(fbohandlergroup.getPosX(i)+ outlet_size, fbohandlergroup.getPosY(i), outlet_size, outlet_size);
	}
}
void JPbox_framedifference::clear()
{

	JPbox::clear();
	/*img.clear();
	cout << "CORRE CLEAR SHADERBOX " << endl;*/
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
