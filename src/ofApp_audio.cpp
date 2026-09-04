#include "ofApp.h"

#include "JPgui/jp_screen.h"
#include "JPgui/jp_button.h"
#include "JPgui/jp_minislider.h"
#include "JPgui/jp_gl_state.h"
#include "JPutils/jp_tooltip.h"

// The AUDIO screen: tune the analyser, and see enough of its internals to work
// out WHY it is doing what it is doing.
//
// The tuning half is the six continuous sources with Threshold/Gain/Add/Smooth
// and the two detectors with sensitivity and refractory. The diagnostic half is
// the part that makes this a debug screen rather than an equaliser: a VJ
// arrives here with one of two questions, and neither is answerable from a
// level meter.
//
//   "everything is pinned at 1.0"  -> the normaliser has collapsed, which you
//        can only see by watching its floor and peak converge.
//   "my kick is not triggering"    -> flux never crossed mean * sensitivity,
//        which you can only see by plotting the two together.
//
// So every row shows the value at each stage of the chain, and the detector
// rows are flux-against-its-own-threshold plots with the refractory window
// shaded, not a knob and a blinking light.

namespace
{
	constexpr float kRowHeight = 46.0f;
	constexpr float kCellHeight = 20.0f;
	constexpr float kCellGap = 5.0f;
	constexpr float kNameWidth = 92.0f;
	constexpr float kSwatchWidth = 9.0f;

	// One colour per traced source, used by BOTH the swatch beside the name and
	// the line on the history plot - a legend that can disagree with its own
	// chart is worse than no legend.
	//
	// Not the semantic COL_ACCENT_* set: this panel already spends red on
	// "collapsed" and on the detector thresholds, so a red trace would read as
	// an alarm. These six are picked to stay apart from each other on the dark
	// ground instead of to mean anything.
	const ofColor kTraceColors[jp_audio_internal::TunedSources] = {
		ofColor(0, 190, 205),      // Low        cyan
		ofColor(70, 200, 130),     // Mid        green
		ofColor(226, 174, 64),     // High       gold
		ofColor(158, 130, 240),    // Low bass   violet
		ofColor(240, 138, 100),    // High mid   coral
		ofColor(175, 185, 195)     // Level      grey
	};
	constexpr float kResetWidth = 20.0f;
	constexpr float kGroupGap = 12.0f;

	// The four columns of a tuned row, in signal order: the value is gated and
	// rescaled, then multiplied, then offset, then smoothed.
	enum Cell { CELL_THRESH = 0, CELL_GAIN, CELL_ADD, CELL_SMOOTH };
	const char *kCellLabels[4] = {"Thresh", "Gain", "Add", "Smooth"};
	const char *kOnsetLabels[2] = {"Sens", "Hold"};
	const char *kOnsetNames[2] = {"Kick", "Snare"};

	// Knob range <-> 0..1 for the track. Kept in one place so the draw pass and
	// the drag cannot disagree about what half way means.
	float cellNormalized(const jp_audio_internal::SourceTuning &t, int cell)
	{
		switch (cell)
		{
		case CELL_THRESH: return t.threshold / 0.95f;
		// Squared, so the useful ground around 1x keeps its resolution while the
		// track still reaches 16x: n=0.25 is unity, n=0.5 is 4x, n=1 is 16x.
		case CELL_GAIN: return std::sqrt(ofClamp(t.gain, 0.0f, 16.0f) / 16.0f);
		case CELL_ADD: return (t.add + 1.0f) * 0.5f;
		default: return t.smoothMs / 1000.0f;
		}
	}

	void cellFromNormalized(jp_audio_internal::SourceTuning &t, int cell, float n)
	{
		n = ofClamp(n, 0.0f, 1.0f);
		switch (cell)
		{
		case CELL_THRESH: t.threshold = n * 0.95f; break;
		case CELL_GAIN: t.gain = 16.0f * n * n; break;
		// Bipolar, with a snap to the centre: 0 is the identity and has to be
		// reachable with the mouse, not only with RESET.
		case CELL_ADD: t.add = std::fabs(n - 0.5f) < 0.02f ? 0.0f :
			n * 2.0f - 1.0f; break;
		default: t.smoothMs = n * 1000.0f; break;
		}
	}

	std::string cellText(const jp_audio_internal::SourceTuning &t, int cell)
	{
		switch (cell)
		{
		case CELL_THRESH: return ofToString(t.threshold, 2);
		case CELL_GAIN: return "x" + ofToString(t.gain, 2);
		case CELL_ADD: return (t.add >= 0.0f ? "+" : "") + ofToString(t.add, 2);
		default: return t.smoothMs <= 0.0f ? "off" :
			ofToString((int)t.smoothMs) + "ms";
		}
	}

	bool isIdentity(const jp_audio_internal::SourceTuning &t)
	{
		return t.threshold == 0.0f && t.gain == 1.0f && t.add == 0.0f &&
			t.smoothMs == 0.0f;
	}

	// A horizontal bar with a ghost behind it. The ghost is the value BEFORE
	// smoothing, so the Smooth knob's effect is visible on its own rather than
	// having to be inferred from a number that moves slower.
	void drawMeter(const ofRectangle &r, float value, float ghost,
		const ofColor &accent)
	{
		// COL_BG_INPUT, not COL_BG_DARK: the panel itself is COL_BG_DARK, so a
		// well painted in it is invisible and the bar floats in nothing.
		ofSetColor(COL_BG_INPUT);
		ofDrawRectRounded(r, 3.0f);
		const float ghostW = r.width * ofClamp(ghost, 0.0f, 1.0f);
		if (ghostW > 0.5f)
		{
			ofSetColor(ofColor(accent, 80));
			ofDrawRectangle(r.x, r.y, ghostW, r.height);
		}
		const float width = r.width * ofClamp(value, 0.0f, 1.0f);
		if (width > 0.5f)
		{
			ofSetColor(ofColor(accent, 235));
			ofDrawRectangle(r.x, r.y, width, r.height);
		}
	}

	// A recessed area for a plot, so it reads as a container.
	void drawWell(const ofRectangle &r)
	{
		ofSetColor(COL_BG_INPUT);
		ofDrawRectRounded(r, 3.0f);
		ofNoFill();
		ofSetColor(ofColor(COL_BORDER_MUTED, 120));
		ofDrawRectRounded(r, 3.0f);
		ofFill();
	}
}

// The input chain: enable, device, calibration, channel, gain and gate.
//
// This used to be the AUDIO IN block on the SETTINGS screen, sitting under the
// OSC ports and above the transition controls. It reads far better here: the
// gate you are setting is drawn on the same scale as the INPUT meter right
// below it, and the device you pick is the one whose spectrum is on screen.
void ofApp::drawAudioInput(const AudioScreenLayout &L)
{
	ofTrueTypeFont &small = jp_constants::p2_font;
	auto caption = [&](const ofRectangle &r, const std::string &text) {
		ofSetColor(COL_TEXT_MUTED);
		small.drawString(text, r.x, r.y - 6.0f);
	};

	const bool live = jp_audio::isRunning();
	caption(L.audioEnable, "INPUT");
	jp_button::draw(L.audioEnable, jp_audio::getEnabled() ? "ON" : "OFF",
		jp_audio::getEnabled(), true,
		live ? COL_ACCENT_GREEN : COL_ACCENT_CYAN);
	jp_tooltip::draw("Turn the audio input on or off", L.audioEnable);

	caption(L.audioDevice, "DEVICE");
	{
		const std::vector<std::string> &names = jp_audio::getInputDeviceNames();
		std::string label = jp_audio::getDeviceName();
		if (label.empty()) label = names.empty() ? "no input device" : "(default)";
		while (!label.empty() &&
			small.stringWidth(label) > L.audioDevice.width - 26.0f)
		{
			label.pop_back();
		}
		jp_button::draw(L.audioDevice, label, false);
		ofSetColor(COL_TEXT_MUTED);
		small.drawString("v", L.audioDevice.getMaxX() - 14.0f,
			L.audioDevice.getMaxY() - 8.0f);
	}
	jp_tooltip::draw("Choose the input device", L.audioDevice);

	caption(L.audioCalibrate, "SET GATE");
	jp_button::draw(L.audioCalibrate, "CALIBRATE", false);
	jp_tooltip::draw("Listen for three seconds and set the gate to the room",
		L.audioCalibrate);

	caption(L.audioAutoGain, "NORMALISE");
	jp_button::draw(L.audioAutoGain, jp_audio::getAutoGain() ? "AUTO" : "MANUAL",
		jp_audio::getAutoGain(), true);
	jp_tooltip::draw(
		"AUTO adapts each band to the incoming level. MANUAL reports raw energy "
		"instead, and high frequencies carry far less of it than bass - on a "
		"kick-and-hat pattern High spans 0.015 in MANUAL against 0.506 in AUTO. "
		"Use MANUAL only when the normaliser's range keeps collapsing.",
		L.audioAutoGain);

	caption(L.audioChannel, "CHANNEL");
	jp_button::draw(L.audioChannel,
		jp_audio::channelModeLabel(jp_audio::getChannelMode()), false);
	jp_tooltip::draw("MIX, LEFT or RIGHT. Right click steps backwards.",
		L.audioChannel);

	caption(L.audioDiv, "DIV");
	jp_button::draw(L.audioDiv, jp_audio::divLabel(jp_audio::getShaderDiv()),
		false);
	jp_tooltip::draw(
		"Which beat division the audio_trigger, audio_express and audio_logic "
		"uniforms use. Right click steps backwards.", L.audioDiv);

	// Gain and gate keep the slider shape they had on SETTINGS.
	auto slider = [&](const ofRectangle &r, float t, const std::string &text) {
		ofSetColor(ofColor(COL_BG_INPUT, 220));
		ofDrawRectRounded(r, 4.0f);
		ofSetColor(ofColor(COL_ACCENT_CYAN, 170));
		ofDrawRectRounded(r.x, r.y, std::max(6.0f, r.width * t), r.height, 4.0f);
		ofSetColor(COL_TEXT_PRIMARY);
		small.drawString(text, r.x + 8.0f, r.getMaxY() - 8.0f);
	};
	caption(L.audioGain, "GAIN");
	const float gain = jp_audio::getGain();
	slider(L.audioGain, ofClamp((gain - 0.05f) / (8.0f - 0.05f), 0.0f, 1.0f),
		"x" + ofToString(gain, 2));
	jp_tooltip::draw("Input gain, applied before any analysis", L.audioGain);

	caption(L.audioGate, "NOISE GATE");
	slider(L.audioGate, ofClamp(jp_audio::getNoiseGate() / 0.25f, 0.0f, 1.0f),
		ofToString(jp_audio::getNoiseGate(), 3));
	jp_tooltip::draw(
		"Below this input RMS the bands are fed silence. Its position is drawn "
		"on the INPUT meter below.", L.audioGate);
}

bool ofApp::handleAudioInputClick(const AudioScreenLayout &L, const ofVec2f &m,
	int button)
{
	// The same split the settings screen uses: cycling buttons answer both
	// buttons and step backwards on the right one, everything else is left only.
	const bool leftButton = button == OF_MOUSE_BUTTON_LEFT;
	const bool cycleButton = leftButton || button == OF_MOUSE_BUTTON_RIGHT;
	const int step = button == OF_MOUSE_BUTTON_RIGHT ? -1 : 1;

	// An open dropdown overlays whatever is under it, so it is tested first and
	// swallows ANY button.
	if (audioMenuOpen)
	{
		const ofRectangle menu = getAudioMenuBounds();
		if (menu.inside(m))
		{
			const std::vector<std::string> &names =
				jp_audio::getInputDeviceNames();
			for (int i = 0; i <= (int)names.size(); i++)
			{
				const ofRectangle row(menu.x, menu.y + 2.0f + i * 24.0f,
					menu.width, 24.0f);
				if (!row.inside(m)) continue;
				jp_audio::setDevice(i == 0 ? "" : names[i - 1]);
				saveSettings();
				break;
			}
		}
		audioMenuOpen = false;
		return true;
	}
	if (leftButton && L.audioEnable.inside(m))
	{
		jp_audio::setEnabled(!jp_audio::getEnabled());
		saveSettings();
		return true;
	}
	if (leftButton && L.audioDevice.inside(m))
	{
		jp_audio::refreshDevices();
		audioMenuOpen = true;
		return true;
	}
	if (leftButton && L.audioCalibrate.inside(m))
	{
		jp_audio::beginCalibration();
		return true;
	}
	if (leftButton && L.audioAutoGain.inside(m))
	{
		jp_audio::setAutoGain(!jp_audio::getAutoGain());
		saveSettings();
		return true;
	}
	if (cycleButton && L.audioChannel.inside(m))
	{
		jp_audio::setChannelMode(
			(jp_audio::getChannelMode() + step + jp_audio::CHANNEL_COUNT) %
			jp_audio::CHANNEL_COUNT);
		saveSettings();
		return true;
	}
	if (cycleButton && L.audioDiv.inside(m))
	{
		jp_audio::setShaderDiv(
			(jp_audio::getShaderDiv() + step + jp_audio::DIV_COUNT) %
			jp_audio::DIV_COUNT);
		saveSettings();
		return true;
	}
	if (leftButton && L.audioGain.inside(m))
	{
		audioGainDragging = true;
		jp_audio::setGain(ofLerp(0.05f, 8.0f,
			ofClamp((m.x - L.audioGain.x) / L.audioGain.width, 0.0f, 1.0f)));
		return true;
	}
	if (leftButton && L.audioGate.inside(m))
	{
		audioGateDragging = true;
		jp_audio::setNoiseGate(ofClamp(
			(m.x - L.audioGate.x) / L.audioGate.width, 0.0f, 1.0f) * 0.25f);
		return true;
	}
	return false;
}

ofApp::AudioScreenLayout ofApp::getAudioScreenLayout() const
{
	AudioScreenLayout l;
	l.frame = jp_screen::frame();
	l.frame.y -= audioScreenScroll;
	const ofRectangle body = jp_screen::body(l.frame);

	// The input chain, in one strip across the top: device and level come
	// BEFORE analysis, so they read first.
	{
		const float h = jp_button::kHeight;
		const float gap = 8.0f;
		const float top = body.y + 14.0f;
		float x = body.x;
		auto slot = [&](ofRectangle &r, float w) {
			r.set(x, top, w, h);
			x += w + gap;
		};
		slot(l.audioEnable, 54.0f);
		slot(l.audioDevice, 240.0f);
		slot(l.audioCalibrate, 92.0f);
		slot(l.audioAutoGain, 86.0f);
		slot(l.audioChannel, 86.0f);
		slot(l.audioDiv, 54.0f);
		x += 8.0f;   // the gain/gate pair reads as its own group
		slot(l.audioGain, 150.0f);
		slot(l.audioGate, 150.0f);
	}
	const float stripH = 14.0f + jp_button::kHeight + 22.0f;
	const ofRectangle columns(body.x, body.y + stripH, body.width,
		std::max(0.0f, body.height - stripH));

	// Two columns, the way SETTINGS already splits: eight tuning rows plus the
	// diagnostics do not fit one column on a 768 px laptop.
	const float columnGap = 24.0f;
	const float leftW = std::min(560.0f, (columns.width - columnGap) * 0.55f);
	l.leftColumn.set(columns.x, columns.y, leftW, columns.height);
	l.rightColumn.set(columns.x + leftW + columnGap, columns.y,
		std::max(240.0f, columns.width - leftW - columnGap), columns.height);

	float y = l.leftColumn.y;
	const float cellW = (l.leftColumn.width - kResetWidth - kCellGap -
		3.0f * kCellGap) / 4.0f;
	for (int i = 0; i < jp_audio_internal::TunedSources; i++)
	{
		l.sourceName[i].set(l.leftColumn.x, y, kNameWidth, kCellHeight);
		l.sourceMeter[i].set(l.leftColumn.x + kNameWidth + kCellGap, y + 3.0f,
			l.leftColumn.width - kNameWidth - kCellGap * 2.0f - kResetWidth -
				46.0f,
			kCellHeight - 9.0f);
		l.sourceReset[i].set(l.leftColumn.getMaxX() - kResetWidth, y,
			kResetWidth, kCellHeight);
		const float cellY = y + kCellHeight + 3.0f;
		for (int c = 0; c < 4; c++)
		{
			l.sourceCell[i][c].set(l.leftColumn.x + c * (cellW + kCellGap),
				cellY, cellW, kCellHeight);
		}
		y += kRowHeight;
	}
	y += kGroupGap;
	const float onsetCellW = (l.leftColumn.width - kCellGap) / 2.0f;
	for (int i = 0; i < jp_audio_internal::Onsets; i++)
	{
		l.onsetName[i].set(l.leftColumn.x, y, kNameWidth, kCellHeight);
		l.onsetPlot[i].set(l.leftColumn.x + kNameWidth + kCellGap, y,
			120.0f, kCellHeight + 8.0f);
		const float cellY = y + kCellHeight + 11.0f;
		for (int c = 0; c < 2; c++)
		{
			l.onsetCell[i][c].set(l.leftColumn.x + c * (onsetCellW + kCellGap),
				cellY, onsetCellW, kCellHeight);
		}
		y += kRowHeight + 8.0f;
	}

	float ry = l.rightColumn.y;
	l.inputMeter.set(l.rightColumn.x, ry, l.rightColumn.width, 22.0f);
	ry += 22.0f + 18.0f;
	l.spectrum.set(l.rightColumn.x, ry, l.rightColumn.width, 70.0f);
	ry += 70.0f + 18.0f;
	l.historyPlot.set(l.rightColumn.x, ry, l.rightColumn.width, 110.0f);
	ry += 110.0f + 18.0f;
	l.resetAllButton.set(l.rightColumn.x, ry, 96.0f, jp_button::kHeight);
	l.selfTestButton.set(l.rightColumn.x + 96.0f + jp_button::kGap, ry,
		l.rightColumn.width - 96.0f - jp_button::kGap, jp_button::kHeight);
	ry += jp_button::kHeight + 14.0f;

	l.contentHeight = jp_screen::kTop +
		std::max(y - l.leftColumn.y, ry - l.rightColumn.y) +
		jp_screen::kHeaderH + jp_screen::kPad + jp_screen::kMarginBottom;
	return l;
}

void ofApp::clampAudioScreenScroll()
{
	// Measured with the scroll removed, or the clamp chases its own offset.
	const float saved = audioScreenScroll;
	audioScreenScroll = 0.0f;
	const float content = getAudioScreenLayout().contentHeight;
	audioScreenScroll = ofClamp(saved, 0.0f,
		std::max(0.0f, content - ofGetHeight()));
}

void ofApp::draw_audio()
{
	// Same clip OPCIONES uses: one 36 px wheel notch against a tab bar that
	// occupies y 8..36 would otherwise paint the panel straight through it.
	const ofRectangle clip(0.0f, jp_screen::kTop,
		(float)ofGetWidth(), (float)ofGetHeight() - jp_screen::kTop);
	jp_gl::ScopedScissor audioClip(clip);

	const AudioScreenLayout L = getAudioScreenLayout();
	const jp_audio_internal::AnalyzerDiagnostics d = jp_audio::getDiagnostics();
	const jp_audio::AudioSnapshot snapshot = jp_audio::getSnapshot();
	const bool live = jp_audio::isRunning() && jp_audio::getEnabled();

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	// An explicit opaque backing rect before the shared chrome. drawFrame fills
	// at alpha 235, which is fine over the node canvas but not over the active
	// render: this screen sits on top of full-brightness video, and the 8% that
	// bleeds through is enough to make a meter unreadable.
	ofEnableAlphaBlending();
	ofSetColor(COL_BG_DARK);
	ofDrawRectRounded(L.frame, jp_screen::kRadius);
	jp_screen::drawFrame(L.frame, "AUDIO ANALYSER",
		live ? jp_audio::getStatus() :
			"input is not running - the numbers below are all zero");

	ofTrueTypeFont &font = jp_constants::p_font;
	ofTrueTypeFont &small = jp_constants::p2_font;

	drawAudioInput(L);

	// ------------------------------------------------ the six tuned sources
	for (int i = 0; i < jp_audio_internal::TunedSources; i++)
	{
		const jp_audio_internal::SourceTuning tuning = jp_audio::getTuning(i);
		const jp_audio_internal::AnalyzerDiagnostics::BandInfo &band = d.bands[i];

		// Clicking the name toggles this source's trace on the history plot, so
		// the name carries the trace's own colour and a swatch of it. The
		// swatch stays visible when the trace is off - dimmed - so the legend
		// still tells you which colour this row WOULD be.
		const bool traced = audioTrace[i];
		ofSetColor(traced ? kTraceColors[i] : ofColor(kTraceColors[i], 70));
		ofDrawRectRounded(L.sourceName[i].x, L.sourceName[i].y + 5.0f,
			kSwatchWidth, kSwatchWidth, 2.0f);
		ofSetColor(traced ? kTraceColors[i] : ofColor(COL_TEXT_SECONDARY, 190));
		font.drawString(jp_audio::tunedSourceLabel(i),
			L.sourceName[i].x + kSwatchWidth + 5.0f,
			L.sourceName[i].y + 14.0f);

		// The meter takes the same colour, so a glance ties the bar, the swatch
		// and the line on the plot together.
		drawMeter(L.sourceMeter[i], band.shaped, band.preSmooth,
			kTraceColors[i]);

		// The normaliser's working range, as a hairline under the bar.
		//
		// norm = (raw - floor) / (peak - floor). When that span collapses the
		// band stops being a level and becomes a comparator: everything pins at
		// 1.0 on the faintest sound, which is this analyser's most confusing
		// failure and was invisible until now. A short red hairline says
		// "collapsed" at a glance.
		const float span = band.peakLevel - band.floorLevel;
		const bool collapsed = span < 0.02f;
		ofSetColor(collapsed ? ofColor(COL_ACCENT_RED, 230) :
			ofColor(COL_ACCENT_GOLD, 150));
		ofDrawRectangle(L.sourceMeter[i].x, L.sourceMeter[i].getMaxY() + 1.0f,
			std::max(2.0f, L.sourceMeter[i].width *
				ofClamp(span / 0.5f, 0.0f, 1.0f)), 1.0f);
		jp_tooltip::draw(collapsed ?
			"The normaliser's range has collapsed: this band now reacts like a "
			"switch. Turn AUTO GAIN off in SETTINGS, or give it more signal." :
			"How much room the auto-gain normaliser is working with",
			L.sourceMeter[i]);

		// The value, right-aligned against the reset button rather than
		// floating over it.
		ofSetColor(COL_TEXT_MUTED);
		const std::string valueText = ofToString(band.shaped, 2);
		small.drawString(valueText,
			L.sourceReset[i].x - 6.0f - small.stringWidth(valueText),
			L.sourceName[i].y + 14.0f);

		// A row that is not at its factory value has to say so - a forgotten
		// knob is otherwise indistinguishable from a broken analyser - and its
		// reset button is the thing that undoes it, so that is what lights up.
		// (This was a separate asterisk, which landed on top of the value.)
		const bool modified = !isIdentity(tuning);
		jp_button::draw(L.sourceReset[i], "o", modified, true,
			COL_ACCENT_GOLD);
		jp_tooltip::draw(modified ?
			"This source is not at its factory value - click to reset it" :
			"Already at the factory value", L.sourceReset[i]);

		for (int c = 0; c < 4; c++)
		{
			jp_minislider::draw(L.sourceCell[i][c], kCellLabels[c],
				cellText(tuning, c), cellNormalized(tuning, c),
				jp_minislider::hovered(L.sourceCell[i][c]));
		}
	}

	// ------------------------------------------------------- the detectors
	for (int i = 0; i < jp_audio_internal::Onsets; i++)
	{
		const jp_audio_internal::AnalyzerDiagnostics::OnsetInfo &onset = d.onsets[i];
		const bool recent = onset.secondsSinceLast < 0.12f;
		ofSetColor(recent ? COL_ACCENT_GOLD : COL_TEXT_SECONDARY);
		font.drawString(kOnsetNames[i], L.onsetName[i].x,
			L.onsetName[i].y + 14.0f);

		// Flux against its own moving threshold. This is the whole answer to
		// "why is my kick not triggering": if the bar never reaches the line,
		// lower Sens; if it does and nothing fires, the refractory or the
		// material gate is holding it.
		drawWell(L.onsetPlot[i]);
		const float scale = std::max(0.00002f, onset.threshold * 1.6f);
		const float fluxH = L.onsetPlot[i].height *
			ofClamp(onset.flux / scale, 0.0f, 1.0f);
		ofSetColor(onset.flux > onset.threshold ?
			ofColor(COL_ACCENT_GREEN, 220) : ofColor(COL_ACCENT_CYAN, 170));
		ofDrawRectangle(L.onsetPlot[i].x + 3.0f,
			L.onsetPlot[i].getMaxY() - fluxH,
			L.onsetPlot[i].width - 6.0f, fluxH);
		const float threshY = L.onsetPlot[i].getMaxY() -
			L.onsetPlot[i].height * ofClamp(onset.threshold / scale, 0.0f, 1.0f);
		// The line the flux has to cross. Everything about "why did this not
		// fire" is the relationship between these two.
		ofSetColor(COL_ACCENT_RED);
		ofDrawRectangle(L.onsetPlot[i].x + 1.0f, threshY,
			L.onsetPlot[i].width - 2.0f, 1.0f);

		// Status beside the well, not inside it.
		const float textX = L.onsetPlot[i].getMaxX() + 10.0f;
		ofSetColor(onset.blockedByRefractory ? COL_ACCENT_GOLD : COL_TEXT_MUTED);
		small.drawString(onset.blockedByRefractory ? "holding" :
			(onset.materialGate ? "listening" : "gated by material"),
			textX, L.onsetPlot[i].y + 12.0f);
		ofSetColor(COL_TEXT_MUTED);
		small.drawString(ofToString((long long)onset.count) + " hits",
			textX, L.onsetPlot[i].y + 24.0f);

		const float sens = jp_audio::getOnsetSensitivity(i);
		const float hold = jp_audio::getOnsetRefractory(i);
		jp_minislider::draw(L.onsetCell[i][0], kOnsetLabels[0],
			ofToString(sens, 2), (sens - 1.0f) / 3.0f,
			jp_minislider::hovered(L.onsetCell[i][0]));
		jp_minislider::draw(L.onsetCell[i][1], kOnsetLabels[1],
			ofToString((int)(hold * 1000.0f)) + "ms",
			(hold - 0.030f) / 0.470f,
			jp_minislider::hovered(L.onsetCell[i][1]));
	}

	// ------------------------------------------------------ right column
	// Input level with the noise gate marked on the SAME scale. Nothing in the
	// app showed the input level before, so "is anything even arriving" was not
	// a question the UI could answer.
	ofSetColor(COL_TEXT_SECONDARY);
	small.drawString("INPUT", L.inputMeter.x, L.inputMeter.y - 7.0f);
	drawWell(L.inputMeter);
	const float rmsW = L.inputMeter.width * ofClamp(d.rms * 4.0f, 0.0f, 1.0f);
	ofSetColor(d.gated ? ofColor(COL_TEXT_MUTED, 200) :
		ofColor(COL_ACCENT_GREEN, 220));
	if (rmsW > 0.5f)
		ofDrawRectangle(L.inputMeter.x, L.inputMeter.y, rmsW,
			L.inputMeter.height);
	const float gateX = L.inputMeter.x + L.inputMeter.width *
		ofClamp(jp_audio::getNoiseGate() * 4.0f, 0.0f, 1.0f);
	ofSetColor(COL_ACCENT_RED);
	ofDrawRectangle(gateX, L.inputMeter.y, 1.0f, L.inputMeter.height);
	ofSetColor(d.gated ? COL_ACCENT_GOLD : COL_TEXT_MUTED);
	small.drawString(d.gated ? "GATED" : "open",
		L.inputMeter.getMaxX() - 42.0f, L.inputMeter.y - 4.0f);

	ofSetColor(COL_TEXT_SECONDARY);
	small.drawString("SPECTRUM (unshaped)", L.spectrum.x, L.spectrum.y - 7.0f);
	drawWell(L.spectrum);
	{
		const int bins = jp_audio::SPECTRUM_BINS;
		const float bw = L.spectrum.width / (float)bins;
		for (int i = 0; i < bins; i++)
		{
			const float h = ofClamp(snapshot.spectrum[i], 0.0f, 1.0f) *
				(L.spectrum.height - 6.0f);
			ofSetColor(ofColor(COL_ACCENT_CYAN, 200));
			ofDrawRectangle(L.spectrum.x + i * bw + 1.0f,
				L.spectrum.getMaxY() - 3.0f - h,
				std::max(1.0f, bw - 2.0f), h);
		}
	}

	// One plot, several traces, chosen by clicking a source name. Eight small
	// separate rings could not answer "how does this row compare to that one",
	// which is nearly always the actual question.
	ofSetColor(COL_TEXT_SECONDARY);
	small.drawString("HISTORY (click a source name to trace it)",
		L.historyPlot.x, L.historyPlot.y - 7.0f);
	drawWell(L.historyPlot);
	{
		const int shown = std::min(d.historyFilled,
			jp_audio_internal::HistoryLength);
		for (int src = 0; src < jp_audio_internal::TunedSources; src++)
		{
			if (!audioTrace[src] || shown < 2) continue;
			ofSetColor(kTraceColors[src]);
			ofSetLineWidth(1.4f);
			ofNoFill();
			ofBeginShape();
			for (int i = 0; i < shown; i++)
			{
				// Oldest first: the ring's write cursor is the newest sample.
				const int at = (d.historyAt - shown + i +
					jp_audio_internal::HistoryLength * 2) %
					jp_audio_internal::HistoryLength;
				const float px = L.historyPlot.x +
					L.historyPlot.width * (float(i) / float(shown - 1));
				// Inset top and bottom so a trace pinned at 1.0 stays inside
				// its well instead of riding the caption above it.
				const float py = L.historyPlot.getMaxY() - 5.0f -
					(L.historyPlot.height - 10.0f) *
					ofClamp(d.history[src][at], 0.0f, 1.0f);
				ofVertex(px, py);
			}
			ofEndShape(false);
			ofFill();
			ofSetLineWidth(1.0f);
		}
	}

	jp_button::draw(L.resetAllButton, "RESET ALL", false);
	jp_tooltip::draw("Return every source and detector to the factory values",
		L.resetAllButton);
	jp_button::draw(L.selfTestButton, "SELF TEST (synthetic, factory)", false);
	jp_tooltip::draw(
		"Runs a separate analyser on a synthetic signal with factory tuning. "
		"It does NOT test your device, gain, gate or the knobs above.",
		L.selfTestButton);

	// The live evidence, which is what actually answers "is my chain working".
	float statusY = L.selfTestButton.getMaxY() + 16.0f;
	ofSetColor(COL_TEXT_MUTED);
	const std::string device = jp_audio::getDeviceName().empty() ?
		"(system default)" : jp_audio::getDeviceName();
	small.drawString("device: " + device, L.rightColumn.x, statusY);
	statusY += 14.0f;
	small.drawString("peak in: " + ofToString(snapshot.inputPeak, 3) +
		(snapshot.clipping ? "   CLIPPING" : ""), L.rightColumn.x, statusY);
	statusY += 14.0f;
	const unsigned long long recentDrops = jp_audio::getRecentDroppedBlocks();
	ofSetColor(recentDrops > 0 ? COL_ACCENT_RED : COL_TEXT_MUTED);
	small.drawString("dropped: " + ofToString((long long)recentDrops) +
		"/s   (" + ofToString((long long)snapshot.droppedBlocks) +
		" since start)", L.rightColumn.x, statusY);
	ofSetColor(COL_TEXT_MUTED);
	statusY += 14.0f;
	if (snapshot.tempoConfidence > 0.0f)
	{
		small.drawString("tempo: " + ofToString(snapshot.detectedBpm, 1) +
			" BPM at " + ofToString((int)(snapshot.tempoConfidence * 100.0f)) +
			"%", L.rightColumn.x, statusY);
		statusY += 14.0f;
	}
	// The single most common reason a band "does not react": with the
	// normaliser off, every band reports its raw energy, and high frequencies
	// carry far less of that than bass. Say so rather than making it something
	// you have to already know.
	if (!jp_audio::getAutoGain())
	{
		ofSetColor(COL_ACCENT_GOLD);
		small.drawString("NORMALISE is MANUAL: bands report raw energy, so High",
			L.rightColumn.x, statusY);
		statusY += 12.0f;
		small.drawString("and Mid read far lower than Low. AUTO evens them out.",
			L.rightColumn.x, statusY);
		statusY += 16.0f;
		ofSetColor(COL_TEXT_MUTED);
	}
	if (!audioSelfTestReport.empty())
	{
		ofSetColor(COL_TEXT_SECONDARY);
		small.drawString(audioSelfTestReport, L.rightColumn.x, statusY);
	}

	// The device dropdown paints last so it covers whatever is beneath it.
	if (audioMenuOpen)
	{
		const ofRectangle menu = getAudioMenuBounds();
		ofSetColor(ofColor(COL_BG_PANEL, 245));
		ofDrawRectRounded(menu, 4.0f);
		ofNoFill();
		ofSetColor(ofColor(COL_ACCENT_CYAN, 200));
		ofDrawRectRounded(menu, 4.0f);
		ofFill();
		const std::vector<std::string> &names = jp_audio::getInputDeviceNames();
		for (int i = 0; i <= (int)names.size(); i++)
		{
			const float ry = menu.y + 2.0f + i * 24.0f;
			if (ry + 24.0f > menu.getMaxY()) break;
			const bool over = ofRectangle(menu.x, ry, menu.width, 24.0f)
				.inside((float)ofGetMouseX(), (float)ofGetMouseY());
			const std::string name = i == 0 ? "(system default)" : names[i - 1];
			if (over)
			{
				ofSetColor(ofColor(COL_BG_HOVER, 230));
				ofDrawRectRounded(menu.x + 2.0f, ry, menu.width - 4.0f,
					24.0f, 3.0f);
			}
			ofSetColor(name == jp_audio::getDeviceName() ?
				COL_ACCENT_CYAN : COL_TEXT_PRIMARY);
			small.drawString(name, menu.x + 8.0f, ry + 16.0f);
		}
	}

	ofPopStyle();
}

void ofApp::applyAudioTuningFromMouse(const AudioScreenLayout &layout,
	float mouseX)
{
	if (audioDragRow < 0 || audioDragCol < 0) return;
	if (audioDragRow < jp_audio_internal::TunedSources)
	{
		const ofRectangle &cell =
			layout.sourceCell[audioDragRow][audioDragCol];
		jp_audio_internal::SourceTuning tuning =
			jp_audio::getTuning(audioDragRow);
		cellFromNormalized(tuning, audioDragCol,
			jp_minislider::normalizedFromMouse(cell, mouseX));
		jp_audio::setTuning(audioDragRow, tuning);
		return;
	}
	const int onset = audioDragRow - jp_audio_internal::TunedSources;
	const ofRectangle &cell = layout.onsetCell[onset][audioDragCol];
	const float n = jp_minislider::normalizedFromMouse(cell, mouseX);
	if (audioDragCol == 0)
		jp_audio::setOnsetSensitivity(onset, 1.0f + n * 3.0f);
	else
		jp_audio::setOnsetRefractory(onset, 0.030f + n * 0.470f);
}

bool ofApp::handleAudioScreenClick(int x, int y, int button)
{
	const AudioScreenLayout L = getAudioScreenLayout();
	const ofVec2f m((float)x, (float)y);
	// Same split the settings screen uses: a slider only ever answers the left
	// button, and right-click keeps meaning "step a cycling control backwards"
	// - there are no cycling controls here, so it is simply inert.
	// The input chain first: its dropdown overlays everything under it.
	if (handleAudioInputClick(L, m, button)) return true;

	const bool leftButton = button == OF_MOUSE_BUTTON_LEFT;
	if (!leftButton) return false;

	if (L.resetAllButton.inside(m))
	{
		for (int i = 0; i < jp_audio_internal::TunedSources; i++)
			jp_audio::setTuning(i, jp_audio_internal::SourceTuning());
		jp_audio::setOnsetSensitivity(jp_audio_internal::ONSET_KICK, 1.6f);
		jp_audio::setOnsetSensitivity(jp_audio_internal::ONSET_SNARE, 1.4f);
		jp_audio::setOnsetRefractory(jp_audio_internal::ONSET_KICK, 0.110f);
		jp_audio::setOnsetRefractory(jp_audio_internal::ONSET_SNARE, 0.060f);
		saveSettings();
		return true;
	}
	if (L.selfTestButton.inside(m))
	{
		std::string report;
		const bool passed = jp_audio::runSelfTest(&report);
		audioSelfTestReport = (passed ? "self test PASSED  " : "self test FAILED  ") +
			report;
		return true;
	}
	for (int i = 0; i < jp_audio_internal::TunedSources; i++)
	{
		if (L.sourceName[i].inside(m) || L.sourceMeter[i].inside(m))
		{
			audioTrace[i] = !audioTrace[i];
			return true;
		}
		if (L.sourceReset[i].inside(m))
		{
			jp_audio::setTuning(i, jp_audio_internal::SourceTuning());
			saveSettings();
			return true;
		}
		for (int c = 0; c < 4; c++)
		{
			if (!L.sourceCell[i][c].inside(m)) continue;
			audioDragRow = i;
			audioDragCol = c;
			applyAudioTuningFromMouse(L, m.x);
			return true;
		}
	}
	for (int i = 0; i < jp_audio_internal::Onsets; i++)
	{
		for (int c = 0; c < 2; c++)
		{
			if (!L.onsetCell[i][c].inside(m)) continue;
			audioDragRow = jp_audio_internal::TunedSources + i;
			audioDragCol = c;
			applyAudioTuningFromMouse(L, m.x);
			return true;
		}
	}
	return false;
}
