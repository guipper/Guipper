
#include "defines.h"
#include "jp_box.h"
#include "../ofApp.h"

namespace
{
	void updateHoverStart(bool isMouseOver, uint64_t &hoverStartMillis)
	{
		if (isMouseOver)
		{
			if (hoverStartMillis == 0)
			{
				hoverStartMillis = ofGetElapsedTimeMillis();
			}
		}
		else
		{
			hoverStartMillis = 0;
		}
	}

	bool shouldShowTooltip(uint64_t hoverStartMillis)
	{
		return hoverStartMillis != 0 && ofGetElapsedTimeMillis() - hoverStartMillis >= 1000;
	}

	void drawTooltip(const string &text, float anchorX, float anchorY)
	{
		float padX = 6;
		float padY = 4;
		float tooltipWidth = jp_constants::p_font.stringWidth(text) + padX * 2;
		float tooltipHeight = jp_constants::p_font.stringHeight(text) + padY * 2;
		float tooltipX = anchorX + tooltipWidth / 2;
		float tooltipY = anchorY - tooltipHeight / 2 - 8;

		ofSetRectMode(OF_RECTMODE_CENTER);
		ofSetColor(20, 230);
		ofRectRounded(tooltipX, tooltipY, tooltipWidth, tooltipHeight, 3);
		ofSetColor(255);
		jp_constants::p_font.drawString(text,
										tooltipX - tooltipWidth / 2 + padX,
										tooltipY + jp_constants::p_font.stringHeight(text) / 2);
	}
}

JPbox::JPbox() {}
JPbox::~JPbox() {}
void JPbox::reloadShaderonly() {}
void JPbox::reload() {}
void JPbox::setup(ofTrueTypeFont &_font)
{
	padding_top = 30;
	padding_leftright = 15;
	padding_bottom = 5;

	fbowidth = 80;
	fboheight = 80;

	triangleangle = 0;

	JPdragobject::setup(ofGetWidth() / 2, ofGetHeight() / 2,
						fbowidth + padding_leftright,
						fboheight + padding_top + padding_bottom);

	Cfront = ofColor(0, 120);
	border = ofColor(0, 200, 200, 120);
	border_mouseover = ofColor(0, 200, 200, 255);
	border_grab = ofColor(0, 255, 0, 255);
	clearBackgroundOverride();

	// font_p = &_font;
	name = "Prueba";

	outlet_x = x + width / 2;
	outlet_y = y;
	outlet_size = 30;

	inlet_size = 20;

	float topButtonSize = outlet_size * 0.42;
	onoff.setup(outlet_x, outlet_y, topButtonSize, topButtonSize);
	onoff.boolValue = false;
	bypass.setup(outlet_x - topButtonSize, outlet_y, topButtonSize, topButtonSize);
	bypass.boolValue = false;
	bypass.value = false;
	bypass.activeFlag = false;
	bypass.paleta = 1;
	titleHoverStartMillis = 0;
	bypassHoverStartMillis = 0;
	onoffHoverStartMillis = 0;
	fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
}
void JPbox::setup(string _directory, string _name)
{
	padding_top = 30;
	padding_leftright = 15;
	padding_bottom = 5;

	fbowidth = 80;
	fboheight = 80;

	triangleangle = 0;

	JPdragobject::setup(ofGetWidth() / 2, ofGetHeight() / 2,
						fbowidth + padding_leftright,
						fboheight + padding_top + padding_bottom);

	Cfront = ofColor(0, 120);
	border = ofColor(0, 200, 200, 120);
	border_mouseover = ofColor(0, 200, 200, 255);
	border_grab = ofColor(0, 255, 0, 255);
	clearBackgroundOverride();

	outlet_x = x + width / 2;
	outlet_y = y;
	outlet_size = 30;

	inlet_size = 20;

	float topButtonSize = outlet_size * 0.42;
	onoff.setup(outlet_x, outlet_y, topButtonSize, topButtonSize);
	onoff.boolValue = false;
	bypass.setup(outlet_x - topButtonSize, outlet_y, topButtonSize, topButtonSize);
	bypass.boolValue = false;
	bypass.value = false;
	bypass.activeFlag = false;
	bypass.paleta = 1;
	titleHoverStartMillis = 0;
	bypassHoverStartMillis = 0;
	onoffHoverStartMillis = 0;
	fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);

	name = _name;
	dir = _directory;
}
void JPbox::update()
{
	parameters.update();
	// onoff.update();
	float topButtonSize = outlet_size * 0.42;
	float topButtonGap = 4;
	float topButtonY = y - height / 2 + padding_top * 0.42;
	float rightButtonX = x + width / 2 - 8 - topButtonSize / 2;

	onoff.width = topButtonSize;
	onoff.height = topButtonSize;
	bypass.width = topButtonSize;
	bypass.height = topButtonSize;
	onoff.setPos(rightButtonX, topButtonY);
	bypass.setPos(rightButtonX - topButtonSize - topButtonGap, topButtonY);
	// updateFBO();

	outlet_x = x + width / 2 - outlet_size / 2;
	outlet_y = y;
}
void JPbox::draw()
{

	ofSetColor(Cfront);
	ofNoFill();

	if (mouseOver() || activeFlag)
	{
		if (activeFlag)
		{
			ofSetColor(border_grab);
		}
		else
		{
			ofSetColor(border_mouseover);
		}
	}
	else
	{
		ofSetColor(border);
	}

	if (!ofGetMousePressed())
	{
		activeFlag = false;
		outletActiveFlag = false;
	}

	// CAJA GRIS:
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofSetLineWidth(useBackgroundOverride ? 2 : 3);
	ofSetColor(useBackgroundOverride ? backgroundBorderOverride : ofColor(0));
	ofRectRounded(x, y, width, height, 10);
	if (useBackgroundOverride)
	{
		ofColor cueBg = backgroundOverride;
		if (mouseOver() || activeFlag)
		{
			cueBg = cueBg.getLerped(ofColor(255), 0.18);
		}
		ofSetColor(cueBg);
		ofFill();
	}
	else if (mouseOver() || activeFlag){
		ofSetColor(200);
		ofFill(); 
	}
	else {
		ofSetColor(150);
		ofFill();
	}
	ofRectRounded(x, y, width, height, 10);
	ofSetColor(Cfront);
	ofSetColor(0);
	float sepsize = 10; // SEPARACION ENTRE LA LINEA Y LA CAJA Y LA ALINEACION DEL TEXTO.
	float linewidth = width / 2 - sepsize;
	float lineheight = 2;
	float titleY = y - height / 2 + padding_top * 0.58;
	float dividerY = y - height / 2 + padding_top * 0.76;
	float nameX = x - width / 2 + sepsize;
	float nameMaxWidth = bypass.x - bypass.width / 2 - 4 - nameX;

	// LINEA DEBAJO DEL TEXTO :
	ofSetLineWidth(lineheight);
	ofSetColor(35);
	ofDrawLine(x - linewidth, dividerY, x + linewidth, dividerY);

	// TEXTO :
	string shortname = name;
	if (jp_constants::p_font.stringWidth(shortname) > nameMaxWidth)
	{
		string dots = "...";
		while (!shortname.empty() && jp_constants::p_font.stringWidth(shortname + dots) > nameMaxWidth)
		{
			shortname.pop_back();
		}
		shortname += dots;
	}
	float nameTextWidth = jp_constants::p_font.stringWidth(shortname);
	float mouseX = JPdragobject::getMouseX();
	float mouseY = JPdragobject::getMouseY();
	bool titleMouseOver = mouseX >= nameX &&
						  mouseX <= nameX + nameTextWidth &&
						  mouseY >= titleY - jp_constants::p_font.stringHeight(shortname) &&
						  mouseY <= titleY + 3;
	updateHoverStart(titleMouseOver, titleHoverStartMillis);
	ofSetColor(20);
	jp_constants::p_font.drawString(shortname,
									nameX,
									titleY);
	// BOTON SET ACTIVE RENDER :
	// DIBUJAR CABLECITO.
	ofSetColor(255);
	if (outletActiveFlag)
	{
		ofSetColor(ofColor(0, 255,0).getLerped(ofColor(255),sin(ofGetElapsedTimeMillis()*0.01)*.5+.5));
		ofDrawLine(outlet_x, outlet_y, mouseX, mouseY);
	}

	// JPbox::draw_outlet();
	ofSetRectMode(OF_RECTMODE_CENTER);
	bool bypassMouseOver = bypass.mouseOver();
	bool onoffMouseOver = onoff.mouseOver();
	updateHoverStart(bypassMouseOver, bypassHoverStartMillis);
	updateHoverStart(onoffMouseOver, onoffHoverStartMillis);

	bypass.draw();
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofColor bypassColor = bypass.boolValue ? ofColor(255, 0, 0, 255) : ofColor(100, 0, 0, 255);
	if (bypassMouseOver)
	{
		bypassColor = bypassColor.getLerped(ofColor(255), 0.35);
	}
	ofSetColor(bypassColor);
	ofDrawRectangle(bypass.x, bypass.y, bypass.width, bypass.height);
	if (bypass.boolValue || bypassMouseOver)
	{
		ofNoFill();
		ofSetColor(bypass.boolValue ? ofColor(255, 180, 180, 255) : ofColor(255, 120, 120, 220));
		ofDrawRectangle(bypass.x, bypass.y, bypass.width, bypass.height);
		ofFill();
	}
	onoff.draw();
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofColor onoffColor = onoff.boolValue ? ofColor(100, 100, 100, 255) : ofColor(255, 255, 255, 255);
	if (onoffMouseOver)
	{
		onoffColor = onoffColor.getLerped(ofColor(255), 0.25);
	}
	ofSetColor(onoffColor);
	ofDrawRectangle(onoff.x, onoff.y, onoff.width, onoff.height);
	if (onoffMouseOver)
	{
		ofNoFill();
		ofSetColor(onoff.boolValue ? ofColor(255, 255, 255, 220) : ofColor(30, 30, 30, 220));
		ofSetLineWidth(1);
		ofDrawRectangle(onoff.x, onoff.y, onoff.width, onoff.height);
		ofFill();
	}
	if (shouldShowTooltip(titleHoverStartMillis))
	{
		drawTooltip(name, nameX + nameTextWidth / 2, titleY - 8);
	}
	if (shouldShowTooltip(bypassHoverStartMillis))
	{
		drawTooltip("Bypass", bypass.x, bypass.y - bypass.height / 2);
	}
	if (shouldShowTooltip(onoffHoverStartMillis))
	{
		drawTooltip("Pause", onoff.x, onoff.y - onoff.height / 2);
	}
	ofSetColor(255, 255, 255, 255);
}
void JPbox::updateFBO()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	fbo.begin();

	ofSetColor(255, 255);
	// ofDrawRectangle(0, 0, fbo.getWidth(), fbo.getHeight());
	fbo.draw(0, 0, fbo.getWidth(), fbo.getHeight());
	// ofSetColor(255, 0, 0);
	// ofNoFill();
	// ofDrawEllipse(fbo.getWidth() / 2, fbo.getHeight() / 2,500,500);
	// ofFill();
	fbo.end();
}
void JPbox::draw_outlet()
{
	float bordersizemult = 0.6;

	float trianglesize = outlet_size * 5.0; // ESTO LO MODIFICO ACA PARA NO MODIFICAR EL DRAGOBJECT
	float spheresize = outlet_size * 1.0;

	float bordertrianglesize = trianglesize * (1.0 + bordersizemult);
	float borderoffsetx = trianglesize * (bordersizemult / 2);

	float xtri = outlet_size / 2;
	float ytri = 0;

	ofSetLineWidth(3);

	// DIBUJAR TRIANGULO BORDE:
	/*ofPushMatrix();
	ofTranslate(outlet_x + trianglesize / 2, outlet_y);
	ofRotate(ofRadToDeg(triangleangle));

	ofSetColor(0);
	ofTranslate(-trianglesize / 2, -trianglesize / 2);
	ofTranslate( (trianglesize / 2)*0.6, trianglesize / 2);
	ofScale(1.08);
	ofTranslate(-trianglesize / 2 * 0.6, -trianglesize / 2);
	ofNoFill();
	ofDrawTriangle(xtri, ytri,
		outlet_size + xtri, outlet_size / 2 + ytri,
		0 + xtri, outlet_size + ytri);
	//ofDrawEllipse(0, 0, spheresize, spheresize);
	ofPopMatrix();*/

	ofFill();

	// DIBUJAR TRIANGULO EXTERNO
	ofPushMatrix();
	ofTranslate(outlet_x + outlet_size / 2, outlet_y);
	ofRotate(ofRadToDeg(triangleangle) + 90);
	if (mouseOverOutlet()){
		//ofSetColor(255, 217, 15, 255);
		ofSetColor(ofColor(255, 217, 15, 250).getLerped(ofColor(255), 0.75));
	}
	else{
		ofSetColor(255, 217, 15, 255);
		//ofSetColor(ofColor(255, 217, 15, 250).getLerped(ofColor(0),0.5));
	}

	//DIBUJO TRIANGULO SIN IMAGEN. ESTE CODIGO SIRVE PARA DEBUGEAR : 
	//ofTranslate(-outlet_size / 2, -outlet_size / 2);
	/*ofDrawTriangle(xtri, ytri,
		outlet_size+ xtri, outlet_size/2+ytri,
		0+ xtri, outlet_size+ ytri);*/
	// ofDrawEllipse(outlet_size / 2, outlet_size / 2, spheresize, spheresize);

	float gotasize = 0.2;
	jp_constants_img::outlet_img.draw(0, 0,
									  jp_constants_img::outlet_img.getWidth() * gotasize,
									  jp_constants_img::outlet_img.getHeight() * gotasize);

	ofPopMatrix();

	ofFill();
	ofSetColor(255, 255, 255, 255);
}
void JPbox::clear()
{
	//cout << "JP_BOX clear" << endl;
	parameters.clear();
	fbohandlergroup.clear();

	fbo.destroy();
	// fbo = nullptr;
}
bool JPbox::mouseOverOutlet()
{	


	float auxoutletsize = outlet_size ;

	float mouseX = JPdragobject::getMouseX();
	float mouseY = JPdragobject::getMouseY();
	if (mouseX > outlet_x - auxoutletsize &&
		mouseX < outlet_x + auxoutletsize &&
		mouseY > outlet_y - auxoutletsize &&
		mouseY < outlet_y + auxoutletsize)
	{
		return true;
	}
	else
	{
		return false;
	}
}
int JPbox::getTipo()
{
	return tipo;
}
void JPbox::setonoff(bool _val)
{
	onoff.boolValue = _val;
	onoff.value = _val;
}
bool JPbox::getonoff()
{
	return onoff.boolValue;
}
void JPbox::setBypass(bool _val)
{
	bypass.boolValue = _val;
	bypass.value = _val;
	bypass.activeFlag = false;
}
bool JPbox::getBypass()
{
	return bypass.boolValue;
}
bool JPbox::tryPassThroughFBO()
{
	if (!bypass.boolValue)
	{
		return false;
	}
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		if (fbohandlergroup.getisPointerSet(i))
		{
			ofSetRectMode(OF_RECTMODE_CORNER);
			ofSetColor(255, 255);
			fbo.begin();
			fbohandlergroup.getFboPointer(i).draw(0, 0, fbo.getWidth(), fbo.getHeight());
			fbo.end();
			return true;
		}
	}
	return false;
}
