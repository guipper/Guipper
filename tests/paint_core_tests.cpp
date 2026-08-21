#include "../src/JPbox/jp_paint_doc.h"

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

	bool near(float a, float b, float tolerance = 0.0001f)
	{
		return std::abs(a - b) < tolerance;
	}

	JPPaintPoint pt(float x, float y, float width = 1.0f)
	{
		JPPaintPoint p;
		p.x = x;
		p.y = y;
		p.width = width;
		return p;
	}

	JPPaintStroke strokeOf(std::size_t pointCount)
	{
		JPPaintStroke stroke;
		for (std::size_t i = 0; i < pointCount; ++i)
		{
			stroke.points.push_back(pt((float)i * 0.01f, 0.5f));
		}
		return stroke;
	}

	// A document of `count` cels, every cel carrying one one-point stroke so
	// the cels are distinguishable and non-empty.
	JPPaintDocument docOf(int count)
	{
		JPPaintDocument doc;
		doc.frames.clear();
		for (int i = 0; i < count; ++i)
		{
			JPPaintFrame frame = jp_paint::makeFrame(doc);
			frame.layers[0].strokes.push_back(strokeOf(1));
			doc.frames.push_back(frame);
		}
		return doc;
	}

	// ------------------------------------------------------------------ time

	void testTicks()
	{
		JPPaintDocument doc = docOf(3);
		expect(jp_paint::tickCount(doc) == 3, "one tick per cel by default");

		doc.frames[1].hold = 4;
		expect(jp_paint::tickCount(doc) == 6, "holds add to the tick count");
		expect(jp_paint::frameAtTick(doc, 0) == 0, "tick 0 is the first cel");
		expect(jp_paint::frameAtTick(doc, 1) == 1, "a held cel starts on time");
		expect(jp_paint::frameAtTick(doc, 4) == 1, "a held cel occupies its whole hold");
		expect(jp_paint::frameAtTick(doc, 5) == 2, "the cel after a hold follows it");

		// A zero or negative hold is data, not a crash: it reads as 1.
		doc.frames[0].hold = 0;
		expect(jp_paint::tickCount(doc) == 6, "a zero hold counts as one tick");

		expect(jp_paint::frameAtTick(doc, -3) == 0, "negative ticks clamp to the first cel");
		expect(jp_paint::frameAtTick(doc, 99) == 2, "overrun ticks clamp to the last cel");
	}

	void testAdvanceLoop()
	{
		JPPaintDocument doc = docOf(4);
		JPMediaState playback;
		playback.loopMode = JPMediaLoopMode::Loop;
		playback.playing = true;
		playback.rate = 1.0f;
		float playhead = 0.0f;

		// 12fps, a quarter second per step: exactly three cels.
		expect(jp_paint::advance(doc, playback, playhead, 0.25f) == 3,
			"advance steps by fps * dt");
		// One more step runs past the end and wraps. Loop snaps to IN and
		// DISCARDS the overshoot rather than carrying it over - that is what
		// jp_media::applyBoundary does for video, and sharing it is the point.
		expect(jp_paint::advance(doc, playback, playhead, 0.25f) == 0,
			"Loop wraps back to the first cel");
		expect(near(playhead, 0.0f), "Loop snaps the playhead to IN");

		// A dt longer than the whole document must still land somewhere legal
		// rather than running away.
		playhead = 0.0f;
		const int cel = jp_paint::advance(doc, playback, playhead, 30.0f);
		expect(cel >= 0 && cel < 4, "an enormous dt still lands on a real cel");
	}

	void testAdvanceOnce()
	{
		JPPaintDocument doc = docOf(3);
		JPMediaState playback;
		playback.loopMode = JPMediaLoopMode::Once;
		playback.playing = true;
		playback.rate = 1.0f;
		float playhead = 0.0f;

		expect(jp_paint::advance(doc, playback, playhead, 10.0f) == 2,
			"Once parks on the last cel");
		expect(!playback.playing, "Once stops the transport at the end");

		// Stopped means stopped: further ticks must not move.
		const float parked = playhead;
		expect(jp_paint::advance(doc, playback, playhead, 1.0f) == 2 &&
			near(playhead, parked), "a stopped transport does not advance");
	}

	void testAdvancePingPong()
	{
		JPPaintDocument doc = docOf(4);
		JPMediaState playback;
		playback.loopMode = JPMediaLoopMode::PingPong;
		playback.playing = true;
		playback.rate = 1.0f;
		float playhead = 0.0f;

		jp_paint::advance(doc, playback, playhead, 0.5f);   // 6 ticks into a 4 tick doc
		expect(playback.reverse, "PingPong reverses at the end");
		expect(playhead >= 0.0f && playhead <= 4.0f,
			"PingPong reflects the overshoot back inside the range");

		// And back again at the near end.
		for (int i = 0; i < 20; ++i) jp_paint::advance(doc, playback, playhead, 0.1f);
		expect(playhead >= 0.0f && playhead <= 4.0f,
			"PingPong stays inside the range over many bounces");
		expect(playback.playing, "PingPong never stops on its own");
	}

	void testAdvanceRange()
	{
		// The inspector's IN/OUT handles trim the cel range. That comes from
		// applyBoundary for free, so it is worth pinning down.
		JPPaintDocument doc = docOf(10);
		JPMediaState playback;
		playback.loopMode = JPMediaLoopMode::Loop;
		playback.playing = true;
		playback.rate = 1.0f;
		playback.rangeIn = 0.2f;
		playback.rangeOut = 0.6f;
		float playhead = 0.0f;

		for (int i = 0; i < 40; ++i)
		{
			jp_paint::advance(doc, playback, playhead, 0.05f);
			expect(playhead >= 1.9f && playhead <= 6.1f,
				"a trimmed range confines the playhead");
			if (failures != 0) return;
		}
	}

	void testAdvanceEmpty()
	{
		JPPaintDocument doc;
		doc.frames.clear();
		JPMediaState playback;
		playback.playing = true;
		float playhead = 7.0f;
		expect(jp_paint::advance(doc, playback, playhead, 1.0f) == 0 &&
			near(playhead, 0.0f), "an empty document reports cel 0");
	}

	// ------------------------------------------------------------ quantizing

	void testQuantize()
	{
		// Round trip has to be accurate to well under a pixel at 4K.
		const float samples[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.123456f};
		for (float value : samples)
		{
			const float back = jp_paint::dequantizeCoord(
				jp_paint::quantizeCoord(value));
			expect(near(back, value, 1.0f / 4096.0f),
				"coordinate round trip is sub-pixel at 4K");
		}

		// Off-canvas geometry survives, which is the whole reason the encoded
		// range overshoots 0..1.
		const float outside = jp_paint::dequantizeCoord(
			jp_paint::quantizeCoord(-0.25f));
		expect(near(outside, -0.25f, 1.0f / 4096.0f),
			"a point off the canvas edge survives quantization");

		expect(near(jp_paint::dequantizeWidth(jp_paint::quantizeWidth(1.0f)),
			1.0f, 0.01f), "width round trip holds 1.0");
		expect(near(jp_paint::dequantizeWidth(jp_paint::quantizeWidth(1.75f)),
			1.75f, 0.01f), "width round trip holds a multiplier above 1");
	}

	void testPackRoundTrip()
	{
		std::vector<JPPaintPoint> points;
		points.push_back(pt(0.1f, 0.2f, 0.5f));
		points.push_back(pt(0.9f, 0.8f, 1.5f));
		points.push_back(pt(-0.3f, 1.2f, 1.0f));

		const std::string packed = jp_paint::packPoints(points);
		std::vector<JPPaintPoint> restored;
		expect(jp_paint::unpackPoints(packed, restored), "packed points unpack");
		expect(restored.size() == points.size(), "point count survives packing");
		for (std::size_t i = 0; i < restored.size() && i < points.size(); ++i)
		{
			expect(near(restored[i].x, points[i].x, 1.0f / 4096.0f) &&
				near(restored[i].y, points[i].y, 1.0f / 4096.0f) &&
				near(restored[i].width, points[i].width, 0.01f),
				"packed point survives the round trip");
		}

		// The size claim the schema rests on.
		expect(packed.size() < points.size() * 20,
			"packing stays under 20 bytes per point");

		std::vector<JPPaintPoint> empty;
		expect(jp_paint::unpackPoints("", empty) && empty.empty(),
			"an empty run unpacks to no points");
		expect(jp_paint::unpackPoints("  \n ", empty) && empty.empty(),
			"whitespace only unpacks to no points");
	}

	void testPackMalformed()
	{
		std::vector<JPPaintPoint> out;
		expect(!jp_paint::unpackPoints("12,34", out) && out.empty(),
			"a truncated triple is rejected");
		expect(!jp_paint::unpackPoints("12,34,56 7,8", out) && out.empty(),
			"a trailing partial triple rejects the whole run");
		expect(!jp_paint::unpackPoints("nonsense", out) && out.empty(),
			"garbage is rejected");
		expect(!jp_paint::unpackPoints("12,,56", out) && out.empty(),
			"a missing field is rejected");
	}

	// ----------------------------------------------------------- simplifying

	void testSimplify()
	{
		// A dead straight run of samples collapses to its endpoints.
		std::vector<JPPaintPoint> line;
		for (int i = 0; i <= 100; ++i) line.push_back(pt((float)i * 0.01f, 0.5f));
		jp_paint::simplify(line, 0.001f);
		expect(line.size() == 2, "a straight run collapses to two points");
		expect(near(line.front().x, 0.0f) && near(line.back().x, 1.0f),
			"simplification preserves the endpoints");

		// A real corner is not allowed to disappear.
		std::vector<JPPaintPoint> corner;
		for (int i = 0; i <= 50; ++i) corner.push_back(pt((float)i * 0.01f, 0.5f));
		for (int i = 1; i <= 50; ++i) corner.push_back(pt(0.5f, 0.5f + (float)i * 0.01f));
		jp_paint::simplify(corner, 0.001f);
		expect(corner.size() == 3, "a corner survives simplification");
		expect(near(corner[1].x, 0.5f) && near(corner[1].y, 0.5f),
			"the retained point is the corner itself");

		// Deviations under epsilon are the ones we are paying to remove.
		std::vector<JPPaintPoint> jitter;
		for (int i = 0; i <= 100; ++i)
		{
			jitter.push_back(pt((float)i * 0.01f, 0.5f + (i % 2 ? 0.0005f : -0.0005f)));
		}
		const std::size_t before = jitter.size();
		jp_paint::simplify(jitter, 0.005f);
		expect(jitter.size() < before / 4,
			"sub-epsilon jitter is removed");

		// Degenerate inputs must be no-ops rather than crashes.
		std::vector<JPPaintPoint> tiny;
		tiny.push_back(pt(0.0f, 0.0f));
		tiny.push_back(pt(1.0f, 1.0f));
		jp_paint::simplify(tiny, 0.1f);
		expect(tiny.size() == 2, "a two point stroke is left alone");

		std::vector<JPPaintPoint> none;
		jp_paint::simplify(none, 0.1f);
		expect(none.empty(), "an empty stroke is left alone");

		// A hairpin: both endpoints coincide, so an infinite-line distance test
		// would measure zero and delete the whole excursion.
		std::vector<JPPaintPoint> hairpin;
		hairpin.push_back(pt(0.0f, 0.0f));
		hairpin.push_back(pt(0.5f, 0.0f));
		hairpin.push_back(pt(0.0f, 0.0f));
		jp_paint::simplify(hairpin, 0.01f);
		expect(hairpin.size() == 3, "a hairpin excursion is not flattened away");
	}

	// ------------------------------------------------------------------- hex

	void testHexParsing()
	{
		float r = 0, g = 0, b = 0, a = 0;

		expect(jp_paint::parseHexColor("#FF8000", r, g, b, a), "#RRGGBB parses");
		expect(near(r, 1.0f) && near(g, 128.0f / 255.0f) && near(b, 0.0f) &&
			near(a, 1.0f), "#RRGGBB is opaque and correct");

		// A pasted web colour usually has no hash, and case should not matter.
		expect(jp_paint::parseHexColor("ff8000", r, g, b, a) && near(r, 1.0f),
			"a missing hash is accepted");
		expect(jp_paint::parseHexColor("#Ff8000", r, g, b, a) && near(r, 1.0f),
			"mixed case is accepted");
		expect(jp_paint::parseHexColor(" #FF8000 ", r, g, b, a) && near(r, 1.0f),
			"surrounding space is ignored");

		expect(jp_paint::parseHexColor("#F80", r, g, b, a), "#RGB shorthand parses");
		expect(near(r, 1.0f) && near(g, 136.0f / 255.0f) && near(b, 0.0f),
			"#RGB expands each digit, so #F80 is #FF8800");

		expect(jp_paint::parseHexColor("#FF800080", r, g, b, a),
			"#RRGGBBAA parses");
		expect(near(a, 128.0f / 255.0f), "the eight digit form carries alpha");

		// Anything else must leave the colour alone rather than half-apply.
		r = g = b = a = 0.5f;
		expect(!jp_paint::parseHexColor("#FF80", r, g, b, a),
			"a four digit run is rejected");
		expect(!jp_paint::parseHexColor("#GG8000", r, g, b, a),
			"a non-hex digit is rejected");
		expect(!jp_paint::parseHexColor("", r, g, b, a), "empty is rejected");
		expect(!jp_paint::parseHexColor("#", r, g, b, a),
			"a bare hash is rejected");
		expect(near(r, 0.5f) && near(a, 0.5f),
			"a rejected string leaves the colour untouched");
	}

	void testHexFormatting()
	{
		expect(jp_paint::formatHexColor(1.0f, 0.5019f, 0.0f, 1.0f) == "#FF8000",
			"an opaque colour formats as six digits");
		expect(jp_paint::formatHexColor(1.0f, 0.5019f, 0.0f, 0.5019f) ==
			"#FF800080", "a translucent colour formats as eight");
		expect(jp_paint::formatHexColor(0.0f, 0.0f, 0.0f, 1.0f) == "#000000",
			"black formats");
		expect(jp_paint::formatHexColor(2.0f, -1.0f, 0.0f, 1.0f) == "#FF0000",
			"out of range channels clamp rather than wrap");

		// Round trip, which is what the field actually does every time it is shown.
		float r = 0, g = 0, b = 0, a = 0;
		const std::string text =
			jp_paint::formatHexColor(0.2f, 0.4f, 0.6f, 0.8f);
		expect(jp_paint::parseHexColor(text, r, g, b, a),
			"a formatted colour parses back");
		expect(near(r, 0.2f, 0.005f) && near(g, 0.4f, 0.005f) &&
			near(b, 0.6f, 0.005f) && near(a, 0.8f, 0.005f),
			"the round trip is accurate to one part in 255");
	}

	// ----------------------------------------------------------------- fills

	// A tiny RGBA canvas helper: opaque white background, so a drawn "stroke" is
	// anything set to another colour.
	struct Canvas
	{
		int w = 0, h = 0;
		std::vector<std::uint8_t> px;
		Canvas(int width, int height) : w(width), h(height),
			px((std::size_t)width * height * 4, 255) {}
		void set(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b,
			std::uint8_t a)
		{
			const std::size_t i = ((std::size_t)y * w + x) * 4;
			px[i] = r; px[i + 1] = g; px[i + 2] = b; px[i + 3] = a;
		}
	};

	void testFloodFillBasics()
	{
		std::vector<std::uint8_t> mask;

		// A uniform canvas fills entirely.
		Canvas open(8, 8);
		expect(jp_paint::floodFill(open.px.data(), 8, 8, 0, 0, 8, mask) == 64,
			"a uniform region fills completely");
		expect(mask.size() == 64 && mask[0] == 255 && mask[63] == 255,
			"the mask covers the whole canvas");

		// Out of bounds and degenerate inputs fill nothing rather than crashing.
		expect(jp_paint::floodFill(open.px.data(), 8, 8, -1, 0, 8, mask) == 0,
			"a negative seed fills nothing");
		expect(jp_paint::floodFill(open.px.data(), 8, 8, 8, 0, 8, mask) == 0,
			"a seed past the right edge fills nothing");
		expect(jp_paint::floodFill(open.px.data(), 8, 8, 0, 8, 8, mask) == 0,
			"a seed past the bottom edge fills nothing");
		expect(jp_paint::floodFill(nullptr, 8, 8, 0, 0, 8, mask) == 0,
			"a null buffer fills nothing");
		expect(jp_paint::floodFill(open.px.data(), 0, 0, 0, 0, 8, mask) == 0,
			"a zero sized canvas fills nothing");
	}

	void testFloodFillBoundary()
	{
		std::vector<std::uint8_t> mask;

		// A vertical black wall down the middle splits the canvas in two.
		Canvas split(9, 5);
		for (int y = 0; y < 5; ++y) split.set(4, y, 0, 0, 0, 255);
		const std::size_t leftSide =
			jp_paint::floodFill(split.px.data(), 9, 5, 0, 0, 8, mask);
		expect(leftSide == 20, "a wall confines the fill to its own side");
		expect(mask[4] == 0, "the wall itself is not filled");
		expect(mask[5] == 0, "the far side is not filled");

		// Seeding the far side fills only the far side.
		expect(jp_paint::floodFill(split.px.data(), 9, 5, 8, 0, 8, mask) == 20,
			"the other side fills to the same size");
		expect(mask[0] == 0, "seeding right does not reach left");

		// A gap in the wall makes it one region again.
		split.set(4, 2, 255, 255, 255, 255);
		expect(jp_paint::floodFill(split.px.data(), 9, 5, 0, 0, 8, mask) == 41,
			"a one pixel gap leaks through and joins both sides");
	}

	void testFloodFillHoles()
	{
		// THE case that rules out contour tracing: filling the background around
		// a closed shape has to leave the shape as a hole. A traced polygon would
		// need even-odd subpaths to express that; a pixel mask just does.
		std::vector<std::uint8_t> mask;
		Canvas ring(7, 7);
		for (int i = 2; i <= 4; ++i)
		{
			ring.set(i, 2, 0, 0, 0, 255);
			ring.set(i, 4, 0, 0, 0, 255);
			ring.set(2, i, 0, 0, 0, 255);
			ring.set(4, i, 0, 0, 0, 255);
		}
		// 49 total - 8 ring pixels - 1 enclosed centre = 40.
		expect(jp_paint::floodFill(ring.px.data(), 7, 7, 0, 0, 8, mask) == 40,
			"the background fills around a closed shape");
		expect(mask[3 * 7 + 3] == 0,
			"the pixel enclosed by the shape stays a hole");

		// And the enclosed pixel is its own region.
		expect(jp_paint::floodFill(ring.px.data(), 7, 7, 3, 3, 8, mask) == 1,
			"the enclosed pixel fills alone");
	}

	void testFloodFillTolerance()
	{
		std::vector<std::uint8_t> mask;
		Canvas shaded(6, 1);
		// A gentle ramp: within tolerance it is one region, below it is not.
		for (int x = 0; x < 6; ++x)
		{
			const std::uint8_t v = (std::uint8_t)(255 - x * 10);
			shaded.set(x, 0, v, v, v, 255);
		}
		expect(jp_paint::floodFill(shaded.px.data(), 6, 1, 0, 0, 0, mask) == 1,
			"zero tolerance stops at the first different pixel");
		expect(jp_paint::floodFill(shaded.px.data(), 6, 1, 0, 0, 255, mask) == 6,
			"full tolerance crosses the whole ramp");
		// Tolerance is measured against the SEED, not the neighbour, so a ramp
		// does not creep indefinitely.
		expect(jp_paint::floodFill(shaded.px.data(), 6, 1, 0, 0, 25, mask) == 3,
			"tolerance is measured against the seed pixel");
	}

	void testFloodFillAlpha()
	{
		// Alpha is part of the comparison: an eraser leaves transparent pixels
		// whose RGB may still match, and a fill must stop at them.
		std::vector<std::uint8_t> mask;
		Canvas erased(5, 1);
		erased.set(2, 0, 255, 255, 255, 0);
		expect(jp_paint::floodFill(erased.px.data(), 5, 1, 0, 0, 8, mask) == 2,
			"a transparent pixel of the same colour still bounds the fill");
	}

	// ------------------------------------------------------------------ undo

	void testEditRoundTrip()
	{
		// Every kind, applied then reverted, must restore the document exactly.
		JPPaintDocument doc = docOf(3);
		doc.frames[1].hold = 5;

		auto framesEqual = [](const JPPaintDocument &a, const JPPaintDocument &b) {
			if (a.frames.size() != b.frames.size()) return false;
			for (std::size_t i = 0; i < a.frames.size(); ++i)
			{
				if (a.frames[i].id != b.frames[i].id) return false;
				if (a.frames[i].hold != b.frames[i].hold) return false;
				if (a.frames[i].layers[0].strokes.size() != b.frames[i].layers[0].strokes.size()) return false;
			}
			return true;
		};

		const JPPaintDocument original = doc;

		JPPaintEdit add;
		add.kind = JPPaintEdit::AddStroke;
		add.frameIndex = 0;
		add.strokeIndex = 1;
		add.stroke = strokeOf(4);
		expect(jp_paint::applyEdit(doc, add), "AddStroke applies");
		expect(doc.frames[0].layers[0].strokes.size() == 2, "AddStroke inserts");
		expect(jp_paint::revertEdit(doc, add), "AddStroke reverts");
		expect(framesEqual(doc, original), "AddStroke round trips");

		JPPaintEdit clear;
		clear.kind = JPPaintEdit::ClearLayer;
		clear.frameIndex = 2;
		clear.layer.sharedStrokes = doc.frames[2].layers[0].strokes;  // captured before
		expect(jp_paint::applyEdit(doc, clear), "ClearFrame applies");
		expect(doc.frames[2].layers[0].strokes.empty(), "ClearFrame empties the cel");
		expect(jp_paint::revertEdit(doc, clear), "ClearFrame reverts");
		expect(framesEqual(doc, original), "ClearFrame round trips");

		JPPaintEdit addFrame;
		addFrame.kind = JPPaintEdit::AddFrame;
		addFrame.frameIndex = 1;
		addFrame.frame = jp_paint::makeFrame(doc);
		expect(jp_paint::applyEdit(doc, addFrame), "AddFrame applies");
		expect(doc.frames.size() == 4, "AddFrame inserts a cel");
		expect(jp_paint::revertEdit(doc, addFrame), "AddFrame reverts");
		expect(framesEqual(doc, original), "AddFrame round trips");

		JPPaintEdit deleteFrame;
		deleteFrame.kind = JPPaintEdit::DeleteFrame;
		deleteFrame.frameIndex = 1;
		deleteFrame.frame = doc.frames[1];
		expect(jp_paint::applyEdit(doc, deleteFrame), "DeleteFrame applies");
		expect(doc.frames.size() == 2, "DeleteFrame removes a cel");
		expect(jp_paint::revertEdit(doc, deleteFrame), "DeleteFrame reverts");
		expect(framesEqual(doc, original), "DeleteFrame round trips, hold included");

		JPPaintEdit move;
		move.kind = JPPaintEdit::MoveFrame;
		move.fromIndex = 0;
		move.toIndex = 2;
		expect(jp_paint::applyEdit(doc, move), "MoveFrame applies");
		expect(doc.frames[2].id == original.frames[0].id, "MoveFrame reorders");
		expect(jp_paint::revertEdit(doc, move), "MoveFrame reverts");
		expect(framesEqual(doc, original), "MoveFrame round trips");

		JPPaintEdit hold;
		hold.kind = JPPaintEdit::SetHold;
		hold.frameIndex = 1;
		hold.previousValue = doc.frames[1].hold;
		hold.intValue = 9;
		expect(jp_paint::applyEdit(doc, hold), "SetHold applies");
		expect(doc.frames[1].hold == 9, "SetHold sets the hold");
		expect(jp_paint::revertEdit(doc, hold), "SetHold reverts");
		expect(framesEqual(doc, original), "SetHold round trips");
	}

	void testLastFrameSurvives()
	{
		JPPaintDocument doc = docOf(1);
		JPPaintEdit deleteFrame;
		deleteFrame.kind = JPPaintEdit::DeleteFrame;
		deleteFrame.frameIndex = 0;
		deleteFrame.frame = doc.frames[0];
		expect(!jp_paint::applyEdit(doc, deleteFrame),
			"the last cel cannot be deleted");
		expect(doc.frames.size() == 1, "the document keeps at least one cel");
	}

	void testEditsRejectBadIndices()
	{
		JPPaintDocument doc = docOf(2);
		JPPaintEdit edit;
		edit.kind = JPPaintEdit::AddStroke;
		edit.frameIndex = 7;
		expect(!jp_paint::applyEdit(doc, edit), "an out of range cel is refused");
		edit.frameIndex = -1;
		expect(!jp_paint::applyEdit(doc, edit), "a negative cel is refused");
		edit.frameIndex = 0;
		edit.strokeIndex = 12;
		expect(!jp_paint::applyEdit(doc, edit), "an out of range stroke slot is refused");
	}

	void testRevisionBumps()
	{
		JPPaintDocument doc = docOf(2);
		const unsigned long long before = doc.frames[0].revision;
		JPPaintEdit edit;
		edit.kind = JPPaintEdit::AddStroke;
		edit.frameIndex = 0;
		edit.stroke = strokeOf(3);
		jp_paint::applyEdit(doc, edit);
		expect(doc.frames[0].revision != before,
			"editing a cel bumps its revision so the raster cache misses");
		expect(doc.frames[1].revision == 0,
			"editing one cel leaves another cel's cached raster valid");
	}

	// ---------------------------------------------------------------- layers

	// THE invariant: every cel carries one JPPaintLayer per document layer, in
	// the same order. Everything that composites, saves or undoes relies on it.
	bool arityHolds(const JPPaintDocument &doc)
	{
		for (const JPPaintFrame &frame : doc.frames)
		{
			if (frame.layers.size() != doc.layers.size()) return false;
		}
		return true;
	}

	void testLayerStructure()
	{
		JPPaintDocument doc = docOf(3);
		expect(doc.layers.size() == 1, "a document starts with one layer");
		expect(arityHolds(doc), "a fresh document satisfies the arity invariant");

		JPPaintEdit add;
		add.kind = JPPaintEdit::AddLayer;
		add.layerIndex = 1;
		add.layer = jp_paint::makeLayer(doc, "ink");
		expect(jp_paint::applyEdit(doc, add), "AddLayer applies");
		expect(doc.layers.size() == 2, "the layer stack grew");
		expect(arityHolds(doc), "adding a layer gives every cel a slot");

		// Draw on the new layer of the middle cel only.
		JPPaintEdit stroke;
		stroke.kind = JPPaintEdit::AddStroke;
		stroke.frameIndex = 1;
		stroke.layerIndex = 1;
		stroke.stroke = strokeOf(4);
		expect(jp_paint::applyEdit(doc, stroke), "AddStroke reaches a layer");
		expect(doc.frames[1].layers[1].strokes.size() == 1,
			"the stroke landed on the addressed layer");
		expect(doc.frames[0].layers[1].strokes.empty(),
			"and on that cel only");
		expect(doc.frames[1].layers[0].strokes.size() == 1,
			"the layer below is untouched");

		expect(jp_paint::revertEdit(doc, add) == false ||
			doc.layers.size() == 1, "AddLayer reverts to one layer");
		expect(arityHolds(doc), "reverting a layer add keeps the invariant");
	}

	void testLayerDeleteCarriesStrokes()
	{
		JPPaintDocument doc = docOf(3);
		JPPaintEdit add;
		add.kind = JPPaintEdit::AddLayer;
		add.layerIndex = 1;
		add.layer = jp_paint::makeLayer(doc, "ink");
		jp_paint::applyEdit(doc, add);
		for (int cel = 0; cel < 3; ++cel)
		{
			JPPaintEdit stroke;
			stroke.kind = JPPaintEdit::AddStroke;
			stroke.frameIndex = cel;
			stroke.layerIndex = 1;
			stroke.stroke = strokeOf(2 + cel);
			jp_paint::applyEdit(doc, stroke);
		}

		// The payload has to be captured BEFORE the delete - it is the inverse.
		JPPaintEdit del;
		del.kind = JPPaintEdit::DeleteLayer;
		del.layerIndex = 1;
		del.layer = doc.layers[1];
		for (const JPPaintFrame &frame : doc.frames)
			del.layerCels.push_back(frame.layers[1]);
		expect(jp_paint::applyEdit(doc, del), "DeleteLayer applies");
		expect(doc.layers.size() == 1 && arityHolds(doc),
			"the layer is gone from the stack and from every cel");

		expect(jp_paint::revertEdit(doc, del), "DeleteLayer reverts");
		expect(doc.layers.size() == 2 && arityHolds(doc), "the layer is back");
		// The whole point: undo restores the DRAWING, not an empty layer.
		expect(doc.frames[0].layers[1].strokes.size() == 1 &&
			doc.frames[2].layers[1].strokes.size() == 1,
			"undo restored the strokes on every cel");
		expect(doc.frames[2].layers[1].strokes[0].points.size() == 4,
			"and restored the right strokes to the right cels");
		expect(doc.layers[1].id == del.layer.id, "the layer kept its identity");
	}

	void testLastLayerSurvives()
	{
		JPPaintDocument doc = docOf(2);
		JPPaintEdit del;
		del.kind = JPPaintEdit::DeleteLayer;
		del.layerIndex = 0;
		del.layer = doc.layers[0];
		expect(!jp_paint::applyEdit(doc, del), "the last layer cannot be deleted");
		expect(doc.layers.size() == 1, "the stack keeps at least one layer");
	}

	void testLayerMove()
	{
		JPPaintDocument doc = docOf(2);
		JPPaintEdit add;
		add.kind = JPPaintEdit::AddLayer;
		add.layerIndex = 1;
		add.layer = jp_paint::makeLayer(doc, "top");
		jp_paint::applyEdit(doc, add);
		const int bottomId = doc.layers[0].id;
		const int topId = doc.layers[1].id;

		// Mark the cel slots so a reorder that moves the info but not the strokes
		// would be caught.
		JPPaintEdit stroke;
		stroke.kind = JPPaintEdit::AddStroke;
		stroke.frameIndex = 0;
		stroke.layerIndex = 1;
		stroke.stroke = strokeOf(9);
		jp_paint::applyEdit(doc, stroke);

		JPPaintEdit move;
		move.kind = JPPaintEdit::MoveLayer;
		move.fromIndex = 1;
		move.toIndex = 0;
		expect(jp_paint::applyEdit(doc, move), "MoveLayer applies");
		expect(doc.layers[0].id == topId && doc.layers[1].id == bottomId,
			"the stack reordered");
		expect(doc.frames[0].layers[0].strokes.size() == 1 &&
			doc.frames[0].layers[0].strokes[0].points.size() == 9,
			"the per-cel strokes moved with their layer");

		expect(jp_paint::revertEdit(doc, move), "MoveLayer reverts");
		expect(doc.layers[0].id == bottomId && doc.layers[1].id == topId,
			"the order is restored");
		expect(doc.frames[0].layers[1].strokes.size() == 1,
			"and so are the strokes");
		expect(arityHolds(doc), "reordering keeps the invariant");
	}

	void testLayerDuplicateAndMergePayloads()
	{
		JPPaintDocument doc = docOf(2);
		JPPaintEdit duplicate;
		duplicate.kind = JPPaintEdit::AddLayer;
		duplicate.layerIndex = 1;
		duplicate.layer = jp_paint::makeLayer(doc, "copy");
		for (const JPPaintFrame &frame : doc.frames)
			duplicate.layerCels.push_back(frame.layers[0]);
		expect(jp_paint::applyEdit(doc, duplicate),
			"AddLayer accepts duplicated cel payloads");
		expect(doc.frames[0].layers[1].strokes.size() == 1 &&
			doc.frames[1].layers[1].strokes.size() == 1,
			"a duplicated layer copies every cel");

		JPPaintEdit merge;
		merge.kind = JPPaintEdit::MergeLayerDown;
		merge.layerIndex = 1;
		merge.previousLayer = doc.layers[0];
		merge.layer = doc.layers[1];
		merge.mergedLayer = doc.layers[0];
		for (const JPPaintFrame &frame : doc.frames)
		{
			merge.previousLayerCels.push_back(frame.layers[0]);
			merge.layerCels.push_back(frame.layers[1]);
			JPPaintLayer result = frame.layers[0];
			result.strokes.insert(result.strokes.end(),
				frame.layers[1].strokes.begin(), frame.layers[1].strokes.end());
			merge.mergedLayerCels.push_back(result);
		}
		expect(jp_paint::applyEdit(doc, merge), "MergeLayerDown applies");
		expect(doc.layers.size() == 1 && arityHolds(doc) &&
			doc.frames[0].layers[0].strokes.size() == 2,
			"merge replaces both layers with their combined strokes");
		expect(jp_paint::revertEdit(doc, merge), "MergeLayerDown reverts");
		expect(doc.layers.size() == 2 && arityHolds(doc) &&
			doc.frames[0].layers[0].strokes.size() == 1 &&
			doc.frames[0].layers[1].strokes.size() == 1,
			"undo restores both original layers and their cel payloads");
	}

	void testBackgroundLayer()
	{
		JPPaintDocument doc = docOf(3);

		// A background layer's strokes are SHARED, so one write shows on every
		// cel - that is the whole feature.
		JPPaintEdit props;
		props.kind = JPPaintEdit::SetLayerProps;
		props.layerIndex = 0;
		props.previousLayer = doc.layers[0];
		props.layer = doc.layers[0];
		props.layer.background = true;
		props.layer.locked = true;
		props.layer.blendMode = 2;
		expect(jp_paint::applyEdit(doc, props), "SetLayerProps applies");
		expect(doc.layers[0].background, "the layer is now a background");
		expect(doc.layers[0].locked && doc.layers[0].blendMode == 2,
			"lock and blend mode are layer properties");

		JPPaintEdit stroke;
		stroke.kind = JPPaintEdit::AddStroke;
		stroke.frameIndex = 1;
		stroke.layerIndex = 0;
		stroke.stroke = strokeOf(3);
		jp_paint::applyEdit(doc, stroke);
		expect(doc.layers[0].sharedStrokes.size() == 1,
			"a background stroke goes to the shared list");
		for (int cel = 0; cel < 3; ++cel)
		{
			const std::vector<JPPaintStroke> *list =
				jp_paint::strokeListFor(doc, cel, 0);
			expect(list != nullptr && list->size() == 1,
				"every cel sees the shared stroke");
		}

		// Editing shared strokes invalidates EVERY cel's cached raster, not one.
		const unsigned long long before = doc.frames[2].revision;
		JPPaintEdit second;
		second.kind = JPPaintEdit::AddStroke;
		second.frameIndex = 0;
		second.layerIndex = 0;
		second.stroke = strokeOf(2);
		jp_paint::applyEdit(doc, second);
		expect(doc.frames[2].revision != before,
			"a shared edit bumps a cel it was not addressed to");

		// Undo is LIFO, so the strokes come back off before the property change
		// does. Reverting the property edit FIRST would restore its snapshot of
		// sharedStrokes and wipe strokes a later edit had added - which the ring
		// can never ask for, and which is why this walks backwards.
		expect(jp_paint::revertEdit(doc, second), "the second stroke reverts");
		expect(jp_paint::revertEdit(doc, stroke), "the first stroke reverts");
		expect(doc.layers[0].sharedStrokes.empty(),
			"reverting both strokes empties the shared list");

		expect(jp_paint::revertEdit(doc, props), "SetLayerProps reverts");
		expect(!doc.layers[0].background, "the background flag is off again");
		// The per-cel strokes were never touched by any of it - that is what makes
		// the flag reversible rather than destructive.
		expect(doc.frames[1].layers[0].strokes.size() == 1,
			"the per-cel strokes are back in play, untouched");
	}

	void testLayerArityAcrossCelEdits()
	{
		// A cel added while two layers exist must arrive with two slots.
		JPPaintDocument doc = docOf(1);
		JPPaintEdit add;
		add.kind = JPPaintEdit::AddLayer;
		add.layerIndex = 1;
		add.layer = jp_paint::makeLayer(doc, "two");
		jp_paint::applyEdit(doc, add);

		JPPaintEdit cel;
		cel.kind = JPPaintEdit::AddFrame;
		cel.frameIndex = 1;
		cel.frame = jp_paint::makeFrame(doc);   // default-constructed: ONE layer
		expect(jp_paint::applyEdit(doc, cel), "AddFrame applies");
		expect(arityHolds(doc),
			"a cel built with fewer layers is squared up on insert");

		JPPaintEdit del;
		del.kind = JPPaintEdit::DeleteFrame;
		del.frameIndex = 1;
		del.frame = doc.frames[1];
		jp_paint::applyEdit(doc, del);
		jp_paint::revertEdit(doc, del);
		expect(arityHolds(doc), "restoring a deleted cel keeps the invariant");
	}

	void testUndoRing()
	{
		JPPaintDocument doc = docOf(1);
		JPPaintUndoRing ring;

		expect(!ring.canUndo() && !ring.canRedo(), "a fresh ring is empty");
		expect(!ring.undo(doc), "undo on an empty ring is a no-op");
		expect(!ring.redo(doc), "redo on an empty ring is a no-op");

		for (int i = 0; i < 3; ++i)
		{
			JPPaintEdit edit;
			edit.kind = JPPaintEdit::AddStroke;
			edit.frameIndex = 0;
			edit.stroke = strokeOf(2);
			jp_paint::applyEdit(doc, edit);
			ring.push(edit);
		}
		expect(doc.frames[0].layers[0].strokes.size() == 4, "three strokes on top of one");

		expect(ring.undo(doc) && ring.undo(doc), "two undos step back");
		expect(doc.frames[0].layers[0].strokes.size() == 2, "undo removes strokes");
		expect(ring.canRedo(), "undone edits are redoable");

		expect(ring.redo(doc), "redo steps forward");
		expect(doc.frames[0].layers[0].strokes.size() == 3, "redo restores a stroke");

		// Branching: a new edit after an undo drops the redo tail.
		expect(ring.undo(doc), "step back to branch");
		JPPaintEdit branch;
		branch.kind = JPPaintEdit::AddStroke;
		branch.frameIndex = 0;
		branch.stroke = strokeOf(7);
		jp_paint::applyEdit(doc, branch);
		ring.push(branch);
		expect(!ring.canRedo(), "a new edit truncates the redo tail");
	}

	void testUndoEviction()
	{
		JPPaintDocument doc = docOf(1);
		JPPaintUndoRing ring;
		for (std::size_t i = 0; i < JPPaintUndoRing::kMaxEntries + 20; ++i)
		{
			JPPaintEdit edit;
			edit.kind = JPPaintEdit::AddStroke;
			edit.frameIndex = 0;
			edit.stroke = strokeOf(1);
			jp_paint::applyEdit(doc, edit);
			ring.push(edit);
		}
		expect(ring.size() == JPPaintUndoRing::kMaxEntries,
			"the ring is bounded by entry count");
		expect(ring.canUndo(), "an evicted ring still undoes its recent entries");

		// The point budget is the second, independent bound.
		JPPaintUndoRing fat;
		JPPaintDocument fatDoc = docOf(1);
		for (int i = 0; i < 10; ++i)
		{
			JPPaintEdit edit;
			edit.kind = JPPaintEdit::AddStroke;
			edit.frameIndex = 0;
			edit.stroke = strokeOf(400000);
			jp_paint::applyEdit(fatDoc, edit);
			fat.push(edit);
		}
		expect(fat.storedPoints() <= JPPaintUndoRing::kMaxPoints,
			"the ring is bounded by stored points");
		expect(fat.size() < 10, "huge strokes evict earlier than the entry cap");
	}

	void testClipPayloadAccounting()
	{
		JPPaintEdit edit;
		edit.stroke = strokeOf(3);
		JPPaintClip clip;
		clip.inverted = true;
		clip.points.push_back(pt(0.1f, 0.1f));
		clip.points.push_back(pt(0.9f, 0.1f));
		clip.points.push_back(pt(0.5f, 0.9f));
		edit.stroke.clips.push_back(clip);
		expect(jp_paint::pointCount(edit) == 6,
			"undo accounting includes non-destructive clip points");
	}

	void testComplementarySelectionCollapse()
	{
		JPPaintStroke outside = strokeOf(3);
		JPPaintClip clip;
		clip.inverted = true;
		clip.points = {pt(0.1f, 0.1f), pt(0.9f, 0.1f), pt(0.5f, 0.9f)};
		outside.clips.push_back(clip);

		JPPaintStroke inside = outside;
		inside.clips.back().inverted = false;
		std::vector<JPPaintStroke> collapsed =
			jp_paint::collapseComplementaryStrokes({outside, inside});
		expect(collapsed.size() == 1 && collapsed[0].clips.empty(),
			"an untouched selection compacts back to its source stroke");

		inside.points[0].x += 0.1f;
		collapsed = jp_paint::collapseComplementaryStrokes({outside, inside});
		expect(collapsed.size() == 2,
			"a moved selection remains split from its source stroke");

		inside = outside;
		inside.clips.back().inverted = false;
		inside.hardness = 0.5f;
		collapsed = jp_paint::collapseComplementaryStrokes({outside, inside});
		expect(collapsed.size() == 2,
			"complementary clips with different hardness never collapse");
		expect(JPPaintStroke().hardness == 1.0f,
			"legacy strokes default to a fully hard edge");
	}

	void testNestedSelectionCollapse()
	{
		JPPaintStroke base = strokeOf(3);
		JPPaintClip first;
		first.points = {pt(0.1f, 0.1f), pt(0.4f, 0.1f), pt(0.2f, 0.4f)};
		JPPaintClip second;
		second.points = {pt(0.6f, 0.6f), pt(0.9f, 0.6f), pt(0.8f, 0.9f)};

		JPPaintStroke outsideBoth = base;
		first.inverted = true;
		second.inverted = true;
		outsideBoth.clips = {first, second};
		JPPaintStroke firstOutsideSecondInside = outsideBoth;
		firstOutsideSecondInside.clips.back().inverted = false;
		JPPaintStroke firstInside = base;
		first.inverted = false;
		firstInside.clips = {first};

		const std::vector<JPPaintStroke> collapsed =
			jp_paint::collapseComplementaryStrokes({outsideBoth,
				firstOutsideSecondInside, firstInside});
		expect(collapsed.size() == 1 && collapsed[0].clips.empty(),
			"nested untouched add/subtract regions render as the source stroke");
	}

	void testReplaceStrokesUndoRedo()
	{
		JPPaintDocument doc = docOf(1);
		const std::vector<JPPaintStroke> original =
			doc.frames[0].layers[0].strokes;
		std::vector<JPPaintStroke> pasted = original;
		pasted.push_back(strokeOf(4));

		JPPaintEdit edit;
		edit.kind = JPPaintEdit::ReplaceStrokes;
		edit.frameIndex = 0;
		edit.layerIndex = 0;
		edit.previousLayer.sharedStrokes = original;
		edit.layer.sharedStrokes = pasted;
		expect(jp_paint::applyEdit(doc, edit) &&
			doc.frames[0].layers[0].strokes.size() == 2,
			"a clipboard-style replacement appends its payload");
		expect(jp_paint::revertEdit(doc, edit) &&
			doc.frames[0].layers[0].strokes.size() == 1,
			"undo removes a pasted payload in one edit");
		expect(jp_paint::applyEdit(doc, edit) &&
			doc.frames[0].layers[0].strokes[1].points.size() == 4,
			"redo restores the complete pasted payload");
	}

	void testUndoAcrossFrameEdits()
	{
		// Undoing a cel delete has to bring the cel's strokes back with it.
		JPPaintDocument doc = docOf(3);
		doc.frames[1].layers[0].strokes.push_back(strokeOf(5));
		const int keptId = doc.frames[1].id;

		JPPaintUndoRing ring;
		JPPaintEdit edit;
		edit.kind = JPPaintEdit::DeleteFrame;
		edit.frameIndex = 1;
		edit.frame = doc.frames[1];     // payload captured BEFORE applying
		jp_paint::applyEdit(doc, edit);
		ring.push(edit);
		expect(doc.frames.size() == 2, "the cel is gone");

		expect(ring.undo(doc), "undo restores the cel");
		expect(doc.frames.size() == 3 && doc.frames[1].id == keptId,
			"the restored cel keeps its identity");
		expect(doc.frames[1].layers[0].strokes.size() == 2,
			"the restored cel keeps its strokes");
	}

	// ------------------------------------------------------- clip collapse

	JPPaintClip squareClip(float x0, float y0, float x1, float y1,
		bool inverted)
	{
		JPPaintClip clip;
		clip.inverted = inverted;
		clip.points.push_back(pt(x0, y0));
		clip.points.push_back(pt(x1, y0));
		clip.points.push_back(pt(x1, y1));
		clip.points.push_back(pt(x0, y1));
		return clip;
	}

	void testClipsKeepPoint()
	{
		std::vector<JPPaintClip> clips;
		clips.push_back(squareClip(0.2f, 0.2f, 0.8f, 0.8f, false));
		expect(jp_paint::clipsKeepPoint(clips, 0.5f, 0.5f),
			"a normal clip keeps its interior");
		expect(!jp_paint::clipsKeepPoint(clips, 0.1f, 0.5f),
			"a normal clip drops its exterior");
		clips[0].inverted = true;
		expect(!jp_paint::clipsKeepPoint(clips, 0.5f, 0.5f),
			"an inverted clip drops its interior");
		expect(jp_paint::clipsKeepPoint(clips, 0.1f, 0.5f),
			"an inverted clip keeps its exterior");

		// Intersected in order: the two halves of one selection keep nothing in
		// common, which is exactly why a pair of them is invisible.
		clips.push_back(squareClip(0.2f, 0.2f, 0.8f, 0.8f, false));
		expect(!jp_paint::clipsKeepPoint(clips, 0.5f, 0.5f) &&
			!jp_paint::clipsKeepPoint(clips, 0.1f, 0.5f),
			"complementary clips keep nothing");
	}

	void testCollapseStrokeClips()
	{
		// A horizontal run crossing a clip that keeps the middle third.
		JPPaintStroke stroke;
		for (int i = 0; i <= 10; ++i)
			stroke.points.push_back(pt((float)i * 0.1f, 0.5f));
		stroke.clips.push_back(squareClip(0.35f, 0.4f, 0.65f, 0.6f, false));

		std::vector<JPPaintStroke> out;
		expect(jp_paint::collapseStrokeClips(stroke, out),
			"a point run collapses");
		expect(out.size() == 1, "one surviving run means one stroke");
		if (out.size() == 1)
		{
			expect(out[0].clips.empty(), "a baked stroke carries no clips");
			expect(out[0].points.size() >= 2, "the surviving run has geometry");
			// Bisected to the clip edge rather than snapped to a stored point,
			// which would have landed on 0.3 and 0.7.
			expect(near(out[0].points.front().x, 0.35f, 0.002f),
				"the run starts at the clip edge");
			expect(near(out[0].points.back().x, 0.65f, 0.002f),
				"the run ends at the clip edge");
			for (const JPPaintPoint &point : out[0].points)
				expect(point.x >= 0.35f - 0.002f && point.x <= 0.65f + 0.002f,
					"no baked point sits outside the clip");
		}

		// An inverted clip over the middle leaves the two ends: one stroke in,
		// two out.
		JPPaintStroke split = stroke;
		split.clips[0].inverted = true;
		out.clear();
		expect(jp_paint::collapseStrokeClips(split, out),
			"an inverted clip collapses");
		expect(out.size() == 2, "a run cut in the middle becomes two strokes");

		// Complementary clips keep nothing, so there is nothing to hand back -
		// and that is a success, not a refusal.
		JPPaintStroke empty = stroke;
		empty.clips.push_back(squareClip(0.35f, 0.4f, 0.65f, 0.6f, true));
		out.clear();
		expect(jp_paint::collapseStrokeClips(empty, out) && out.empty(),
			"a fully clipped stroke collapses to nothing");

		// A filled shape has no point run to bake.
		JPPaintStroke filled = stroke;
		filled.tool = (int)JPPaintTool::Lasso;
		out.clear();
		expect(!jp_paint::collapseStrokeClips(filled, out) && out.empty(),
			"a filled lasso refuses to collapse");
		filled.tool = (int)JPPaintTool::Fill;
		expect(!jp_paint::collapseStrokeClips(filled, out),
			"a bucket fill refuses to collapse");

		// Everything else about the stroke survives the bake.
		JPPaintStroke styled = stroke;
		styled.r = 0.25f;
		styled.size = 0.031f;
		styled.hardness = 0.4f;
		styled.erase = true;
		out.clear();
		jp_paint::collapseStrokeClips(styled, out);
		expect(out.size() == 1 && near(out[0].r, 0.25f) &&
			near(out[0].size, 0.031f) && near(out[0].hardness, 0.4f) &&
			out[0].erase,
			"a baked stroke keeps colour, size, hardness and erase");
	}

	// ------------------------------------------------------- region tracing

	// A mask with a filled rectangle from (x0,y0) to (x1,y1) exclusive.
	std::vector<std::uint8_t> maskRect(int w, int h, int x0, int y0,
		int x1, int y1)
	{
		std::vector<std::uint8_t> mask((std::size_t)w * (std::size_t)h, 0);
		for (int y = y0; y < y1; ++y)
			for (int x = x0; x < x1; ++x)
				mask[(std::size_t)y * (std::size_t)w + (std::size_t)x] = 255;
		return mask;
	}

	// Twice the enclosed area, signed. Positive and negative just mean the two
	// orientations; a hole traced by the same walk comes out opposite to the
	// outer edge it sits in.
	float loopArea2(const std::vector<JPPaintPoint> &loop)
	{
		float sum = 0.0f;
		for (std::size_t i = 0; i < loop.size(); ++i)
		{
			const JPPaintPoint &a = loop[i];
			const JPPaintPoint &b = loop[(i + 1) % loop.size()];
			sum += a.x * b.y - b.x * a.y;
		}
		return sum;
	}

	void testTraceMaskContours()
	{
		std::vector<std::vector<JPPaintPoint>> contours;

		// Empty mask, nothing to trace.
		jp_paint::traceMaskContours(std::vector<std::uint8_t>(64, 0), 8, 8,
			contours);
		expect(contours.empty(), "an empty mask traces no contours");

		// A square: one closed loop, on the pixel GRID, so its corners are the
		// outside of the filled pixels rather than their centres.
		jp_paint::traceMaskContours(maskRect(10, 10, 2, 2, 6, 6), 10, 10,
			contours);
		expect(contours.size() == 1, "a solid square traces one contour");
		if (contours.size() == 1)
		{
			float minX = 1.0f, minY = 1.0f, maxX = 0.0f, maxY = 0.0f;
			for (const JPPaintPoint &point : contours[0])
			{
				minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
				minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
			}
			expect(near(minX, 0.2f) && near(minY, 0.2f) &&
				near(maxX, 0.6f) && near(maxY, 0.6f),
				"the contour bounds the filled pixels on the grid");
			expect(contours[0].size() == 16,
				"the staircase keeps one corner per boundary edge");
		}

		// A ring: the hole is traced too, in the opposite orientation, which is
		// what an ODD winding rule renders as a hole.
		std::vector<std::uint8_t> ring = maskRect(12, 12, 2, 2, 10, 10);
		for (int y = 4; y < 8; ++y)
			for (int x = 4; x < 8; ++x)
				ring[(std::size_t)y * 12 + (std::size_t)x] = 0;
		jp_paint::traceMaskContours(ring, 12, 12, contours);
		expect(contours.size() == 2, "a ring traces an outer edge and a hole");
		if (contours.size() == 2)
		{
			expect(loopArea2(contours[0]) * loopArea2(contours[1]) < 0.0f,
				"the hole winds against the outer edge");
		}

		// Two separate blobs are two loops, whatever order they are found in.
		std::vector<std::uint8_t> pair = maskRect(16, 8, 1, 1, 4, 4);
		for (int y = 1; y < 4; ++y)
			for (int x = 10; x < 14; ++x)
				pair[(std::size_t)y * 16 + (std::size_t)x] = 255;
		jp_paint::traceMaskContours(pair, 16, 8, contours);
		expect(contours.size() == 2, "two islands trace two contours");

		// A region touching the canvas edge is closed by the border.
		jp_paint::traceMaskContours(maskRect(8, 8, 0, 0, 8, 8), 8, 8, contours);
		expect(contours.size() == 1, "a full canvas traces one contour");
		if (contours.size() == 1)
			expect(contours[0].size() == 4 ||
				near(std::abs(loopArea2(contours[0])), 2.0f),
				"a full canvas contour encloses the whole canvas");

		// Simplification collapses the staircase but keeps the corners.
		jp_paint::traceMaskContours(maskRect(100, 100, 20, 20, 60, 60), 100, 100,
			contours, 0.02f);
		expect(contours.size() == 1 && contours[0].size() <= 6,
			"simplification reduces a square to its corners");

		// The point ceiling is honoured by simplifying harder, not by dropping
		// part of an outline.
		std::vector<std::uint8_t> noisy((std::size_t)64 * 64, 0);
		for (int y = 0; y < 64; ++y)
			for (int x = 0; x < 64; ++x)
				if (((x / 2) + (y / 2)) % 2 == 0)
					noisy[(std::size_t)y * 64 + (std::size_t)x] = 255;
		jp_paint::traceMaskContours(noisy, 64, 64, contours, 0.0f, 200);
		std::size_t total = 0;
		for (const std::vector<JPPaintPoint> &loop : contours)
			total += loop.size();
		expect(total <= 200, "tracing respects its point ceiling");
	}

	void testRegionAccounting()
	{
		JPPaintStroke region;
		region.tool = (int)JPPaintTool::Region;
		region.points = {pt(0.1f, 0.1f), pt(0.9f, 0.1f), pt(0.9f, 0.9f)};
		region.contours.push_back({pt(0.4f, 0.4f), pt(0.6f, 0.4f),
			pt(0.6f, 0.6f), pt(0.5f, 0.6f)});

		JPPaintEdit edit;
		edit.kind = JPPaintEdit::AddStroke;
		edit.stroke = region;
		expect(jp_paint::pointCount(edit) == 7,
			"the undo budget counts a region's holes as well as its outline");

		// Two regions that differ only in a hole are not the two halves of one
		// selection, so they must not be merged into one.
		JPPaintStroke a = region;
		JPPaintStroke b = region;
		a.clips.push_back(squareClip(0.2f, 0.2f, 0.8f, 0.8f, false));
		b.clips.push_back(squareClip(0.2f, 0.2f, 0.8f, 0.8f, true));
		b.contours[0][0].x = 0.41f;
		JPPaintStroke merged;
		expect(!jp_paint::mergeComplementaryStrokes(a, b, merged),
			"regions with different holes are not complementary halves");
		b.contours[0][0].x = 0.4f;
		expect(jp_paint::mergeComplementaryStrokes(a, b, merged) &&
			merged.clips.empty(),
			"two halves of one region selection still collapse");
	}

	// ------------------------------------------------------------- symmetry

	void testSymmetryMirrors()
	{
		JPPaintStroke stroke;
		stroke.points = {pt(0.2f, 0.3f), pt(0.25f, 0.35f, 0.5f)};
		stroke.contours.push_back({pt(0.21f, 0.31f), pt(0.22f, 0.32f),
			pt(0.23f, 0.33f)});
		stroke.clips.push_back(squareClip(0.1f, 0.1f, 0.4f, 0.4f, false));

		const JPPaintStroke flipped = jp_paint::mirrorStroke(stroke, true, false);
		expect(near(flipped.points[0].x, 0.8f) &&
			near(flipped.points[0].y, 0.3f), "mirroring x reflects about 0.5");
		expect(near(flipped.points[1].width, 0.5f),
			"mirroring keeps per-point width");
		expect(near(flipped.contours[0][0].x, 0.79f),
			"a region's holes are mirrored too");
		expect(near(flipped.clips[0].points[0].x, 0.9f),
			"clips travel with the paint they cut");

		std::vector<JPPaintStroke> mirrors;
		jp_paint::appendSymmetryStrokes(stroke, 0, mirrors);
		expect(mirrors.empty(), "symmetry off adds nothing");
		jp_paint::appendSymmetryStrokes(stroke, 1, mirrors);
		expect(mirrors.size() == 1, "one axis adds one mirror");
		mirrors.clear();
		jp_paint::appendSymmetryStrokes(stroke, 3, mirrors);
		expect(mirrors.size() == 3, "both axes add three mirrors");

		// A stroke lying exactly on the axis is its own mirror, and committing
		// it twice would double its density.
		JPPaintStroke centred;
		centred.points = {pt(0.5f, 0.2f), pt(0.5f, 0.8f)};
		mirrors.clear();
		jp_paint::appendSymmetryStrokes(centred, 1, mirrors);
		expect(mirrors.empty(), "a stroke on the axis is not doubled");
		mirrors.clear();
		jp_paint::appendSymmetryStrokes(centred, 3, mirrors);
		expect(mirrors.size() == 1,
			"four way symmetry of an axis stroke adds only the other reflection");
	}

	void testGroupedStrokeUndo()
	{
		JPPaintDocument doc = docOf(2);
		JPPaintEdit edit;
		edit.kind = JPPaintEdit::AddStroke;
		edit.frameIndex = 0;
		edit.layerIndex = 0;
		edit.strokeIndex = (int)doc.frames[0].layers[0].strokes.size();
		edit.stroke = strokeOf(3);
		edit.extraStrokes.push_back(strokeOf(4));
		edit.extraStrokes.push_back(strokeOf(5));

		const std::size_t before = doc.frames[0].layers[0].strokes.size();
		expect(jp_paint::applyEdit(doc, edit), "a stroke group applies");
		expect(doc.frames[0].layers[0].strokes.size() == before + 3,
			"the group and its mirrors land together");
		expect(doc.frames[0].layers[0].strokes[before + 1].points.size() == 4 &&
			doc.frames[0].layers[0].strokes[before + 2].points.size() == 5,
			"the group keeps its commit order");
		expect(jp_paint::revertEdit(doc, edit), "a stroke group reverts");
		expect(doc.frames[0].layers[0].strokes.size() == before,
			"one undo takes the whole group");
		expect(jp_paint::pointCount(edit) == 12,
			"the undo budget counts every stroke in the group");
	}

	void testAreaTools()
	{
		// An area is filled, so it has no nib to scale and a lasso drawn inside
		// it still touches it. A ribbon is neither.
		expect(jp_paint::isAreaTool((int)JPPaintTool::Lasso) &&
			jp_paint::isAreaTool((int)JPPaintTool::Region) &&
			!jp_paint::isAreaTool((int)JPPaintTool::Rect) &&
			!jp_paint::isAreaTool((int)JPPaintTool::Brush) &&
			!jp_paint::isAreaTool((int)JPPaintTool::Fill),
			"the area tools are the ones that fill a shape");
	}

	void testCurrentFrameClamp()
	{
		JPPaintDocument doc = docOf(3);
		doc.currentFrame = 2;
		JPPaintEdit edit;
		edit.kind = JPPaintEdit::DeleteFrame;
		edit.frameIndex = 2;
		edit.frame = doc.frames[2];
		jp_paint::applyEdit(doc, edit);
		expect(doc.currentFrame == 1,
			"deleting the selected cel moves the selection into range");
	}
}

int main()
{
	testTicks();
	testAdvanceLoop();
	testAdvanceOnce();
	testAdvancePingPong();
	testAdvanceRange();
	testAdvanceEmpty();
	testQuantize();
	testPackRoundTrip();
	testPackMalformed();
	testSimplify();
	testHexParsing();
	testHexFormatting();
	testFloodFillBasics();
	testFloodFillBoundary();
	testFloodFillHoles();
	testFloodFillTolerance();
	testFloodFillAlpha();
	testEditRoundTrip();
	testLastFrameSurvives();
	testEditsRejectBadIndices();
	testRevisionBumps();
	testLayerStructure();
	testLayerDeleteCarriesStrokes();
	testLastLayerSurvives();
	testLayerMove();
	testLayerDuplicateAndMergePayloads();
	testBackgroundLayer();
	testLayerArityAcrossCelEdits();
	testUndoRing();
	testUndoEviction();
	testClipPayloadAccounting();
	testComplementarySelectionCollapse();
	testNestedSelectionCollapse();
	testReplaceStrokesUndoRedo();
	testUndoAcrossFrameEdits();
	testCurrentFrameClamp();
	testClipsKeepPoint();
	testCollapseStrokeClips();
	testTraceMaskContours();
	testRegionAccounting();
	testSymmetryMirrors();
	testGroupedStrokeUndo();
	testAreaTools();
	if (failures != 0)
	{
		std::cerr << failures << " paint core test(s) failed\n";
		return 1;
	}
	std::cout << "paint core tests passed\n";
	return 0;
}
