#include "jp_complexslider.h"
#include "../JPutils/jp_tooltip.h"
#include <algorithm>
JPHandler::JPHandler() {}
JPHandler::~JPHandler() {}

namespace {
constexpr bool kShowInspectorClickBounds = false;

std::string blendModeNameFromValue(float normalizedValue)
{
	const int mode = static_cast<int>(ofMap(normalizedValue, 0.0f, 1.0f, 0.0f, 25.0f, true));
	switch (mode)
	{
	case 1: return "ADD";
	case 2: return "AVERAGE";
	case 3: return "COLOR_BURN";
	case 4: return "COLOR_DODGE";
	case 5: return "DARKEN";
	case 6: return "DIFFERENCE";
	case 7: return "EXCLUSION";
	case 8: return "GLOW";
	case 9: return "HARD_LIGHT";
	case 10: return "HARD_MIX";
	case 11: return "LIGHTEN";
	case 12: return "LINEAR_BURN";
	case 13: return "LINEAR_DODGE";
	case 14: return "LINEAR_LIGHT";
	case 15: return "MULTIPLY";
	case 16: return "NEGATION";
	case 17: return "NORMAL";
	case 18: return "OVERLAY";
	case 19: return "PHOENIX";
	case 20: return "PIN_LIGHT";
	case 21: return "REFLECT";
	case 22: return "SCREEN";
	case 23: return "SOFT_LIGHT";
	case 24: return "SUBTRACT";
	case 25: return "VIVID_LIGHT";
	default: return "NONE";
	}
}

std::string fitAutomationLabel(std::string text, float maxWidth)
{
	if (jp_constants::p_font.stringWidth(text) <= maxWidth)
	{
		return text;
	}
	while (text.size() > 1 &&
		jp_constants::p_font.stringWidth(text + "..") > maxWidth)
	{
		text.pop_back();
	}
	return text + "..";
}

void drawClickBounds(JPdragobject &control, bool enabled = true)
{
	if (!kShowInspectorClickBounds)
	{
		return;
	}

	const bool hovered = enabled && control.mouseOver();
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofNoFill();
	ofSetLineWidth(hovered ? 1.5f : 1.0f);
	ofSetColor(hovered ? COL_ACCENT_GOLD :
		(enabled ? ofColor(COL_ACCENT_CYAN, 145) :
			ofColor(COL_BORDER_MUTED, 90)));
	ofDrawRectRounded(
		control.x - control.width / 2.0f,
		control.y - control.height / 2.0f,
		control.width,
		control.height,
		2.0f);
	ofPopStyle();
}

std::string bpmRateLabel(int rate)
{
	switch (rate)
	{
	case JPParameter::BPM_RATE_QUARTER: return "1/4x";
	case JPParameter::BPM_RATE_HALF: return "1/2x";
	case JPParameter::BPM_RATE_DOUBLE: return "2x";
	case JPParameter::BPM_RATE_QUADRUPLE: return "4x";
	default: return "1x";
	}
}

void drawBpmRateControl(JPdragobject &control, int rate)
{
	const bool hovered = control.mouseOver();
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(hovered ? ofColor(COL_BG_HOVER, 235) :
		ofColor(COL_BG_INPUT, 210));
	ofDrawRectRounded(
		control.x - control.width / 2.0f,
		control.y - control.height / 2.0f,
		control.width,
		control.height,
		3.0f);
	ofNoFill();
	ofSetColor(hovered ? COL_TEXT_PRIMARY :
		ofColor(COL_ACCENT_CYAN, 210));
	ofDrawRectRounded(
		control.x - control.width / 2.0f,
		control.y - control.height / 2.0f,
		control.width,
		control.height,
		3.0f);
	ofFill();
	const string label = bpmRateLabel(rate);
	ofSetColor(hovered ? COL_TEXT_PRIMARY : COL_ACCENT_CYAN);
	jp_constants::p_font.drawString(
		label,
		control.x - jp_constants::p_font.stringWidth(label) / 2.0f,
		control.y + jp_constants::p_font.stringHeight(label) / 2.0f);
	ofPopStyle();
}
}

void JPHandler::setup(float _x, float _y, float _w, float _h)
{
	JPdragobject::setup(_x, _y, _w, _h);
	paleta = 1;
	useTexture = true;
	isLeft = true;
	activeFlag = false;
}
void JPHandler::draw()
{
	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
	if (ofGetMousePressed() && mouseOver())
	{
		activeFlag = true;
	}

	const bool hovered = mouseOver();
	const bool grabbed = mouseGrab();
	const float halfMark = std::min(8.0f, height * 0.5f - 2.0f);
	const float inward = isLeft ? 1.0f : -1.0f;

	ofPushStyle();
	ofSetColor(grabbed ? COL_TEXT_PRIMARY :
		(hovered ? COL_ACCENT_CYAN : COL_ACCENT_CYAN_DIM));
	ofSetLineWidth(grabbed || hovered ? 2.0f : 1.5f);
	ofDrawLine(x, y - halfMark, x, y + halfMark);
	ofDrawLine(x, y - halfMark, x + inward * 4.0f, y - halfMark);
	ofDrawLine(x, y + halfMark, x + inward * 4.0f, y + halfMark);
	ofPopStyle();
}

JPComplexSlider::JPComplexSlider() {}
JPComplexSlider::~JPComplexSlider() {}

void JPComplexSlider::setup(float _x, float _y, float _width, float _height, JPParameter *_parameters)
{
	parameters = _parameters;
	x = _x;
	y = _y;
	width = _width;
	height = _height;

	// parameters->min = 0.0;
	// parameters->max = 1.0;

	// min = parameters->min;
	// max = parameters->max;
	value = parameters->floatValue;
	name = parameters->name;
	speed = parameters->speed;
	// cout << "MOVTYPE" << parameters->movtype << endl;

	boton_collapse.movtype = parameters->movtype;
	slider_value.movtype = parameters->movtype;

	// cout << "CORRE ESTA MIERDA" << endl;
	//_parameters.saludar();

	boton_collapse.setParametersPointer(parameters);
	boton_direccion.setParametersPointer(parameters);
	slider_value.setParametersPointer(parameters);
	boton_idayvuelta.setParametersPointer(parameters);
	boton_random.setParametersPointer(parameters);
	boton_bpm.setParametersPointer(parameters);
	slider_speed.setParametersPointer(parameters);

	activable2 = true;
	controllertype = COMPLEXSLIDER;
	testcol = ofColor(ofRandom(255), ofRandom(255), ofRandom(255));
	activeFlag = false;
	overboton_collapse = false;
	paleta = 0;
	setPosAndSize();

	useTexture = false;
	if (parameters->movtype == 0)
	{
		boton_collapse.boolValue = true;
	}
	else
	{
		boton_collapse.boolValue = false;
	}
	// cout << "SETUP JPPARAMETER" << endl;
}
float JPComplexSlider::getValue()
{
	return value;
}
void JPComplexSlider::draw()
{

	ofSetColor(COL_TEXT_PRIMARY, 100);
	ofSetRectMode(OF_RECTMODE_CENTER);
	const float panelInsetX = 1.0f;
	const float panelInsetY = 1.5f;
	jp_constants_img::fondo_parametro.draw(
		x, y,
		std::max(1.0f, width - panelInsetX * 2.0f),
		std::max(1.0f, height - panelInsetY * 2.0f));

	// Dibujar cuadrito celeste :
	if (parameters->movtype != 0)
	{
		string Strvalue = ofToString(parameters->floatValue, 2);
		if (name == "blendmode")
		{
			Strvalue = blendModeNameFromValue(parameters->floatValue);
		}
		const float labelInset = 3.0f;
		const float labelLeft =
			slider_value.x - slider_value.width / 2.0f + labelInset;
		const float labelRight =
			slider_value.x + slider_value.width / 2.0f - labelInset;
		const float valueWidth =
			jp_constants::p_font.stringWidth(Strvalue);
		const float labelBaseline = y - height * 0.18f;
		const string displayName = fitAutomationLabel(
			name, std::max(12.0f,
				labelRight - valueWidth - 12.0f - labelLeft));

		ofSetColor(COL_ACCENT_CYAN);
		jp_constants::p_font.drawString(
			displayName, labelLeft, labelBaseline);
		jp_constants::p_font.drawString(
			Strvalue, labelRight - valueWidth, labelBaseline);
	}
	slider_value.draw();
	boton_collapse.draw();
	jp_tooltip::draw("Toggle parameter automation",
		boton_collapse.x - boton_collapse.width / 2.0f,
		boton_collapse.y - boton_collapse.height / 2.0f,
		boton_collapse.width, boton_collapse.height);

	// movtype = parameters->movtype;
	if (parameters->movtype != 0)
	{
		ofSetRectMode(OF_RECTMODE_CENTER);
		handler1.draw();
		handler2.draw();
		ofSetColor(COL_ACCENT_CYAN, 255);

		ofSetRectMode(OF_RECTMODE_CORNER);
		// slider_value.value = value;
		slider_speed.draw();
		speed = slider_speed.value;
		boton_idayvuelta.draw();
		boton_random.draw();
		boton_direccion.draw();
		if (parameters->bpmEligible)
		{
			boton_bpm.draw();
			if (parameters->movtype == JPParameter::BPM)
			{
				drawBpmRateControl(bpm_rate_button, parameters->bpmRate);
			}
		}

		jp_tooltip::draw(
			parameters->movtype == JPParameter::BPM ?
				"BPM pulse decay" : "Automation speed",
			slider_speed.x - slider_speed.width / 2.0f,
			slider_speed.y - slider_speed.height / 2.0f,
			slider_speed.width, slider_speed.height);
		jp_tooltip::draw("Ping-pong automation mode",
			boton_idayvuelta.x - boton_idayvuelta.width / 2.0f,
			boton_idayvuelta.y - boton_idayvuelta.height / 2.0f,
			boton_idayvuelta.width, boton_idayvuelta.height);
		jp_tooltip::draw("Random automation mode",
			boton_random.x - boton_random.width / 2.0f,
			boton_random.y - boton_random.height / 2.0f,
			boton_random.width, boton_random.height);
		const string directionTooltip =
			parameters->movtype == JPParameter::GOIZQ ?
				"Automation direction: reverse" :
				"Automation direction: forward";
		jp_tooltip::draw(directionTooltip,
			boton_direccion.x - boton_direccion.width / 2.0f,
			boton_direccion.y - boton_direccion.height / 2.0f,
			boton_direccion.width, boton_direccion.height);
		if (parameters->bpmEligible)
		{
			jp_tooltip::draw("Sync parameter to global BPM",
				boton_bpm.x - boton_bpm.width / 2.0f,
				boton_bpm.y - boton_bpm.height / 2.0f,
				boton_bpm.width, boton_bpm.height);
			if (parameters->movtype == JPParameter::BPM)
			{
				jp_tooltip::draw(
					"BPM pulse rate: " + bpmRateLabel(parameters->bpmRate),
					bpm_rate_button.x - bpm_rate_button.width / 2.0f,
					bpm_rate_button.y - bpm_rate_button.height / 2.0f,
					bpm_rate_button.width, bpm_rate_button.height);
			}
		}
	}

	drawClickBounds(boton_collapse, activable2);
	drawClickBounds(slider_value, slider_value.activable2);
	if (parameters->movtype != 0)
	{
		drawClickBounds(handler1);
		drawClickBounds(handler2);
		drawClickBounds(slider_speed, slider_speed.activable2);
		drawClickBounds(boton_idayvuelta, boton_idayvuelta.activable2);
		drawClickBounds(boton_random, boton_random.activable2);
		drawClickBounds(boton_direccion, boton_direccion.activable2);
		if (parameters->bpmEligible)
		{
			drawClickBounds(boton_bpm, boton_bpm.activable2);
			if (parameters->movtype == JPParameter::BPM)
			{
				drawClickBounds(bpm_rate_button, boton_bpm.activable2);
			}
		}
	}
}
void JPComplexSlider::update()
{
	overboton_collapse = boton_collapse.mouseOver();
	if (mouseOver())
	{
	}

	// UPDATE
	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
	if (mouseOver() && ofGetMousePressed() && activable2)
	{
		activeFlag = true;
	}
	if (!activable2)
	{
		slider_speed.activable2 = false;
		boton_collapse.activable2 = false;
		boton_idayvuelta.activable2 = false;
		boton_random.activable2 = false;
		boton_direccion.activable2 = false;
		boton_bpm.activable2 = false;
		handler1.activeFlag = false;
		handler2.activeFlag = false;
	}
	slider_value.activable2 = activable2;
	if (activeFlag && activable2)
	{
		if (parameters->movtype != 0)
		{
			if (!slider_speed.activeFlag)
			{
				// slider_speed.activeFlag = false;
				if (handler1.activeFlag)
				{
					handler1.setPos(ofGetMouseX(), handler1.y);
					handler1.x = ofClamp(handler1.x,
										 slider_value.x - slider_value.width / 2,
										 handler2.x);
					parameters->min = ofMap(handler1.x,
											slider_value.x - slider_value.width / 2,
											slider_value.x + slider_value.width / 2,
											0.0, 1.0);
				}
				else if (handler2.activeFlag)
				{
					handler2.setPos(ofGetMouseX(), handler2.y);
					// Max handle can't cross below the min handle.
					handler2.x = ofClamp(handler2.x,
										 handler1.x,
										 slider_value.x + slider_value.width / 2);
					parameters->max = ofMap(handler2.x,
											slider_value.x - slider_value.width / 2,
											slider_value.x + slider_value.width / 2,
											0.0, 1.0);
				}
			}
			else
			{
				slider_speed.activable2 = true;
			}
			if (!handler1.activeFlag && !handler2.activeFlag && !slider_speed.activeFlag)
			{
				boton_collapse.activable2 = true;
				boton_idayvuelta.activable2 = true;
				boton_random.activable2 = true;
				boton_direccion.activable2 = true;
				if (parameters->bpmEligible)
				{
					boton_bpm.activable2 = true;
				}
			}
			else
			{
				boton_collapse.activable2 = false;
				boton_idayvuelta.activable2 = false;
				boton_random.activable2 = false;
				boton_direccion.activable2 = false;
				boton_bpm.activable2 = false;
			}
		}
	}
	// Automation is handled once in JPboxgroup::update_mousePressed(). Keeping
	// draw-time polling disabled prevents a rebuilt button from toggling again
	// while the same physical press is still down.
	boton_collapse.activable2 = false;
}
void JPComplexSlider::setPosAndSize()
{

	// Este es como el setup de todos los elementos :
	float b_cx = x - width / 2 + 20;
	boton_collapse.setup(b_cx,
						 y, 24, 24);

	if (parameters->movtype == 0)
	{
		float slidervaluewidth = width * 3 / 4;
		slider_value.setup(x, y,
						   slidervaluewidth,
						   height * 8 / 10,
						   0.0,
						   1.0,
						   value,
						   name);
	}
	else
	{
		const bool hasBpmMode = parameters->bpmEligible;
		float botonsepx = 35; // ESTA HABRIA QUE HACERLA VARIABLE GLOBAL EN OTRO LUGAR O COMO HACEMO ?!
		float slidervaluewidth = width *
			(hasBpmMode ? 1.7f : 2.0f) / 4.0f;

		float slidervaluex = boton_collapse.x +
							 slidervaluewidth / 2 +
							 boton_collapse.width * 0.75 +
							 botonsepx / 4;

		// slider_value.setPos(slidervaluex, y);
		// Esto lo hace con un rect mode center. Pero hay como que cambiarlo digamos
		const float rangeControlHeight = 20.0f;
		slider_value.setup(slidervaluex,
						   y + height * 0.22f,
						   slidervaluewidth,
						   rangeControlHeight,
						   0.0,
						   1.0,
						   value,
						   name);

		const float sliderspeedw = hasBpmMode ? 38.0f : 40.0f;
		const float sliderspeedh = 40.0f;
		const float modeButtonSize = hasBpmMode ? 24.0f : 26.0f;
		const float modeButtonGap = hasBpmMode ? 4.0f : 6.0f;

		float pos = slidervaluex + slider_value.width / 2 + sliderspeedw / 2;

		slider_speed.setup(pos += botonsepx / 4,
						   y,
						   sliderspeedw,
						   sliderspeedh,
						   0.0,
						   1.0,
						   speed);

		pos += sliderspeedw / 2.0f + modeButtonGap +
			modeButtonSize / 2.0f;
		boton_idayvuelta.setup(
			pos, y, modeButtonSize, modeButtonSize);

		pos += modeButtonSize + modeButtonGap;
		boton_random.setup(
			pos, y, modeButtonSize, modeButtonSize);

		pos += modeButtonSize + modeButtonGap;
		boton_direccion.setup(
			pos, y, modeButtonSize, modeButtonSize);
		if (hasBpmMode)
		{
			pos += modeButtonSize + modeButtonGap;
			boton_bpm.setup(
				pos, y, modeButtonSize, modeButtonSize);
			const float rateWidth = 32.0f;
			pos += modeButtonSize / 2.0f + modeButtonGap +
				rateWidth / 2.0f;
			bpm_rate_button.setup(
				pos,
				y,
				rateWidth,
				modeButtonSize);
		}

		slider_speed.paleta = 1;
		controllertype = SLIDER;

		// speed = slider_speed.value;

		const float handlerw = 14.0f;
		const float handlerh = rangeControlHeight;

		// parameters->min = ofClamp(parameters->min, 0.0, 1.0);
		//	parameters->max = ofClamp(parameters->max, 0.0, 1.0);
		// parameters->max = 1.0;

		float handler1_x = ofMap(parameters->min,
								 0.0, 1.0,
								 slider_value.x - slider_value.width / 2,
								 slider_value.x + slider_value.width / 2);

		float handler2_x = ofMap(parameters->max,
								 0.0, 1.0,
								 slider_value.x - slider_value.width / 2,
								 slider_value.x + slider_value.width / 2);

		handler1.setup(handler1_x,
					   slider_value.y,
					   handlerw,
					   handlerh);

		handler2.setup(handler2_x,
					   slider_value.y,
					   handlerw,
					   handlerh);
	}
	handler2.isLeft = false;
	boton_idayvuelta.setUseTexture(boton_idayvuelta.IDAYVUELTA);
	boton_random.setUseTexture(boton_idayvuelta.RAN);
	boton_direccion.setUseTexture(boton_idayvuelta.GODER);
	if (parameters->bpmEligible)
	{
		boton_bpm.setUseTexture(boton_bpm.BPM_SYNC);
	}
	boton_collapse.setUseTexture(boton_collapse.COLLAPSE);
	boton_collapse.activable = true; // Force activable after setUseTexture (needsUpdate may be true during setControllers)
	boton_collapse.activable2 = false;
}
