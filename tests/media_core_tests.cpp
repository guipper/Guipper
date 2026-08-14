#include "../src/JPbox/jp_media_state.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
	int failures = 0;

	void expect(bool condition, const std::string &message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}

	bool near(float a, float b)
	{
		return std::abs(a-b) < 0.0001f;
	}

	void testNormalization()
	{
		JPMediaState state;
		state.rangeIn = -1.0f;
		state.rangeOut = 2.0f;
		state.position = 3.0f;
		state.rate = 9.0f;
		state.volume = -1.0f;
		jp_media::normalize(state);
		expect(near(state.rangeIn,0.0f) && near(state.rangeOut,1.0f),
			"normalization clamps range");
		expect(near(state.position,1.0f), "normalization clamps playhead");
		expect(near(state.rate,4.0f), "normalization clamps rate");
		expect(near(state.volume,0.0f), "normalization clamps volume");

		state.rangeIn = .7f;
		state.rangeOut = .2f;
		state.position = .3f;
		jp_media::normalize(state);
		expect(near(state.rangeOut,.7f) && near(state.position,.7f),
			"crossed and zero-width ranges hold one frame");
	}

	void testOnce()
	{
		JPMediaState state;
		state.rangeIn=.2f; state.rangeOut=.8f;
		state.loopMode=JPMediaLoopMode::Once;
		float position=.9f;
		expect(jp_media::applyBoundary(state,position) && near(position,.8f) &&
			!state.playing, "forward Once stops at OUT");
		state.playing=true; state.reverse=true; position=.1f;
		expect(jp_media::applyBoundary(state,position) && near(position,.2f) &&
			!state.playing, "reverse Once stops at IN");
	}

	void testLoop()
	{
		JPMediaState state;
		state.rangeIn=.2f; state.rangeOut=.8f;
		state.loopMode=JPMediaLoopMode::Loop;
		float position=.9f;
		expect(jp_media::applyBoundary(state,position) && near(position,.2f),
			"forward Loop wraps to IN");
		state.reverse=true; position=.1f;
		expect(jp_media::applyBoundary(state,position) && near(position,.8f),
			"reverse Loop wraps to OUT");
	}

	void testPingPong()
	{
		JPMediaState state;
		state.rangeIn=.2f; state.rangeOut=.8f;
		state.loopMode=JPMediaLoopMode::PingPong;
		float position=.9f;
		expect(jp_media::applyBoundary(state,position) && near(position,.7f) &&
			state.reverse, "forward Ping-pong reverses at OUT");
		position=.1f;
		expect(jp_media::applyBoundary(state,position) && near(position,.3f) &&
			!state.reverse, "reverse Ping-pong reverses at IN");
		position=.81f;
		expect(jp_media::applyBoundary(state,position) && near(position,.79f) &&
			state.reverse, "Ping-pong preserves small endpoint overshoot");
		state.rangeIn=.4f; state.rangeOut=.5f; state.reverse=false; position=.87f;
		expect(jp_media::applyBoundary(state,position) && near(position,.47f) &&
			!state.reverse, "Ping-pong folds large overshoot through a narrow custom range");
	}

	void testRangeCapture()
	{
		JPMediaState state;
		state.position=.35f; state.rangeIn=.1f; state.rangeOut=.8f;
		jp_media::captureRangeIn(state);
		expect(near(state.rangeIn,.35f) && near(state.rangeOut,.8f),
			"IN captures the current playhead");
		state.position=.65f;
		jp_media::captureRangeOut(state);
		expect(near(state.rangeIn,.35f) && near(state.rangeOut,.65f),
			"OUT captures the current playhead");

		state.position=.9f;
		jp_media::captureRangeIn(state);
		expect(near(state.rangeIn,.9f) && near(state.rangeOut,.9f),
			"IN crossing OUT moves OUT to the captured frame");
		state.position=.2f;
		jp_media::captureRangeOut(state);
		expect(near(state.rangeIn,.2f) && near(state.rangeOut,.2f),
			"OUT crossing IN moves IN to the captured frame");
	}

	// Display rank for the transform parameters. The inspector rows and the
	// MIDI bind slots both sort by this, so a wrong rank silently points a knob
	// at a different row than the one it sits beside.
	void testTransformRank()
	{
		expect(jp_media::transformParameterRank("scale ratio") == 0 &&
			jp_media::transformParameterRank("scaleratio") == 0 &&
			jp_media::transformParameterRank("scale_ratio") == 0,
			"every spelling of scale ratio ranks first");
		// A GLSL identifier cannot contain a space, which is why the shader
		// spelling has to be recognised at all.
		expect(jp_media::isScaleRatioParameter("scaleratio"),
			"the shader spelling is recognised as a scale ratio");

		expect(jp_media::transformParameterRank("scalex") <
				jp_media::transformParameterRank("scaley") &&
			jp_media::transformParameterRank("scaley") <
				jp_media::transformParameterRank("offsetx") &&
			jp_media::transformParameterRank("offsetx") <
				jp_media::transformParameterRank("offsety"),
			"scalex, scaley, offsetx, offsety rank in that order");
		expect(jp_media::transformParameterRank("scale ratio") <
				jp_media::transformParameterRank("scalex"),
			"scale ratio outranks scalex");

		for (const char *other : {"rotacion", "camaraindex", "strech",
			"reciever", "intensity", ""})
		{
			expect(!jp_media::isRankedTransformParameter(other),
				"unrelated parameters are unranked");
			expect(jp_media::transformParameterRank(other) >
				jp_media::transformParameterRank("offsety"),
				"unranked parameters sort after the transform block");
		}
	}

	// The transform the camera, NDI and Spout boxes share.
	//
	// The first case is the important one: it reproduces the exact ofMap maths
	// those three used before the uniform zoom existed. If it ever drifts,
	// every saved composition using one of those boxes silently reframes.
	void testLegacyTransform()
	{
		const float W = 1920.0f, H = 1080.0f;
		// Reference implementation, copied from what the boxes did inline.
		auto reference = [&](float sx, float sy, float ox, float oy,
			float &x, float &y, float &w, float &h)
		{
			w = sx * W;
			h = sy * H;
			const float dx = -W/2.0f - w/2.0f + ox * (W + w);
			const float dy = -H/2.0f - h/2.0f + oy * (H + h);
			x = W/2.0f - w/2.0f + dx;
			y = H/2.0f - h/2.0f + dy;
		};
		const float samples[][4] = {
			{0.5f, 0.5f, 0.5f, 0.5f}, {0.25f, 0.75f, 0.0f, 1.0f},
			{1.0f, 1.0f, 0.3f, 0.8f}, {0.0f, 0.0f, 0.5f, 0.5f},
		};
		for (const auto &s : samples)
		{
			float rx, ry, rw, rh;
			reference(s[0], s[1], s[2], s[3], rx, ry, rw, rh);
			const jp_media::JPMediaRect got = jp_media::legacyTransformRect(
				s[0], s[1], s[2], s[3], 1.0f, W, H);
			expect(near(got.x, rx) && near(got.y, ry) &&
				near(got.width, rw) && near(got.height, rh),
				"ratio 1.0 reproduces the original transform exactly");
		}

		// The zoom scales about the centre.
		const jp_media::JPMediaRect one = jp_media::legacyTransformRect(
			0.5f, 0.5f, 0.5f, 0.5f, 1.0f, W, H);
		const jp_media::JPMediaRect two = jp_media::legacyTransformRect(
			0.5f, 0.5f, 0.5f, 0.5f, 2.0f, W, H);
		expect(near(two.width, one.width*2.0f) &&
			near(two.height, one.height*2.0f), "zoom scales the size");
		expect(near(one.x + one.width*0.5f, two.x + two.width*0.5f) &&
			near(one.y + one.height*0.5f, two.y + two.height*0.5f),
			"zoom keeps the centre fixed at a centred offset");

		// A zoomed source must still clear the canvas completely, which is why
		// the offsets are derived after the zoom rather than before it.
		const jp_media::JPMediaRect left = jp_media::legacyTransformRect(
			0.5f, 0.5f, 0.0f, 0.5f, 2.0f, W, H);
		expect(left.x + left.width <= 0.0001f,
			"offset 0 clears the left edge even when zoomed");
		const jp_media::JPMediaRect right = jp_media::legacyTransformRect(
			0.5f, 0.5f, 1.0f, 0.5f, 2.0f, W, H);
		expect(right.x >= W - 0.0001f,
			"offset 1 clears the right edge even when zoomed");

		// Out-of-range zoom is clamped to the slider's declared range.
		const jp_media::JPMediaRect clampLow = jp_media::legacyTransformRect(
			0.5f, 0.5f, 0.5f, 0.5f, -5.0f, W, H);
		const jp_media::JPMediaRect atMin = jp_media::legacyTransformRect(
			0.5f, 0.5f, 0.5f, 0.5f, 0.1f, W, H);
		expect(near(clampLow.width, atMin.width), "zoom clamps at 0.1");
		const jp_media::JPMediaRect clampHigh = jp_media::legacyTransformRect(
			0.5f, 0.5f, 0.5f, 0.5f, 99.0f, W, H);
		const jp_media::JPMediaRect atMax = jp_media::legacyTransformRect(
			0.5f, 0.5f, 0.5f, 0.5f, 4.0f, W, H);
		expect(near(clampHigh.width, atMax.width), "zoom clamps at 4.0");
	}

	// The render signature decides whether a media box may skip its FBO pass.
	// A false positive here means a frozen image on screen, so the cases that
	// must NOT match matter more than the one that must.
	JPMediaRenderSignature baseSignature()
	{
		JPMediaRenderSignature s;
		s.valid = true;
		s.fitMode = 1;
		s.scaleX = 0.5f; s.scaleY = 0.5f;
		s.offsetX = 0.5f; s.offsetY = 0.5f;
		s.scaleRatio = 1.0f;
		s.targetW = 1920.0f; s.targetH = 1080.0f;
		s.sourceW = 640.0f; s.sourceH = 480.0f;
		s.sourceGeneration = 7;
		return s;
	}

	void testRenderSignature()
	{
		const JPMediaRenderSignature base = baseSignature();
		expect(base.matches(base), "an unchanged signature matches itself");

		// Nothing rendered yet: the first pass must always run, and two boxes
		// that have both rendered nothing are not evidence either FBO is current.
		JPMediaRenderSignature fresh;
		expect(!fresh.matches(base), "an invalid signature never matches");
		expect(!base.matches(fresh), "matching against invalid is not allowed");
		expect(!fresh.matches(fresh), "two invalid signatures do not match");

		// Every field that feeds the composite must force a repaint.
		auto differs = [&](JPMediaRenderSignature changed, const char *what)
		{
			expect(!changed.matches(base), what);
		};
		JPMediaRenderSignature s = base; s.fitMode = 2;
		differs(s, "fit mode change repaints");
		s = base; s.scaleX += 0.01f; differs(s, "scaleX change repaints");
		s = base; s.scaleY += 0.01f; differs(s, "scaleY change repaints");
		s = base; s.offsetX += 0.01f; differs(s, "offsetX change repaints");
		s = base; s.offsetY += 0.01f; differs(s, "offsetY change repaints");
		s = base; s.scaleRatio += 0.01f; differs(s, "scale ratio change repaints");
		s = base; s.sourceW = 1280.0f; differs(s, "source width change repaints");
		s = base; s.sourceH = 720.0f; differs(s, "source height change repaints");
		// A render-resolution change reallocates the FBO, so a stale signature
		// would leave it blank.
		s = base; s.targetW = 1280.0f; differs(s, "render width change repaints");
		s = base; s.targetH = 720.0f; differs(s, "render height change repaints");
		// New decoded video frame / new GIF frame / image finished loading.
		s = base; s.sourceGeneration = 8;
		differs(s, "a new source frame repaints");

		// Parameters arrive via lerping and audio shaping, so the last bits can
		// wobble without changing a single pixel.
		s = base; s.scaleX += 1.0e-8f;
		expect(s.matches(base), "sub-threshold float noise does not repaint");
	}
}

int main()
{
	testNormalization();
	testOnce();
	testLoop();
	testPingPong();
	testRangeCapture();
	testTransformRank();
	testLegacyTransform();
	testRenderSignature();
	if (failures != 0)
	{
		std::cerr << failures << " media transport test(s) failed\n";
		return 1;
	}
	std::cout << "media transport tests passed\n";
	return 0;
}
