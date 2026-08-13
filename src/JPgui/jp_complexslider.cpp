#include "jp_complexslider.h"
#include "../JPutils/jp_audio.h"
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
	if (jp_constants::inspector_secondary_font.stringWidth(text) <= maxWidth)
	{
		return text;
	}
	while (text.size() > 1 &&
		jp_constants::inspector_secondary_font.stringWidth(text + "..") > maxWidth)
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

// One chip renderer for every cycling label on a slider row: the BPM rate, the
// audio source and the rhythm division. This was drawBpmRateControl, which knew
// how to format a bpmRate - so a second chip meant a second copy of the drawing.
// The little marker at the head of the second line, so the chips there are
// obviously "this mode's settings" and not more of the main row.
void drawModifierRowIcon(float x, float y, bool audio)
{
	ofPushStyle();
	ofSetColor(ofColor(COL_ACCENT_CYAN, 190));
	ofSetLineWidth(1.7f);
	if (audio)
	{
		// Level-meter bars, matching the audio mode button.
		ofDrawLine(x - 5.0f, y + 4.0f, x - 5.0f, y - 1.0f);
		ofDrawLine(x, y + 4.0f, x, y - 5.0f);
		ofDrawLine(x + 5.0f, y + 4.0f, x + 5.0f, y + 1.0f);
	}
	else
	{
		// The BPM pulse glyph, matching the BPM sync button.
		ofBeginShape();
		ofVertex(x - 6.0f, y);
		ofVertex(x - 3.0f, y);
		ofVertex(x - 1.0f, y - 5.0f);
		ofVertex(x + 1.0f, y + 5.0f);
		ofVertex(x + 3.0f, y);
		ofVertex(x + 6.0f, y);
		ofEndShape(false);
	}
	ofPopStyle();
}

void drawLabelChip(JPdragobject &control, const std::string &label,
	bool muted = false, bool compact = false)
{
	ofTrueTypeFont &font = jp_constants::p2_font;
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
	const ofColor accent = muted ? COL_TEXT_MUTED : ofColor(COL_ACCENT_CYAN, 210);
	ofSetColor(hovered ? COL_TEXT_PRIMARY : accent);
	ofDrawRectRounded(
		control.x - control.width / 2.0f,
		control.y - control.height / 2.0f,
		control.width,
		control.height,
		3.0f);
	ofFill();
	ofSetColor(hovered ? COL_TEXT_PRIMARY :
		(muted ? COL_TEXT_MUTED : COL_ACCENT_CYAN));
	const ofRectangle glyphBounds =
		font.getStringBoundingBox("Ag", 0.0f, 0.0f);
	font.drawString(
		label,
		control.x - font.stringWidth(label) / 2.0f,
		control.y - (glyphBounds.y + glyphBounds.height * 0.5f));
	ofPopStyle();
}

// One container behind the whole audio block - chip row plus the shaping grid -
// so a parameter that follows audio reads as a single unit instead of loose
// controls floating between the parameter rows above and below it.
void drawAudioBlockPanel(const ofRectangle &r)
{
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofFill();
	ofSetColor(ofColor(COL_BG_PANEL, 150));
	ofDrawRectRounded(r.x, r.y, r.width, r.height, 5.0f);
	ofNoFill();
	ofSetLineWidth(1.0f);
	ofSetColor(ofColor(COL_ACCENT_CYAN, 70));
	ofDrawRectRounded(r.x, r.y, r.width, r.height, 5.0f);
	ofFill();
	ofPopStyle();
}

void drawAudioShapingSlider(JPdragobject &control,
	const std::string &label, const std::string &value,
	float normalized, bool muted)
{
	const bool hovered = control.mouseOver();
	const float left = control.x - control.width / 2.0f;
	const float top = control.y - control.height / 2.0f;
	const float inset = 7.0f;
	const float trackY = top + control.height - 5.0f;
	const float trackWidth = std::max(1.0f, control.width - inset * 2.0f);
	const float fillWidth = trackWidth * ofClamp(normalized, 0.0f, 1.0f);

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(hovered ? ofColor(COL_BG_HOVER, 235) :
		ofColor(COL_BG_INPUT, 210));
	ofDrawRectRounded(left, top, control.width, control.height, 3.0f);

	ofSetColor(muted ? COL_TEXT_MUTED : COL_TEXT_PRIMARY);
	ofTrueTypeFont &font = jp_constants::p2_font;
	const ofRectangle glyphBounds = font.getStringBoundingBox("Ag", 0.0f, 0.0f);
	const float textY = top + 7.5f -
		(glyphBounds.y + glyphBounds.height * 0.5f);
	font.drawString(label, left + inset, textY);
	const float valueWidth = font.stringWidth(value);
	font.drawString(value,
		left + control.width - inset - valueWidth, textY);

	ofSetColor(ofColor(COL_BORDER_MUTED, 175));
	ofDrawRectangle(left + inset, trackY - 1.0f, trackWidth, 2.0f);
	const ofColor accent = muted ? COL_TEXT_MUTED : COL_ACCENT_CYAN;
	ofSetColor(accent);
	if (fillWidth > 0.5f)
		ofDrawRectangle(left + inset, trackY - 1.0f, fillWidth, 2.0f);
	ofDrawCircle(left + inset + fillWidth, trackY, hovered ? 3.0f : 2.5f);
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
		(hovered ? COL_ACCENT_GOLD : COL_ACCENT_GOLD_DIM));
	ofSetLineWidth(grabbed || hovered ? 2.0f : 1.5f);
	ofDrawLine(x, y - halfMark, x, y + halfMark);
	ofDrawLine(x, y - halfMark, x + inward * 4.0f, y - halfMark);
	ofDrawLine(x, y + halfMark, x + inward * 4.0f, y + halfMark);
	ofPopStyle();
}

JPComplexSlider::JPComplexSlider() {}
JPComplexSlider::~JPComplexSlider() {}

const JPComplexSlider::LayoutMetrics &JPComplexSlider::layoutMetrics()
{
	static const LayoutMetrics metrics;
	return metrics;
}

float JPComplexSlider::requiredHeight(const JPParameter *parameter,
	float standardHeight)
{
	if (parameter == nullptr || parameter->movtype == JPParameter::STANDART)
		return standardHeight;
	const LayoutMetrics &metrics = layoutMetrics();
	if (parameter->movtype == JPParameter::AUDIO && parameter->audioShapingOpen)
		return metrics.expandedAudioHeight;
	if (parameter->movtype == JPParameter::BPM ||
		parameter->movtype == JPParameter::AUDIO)
		return metrics.modifierHeight;
	return metrics.automatedHeight;
}

int JPComplexSlider::audioShapingControlAt(float mouseX, float mouseY) const
{
	auto inside = [&](const JPdragobject &control)
	{
		return mouseX >= control.x - control.width / 2.0f &&
			mouseX <= control.x + control.width / 2.0f &&
			mouseY >= control.y - control.height / 2.0f &&
			mouseY <= control.y + control.height / 2.0f;
	};
	if (inside(audio_amount_button)) return AUDIO_SHAPING_AMOUNT;
	if (inside(audio_threshold_button)) return AUDIO_SHAPING_THRESHOLD;
	if (inside(audio_curve_button)) return AUDIO_SHAPING_CURVE;
	if (inside(audio_attack_button)) return AUDIO_SHAPING_ATTACK;
	if (inside(audio_release_button)) return AUDIO_SHAPING_RELEASE;
	return AUDIO_SHAPING_NONE;
}

float JPComplexSlider::audioShapingControlNormalized(int control) const
{
	if (parameters == nullptr) return 0.0f;
	switch (control)
	{
	case AUDIO_SHAPING_AMOUNT:
		return ofClamp(parameters->audioAmount, 0.0f, 1.0f);
	case AUDIO_SHAPING_THRESHOLD:
		return ofMap(parameters->audioThreshold, 0.0f, 0.95f,
			0.0f, 1.0f, true);
	case AUDIO_SHAPING_CURVE:
		return ofClamp(std::log(std::max(0.25f, parameters->audioCurve) / 0.25f) /
			std::log(16.0f), 0.0f, 1.0f);
	case AUDIO_SHAPING_ATTACK:
		return std::sqrt(ofClamp(parameters->audioAttackMs / 500.0f,
			0.0f, 1.0f));
	case AUDIO_SHAPING_RELEASE:
		return ofClamp(std::log(std::max(20.0f, parameters->audioReleaseMs) / 20.0f) /
			std::log(100.0f), 0.0f, 1.0f);
	default:
		return 0.0f;
	}
}

bool JPComplexSlider::setAudioShapingControlFromMouse(int control,
	float mouseX)
{
	if (parameters == nullptr) return false;
	const JPdragobject *target = nullptr;
	switch (control)
	{
	case AUDIO_SHAPING_AMOUNT: target = &audio_amount_button; break;
	case AUDIO_SHAPING_THRESHOLD: target = &audio_threshold_button; break;
	case AUDIO_SHAPING_CURVE: target = &audio_curve_button; break;
	case AUDIO_SHAPING_ATTACK: target = &audio_attack_button; break;
	case AUDIO_SHAPING_RELEASE: target = &audio_release_button; break;
	default: return false;
	}
	const float normalized = ofMap(mouseX,
		target->x - target->width / 2.0f + 7.0f,
		target->x + target->width / 2.0f - 7.0f,
		0.0f, 1.0f, true);
	float *value = nullptr;
	float next = 0.0f;
	switch (control)
	{
	case AUDIO_SHAPING_AMOUNT:
		value = &parameters->audioAmount;
		next = normalized;
		break;
	case AUDIO_SHAPING_THRESHOLD:
		value = &parameters->audioThreshold;
		next = normalized * 0.95f;
		break;
	case AUDIO_SHAPING_CURVE:
		value = &parameters->audioCurve;
		next = 0.25f * std::pow(16.0f, normalized);
		break;
	case AUDIO_SHAPING_ATTACK:
		value = &parameters->audioAttackMs;
		next = normalized * normalized * 500.0f;
		break;
	case AUDIO_SHAPING_RELEASE:
		value = &parameters->audioReleaseMs;
		next = 20.0f * std::pow(100.0f, normalized);
		break;
	default:
		return false;
	}
	if (std::abs(*value - next) < 0.0001f) return false;
	*value = next;
	return true;
}

int JPComplexSlider::rangeHandleAt(float mouseX, float mouseY) const
{
	if (parameters == nullptr || !parameters->rangeEnabled) return 0;
	auto inside = [&](const JPHandler &handle)
	{
		return mouseX >= handle.x - handle.width * 0.5f &&
			mouseX <= handle.x + handle.width * 0.5f &&
			mouseY >= handle.y - handle.height * 0.5f &&
			mouseY <= handle.y + handle.height * 0.5f;
	};
	const bool lower = inside(handler1);
	const bool upper = inside(handler2);
	if (lower && upper)
		return mouseX < handler1.x ? 1 : 2;
	if (lower) return 1;
	if (upper) return 2;
	return 0;
}

bool JPComplexSlider::setRangeHandleFromMouse(int handle, float mouseX)
{
	if (parameters == nullptr || !parameters->rangeEnabled ||
		(handle != 1 && handle != 2)) return false;
	const float next = ofMap(mouseX,
		slider_value.x - slider_value.width * 0.5f,
		slider_value.x + slider_value.width * 0.5f,
		parameters->nativeMin, parameters->nativeMax, true);
	const float previous = handle == 1 ? parameters->min : parameters->max;
	if (handle == 1) parameters->setRangeStart(next);
	else parameters->setRangeEnd(next);
	setPosAndSize();
	return std::abs(previous - next) > 0.000001f;
}

void JPComplexSlider::setup(float _x, float _y, float _width, float _height, JPParameter *_parameters)
{
	parameters = _parameters;
	x = _x;
	y = _y;
	width = _width;
	height = _height;
	setFontPointer(jp_constants::inspector_body_font);
	slider_value.setFontPointer(jp_constants::inspector_body_font);

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
	slider_value.setParametersPointer(parameters);
	boton_idayvuelta.setParametersPointer(parameters);
	boton_bpm.setParametersPointer(parameters);
	// Without this JPToogle::draw() dereferences a null `parameters` the first
	// time an inspector shows a float slider - i.e. the instant you click a box.
	boton_audio.setParametersPointer(parameters);
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
	if (parameters->movtype == JPParameter::AUDIO)
	{
		// Audio is one coherent card. The old full-row texture plus a second
		// nested panel created the oversized grey shell visible around the
		// source and shaping controls.
		drawAudioBlockPanel(ofRectangle(
			x - width * 0.5f + panelInsetX,
			y - height * 0.5f + panelInsetY,
			std::max(1.0f, width - panelInsetX * 2.0f),
			std::max(1.0f, height - panelInsetY * 2.0f)));
	}
	else
	{
		jp_constants_img::fondo_parametro.draw(
			x, y,
			std::max(1.0f, width - panelInsetX * 2.0f),
			std::max(1.0f, height - panelInsetY * 2.0f));
	}

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
			jp_constants::inspector_secondary_font.stringWidth(Strvalue);
		const float labelBaseline = primaryRowY - 5.0f;
		const string displayName = fitAutomationLabel(
			name, std::max(12.0f,
				labelRight - valueWidth - 12.0f - labelLeft));

		ofSetColor(COL_ACCENT_CYAN);
		jp_constants::inspector_secondary_font.drawString(
			displayName, labelLeft, labelBaseline);
		jp_constants::inspector_secondary_font.drawString(
			Strvalue, labelRight - valueWidth, labelBaseline);
	}
	slider_value.draw();
	boton_collapse.draw();
	jp_tooltip::draw("Toggle parameter automation",
		boton_collapse.x - boton_collapse.width / 2.0f,
		boton_collapse.y - boton_collapse.height / 2.0f,
		boton_collapse.width, boton_collapse.height);

	// movtype = parameters->movtype;
	if (parameters->rangeEnabled)
	{
		ofSetRectMode(OF_RECTMODE_CENTER);
		handler1.draw();
		handler2.draw();
		jp_tooltip::draw("Drag lower custom-range limit; Down Arrow captures current value",
			handler1.x - handler1.width * 0.5f,
			handler1.y - handler1.height * 0.5f,
			handler1.width, handler1.height);
		jp_tooltip::draw("Drag upper custom-range limit; Up Arrow captures current value",
			handler2.x - handler2.width * 0.5f,
			handler2.y - handler2.height * 0.5f,
			handler2.width, handler2.height);
	}

	if (parameters->movtype != 0)
	{
		ofSetColor(COL_ACCENT_CYAN, 255);

		ofSetRectMode(OF_RECTMODE_CORNER);
		// slider_value.value = value;
		slider_speed.draw();
		speed = slider_speed.value;
		boton_idayvuelta.draw();
		if (parameters->bpmEligible)
		{
			boton_bpm.draw();
		}
		if (parameters->audioEligible)
		{
			boton_audio.draw();
		}

		// --- second line: this mode's own controls ---
		if (parameters->movtype == JPParameter::BPM)
		{
			drawModifierRowIcon(bpm_rate_button.x - 32.0f / 2.0f - 12.0f,
				bpm_rate_button.y, false);
			drawLabelChip(bpm_rate_button, bpmRateLabel(parameters->bpmRate));
		}
		else if (parameters->movtype == JPParameter::AUDIO)
		{
			// Muted when nothing is listening, so a dead input shows on the
			// slider itself and not only in SETTINGS.
			const bool muted = !jp_audio::isRunning();

			drawModifierRowIcon(
				audio_source_button.x - audio_source_button.width / 2.0f - 12.0f,
				audio_source_button.y, true);
			drawLabelChip(audio_source_button,
				jp_audio::sourceLabel(parameters->audioSource), muted);
			if (jp_audio::isRhythmSource(parameters->audioSource))
				drawLabelChip(audio_div_button, string("Every ") +
					jp_audio::divLabel(parameters->audioDiv), muted);
			drawLabelChip(audio_shape_button,
				parameters->audioShapingOpen ? "Close" : "Shaping", muted);
			if (parameters->audioShapingOpen)
			{
				drawAudioShapingSlider(audio_amount_button, "Amount",
					ofToString(parameters->audioAmount, 2),
					audioShapingControlNormalized(AUDIO_SHAPING_AMOUNT), muted);
				drawAudioShapingSlider(audio_threshold_button, "Threshold",
					ofToString(parameters->audioThreshold, 2),
					audioShapingControlNormalized(AUDIO_SHAPING_THRESHOLD), muted);
				drawAudioShapingSlider(audio_curve_button, "Curve",
					ofToString(parameters->audioCurve, 2),
					audioShapingControlNormalized(AUDIO_SHAPING_CURVE), muted);
				// Same cell shape as the five sliders around it: label left,
				// value right, bar underneath. It was a centred chip, which
				// read as a different KIND of control and broke the grid.
				drawAudioShapingSlider(audio_invert_button, "Invert",
					parameters->audioInvert ? "Inverted" : "Normal",
					parameters->audioInvert ? 1.0f : 0.0f, muted);
				drawAudioShapingSlider(audio_attack_button, "Attack",
					ofToString((int)parameters->audioAttackMs) + " ms",
					audioShapingControlNormalized(AUDIO_SHAPING_ATTACK), muted);
				drawAudioShapingSlider(audio_release_button, "Release",
					ofToString((int)parameters->audioReleaseMs) + " ms",
					audioShapingControlNormalized(AUDIO_SHAPING_RELEASE), muted);
			}
		}

		jp_tooltip::draw(
			parameters->movtype == JPParameter::BPM ?
				"BPM pulse decay" : "Automation speed",
			slider_speed.x - slider_speed.width / 2.0f,
			slider_speed.y - slider_speed.height / 2.0f,
			slider_speed.width, slider_speed.height);
		string automationPattern = "Ping-pong";
		if (parameters->movtype == JPParameter::RANDOM)
			automationPattern = "Random";
		else if (parameters->movtype == JPParameter::GODER)
			automationPattern = "Forward";
		else if (parameters->movtype == JPParameter::GOIZQ)
			automationPattern = "Reverse";
		jp_tooltip::draw("Automation pattern: " + automationPattern +
			" (click to change)",
			boton_idayvuelta.x - boton_idayvuelta.width / 2.0f,
			boton_idayvuelta.y - boton_idayvuelta.height / 2.0f,
			boton_idayvuelta.width, boton_idayvuelta.height);
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
		if (parameters->audioEligible)
		{
			jp_tooltip::draw(jp_audio::isRunning() ?
					"React to audio input" :
					"React to audio input (no input running)",
				boton_audio.x - boton_audio.width / 2.0f,
				boton_audio.y - boton_audio.height / 2.0f,
				boton_audio.width, boton_audio.height);
			if (parameters->movtype == JPParameter::AUDIO)
			{
				auto drawAudioTooltip = [](const JPdragobject &button,
					const string &message)
				{
					jp_tooltip::draw(message,
						button.x - button.width / 2.0f,
						button.y - button.height / 2.0f,
						button.width, button.height);
				};
				if (parameters->audioShapingOpen)
				{
					drawAudioTooltip(audio_source_button,
						string("Audio source: ") +
							jp_audio::sourceLabel(parameters->audioSource));
					if (jp_audio::isRhythmSource(parameters->audioSource))
					{
						drawAudioTooltip(audio_div_button,
							string("Fire every ") +
								jp_audio::divLabel(parameters->audioDiv) +
								" beats");
					}
					drawAudioTooltip(audio_amount_button,
						"Amount: drag to set modulation depth (0 to 1)");
					drawAudioTooltip(audio_invert_button,
						"Toggle audio response polarity");
					drawAudioTooltip(audio_threshold_button,
						"Threshold: drag to ignore quiet audio (0 to 0.95)");
					drawAudioTooltip(audio_curve_button,
						"Curve: drag to shape response (0.25 to 4)");
					drawAudioTooltip(audio_attack_button,
						"Attack: drag to set rise time (0 to 500 ms)");
					drawAudioTooltip(audio_release_button,
						"Release: drag to set fall time (20 to 2000 ms)");
					drawAudioTooltip(audio_shape_button,
						"Return to audio source controls");
				}
				else
				{
					drawAudioTooltip(audio_source_button,
						string("Audio source: ") +
							jp_audio::sourceLabel(parameters->audioSource));
					if (jp_audio::isRhythmSource(parameters->audioSource))
					{
						drawAudioTooltip(audio_div_button,
							string("Fire every ") +
								jp_audio::divLabel(parameters->audioDiv) +
								" beats");
					}
					drawAudioTooltip(audio_shape_button,
						"Open audio response shaping");
				}
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
		if (parameters->bpmEligible)
		{
			drawClickBounds(boton_bpm, boton_bpm.activable2);
			if (parameters->movtype == JPParameter::BPM)
			{
				drawClickBounds(bpm_rate_button, boton_bpm.activable2);
			}
		}
		if (parameters->movtype == JPParameter::AUDIO)
		{
			drawClickBounds(audio_shape_button);
			drawClickBounds(audio_source_button);
			if (jp_audio::isRhythmSource(parameters->audioSource))
				drawClickBounds(audio_div_button);
			if (parameters->audioShapingOpen)
			{
				drawClickBounds(audio_amount_button);
				drawClickBounds(audio_invert_button);
				drawClickBounds(audio_threshold_button);
				drawClickBounds(audio_curve_button);
				drawClickBounds(audio_attack_button);
				drawClickBounds(audio_release_button);
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
		boton_bpm.activable2 = false;
		handler1.activeFlag = false;
		handler2.activeFlag = false;
	}
	slider_value.activable2 = activable2 && !rangeHandleDragging;
	// Automation is handled once in JPboxgroup::update_mousePressed(). Keeping
	// draw-time polling disabled prevents a rebuilt button from toggling again
	// while the same physical press is still down.
	boton_collapse.activable2 = false;
}
void JPComplexSlider::setPosAndSize()
{
	const LayoutMetrics &layout = layoutMetrics();
	builtForMovtype = parameters != nullptr ? parameters->movtype : -1;
	builtForAudioSource = parameters != nullptr ? parameters->audioSource : -1;
	// Automated controllers are top-aligned vertical bands. Their primary line
	// always occupies the same first 50 px, while source and shaping bands grow
	// downward. This keeps expansion from moving the parameter users clicked.
	primaryRowY = parameters != nullptr && parameters->movtype != 0 ?
		y - height * 0.5f + layout.primaryRowOffset : y;

	// Keep expanded rows visually inside their card. The automation toggle and
	// the last mode button previously sat almost against the two vertical edges.
	const bool automated = parameters != nullptr && parameters->movtype != 0;
	const float automatedInlinePadding = 10.0f;
	float b_cx = x - width / 2 + (automated ?
		automatedInlinePadding + 10.0f : 18.0f);
	boton_collapse.setup(b_cx,
						 primaryRowY, 20, 20);

	if (parameters->movtype == 0)
	{
		// Fill the complete content lane between the automation toggle and the
		// row-action columns. A centered fractional width left two inert grey
		// bands that looked interactive but did nothing.
		const float sliderLeft =
			boton_collapse.x + boton_collapse.width * 0.5f + 4.0f;
		const float sliderRight = x + width * 0.5f - 4.0f;
		const float slidervaluewidth = std::max(24.0f,
			sliderRight - sliderLeft);
		slider_value.setup(sliderLeft + slidervaluewidth * 0.5f,
						   primaryRowY,
						   slidervaluewidth,
						   height * 8 / 10,
				parameters->nativeMin,
				parameters->nativeMax,
						   value,
						   name);
	}
	else
	{
		const bool hasBpmMode = parameters->bpmEligible;
		const bool hasAudioMode = parameters->audioEligible;
		float botonsepx = 28;

		const float sliderspeedw = 34.0f;
		const float sliderspeedh = 34.0f;
		const float modeButtonSize = 22.0f;
		const float modeButtonGap = 4.0f;
		// BPM and AUDIO put their chips on a SECOND line, so the main line no
		// longer reserves room for them and the value slider keeps its width.
		// JPboxgroup makes those rows taller, and
		// rebuildControllersIfLayoutStale() re-runs this whenever movtype
		// changes - including from the buttons that change it inside draw().
		const bool secondRow =
			parameters->movtype == JPParameter::BPM ||
			parameters->movtype == JPParameter::AUDIO;
		const float rightPad = automatedInlinePadding;

		// The value slider takes whatever is LEFT OVER, instead of a hardcoded
		// fraction of the row. The fractions could not survive adding a control:
		// the audio button pushed the BPM rate chip 28px past the panel edge,
		// and the rhythm division chip fell off entirely. Sizing by subtraction
		// means every combination of buttons fits by construction.
		const int modeButtons =
			1 + (hasBpmMode ? 1 : 0) + (hasAudioMode ? 1 : 0);
		const float sliderLeft = boton_collapse.x +
			boton_collapse.width * 0.5f + 4.0f;
		const float tail =
			sliderspeedw + botonsepx / 4.0f +
			modeButtonGap + modeButtonSize +
			(float)(modeButtons - 1) * (modeButtonSize + modeButtonGap);
		float slidervaluewidth = std::max(60.0f,
			(x + width / 2.0f - rightPad) - sliderLeft - tail);

		float slidervaluex = sliderLeft + slidervaluewidth / 2;

		// The row is 20px taller in these modes and the extra space is added
		// BELOW, so the second line drops into it and the main line does not
		// move. Deliberately not shifting `y`: it is also this controller's
		// own hit-test anchor and boton_collapse is already placed from it.
		const float secondRowOffset = secondRow ? layout.secondRowOffset : 0.0f;

		// slider_value.setPos(slidervaluex, y);
		// Esto lo hace con un rect mode center. Pero hay como que cambiarlo digamos
		const float rangeControlHeight = 16.0f;
		slider_value.setup(slidervaluex,
						   primaryRowY + 10.0f,
						   slidervaluewidth,
						   rangeControlHeight,
						   parameters->nativeMin,
						   parameters->nativeMax,
						   value,
						   name);

		float pos = slidervaluex + slider_value.width / 2 + sliderspeedw / 2;

		slider_speed.setup(pos += botonsepx / 4,
						   primaryRowY,
						   sliderspeedw,
						   sliderspeedh,
						   0.0,
						   1.0,
						   speed);

		pos += sliderspeedw / 2.0f + modeButtonGap +
			modeButtonSize / 2.0f;
		boton_idayvuelta.setup(
			pos, primaryRowY, modeButtonSize, modeButtonSize);

		if (hasBpmMode)
		{
			pos += modeButtonSize + modeButtonGap;
			boton_bpm.setup(
				pos, primaryRowY, modeButtonSize, modeButtonSize);
		}
		if (hasAudioMode)
		{
			pos += modeButtonSize + modeButtonGap;
			boton_audio.setup(
				pos, primaryRowY, modeButtonSize, modeButtonSize);
		}
		// One chip slot, shared: a parameter is either in BPM mode or in AUDIO
		// mode, never both, so the two never need to be on screen together.
		// Second line: the mode's own controls, left-aligned under the slider
		// and preceded by a small icon so it is obvious which mode they belong
		// to. Positions are reserved for both modes so nothing shifts when the
		// mode changes.
		{
			// Right-aligned to the same margin the mode buttons end on, so the
			// second line reads as belonging to them rather than to the slider
			// track it would otherwise sit under.
			const float rowY = primaryRowY + secondRowOffset;
			const float rightEdge = x + width / 2.0f - rightPad;
			const float chipGap = 6.0f;
			const float srcW = ofClamp(
				jp_constants::inspector_secondary_font.stringWidth(
					jp_audio::sourceLabel(parameters->audioSource)) + 18.0f,
				56.0f, 112.0f);
			const float divW = 60.0f, shapeW = 60.0f;
			const bool divVisible =
				parameters->movtype == JPParameter::AUDIO &&
				jp_audio::isRhythmSource(parameters->audioSource);

			bpm_rate_button.setup(rightEdge - 32.0f / 2.0f, rowY,
				32.0f, modeButtonSize);

			// One left margin for the whole audio block. The chip row used to
			// start 8px inside the shaping grid below it, so the icon and the
			// "Amount" cell did not share an edge.
			const float audioBlockLeft = x - width / 2.0f + 14.0f;
			const float iconLead = 18.0f;   // icon half-width plus its gap
			const float audioLeft = audioBlockLeft + iconLead;
			float cursor = audioLeft;
			audio_source_button.setup(cursor + srcW / 2.0f, rowY,
				srcW, modeButtonSize);
			cursor += srcW + chipGap;
			if (divVisible)
			{
				audio_div_button.setup(cursor + divW / 2.0f, rowY,
					divW, modeButtonSize);
				cursor += divW + chipGap;
			}
			else
				audio_div_button.setup(rightEdge + divW, rowY,
					divW, modeButtonSize);
			audio_shape_button.setup(cursor + shapeW / 2.0f, rowY,
				shapeW, modeButtonSize);

			if (parameters->movtype == JPParameter::AUDIO && parameters->audioShapingOpen)
			{
				// Keep source/division on their normal row, then use two
				// left-aligned shaping rows. This stays readable at minimum width.
				const float leftEdge = audioBlockLeft;
				const float advancedH = layout.shapingControlHeight;
				const float firstY = primaryRowY + layout.shapingFirstOffset;
				const float secondY = firstY + layout.shapingRowStep;
				const float thirdY = secondY + layout.shapingRowStep;
				const float availableWidth = std::max(220.0f, rightEdge - leftEdge);
				const float advancedGap = layout.shapingColumnGap;
				const float columnWidth =
					(availableWidth - advancedGap) / 2.0f;
				audio_amount_button.setup(leftEdge + columnWidth / 2.0f,
					firstY, columnWidth, advancedH);
				audio_threshold_button.setup(
					leftEdge + columnWidth + advancedGap + columnWidth / 2.0f,
					firstY, columnWidth, advancedH);
				audio_curve_button.setup(leftEdge + columnWidth / 2.0f,
					secondY, columnWidth, advancedH);
				audio_invert_button.setup(
					leftEdge + columnWidth + advancedGap + columnWidth / 2.0f,
					secondY, columnWidth, advancedH);
				audio_attack_button.setup(leftEdge + columnWidth / 2.0f,
					thirdY, columnWidth, advancedH);
				audio_release_button.setup(
					leftEdge + columnWidth + advancedGap + columnWidth / 2.0f,
					thirdY, columnWidth, advancedH);
			}
		}

		slider_speed.paleta = 1;
		controllertype = SLIDER;

		// speed = slider_speed.value;

	}
	const float handlerw = 14.0f;
	const float handlerh = std::max(16.0f, slider_value.height);
	const float handler1_x = ofMap(parameters->min,
		parameters->nativeMin, parameters->nativeMax,
		slider_value.x - slider_value.width / 2,
		slider_value.x + slider_value.width / 2, true);
	const float handler2_x = ofMap(parameters->max,
		parameters->nativeMin, parameters->nativeMax,
		slider_value.x - slider_value.width / 2,
		slider_value.x + slider_value.width / 2, true);
	handler1.setup(handler1_x, slider_value.y, handlerw, handlerh);
	handler2.setup(handler2_x, slider_value.y, handlerw, handlerh);
	handler2.isLeft = false;
	boton_idayvuelta.setUseTexture(boton_idayvuelta.IDAYVUELTA);
	if (parameters->bpmEligible)
	{
		boton_bpm.setUseTexture(boton_bpm.BPM_SYNC);
	}
	if (parameters->audioEligible)
	{
		boton_audio.setUseTexture(boton_audio.AUDIO_SRC);
	}
	boton_collapse.setUseTexture(boton_collapse.COLLAPSE);
	boton_collapse.activable = true; // Force activable after setUseTexture (needsUpdate may be true during setControllers)
	boton_collapse.activable2 = false;
}
