#include "jp_slider.h"
#include <algorithm>

JPSlider::JPSlider() {}
JPSlider::~JPSlider() {}

namespace {
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
}

void JPSlider::setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value, string _name)
{
	setup(_x, _y, _width, _height, _min, _max, _value);
	showtext = true;
	name = _name;
}
void JPSlider::setup(float _x, float _y, float _width, float _height, float _min, float _max, float _value)
{
	x = _x;
	y = _y;
	width = _width;
	height = _height;
	min = _min;
	max = _max;
	value = _value;
	showtext = false;
	useSpecialColors = false;
	activeFlag = false;
	paleta = 0;
	useTexture = false;
}
void JPSlider::setSpecialColors(ofColor _Cback,
								ofColor _Cactive,
								ofColor _CmouseOver,
								ofColor _Cfront)
{

	/* Cback = _Cback;
	 Cactive = _Cactive;
	 Cmouseover = _CmouseOver;
	 Cfront = _Cfront;*/
}
float JPSlider::getValue()
{
	return value;
}
void JPSlider::draw()
{
	value = parameters->floatValue;
	/*if (mouseOver() && !activeFlag && ofGetMousePressed() ) {
		activeFlag = true;
	}*/

	if (mouseOver() && ofGetMousePressed() && activable2)
	{
		activeFlag = true;
	}
	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
	if (activeFlag)
	{
		// cout << "MUEVE SLIDER " << endl;
		value = ofMap(ofGetMouseX(), x - width / 2, x + width / 2, min, max);
		value = ofClamp(value, parameters->effectiveMin(), parameters->effectiveMax());
		parameters->floatValue = value;
		parameters->floatLerpValue = value;
	}
	if (parameters->movtype == 0)
	{
		float left = x - width / 2;
		float top = y - height / 2;
		float fillW = ofMap(parameters->floatValue, min, max, 0, width, true);
		float trackR = std::min(4.0f, height * 0.5f);

		ofSetRectMode(OF_RECTMODE_CORNER);
		// Soft dark track
		ofSetColor(COL_BG_INPUT);
		ofDrawRectRounded(left, top, width, height, trackR);

		// Value fill (cyan; brighter on hover/active)
		ofSetColor(activeFlag ? COL_ACCENT_CYAN : (mouseOver() ? COL_ACCENT_CYAN : COL_ACCENT_CYAN_DIM));
		if (fillW > 1.0f)
		{
			float fr = std::min(trackR, fillW * 0.5f);
			ofDrawRectRounded(left, top, fillW, height, fr);
		}

		if (showtext)
		{
			ofTrueTypeFont &labelFont = font_p != nullptr ?
				*font_p : jp_constants::p_font;
			const bool integerMillimeters =
				name == "near mm" || name == "far mm";
			string valueStr = ofToString(
				parameters->floatValue, integerMillimeters ? 0 : 2);
			if (name == "blendmode")
			{
				valueStr = blendModeNameFromValue(parameters->floatValue);
			}
			float textY = y + labelFont.stringHeight(name) / 2;
			// Name left-aligned, value right-aligned for a clean, readable row.
			ofSetColor(COL_TEXT_PRIMARY);
			labelFont.drawString(name, left + 8, textY);
			ofSetColor(COL_TEXT_PRIMARY, 220);
			float vw = labelFont.stringWidth(valueStr);
			labelFont.drawString(valueStr, left + width - vw - 8, textY);
		}
		ofSetColor(jp_constants::textcolor);
	}
	else
	{
		const float left = x - width / 2.0f;
		const float right = x + width / 2.0f;
		const float trackHeight = 6.0f;
		const float trackTop = y - trackHeight / 2.0f;
		const float rangeLeft = ofMap(
			ofClamp(parameters->min, min, max),
			min, max, left, right, true);
		const float rangeRight = ofMap(
			ofClamp(parameters->max, min, max),
			min, max, left, right, true);
		const float currentX = ofMap(
			parameters->floatValue, min, max, left, right, true);

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(mouseOver() ? ofColor(COL_BG_HOVER, 245) :
			ofColor(COL_BG_INPUT, 235));
		ofDrawRectRounded(left, trackTop, width, trackHeight, 3.0f);

		if (parameters->rangeEnabled && rangeRight >= rangeLeft)
		{
			ofSetColor(ofColor(COL_ACCENT_CYAN, 105));
			ofDrawRectRounded(rangeLeft, trackTop,
				rangeRight - rangeLeft, trackHeight, 3.0f);
		}
		ofSetColor(activeFlag ? COL_TEXT_PRIMARY : COL_ACCENT_CYAN);
		ofDrawCircle(currentX, y, activeFlag ? 3.5f : 3.0f);
		ofSetColor(jp_constants::textcolor);
	}
	// ESTO DE ACA EN REALIDAD IRIA COMO EN UN UPDATE NO EN UN DRAW. PERO BUENO ; POR AHORA QUEDA ACA TOTAL SON 2 IFS NOMA
	if (!ofGetMousePressed())
	{
		activeFlag = false;
	}
}
