
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
		ofSetColor(COL_BG_INPUT, 230);
		ofRectRounded(tooltipX, tooltipY, tooltipWidth, tooltipHeight, 3);
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString(text,
										tooltipX - tooltipWidth / 2 + padX,
										tooltipY + jp_constants::p_font.stringHeight(text) / 2);
	}
}

std::string jp_boxuid::mint()
{
	// Function-local statics: no static-initialisation-order problem, and the
	// salt is fixed for the process.
	static const uint64_t salt = (uint64_t)ofGetSystemTimeMillis();
	static uint64_t counter = 0;
	return "b" + ofToString(salt) + "_" + ofToString(++counter);
}

JPbox::JPbox()
{
	uid = jp_boxuid::mint();
	// In the constructor, not setup(): a box can be linked by load or paste
	// before either setup overload runs, and &fbo is stable for its lifetime.
	fbohandlergroup.setOwnerFbo(&fbo);
}
JPbox::~JPbox() {}
void JPbox::saveCustomState(ofXml &boxNode) const {}
void JPbox::loadCustomState(const ofXml &boxNode) {}
void JPbox::copyCustomStateFrom(const JPbox *source) {}
void JPbox::reloadShaderonly() {}
void JPbox::reload() {}
void JPbox::setup(ofTrueTypeFont &_font)
{
	isactiverender = false;
	padding_top = 30;
	padding_leftright = 15;
	padding_bottom = 5;

	fbowidth = 80;
	fboheight = 80;

	triangleangle = 0;

	JPdragobject::setup(ofGetWidth() / 2, ofGetHeight() / 2,
						fbowidth + padding_leftright,
						fboheight + padding_top + padding_bottom);

	Cfront = ofColor(COL_BG_INPUT, 120);
	border = COL_BOX_BORDER;
	border_mouseover = COL_BOX_BORDER_HOVER;
	border_grab = COL_BOX_BORDER_GRAB;
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
	isactiverender = false;
	padding_top = 30;
	padding_leftright = 15;
	padding_bottom = 5;

	fbowidth = 80;
	fboheight = 80;

	triangleangle = 0;

	JPdragobject::setup(ofGetWidth() / 2, ofGetHeight() / 2,
						fbowidth + padding_leftright,
						fboheight + padding_top + padding_bottom);

	Cfront = ofColor(COL_BG_INPUT, 120);
	border = COL_BOX_BORDER;
	border_mouseover = COL_BOX_BORDER_HOVER;
	border_grab = COL_BOX_BORDER_GRAB;
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
	// The squares stay 12.6px; only the clickable area grows.
	//
	// Horizontally that is capped at HALF the gap between them - any more and
	// the two hit rects overlap, making which button you hit depend on the
	// order they happen to be tested in. Vertically there is no neighbour, so
	// it can be generous, and vertical slop is what a small target needs most
	// once the canvas is zoomed out.
	onoff.hitPaddingX = bypass.hitPaddingX = topButtonGap * 0.5f;
	onoff.hitPaddingY = bypass.hitPaddingY = topButtonSize * 0.5f;
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

	// CAJA OSCURA (unified dark theme):
	ofSetRectMode(OF_RECTMODE_CENTER);
	ofSetLineWidth(useBackgroundOverride ? 2 : 3);
	ofSetColor(useBackgroundOverride ? backgroundBorderOverride : COL_BOX_BORDER);
	ofRectRounded(x, y, width, height, 10);
	if (useBackgroundOverride)
	{
		ofColor cueBg = backgroundOverride;
		if (mouseOver() || activeFlag)
		{
			cueBg = cueBg.getLerped(COL_TEXT_PRIMARY, 0.18);
		}
		ofSetColor(cueBg);
		ofFill();
	}
	else if (mouseOver() || activeFlag){
		ofSetColor(COL_BG_HOVER);
		ofFill(); 
	}
	else {
		ofSetColor(COL_BG_BOX);
		ofFill();
	}
	ofRectRounded(x, y, width, height, 10);
	ofSetColor(Cfront);
	ofSetColor(COL_BORDER_MUTED);
	float sepsize = 10; // SEPARACION ENTRE LA LINEA Y LA CAJA Y LA ALINEACION DEL TEXTO.
	float linewidth = width / 2 - sepsize;
	float lineheight = 2;
	float titleY = y - height / 2 + padding_top * 0.58;
	float dividerY = y - height / 2 + padding_top * 0.76;
	float nameX = x - width / 2 + sepsize;
	float nameMaxWidth = bypass.x - bypass.width / 2 - 4 - nameX;

	// LINEA DEBAJO DEL TEXTO :
	ofSetLineWidth(lineheight);
	ofSetColor(COL_BORDER_MUTED);
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
	ofSetColor(COL_TEXT_PRIMARY);
	jp_constants::p_font.drawString(shortname,
									nameX,
									titleY);
	// BOTON SET ACTIVE RENDER :
	// DIBUJAR CABLECITO.
	ofSetColor(COL_TEXT_PRIMARY);
	if (outletActiveFlag)
	{
		ofSetColor(COL_ACCENT_CYAN.getLerped(COL_TEXT_PRIMARY,sin(ofGetElapsedTimeMillis()*0.01)*.5+.5));
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
	ofColor bypassColor = bypass.boolValue ? COL_ACCENT_RED : COL_ACCENT_RED_DIM;
	if (bypassMouseOver)
	{
		bypassColor = bypassColor.getLerped(COL_TEXT_PRIMARY, 0.35);
	}
	ofSetColor(bypassColor);
	ofDrawRectangle(bypass.x, bypass.y, bypass.width, bypass.height);
	if (bypass.boolValue || bypassMouseOver)
	{
		ofNoFill();
		ofSetColor(bypass.boolValue ? ofColor(COL_TEXT_PRIMARY, 255) : ofColor(COL_TEXT_PRIMARY, 200));
		ofDrawRectangle(bypass.x, bypass.y, bypass.width, bypass.height);
		ofFill();
	}
	onoff.draw();
	ofSetRectMode(OF_RECTMODE_CENTER);
	// Semantic: playing (onoff true) = green (live), paused = amber.
	ofColor onoffColor = onoff.boolValue ? COL_ACCENT_GREEN : COL_ACCENT_GOLD_DIM;
	if (onoffMouseOver)
	{
		onoffColor = onoffColor.getLerped(COL_TEXT_PRIMARY, 0.25);
	}
	ofSetColor(onoffColor);
	ofDrawRectangle(onoff.x, onoff.y, onoff.width, onoff.height);
	if (onoffMouseOver)
	{
		ofNoFill();
		ofSetColor(onoff.boolValue ? ofColor(COL_TEXT_PRIMARY, 220) : ofColor(COL_BG_INPUT, 220));
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
	ofSetColor(COL_TEXT_PRIMARY);
}
void JPbox::updateFBO()
{
	// Paused boxes hold their last rendered frame. Drawing an FBO into itself
	// is undefined feedback and, with alpha blending enabled, repeatedly
	// premultiplies soft edges and tints them with the UI text color.
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
		ofSetColor(COL_ACCENT_CYAN_DIM.getLerped(COL_TEXT_PRIMARY, 0.75));
	}
	else{
		ofSetColor(COL_ACCENT_CYAN_DIM);
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
	ofSetColor(COL_TEXT_PRIMARY);
}
void JPbox::clear()
{
	//cout << "JP_BOX clear" << endl;
	parameters.clear();
	fbohandlergroup.clear();

	fbo.destroy();
	// fbo = nullptr;
}
ofRectangle JPbox::outletBounds() const
{
	// Centred on the DOT, which draw_outlet puts at outlet_x + outlet_size/2 -
	// not at outlet_x. The original inline test used outlet_x, so the clickable
	// region sat half an outlet to the LEFT of the thing being aimed at: it
	// still covered the dot, but with no margin at all on its outer side and
	// 45px of dead area reaching back into the box, where it competed with the
	// box's own drag area.
	const float centreX = outlet_x + outlet_size * 0.5f;
	// Comfortably larger than the dot without reaching so far past the box edge
	// that it starts catching clicks meant for a neighbouring box's inlet -
	// boxes are dropped about 112px apart.
	const float half = outlet_size * 0.75f;
	return ofRectangle(centreX - half, outlet_y - half, half * 2.0f, half * 2.0f);
}

bool JPbox::mouseOverOutlet()
{
	// Does not go through mouseOver(), so it needs the same occlusion guard.
	if (!jp_pointer::available()) return false;
	return outletBounds().inside(
		JPdragobject::getMouseX(), JPdragobject::getMouseY());
}

void JPbox::drawHitboxDebug()
{
	if (!jp_hitbox::debugEnabled()) return;
	// Drawn from JPboxgroup AFTER the box has painted itself, so the outlines
	// sit on top of the dots and squares they describe rather than under them.
	//
	// The box's own selectable/draggable area.
	jp_hitbox::draw(hitBounds(), mouseOver());
	// The two top-right toggles.
	jp_hitbox::draw(bypass.hitBounds(), bypass.mouseOver());
	jp_hitbox::draw(onoff.hitBounds(), onoff.mouseOver());
	// Texture OUT.
	jp_hitbox::draw(outletBounds(), mouseOverOutlet());
	// Texture IN, one per inlet. Drawn from the base class because the dots
	// themselves are drawn by each box type - repeating this in every one of
	// them is how an overlay ends up missing exactly the box you are debugging.
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		jp_hitbox::draw(fbohandlergroup.getHitBounds(i),
			fbohandlergroup.mouseOver(i));
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
void JPbox::setOutputCandidate(bool _val)
{
	outputCandidate = _val;
}
bool JPbox::getOutputCandidate() const
{
	return outputCandidate;
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
			ofPushStyle();
			ofSetRectMode(OF_RECTMODE_CORNER);
			ofSetColor(255, 255, 255, 255);
			fbo.begin();
			ofClear(0, 0, 0, 0);
			ofEnableBlendMode(OF_BLENDMODE_DISABLED);
			fbohandlergroup.getFboPointer(i).draw(0, 0, fbo.getWidth(), fbo.getHeight());
			fbo.end();
			ofEnableAlphaBlending();
			ofPopStyle();
			return true;
		}
	}
	return false;
}
