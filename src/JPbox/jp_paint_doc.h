#pragma once

#include "jp_media_state.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

// The paint box's document, as pure value types.
//
// Deliberately free of openFrameworks: no ofMain.h, no ofVec2f, no ofFloatColor.
// Everything that can be decided without a GL context lives here - playback
// advance, stroke simplification, point packing, undo - so tests/ can compile it
// with a bare c++ and exercise it directly, the way jp_media_state.h already is.
// Rasterization and every GL call live in jp_box_paint.cpp.
//
// COORDINATES ARE NORMALIZED, never pixels. jp_constants::renderWidth/Height is
// a user setting that changes at runtime; a drawing stored in pixels would
// silently rescale the moment somebody touched it. Same convention as
// AdvancedMappingLayer (jp_box_shader.h).

// The ONE tool enum. These ints are written to savefiles as <tool>, so this is
// APPEND ONLY - reordering it silently rewrites every drawing ever saved.
//
// There used to be a second, parallel enum in JPboxgroup that happened to agree
// on the first five members and was assigned straight into stroke.tool. The
// toolbar's display order lives in an array in JPboxgroup_paint.cpp instead, so a
// UI rearrangement can never reach these numbers.
enum class JPPaintTool
{
	Brush = 0,
	Eraser,
	Line,
	Rect,
	Ellipse,
	Fill,
	Eyedropper,
	// Freehand that closes to its start point on release and fills.
	Lasso,
	RectSelect,
	LassoSelect,
	// A materialised bucket fill: the region the flood actually covered, stored
	// as closed contours. Fill above is the OLD form, kept because savefiles
	// contain it - see the note on JPPaintStroke::contours.
	Region
};

namespace jp_paint
{
	// Does this tool's mark enclose an area rather than trace a line? An area
	// has no nib to scale and can be selected by a lasso that sits entirely
	// inside it, neither of which is true of a ribbon.
	inline bool isAreaTool(int tool)
	{
		return tool == (int)JPPaintTool::Lasso ||
			tool == (int)JPPaintTool::Region;
	}
}

struct JPPaintPoint
{
	// 0..1 across the canvas, but NOT clamped to it: a stroke may legitimately
	// run off the edge, and clipping it here would flatten the overhang onto
	// the border instead of letting it leave.
	float x = 0.0f;
	float y = 0.0f;
	// Multiplier on the stroke's base size, so a pressure or speed ramp costs
	// one byte per point instead of a second absolute width.
	float width = 1.0f;
};

// A non-destructive lasso clip. Keeping the original stroke geometry and
// masking it at raster time avoids changing round caps, joins, closed shapes
// and pressure widths when a selection crosses a stroke.
struct JPPaintClip
{
	std::vector<JPPaintPoint> points;
	bool inverted = false;
};

struct JPPaintStroke
{
	std::vector<JPPaintPoint> points;
	// Plain floats rather than ofFloatColor - that type would drag ofMain.h in
	// and cost this header its testability.
	float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
	// Half-width, normalized to canvas WIDTH only. Normalizing each axis
	// separately would turn every brush dab elliptical on a non-square canvas.
	float size = 0.012f;
	// 1 is the historical hard-edged brush. Lower values keep an opaque core
	// and feather the remainder of the radius at raster time.
	float hardness = 1.0f;
	bool erase = false;
	int tool = (int)JPPaintTool::Brush;
	// Fill only: how close a pixel has to be to the seed pixel to count as the
	// same region, per premultiplied channel. Ignored by every other tool.
	float tolerance = 0.12f;
	// Clips are intersected in order. An inverted clip keeps the exterior of
	// its polygon; a normal clip keeps the interior. See kMaxStrokeClips for
	// how deep the nesting is allowed to go and what happens at the bottom.
	std::vector<JPPaintClip> clips;
	// Region only. `points` is the region's first contour and these are the
	// rest, holes included - which is why the whole set is filled with an ODD
	// winding rule rather than the union a lasso wants.
	//
	// This is what makes a bucket fill a THING rather than a command. A Fill
	// stroke stores only a seed and a tolerance, so it had to re-flood on every
	// rebuild - one full readback and canvas scan each - it changed retroactively
	// when anything under it changed, and it could not be selected or moved,
	// because moving a seed is not moving the pixels the seed found. A Region is
	// ordinary geometry and has none of those properties. Fill is still rendered
	// exactly as it always was, for documents that contain it.
	std::vector<std::vector<JPPaintPoint>> contours;
};

// One layer's worth of one cel. A cel holds one of these per document layer, in
// the same order, always - see syncLayerArity.
struct JPPaintLayer
{
	std::vector<JPPaintStroke> strokes;
};

// A layer, described once for the whole document rather than per cel: renaming,
// hiding or reordering a layer is a document-level act, not a per-cel one.
struct JPPaintLayerInfo
{
	std::string name;
	bool visible = true;
	bool locked = false;
	float opacity = 1.0f;
	// 0 normal, 1 multiply, 2 screen, 3 additive. Kept as an integer so old
	// savefiles need no enum migration and unknown values can be clamped.
	int blendMode = 0;
	// A background layer ignores its per-cel strokes and draws `sharedStrokes`
	// on EVERY cel, so a static backdrop is drawn once instead of copied onto
	// each one. The per-cel strokes are KEPT, not discarded, so turning the flag
	// back off restores whatever was there.
	bool background = false;
	// -1 means no label.  Otherwise this indexes the editor's fixed palette.
	int labelColor = -1;
	std::vector<JPPaintStroke> sharedStrokes;
	int id = 0;
};

struct JPPaintFrame
{
	// Parallel to JPPaintDocument::layers. Index 0 is the BOTTOM of the stack,
	// so compositing is a plain forward walk.
	std::vector<JPPaintLayer> layers = std::vector<JPPaintLayer>(1);
	// Procreate's "hold duration": how many playback ticks this cel occupies.
	// Always read through std::max(1, hold) - a zero would make tickCount lie.
	int hold = 1;
	// Stable identity plus an edit counter. The raster cache keys on
	// (id, revision), so inserting or reordering cels never invalidates an
	// unrelated cel's cached pixels, and editing one always does. A parallel
	// index-keyed array would have to be shuffled by every insert and would
	// drift the first time somebody forgot.
	int id = 0;
	unsigned long long revision = 0;
};

struct JPPaintDocument
{
	// ALWAYS at least one cel. Every mutator here enforces it; the editor never
	// has to special-case an empty document.
	std::vector<JPPaintFrame> frames = std::vector<JPPaintFrame>(1);
	// ALWAYS at least one layer, for the same reason there is always one cel.
	std::vector<JPPaintLayerInfo> layers = std::vector<JPPaintLayerInfo>(1);
	int currentFrame = 0;
	int currentLayer = 0;
	float fps = 12.0f;
	int onionBefore = 1;
	int onionAfter = 1;
	float onionOpacity = 0.35f;
	// Transparent by default: this is a texture in a patch, not a page. An
	// opaque white canvas would blot out everything composited underneath it.
	float bgR = 0.0f, bgG = 0.0f, bgB = 0.0f, bgA = 0.0f;
	// Drawing symmetry: 0 off, 1 mirrors across the vertical centre line, 2
	// across the horizontal one, 3 both, which is four way. A mirrored copy is
	// committed as a REAL stroke beside the original rather than being a render
	// time trick, so afterwards it can be erased, selected and moved on its own -
	// and a document drawn with symmetry on opens the same in a version that
	// never heard of it.
	int symmetry = 0;
	// Native PAINT resolution.  The node output can be resampled to the global
	// graph resolution, but stroke rasterization and exports use these values.
	int canvasWidth = 1920;
	int canvasHeight = 1080;
	// Never reused within a document, so a cached raster cannot be handed to a
	// different cel that happened to land on the same index.
	int nextFrameId = 1;
	int nextLayerId = 1;
};

namespace jp_paint
{
	inline bool sameStrokePoint(const JPPaintPoint &a,
		const JPPaintPoint &b)
	{
		return a.x == b.x && a.y == b.y && a.width == b.width;
	}

	inline bool sameStrokePoints(const std::vector<JPPaintPoint> &a,
		const std::vector<JPPaintPoint> &b)
	{
		if (a.size() != b.size()) return false;
		for (std::size_t i = 0; i < a.size(); ++i)
			if (!sameStrokePoint(a[i], b[i])) return false;
		return true;
	}

	inline bool sameStrokeClip(const JPPaintClip &a, const JPPaintClip &b,
		bool compareInversion = true)
	{
		return (!compareInversion || a.inverted == b.inverted) &&
			sameStrokePoints(a.points, b.points);
	}

	// How many clips one stroke may carry.
	//
	// This is NOT a rasterizer limit: beginStrokeClip counts clips in the
	// stencil buffer, so it handles far more than this. The cap is here because
	// every selection of a stroke that STRADDLES the lasso leaves one more clip
	// and one more copy behind, and unbounded nesting would be unbounded
	// doubling. A stroke that reaches the cap is baked by collapseStrokeClips
	// rather than quietly dropping out of every future selection, which is what
	// a silent cap used to do.
	constexpr std::size_t kMaxStrokeClips = 32;

	inline bool pointInPolygon(const std::vector<JPPaintPoint> &polygon,
		float x, float y)
	{
		if (polygon.size() < 3) return false;
		bool inside = false;
		std::size_t j = polygon.size() - 1;
		for (std::size_t i = 0; i < polygon.size(); ++i)
		{
			if (((polygon[i].y > y) != (polygon[j].y > y)) &&
				(x < (polygon[j].x - polygon[i].x) * (y - polygon[i].y) /
					(polygon[j].y - polygon[i].y) + polygon[i].x))
			{
				inside = !inside;
			}
			j = i;
		}
		return inside;
	}

	// A point survives a stroke's clips when every one of them keeps it: a
	// normal clip keeps its interior, an inverted clip keeps its exterior.
	inline bool clipsKeepPoint(const std::vector<JPPaintClip> &clips,
		float x, float y)
	{
		for (const JPPaintClip &clip : clips)
		{
			if (clip.points.size() < 3) continue;
			if (pointInPolygon(clip.points, x, y) == clip.inverted) return false;
		}
		return true;
	}

	// Bake a stroke's clips into its point run: the same paint, as zero or more
	// strokes carrying no clips at all.
	//
	// Returns false and leaves `out` alone for a stroke whose mark is an AREA
	// rather than a run of points - a filled lasso would need real polygon
	// boolean ops, and a bucket fill is a command whose result depends on every
	// pixel under it.
	//
	// Cut ends get the round cap every other stroke end has, and the cut follows
	// the CENTRELINE, so up to one brush radius of paint crosses the boundary in
	// either direction. A baked stroke is therefore not pixel-identical to the
	// clipped one it replaces - which is the whole reason clipping is normally
	// kept non-destructive, and the reason this only runs at the cap.
	inline bool collapseStrokeClips(const JPPaintStroke &stroke,
		std::vector<JPPaintStroke> &out)
	{
		if (stroke.tool == (int)JPPaintTool::Lasso ||
			stroke.tool == (int)JPPaintTool::Region ||
			stroke.tool == (int)JPPaintTool::Fill) return false;
		if (stroke.clips.empty())
		{
			out.push_back(stroke);
			return true;
		}
		if (stroke.points.empty()) return true;

		// Where a segment crosses a clip edge, bisect for the crossing rather
		// than snapping to the nearest stored point: at brush sizes the stored
		// points can be several pixels apart, and snapping would visibly move
		// the cut.
		auto crossing = [&stroke](const JPPaintPoint &inside,
			const JPPaintPoint &outside)
		{
			float lo = 0.0f, hi = 1.0f;
			for (int step = 0; step < 12; ++step)
			{
				const float mid = (lo + hi) * 0.5f;
				const float x = inside.x + (outside.x - inside.x) * mid;
				const float y = inside.y + (outside.y - inside.y) * mid;
				if (clipsKeepPoint(stroke.clips, x, y)) lo = mid;
				else hi = mid;
			}
			JPPaintPoint point;
			point.x = inside.x + (outside.x - inside.x) * lo;
			point.y = inside.y + (outside.y - inside.y) * lo;
			point.width = inside.width + (outside.width - inside.width) * lo;
			return point;
		};

		JPPaintStroke piece = stroke;
		piece.clips.clear();
		piece.points.clear();
		auto flush = [&out, &piece]()
		{
			if (piece.points.empty()) return;
			out.push_back(piece);
			piece.points.clear();
		};

		bool previousKept = false;
		for (std::size_t i = 0; i < stroke.points.size(); ++i)
		{
			const JPPaintPoint &point = stroke.points[i];
			const bool kept = clipsKeepPoint(stroke.clips, point.x, point.y);
			if (kept)
			{
				// Entering: start the run at the boundary, not at the first
				// stored point inside it.
				if (!previousKept && i > 0)
					piece.points.push_back(crossing(point, stroke.points[i - 1]));
				piece.points.push_back(point);
			}
			else if (previousKept)
			{
				piece.points.push_back(crossing(stroke.points[i - 1], point));
				flush();
			}
			previousKept = kept;
		}
		flush();
		return true;
	}

	// ------------------------------------------------------------- symmetry

	// A copy of the stroke mirrored about the canvas centre. Mirroring reverses
	// the point order's handedness, which matters to nothing here: a ribbon has
	// no direction, and a region is filled with an ODD winding rule.
	inline JPPaintStroke mirrorStroke(const JPPaintStroke &stroke,
		bool mirrorX, bool mirrorY)
	{
		JPPaintStroke copy = stroke;
		auto flip = [mirrorX, mirrorY](JPPaintPoint &point)
		{
			if (mirrorX) point.x = 1.0f - point.x;
			if (mirrorY) point.y = 1.0f - point.y;
		};
		for (JPPaintPoint &point : copy.points) flip(point);
		for (std::vector<JPPaintPoint> &contour : copy.contours)
			for (JPPaintPoint &point : contour) flip(point);
		// The clips travel with the paint they were cutting, or a mirrored copy
		// of a clipped stroke would show the part the clip had removed.
		for (JPPaintClip &clip : copy.clips)
			for (JPPaintPoint &point : clip.points) flip(point);
		return copy;
	}

	// The mirrors a stroke needs under this symmetry mode, appended to `out`.
	// The stroke itself is NOT included - the caller already has it.
	//
	// A mirror that lands exactly on its source is dropped: a stroke drawn down
	// the axis would otherwise be committed twice, which is invisible while it
	// is opaque and doubles its density the moment it is not.
	inline void appendSymmetryStrokes(const JPPaintStroke &stroke, int symmetry,
		std::vector<JPPaintStroke> &out)
	{
		if (symmetry <= 0 || symmetry > 3 || stroke.points.empty()) return;
		const bool acrossX = (symmetry & 1) != 0;
		const bool acrossY = (symmetry & 2) != 0;
		auto add = [&stroke, &out](bool mirrorX, bool mirrorY)
		{
			JPPaintStroke mirrored = mirrorStroke(stroke, mirrorX, mirrorY);
			if (sameStrokePoints(mirrored.points, stroke.points)) return;
			for (const JPPaintStroke &existing : out)
				if (sameStrokePoints(mirrored.points, existing.points)) return;
			out.push_back(std::move(mirrored));
		};
		if (acrossX) add(true, false);
		if (acrossY) add(false, true);
		if (acrossX && acrossY) add(true, true);
	}

	// A selection stores an unchanged vector twice, with complementary final
	// clips. If neither half was transformed, the pair is still exactly the
	// original stroke and can be losslessly compacted. Besides removing a
	// raster seam, doing this before another selection prevents old invisible
	// halves from being selected and exposing the path of an earlier lasso.
	inline bool mergeComplementaryStrokes(const JPPaintStroke &a,
		const JPPaintStroke &b, JPPaintStroke &merged)
	{
		if (a.tool != b.tool || a.erase != b.erase ||
			a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a ||
			a.size != b.size || a.hardness != b.hardness ||
			a.tolerance != b.tolerance ||
			!sameStrokePoints(a.points, b.points) ||
			a.contours.size() != b.contours.size() ||
			a.clips.size() != b.clips.size() || a.clips.empty()) return false;
		for (std::size_t i = 0; i < a.contours.size(); ++i)
			if (!sameStrokePoints(a.contours[i], b.contours[i])) return false;

		const std::size_t last = a.clips.size() - 1;
		for (std::size_t i = 0; i < last; ++i)
			if (!sameStrokeClip(a.clips[i], b.clips[i])) return false;
		if (a.clips[last].inverted == b.clips[last].inverted ||
			!sameStrokeClip(a.clips[last], b.clips[last], false)) return false;

		merged = a;
		merged.clips.pop_back();
		return true;
	}

	inline std::vector<JPPaintStroke> collapseComplementaryStrokes(
		const std::vector<JPPaintStroke> &strokes)
	{
		std::vector<JPPaintStroke> collapsed;
		collapsed.reserve(strokes.size());
		for (const JPPaintStroke &stroke : strokes)
		{
			collapsed.push_back(stroke);
			while (collapsed.size() >= 2)
			{
				JPPaintStroke merged;
				const std::size_t n = collapsed.size();
				if (!mergeComplementaryStrokes(collapsed[n - 2],
					collapsed[n - 1], merged)) break;
				collapsed.pop_back();
				collapsed.back() = merged;
			}
		}
		return collapsed;
	}

	// ---------------------------------------------------------------- timing

	inline int holdOf(const JPPaintFrame &frame)
	{
		return std::max(1, frame.hold);
	}

	inline int tickCount(const JPPaintDocument &doc)
	{
		int total = 0;
		for (const JPPaintFrame &frame : doc.frames) total += holdOf(frame);
		return total;
	}

	inline int frameAtTick(const JPPaintDocument &doc, int tick)
	{
		if (doc.frames.empty()) return 0;
		if (tick <= 0) return 0;
		int accumulated = 0;
		for (std::size_t i = 0; i < doc.frames.size(); ++i)
		{
			accumulated += holdOf(doc.frames[i]);
			if (tick < accumulated) return (int)i;
		}
		return (int)doc.frames.size() - 1;
	}

	// The cel range a normalized IN/OUT selection covers, in the same tick math
	// playback uses - so exporting a trimmed range and playing it back pick the
	// same cels. Inclusive at both ends, and always at least one cel.
	inline void celRange(const JPPaintDocument &doc, float rangeIn,
		float rangeOut, int &firstCel, int &lastCel)
	{
		const int last = doc.frames.empty() ? 0 : (int)doc.frames.size() - 1;
		const int ticks = tickCount(doc);
		const float in = std::clamp(rangeIn, 0.0f, 1.0f);
		const float out = std::clamp(rangeOut, in, 1.0f);
		firstCel = frameAtTick(doc,
			(int)std::floor(in * (float)ticks));
		// The out point is a boundary, so the last cel is the one the tick just
		// BEFORE it lands on.
		lastCel = frameAtTick(doc,
			std::max(0, (int)std::ceil(out * (float)ticks) - 1));
		firstCel = std::clamp(firstCel, 0, last);
		lastCel = std::clamp(std::max(firstCel, lastCel), 0, last);
	}

	inline int celAtPlayhead(const JPPaintDocument &doc, float playheadTicks)
	{
		const int ticks = tickCount(doc);
		if (ticks <= 0) return 0;
		int tick = (int)std::floor(playheadTicks);
		// A playhead parked exactly on the end (Once mode does that) must show
		// the LAST cel, not wrap to a cel that does not exist.
		tick = std::clamp(tick, 0, ticks - 1);
		return frameAtTick(doc, tick);
	}

	// Advances `playheadTicks` by `dt` seconds and returns the cel to display.
	//
	// The loop / ping-pong / once decision is delegated wholesale to
	// jp_media::applyBoundary by expressing the playhead as a 0..1 position.
	// That is real reuse, not a coincidence of shape: it means a paint box, a
	// video box and a GIF all reflect a ping-pong overshoot the same way, and
	// the inspector's IN/OUT range handles trim a cel range for free.
	inline int advance(const JPPaintDocument &doc, JPMediaState &playback,
		float &playheadTicks, float dt)
	{
		const int ticks = tickCount(doc);
		if (ticks <= 0)
		{
			playheadTicks = 0.0f;
			playback.position = 0.0f;
			return 0;
		}
		const float span = (float)ticks;
		if (playback.playing && dt > 0.0f && doc.fps > 0.0f)
		{
			const float step = dt * doc.fps * playback.rate;
			playheadTicks += playback.reverse ? -step : step;
			float position = playheadTicks / span;
			if (jp_media::applyBoundary(playback, position))
			{
				playheadTicks = position * span;
			}
		}
		playheadTicks = std::clamp(playheadTicks,
			playback.rangeIn * span, playback.rangeOut * span);
		playback.position = playheadTicks / span;
		return celAtPlayhead(doc, playheadTicks);
	}

	// ------------------------------------------------------------ quantizing

	// Points are stored in the session XML as one packed text run per stroke.
	// A five minute doodle is tens of thousands of points, and a verbose
	// <point><x/><y/></point> triple costs well over a hundred bytes each -
	// enough to turn a session file into megabytes of coordinates.
	//
	// The encoded range deliberately overshoots the canvas on both sides so a
	// stroke that runs off the edge survives a save. 16 bits over 2.0 units is
	// 3e-5, an eighth of a pixel at 4K, so the quantization is invisible.
	constexpr float kCoordMin = -0.5f;
	constexpr float kCoordSpan = 2.0f;
	constexpr float kCoordScale = 65535.0f;
	// Widths run past 1.0 because the multiplier is allowed to grow a stroke,
	// not only shrink it.
	constexpr float kWidthMax = 2.0f;
	constexpr float kWidthScale = 127.5f;

	inline std::uint16_t quantizeCoord(float value)
	{
		const float clamped = std::clamp(value, kCoordMin, kCoordMin + kCoordSpan);
		const float t = (clamped - kCoordMin) / kCoordSpan;
		return (std::uint16_t)std::lround(t * kCoordScale);
	}

	inline float dequantizeCoord(std::uint16_t quantized)
	{
		return kCoordMin + ((float)quantized / kCoordScale) * kCoordSpan;
	}

	inline std::uint8_t quantizeWidth(float value)
	{
		return (std::uint8_t)std::lround(
			std::clamp(value, 0.0f, kWidthMax) * kWidthScale);
	}

	inline float dequantizeWidth(std::uint8_t quantized)
	{
		return (float)quantized / kWidthScale;
	}

	inline std::string packPoints(const std::vector<JPPaintPoint> &points)
	{
		std::string out;
		out.reserve(points.size() * 16);
		for (std::size_t i = 0; i < points.size(); ++i)
		{
			if (i != 0) out.push_back(' ');
			out += std::to_string((unsigned)quantizeCoord(points[i].x));
			out.push_back(',');
			out += std::to_string((unsigned)quantizeCoord(points[i].y));
			out.push_back(',');
			out += std::to_string((unsigned)quantizeWidth(points[i].width));
		}
		return out;
	}

	// Returns false and leaves `out` empty on malformed input, so a corrupted
	// savefile drops one stroke rather than loading a stroke of garbage.
	inline bool unpackPoints(const std::string &text,
		std::vector<JPPaintPoint> &out)
	{
		out.clear();
		const std::size_t n = text.size();
		std::size_t i = 0;
		auto isSpace = [](char c) {
			return c == ' ' || c == '\t' || c == '\n' || c == '\r';
		};
		auto readNumber = [&](long &value) {
			const std::size_t start = i;
			while (i < n && text[i] >= '0' && text[i] <= '9') ++i;
			if (i == start) return false;
			value = std::strtol(text.c_str() + start, nullptr, 10);
			return true;
		};
		while (true)
		{
			while (i < n && isSpace(text[i])) ++i;
			if (i >= n) break;
			long qx = 0, qy = 0, qw = 0;
			const bool ok =
				readNumber(qx) && i < n && text[i] == ',' && (++i, true) &&
				readNumber(qy) && i < n && text[i] == ',' && (++i, true) &&
				readNumber(qw);
			if (!ok)
			{
				out.clear();
				return false;
			}
			JPPaintPoint point;
			point.x = dequantizeCoord((std::uint16_t)std::clamp(qx, 0L, 65535L));
			point.y = dequantizeCoord((std::uint16_t)std::clamp(qy, 0L, 65535L));
			point.width = dequantizeWidth((std::uint8_t)std::clamp(qw, 0L, 255L));
			out.push_back(point);
		}
		return true;
	}

	// ------------------------------------------------------------------ fills

	// Scanline flood fill over a tightly packed RGBA byte buffer.
	//
	// Takes a raw pointer rather than ofPixels on purpose: that keeps it in this
	// header, where tests/ can exercise it without a GL context. The buffer the
	// caller hands over is PREMULTIPLIED, which changes nothing here - the
	// comparison is per-channel on whatever is stored.
	//
	// Writes 255 into `mask` for every pixel in the seed's region, 0 elsewhere,
	// and returns how many were filled. An out of bounds seed fills nothing.
	//
	// A bucket needs no polygon and no contour tracing: the region is whatever
	// the pixels say it is, so holes and islands come out right without the
	// even-odd subpath behaviour this codebase deliberately avoids.
	inline std::size_t floodFill(const std::uint8_t *rgba, int width, int height,
		int seedX, int seedY, int tolerance, std::vector<std::uint8_t> &mask)
	{
		const std::size_t total = (std::size_t)std::max(0, width) *
			(std::size_t)std::max(0, height);
		mask.assign(total, 0);
		if (rgba == nullptr || width <= 0 || height <= 0) return 0;
		if (seedX < 0 || seedY < 0 || seedX >= width || seedY >= height) return 0;

		const std::uint8_t *seed =
			rgba + ((std::size_t)seedY * (std::size_t)width + (std::size_t)seedX) * 4;
		const int tol = std::clamp(tolerance, 0, 255);
		auto matches = [&](int x, int y) {
			const std::uint8_t *p =
				rgba + ((std::size_t)y * (std::size_t)width + (std::size_t)x) * 4;
			return std::abs((int)p[0] - (int)seed[0]) <= tol &&
				std::abs((int)p[1] - (int)seed[1]) <= tol &&
				std::abs((int)p[2] - (int)seed[2]) <= tol &&
				std::abs((int)p[3] - (int)seed[3]) <= tol;
		};
		auto at = [&](int x, int y) {
			return (std::size_t)y * (std::size_t)width + (std::size_t)x;
		};

		std::size_t filled = 0;
		// Flattened x,y pairs. Iterative because a two megapixel region would
		// recurse two million deep.
		std::vector<int> stack;
		stack.push_back(seedX);
		stack.push_back(seedY);
		while (!stack.empty())
		{
			const int y = stack.back(); stack.pop_back();
			const int x0 = stack.back(); stack.pop_back();
			if (mask[at(x0, y)] != 0) continue;
			if (!matches(x0, y)) continue;

			int left = x0;
			while (left > 0 && mask[at(left - 1, y)] == 0 && matches(left - 1, y))
				--left;
			int right = x0;
			while (right + 1 < width && mask[at(right + 1, y)] == 0 &&
				matches(right + 1, y))
				++right;
			for (int x = left; x <= right; ++x)
			{
				mask[at(x, y)] = 255;
				++filled;
			}

			// Push only where a NEW span begins on the neighbouring row. Pushing
			// every column instead would grow the stack to the size of the
			// region - sixteen megabytes on a 4K fill.
			auto scan = [&](int lo, int hi, int row) {
				if (row < 0 || row >= height) return;
				bool inSpan = false;
				for (int x = lo; x <= hi; ++x)
				{
					const bool ok = mask[at(x, row)] == 0 && matches(x, row);
					if (ok && !inSpan)
					{
						stack.push_back(x);
						stack.push_back(row);
						inSpan = true;
					}
					else if (!ok)
					{
						inSpan = false;
					}
				}
			};
			scan(left, right, y - 1);
			scan(left, right, y + 1);
		}
		return filled;
	}

	// ------------------------------------------------------------------- hex

	// Parses #RGB, #RRGGBB or #RRGGBBAA, with or without the hash and in either
	// case. Returns false and touches nothing on anything else, so a half-typed
	// field never half-applies.
	//
	// Pure, and therefore in this header rather than the panel: a colour the user
	// typed is exactly the kind of input worth testing without a window.
	inline bool parseHexColor(const std::string &text, float &r, float &g,
		float &b, float &a)
	{
		std::string digits;
		for (char c : text)
		{
			if (c == '#' || c == ' ' || c == '\t') continue;
			const char lower = (char)std::tolower((unsigned char)c);
			const bool hex = (lower >= '0' && lower <= '9') ||
				(lower >= 'a' && lower <= 'f');
			if (!hex) return false;
			digits.push_back(lower);
		}
		if (digits.size() != 3 && digits.size() != 6 && digits.size() != 8)
		{
			return false;
		}
		auto nibble = [](char c) {
			return c <= '9' ? (int)(c - '0') : (int)(c - 'a') + 10;
		};
		if (digits.size() == 3)
		{
			// #abc means #aabbcc, the usual shorthand.
			r = (float)(nibble(digits[0]) * 17) / 255.0f;
			g = (float)(nibble(digits[1]) * 17) / 255.0f;
			b = (float)(nibble(digits[2]) * 17) / 255.0f;
			a = 1.0f;
			return true;
		}
		auto byteAt = [&](std::size_t i) {
			return (float)(nibble(digits[i]) * 16 + nibble(digits[i + 1])) / 255.0f;
		};
		r = byteAt(0);
		g = byteAt(2);
		b = byteAt(4);
		// Alpha is optional: six digits means opaque, which is what a pasted web
		// colour means.
		a = digits.size() == 8 ? byteAt(6) : 1.0f;
		return true;
	}

	// The canonical form. Alpha is only written when it is not fully opaque, so a
	// plain colour round trips as the six digit form people recognise.
	inline std::string formatHexColor(float r, float g, float b, float a)
	{
		auto byteOf = [](float v) {
			return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
		};
		const char *digits = "0123456789ABCDEF";
		auto append = [&](std::string &out, int value) {
			out.push_back(digits[(value >> 4) & 0xF]);
			out.push_back(digits[value & 0xF]);
		};
		std::string out = "#";
		append(out, byteOf(r));
		append(out, byteOf(g));
		append(out, byteOf(b));
		if (byteOf(a) < 255) append(out, byteOf(a));
		return out;
	}

	// ----------------------------------------------------------- simplifying

	// Ramer-Douglas-Peucker. Mouse sampling produces a point per frame whether
	// the cursor moved a pixel or a hundred, and a slow deliberate stroke can
	// pile up thousands of near-duplicates. Running this once when the stroke is
	// committed typically drops 60-80% of them with no visible change.
	//
	// Iterative rather than recursive: a long stroke would otherwise recurse
	// once per retained point and can overflow the stack.
	inline void simplify(std::vector<JPPaintPoint> &points, float epsilon)
	{
		if (points.size() < 3 || epsilon <= 0.0f) return;
		std::vector<bool> keep(points.size(), false);
		keep.front() = true;
		keep.back() = true;
		std::vector<std::pair<std::size_t, std::size_t>> stack;
		stack.push_back(std::make_pair((std::size_t)0, points.size() - 1));
		while (!stack.empty())
		{
			const std::pair<std::size_t, std::size_t> range = stack.back();
			stack.pop_back();
			const std::size_t first = range.first;
			const std::size_t last = range.second;
			if (last <= first + 1) continue;
			const float ax = points[first].x, ay = points[first].y;
			const float dx = points[last].x - ax, dy = points[last].y - ay;
			const float lengthSq = dx * dx + dy * dy;
			float worst = 0.0f;
			std::size_t worstIndex = first;
			for (std::size_t k = first + 1; k < last; ++k)
			{
				float ex = points[k].x - ax;
				float ey = points[k].y - ay;
				if (lengthSq > 1e-12f)
				{
					// Project onto the segment, clamped, so a hairpin whose
					// endpoints nearly coincide measures real distance rather
					// than distance to an infinite line.
					float t = (ex * dx + ey * dy) / lengthSq;
					t = std::clamp(t, 0.0f, 1.0f);
					ex = points[k].x - (ax + t * dx);
					ey = points[k].y - (ay + t * dy);
				}
				const float distance = std::sqrt(ex * ex + ey * ey);
				if (distance > worst)
				{
					worst = distance;
					worstIndex = k;
				}
			}
			if (worst > epsilon)
			{
				keep[worstIndex] = true;
				stack.push_back(std::make_pair(first, worstIndex));
				stack.push_back(std::make_pair(worstIndex, last));
			}
		}
		std::vector<JPPaintPoint> kept;
		kept.reserve(points.size());
		for (std::size_t k = 0; k < points.size(); ++k)
		{
			if (keep[k]) kept.push_back(points[k]);
		}
		points.swap(kept);
	}

	// -------------------------------------------------------- region tracing

	// The outline of a flood filled region, as closed contours in NORMALIZED
	// coordinates. Replaces `out` rather than appending to it.
	//
	// Walks the PIXEL GRID, not pixel centres: every edge between a filled pixel
	// and an unfilled one - or the canvas border - is one segment of the
	// boundary, and the segments chain head to tail into closed loops. Holes and
	// islands come out of the same walk as the outer edge, so there is no "is
	// this one a hole" question to get wrong; a loop inside a loop IS a hole once
	// the set is filled with an ODD winding rule.
	//
	// Every segment is emitted with the filled side on its RIGHT, which is what
	// makes the chaining unambiguous: the segment continuing a loop is the one
	// that STARTS at the corner the last one ended on.
	//
	// epsilon is a Douglas-Peucker tolerance in normalized units; 0 keeps the
	// pixel staircase. maxPoints is a ceiling on the whole result - a region
	// traced at 4K can have a boundary tens of thousands of corners long, and
	// that would go into the savefile. The tolerance is doubled until the result
	// fits rather than truncating a contour, which would leave a hole in the
	// region's outline.
	inline void traceMaskContours(const std::vector<std::uint8_t> &mask,
		int width, int height,
		std::vector<std::vector<JPPaintPoint>> &out,
		float epsilon = 0.0f, std::size_t maxPoints = 12000)
	{
		out.clear();
		if (width <= 0 || height <= 0) return;
		if (mask.size() != (std::size_t)width * (std::size_t)height) return;

		const int stride = width + 1;
		auto corner = [stride](int x, int y) { return y * stride + x; };
		auto filled = [&mask, width, height](int x, int y) {
			if (x < 0 || y < 0 || x >= width || y >= height) return false;
			return mask[(std::size_t)y * (std::size_t)width + (std::size_t)x] != 0;
		};

		// (start corner, end corner) pairs, sorted by start so the walk can find
		// its continuation with a binary search. The count is the boundary
		// length, so this stays in the thousands even for a big region - which is
		// why it is a sorted list and not a lookup table the size of the canvas.
		std::vector<std::pair<int, int>> edges;
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				if (!filled(x, y)) continue;
				if (!filled(x, y - 1))
					edges.push_back({corner(x, y), corner(x + 1, y)});
				if (!filled(x + 1, y))
					edges.push_back({corner(x + 1, y), corner(x + 1, y + 1)});
				if (!filled(x, y + 1))
					edges.push_back({corner(x + 1, y + 1), corner(x, y + 1)});
				if (!filled(x - 1, y))
					edges.push_back({corner(x, y + 1), corner(x, y)});
			}
		}
		if (edges.empty()) return;
		std::sort(edges.begin(), edges.end());
		std::vector<bool> used(edges.size(), false);

		// The first unused edge starting at this corner, or -1.
		auto next = [&edges, &used](int startCorner) {
			std::size_t lo = (std::size_t)(std::lower_bound(edges.begin(),
				edges.end(), std::make_pair(startCorner, -1)) - edges.begin());
			for (; lo < edges.size() && edges[lo].first == startCorner; ++lo)
				if (!used[lo]) return (long)lo;
			return (long)-1;
		};

		const float fw = (float)width;
		const float fh = (float)height;
		std::vector<std::vector<JPPaintPoint>> loops;
		for (std::size_t seed = 0; seed < edges.size(); ++seed)
		{
			if (used[seed]) continue;
			std::vector<JPPaintPoint> loop;
			long current = (long)seed;
			while (current >= 0 && !used[(std::size_t)current])
			{
				used[(std::size_t)current] = true;
				const int at = edges[(std::size_t)current].first;
				JPPaintPoint point;
				point.x = (float)(at % stride) / fw;
				point.y = (float)(at / stride) / fh;
				point.width = 1.0f;
				loop.push_back(point);
				current = next(edges[(std::size_t)current].second);
			}
			// Fewer than three corners cannot enclose area.
			if (loop.size() >= 3) loops.push_back(std::move(loop));
		}
		if (loops.empty()) return;

		// Simplify closed: the first corner is repeated at the end so the
		// Douglas-Peucker pins a segment of the loop rather than a corner, then
		// the duplicate is dropped again.
		float tolerance = epsilon;
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			out = loops;
			if (tolerance > 0.0f)
			{
				for (std::vector<JPPaintPoint> &loop : out)
				{
					loop.push_back(loop.front());
					simplify(loop, tolerance);
					if (loop.size() > 1) loop.pop_back();
				}
			}
			std::size_t total = 0;
			for (const std::vector<JPPaintPoint> &loop : out) total += loop.size();
			if (total <= maxPoints) break;
			// The first pass may have had no tolerance at all to double.
			tolerance = tolerance > 0.0f ? tolerance * 2.0f : 1.0f / fw;
		}
		// A loop that simplification flattened encloses nothing.
		out.erase(std::remove_if(out.begin(), out.end(),
			[](const std::vector<JPPaintPoint> &loop) { return loop.size() < 3; }),
			out.end());
	}

	// ------------------------------------------------------------- mutations

	inline JPPaintFrame makeFrame(JPPaintDocument &doc)
	{
		JPPaintFrame frame;
		frame.id = doc.nextFrameId++;
		return frame;
	}

	inline void touchFrame(JPPaintDocument &doc, int frameIndex)
	{
		if (frameIndex < 0 || frameIndex >= (int)doc.frames.size()) return;
		++doc.frames[(std::size_t)frameIndex].revision;
	}

	inline void clampCurrentFrame(JPPaintDocument &doc)
	{
		if (doc.frames.empty()) doc.frames.resize(1);
		doc.currentFrame = std::clamp(doc.currentFrame, 0,
			(int)doc.frames.size() - 1);
	}

	// ---------------------------------------------------------------- layers

	inline JPPaintLayerInfo makeLayer(JPPaintDocument &doc, const std::string &name)
	{
		JPPaintLayerInfo layer;
		layer.id = doc.nextLayerId++;
		layer.name = name;
		return layer;
	}

	// THE invariant: every cel carries exactly one JPPaintLayer per document
	// layer, in the same order. Called after anything structural, so no other
	// code has to check.
	inline void syncLayerArity(JPPaintDocument &doc)
	{
		if (doc.layers.empty()) doc.layers.resize(1);
		for (JPPaintFrame &frame : doc.frames)
		{
			frame.layers.resize(doc.layers.size());
		}
	}

	inline void clampCurrentLayer(JPPaintDocument &doc)
	{
		if (doc.layers.empty()) doc.layers.resize(1);
		doc.currentLayer = std::clamp(doc.currentLayer, 0,
			(int)doc.layers.size() - 1);
	}

	// A background layer, a reorder, or any layer property change alters what
	// EVERY cel composites to, so every cached raster is stale. Funnelled through
	// one helper so a new edit kind cannot forget one path.
	inline void bumpAllFrames(JPPaintDocument &doc)
	{
		for (JPPaintFrame &frame : doc.frames) ++frame.revision;
	}

	// The stroke list an edit should act on: a background layer's strokes are
	// shared across cels and live on the layer, everything else is per cel.
	// Returns nullptr for an out of range index.
	inline std::vector<JPPaintStroke> *strokeListFor(JPPaintDocument &doc,
		int frameIndex, int layerIndex)
	{
		if (layerIndex < 0 || layerIndex >= (int)doc.layers.size()) return nullptr;
		if (doc.layers[(std::size_t)layerIndex].background)
		{
			return &doc.layers[(std::size_t)layerIndex].sharedStrokes;
		}
		if (frameIndex < 0 || frameIndex >= (int)doc.frames.size()) return nullptr;
		JPPaintFrame &frame = doc.frames[(std::size_t)frameIndex];
		if (layerIndex >= (int)frame.layers.size()) return nullptr;
		return &frame.layers[(std::size_t)layerIndex].strokes;
	}

	inline const std::vector<JPPaintStroke> *strokeListFor(
		const JPPaintDocument &doc, int frameIndex, int layerIndex)
	{
		return strokeListFor(const_cast<JPPaintDocument &>(doc),
			frameIndex, layerIndex);
	}

	// Invalidates whatever the edit actually changed: one cel, or all of them
	// when the strokes are shared.
	inline void touchLayer(JPPaintDocument &doc, int frameIndex, int layerIndex)
	{
		if (layerIndex >= 0 && layerIndex < (int)doc.layers.size() &&
			doc.layers[(std::size_t)layerIndex].background)
		{
			bumpAllFrames(doc);
			return;
		}
		touchFrame(doc, frameIndex);
	}

	// Total strokes a cel composites, shared layers included. Used by the editor
	// to tell an empty cel from a drawn one.
	inline std::size_t celStrokeCount(const JPPaintDocument &doc, int frameIndex)
	{
		std::size_t total = 0;
		for (int layer = 0; layer < (int)doc.layers.size(); ++layer)
		{
			const std::vector<JPPaintStroke> *list =
				strokeListFor(doc, frameIndex, layer);
			if (list != nullptr) total += list->size();
		}
		return total;
	}
}

// --------------------------------------------------------------------- undo

// One reversible document mutation.
//
// A command ring, not document snapshots: snapshotting every cel's stroke list
// on every dab would cost more memory than the drawing itself. Each edit is
// recorded AFTER the caller has applied it, and carries whatever payload its
// inverse needs.
struct JPPaintEdit
{
	// In-memory only - these are never written to a savefile, so unlike
	// JPPaintTool they can be reordered freely.
	enum Kind
	{
		AddStroke = 0,
		ClearLayer,
		AddFrame,
		DeleteFrame,
		MoveFrame,
		SetHold,
		AddLayer,
		DeleteLayer,
		MoveLayer,
		SetLayerProps,
		ReplaceStrokes,
		MergeLayerDown
	};

	int kind = AddStroke;
	int frameIndex = 0;
	// Which layer the edit acts on. A background layer's strokes are shared, so
	// the same index can mean "one cel" or "every cel" - strokeListFor decides.
	int layerIndex = 0;
	int strokeIndex = -1;
	int fromIndex = -1;
	int toIndex = -1;
	int intValue = 0;      // SetHold: the new hold
	int previousValue = 0; // SetHold: the hold it replaced
	JPPaintStroke stroke;  // AddStroke
	// AddStroke: strokes inserted immediately after `stroke`, as ONE step. This
	// is how a symmetry mirror is committed: undoing a dab must not leave its
	// mirror behind, and two edits would take two undos.
	std::vector<JPPaintStroke> extraStrokes;
	JPPaintFrame frame;    // AddFrame / DeleteFrame / ClearLayer
	// AddLayer / DeleteLayer: the layer itself. SetLayerProps: the NEW props,
	// with previousLayer holding what they replaced.
	JPPaintLayerInfo layer;
	JPPaintLayerInfo previousLayer;
	// DeleteLayer: the deleted layer's strokes in every cel, in cel order. This
	// is what makes undoing a layer delete restore the drawing rather than an
	// empty layer.
	std::vector<JPPaintLayer> layerCels;
	// MergeLayerDown stores the lower layer before and after flattening. `layer`
	// and `layerCels` hold the removed upper layer.
	JPPaintLayerInfo mergedLayer;
	std::vector<JPPaintLayer> previousLayerCels;
	std::vector<JPPaintLayer> mergedLayerCels;
};

namespace jp_paint
{
	inline std::size_t pointCount(const JPPaintEdit &edit)
	{
		auto strokePoints = [](const JPPaintStroke &stroke) {
			std::size_t count = stroke.points.size();
			for (const JPPaintClip &clip : stroke.clips)
				count += clip.points.size();
			// A traced region carries most of its geometry here, so leaving it
			// out would let a handful of fills blow past the ring's budget.
			for (const std::vector<JPPaintPoint> &contour : stroke.contours)
				count += contour.size();
			return count;
		};
		std::size_t total = strokePoints(edit.stroke);
		for (const JPPaintStroke &stroke : edit.extraStrokes)
			total += strokePoints(stroke);
		for (const JPPaintLayer &layer : edit.frame.layers)
		{
			for (const JPPaintStroke &stroke : layer.strokes)
			{
				total += strokePoints(stroke);
			}
		}
		for (const JPPaintStroke &stroke : edit.layer.sharedStrokes)
		{
			total += strokePoints(stroke);
		}
		for (const JPPaintStroke &stroke : edit.previousLayer.sharedStrokes)
		{
			total += strokePoints(stroke);
		}
		// A deleted layer's payload is the biggest thing the ring ever holds, so
		// leaving it out of the budget would let a few of them blow past the cap.
		for (const JPPaintLayer &layer : edit.layerCels)
		{
			for (const JPPaintStroke &stroke : layer.strokes)
			{
				total += strokePoints(stroke);
			}
		}
		for (const JPPaintLayer &layer : edit.previousLayerCels)
			for (const JPPaintStroke &stroke : layer.strokes)
				total += strokePoints(stroke);
		for (const JPPaintLayer &layer : edit.mergedLayerCels)
			for (const JPPaintStroke &stroke : layer.strokes)
				total += strokePoints(stroke);
		for (const JPPaintStroke &stroke : edit.mergedLayer.sharedStrokes)
			total += strokePoints(stroke);
		return total;
	}

	namespace detail
	{
		inline bool validFrame(const JPPaintDocument &doc, int index)
		{
			return index >= 0 && index < (int)doc.frames.size();
		}
	}

	// Applies an edit in the REDO direction.
	inline bool applyEdit(JPPaintDocument &doc, const JPPaintEdit &edit)
	{
		switch (edit.kind)
		{
		case JPPaintEdit::AddStroke:
		{
			std::vector<JPPaintStroke> *list =
				strokeListFor(doc, edit.frameIndex, edit.layerIndex);
			if (list == nullptr) return false;
			const int at = edit.strokeIndex < 0 ?
				(int)list->size() : edit.strokeIndex;
			if (at < 0 || at > (int)list->size()) return false;
			list->insert(list->begin() + at, edit.stroke);
			// Contiguous and in order, so the group's z order is the order it
			// was committed in and revert can erase one run.
			list->insert(list->begin() + at + 1, edit.extraStrokes.begin(),
				edit.extraStrokes.end());
			touchLayer(doc, edit.frameIndex, edit.layerIndex);
			return true;
		}
		case JPPaintEdit::ClearLayer:
		{
			std::vector<JPPaintStroke> *list =
				strokeListFor(doc, edit.frameIndex, edit.layerIndex);
			if (list == nullptr) return false;
			list->clear();
			touchLayer(doc, edit.frameIndex, edit.layerIndex);
			return true;
		}
		case JPPaintEdit::AddFrame:
		{
			if (edit.frameIndex < 0 ||
				edit.frameIndex > (int)doc.frames.size()) return false;
			doc.frames.insert(doc.frames.begin() + edit.frameIndex, edit.frame);
			// A cel built before a layer was added would be one layer short.
			syncLayerArity(doc);
			clampCurrentFrame(doc);
			return true;
		}
		case JPPaintEdit::DeleteFrame:
		{
			if (!detail::validFrame(doc, edit.frameIndex)) return false;
			// The last cel can never be deleted; the document invariant is at
			// least one frame, and the editor relies on it everywhere.
			if (doc.frames.size() <= 1) return false;
			doc.frames.erase(doc.frames.begin() + edit.frameIndex);
			clampCurrentFrame(doc);
			return true;
		}
		case JPPaintEdit::MoveFrame:
		{
			if (!detail::validFrame(doc, edit.fromIndex)) return false;
			if (!detail::validFrame(doc, edit.toIndex)) return false;
			JPPaintFrame moved = doc.frames[(std::size_t)edit.fromIndex];
			doc.frames.erase(doc.frames.begin() + edit.fromIndex);
			doc.frames.insert(doc.frames.begin() + edit.toIndex, moved);
			clampCurrentFrame(doc);
			return true;
		}
		case JPPaintEdit::SetHold:
		{
			if (!detail::validFrame(doc, edit.frameIndex)) return false;
			doc.frames[(std::size_t)edit.frameIndex].hold =
				std::max(1, edit.intValue);
			return true;
		}
		case JPPaintEdit::AddLayer:
		{
			if (edit.layerIndex < 0 ||
				edit.layerIndex > (int)doc.layers.size()) return false;
			doc.layers.insert(doc.layers.begin() + edit.layerIndex, edit.layer);
			// Every cel gains a slot at the same position, or the parallel arrays
			// would silently shear.
			for (std::size_t f = 0; f < doc.frames.size(); ++f)
			{
				JPPaintFrame &frame = doc.frames[f];
				const int at = std::min(edit.layerIndex, (int)frame.layers.size());
				frame.layers.insert(frame.layers.begin() + at,
					f < edit.layerCels.size() ? edit.layerCels[f] : JPPaintLayer());
			}
			syncLayerArity(doc);
			clampCurrentLayer(doc);
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::DeleteLayer:
		{
			if (edit.layerIndex < 0 ||
				edit.layerIndex >= (int)doc.layers.size()) return false;
			// The last layer can never be deleted, same rule as the last cel.
			if (doc.layers.size() <= 1) return false;
			doc.layers.erase(doc.layers.begin() + edit.layerIndex);
			for (JPPaintFrame &frame : doc.frames)
			{
				if (edit.layerIndex < (int)frame.layers.size())
				{
					frame.layers.erase(frame.layers.begin() + edit.layerIndex);
				}
			}
			syncLayerArity(doc);
			clampCurrentLayer(doc);
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::MoveLayer:
		{
			const int count = (int)doc.layers.size();
			if (edit.fromIndex < 0 || edit.fromIndex >= count) return false;
			if (edit.toIndex < 0 || edit.toIndex >= count) return false;
			JPPaintLayerInfo moved = doc.layers[(std::size_t)edit.fromIndex];
			doc.layers.erase(doc.layers.begin() + edit.fromIndex);
			doc.layers.insert(doc.layers.begin() + edit.toIndex, moved);
			for (JPPaintFrame &frame : doc.frames)
			{
				if (edit.fromIndex >= (int)frame.layers.size()) continue;
				JPPaintLayer cel = frame.layers[(std::size_t)edit.fromIndex];
				frame.layers.erase(frame.layers.begin() + edit.fromIndex);
				const int at = std::min(edit.toIndex, (int)frame.layers.size());
				frame.layers.insert(frame.layers.begin() + at, cel);
			}
			syncLayerArity(doc);
			clampCurrentLayer(doc);
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::SetLayerProps:
		{
			if (edit.layerIndex < 0 ||
				edit.layerIndex >= (int)doc.layers.size()) return false;
			JPPaintLayerInfo &target = doc.layers[(std::size_t)edit.layerIndex];
			target.name = edit.layer.name;
			target.visible = edit.layer.visible;
			target.locked = edit.layer.locked;
			target.opacity = std::clamp(edit.layer.opacity, 0.0f, 1.0f);
			target.blendMode = std::clamp(edit.layer.blendMode, 0, 3);
			target.background = edit.layer.background;
			target.labelColor = std::clamp(edit.layer.labelColor, -1, 7);
			// The shared strokes travel with the properties, because turning the
			// background flag ON adopts whatever was drawn on the layer - see
			// JPbox_paint::toggleLayerBackground. previousLayer carries the old
			// list, so this is still an exact inverse.
			target.sharedStrokes = edit.layer.sharedStrokes;
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::ReplaceStrokes:
		{
			std::vector<JPPaintStroke> *list =
				strokeListFor(doc, edit.frameIndex, edit.layerIndex);
			if (list == nullptr) return false;
			*list = edit.layer.sharedStrokes;
			touchLayer(doc, edit.frameIndex, edit.layerIndex);
			return true;
		}
		case JPPaintEdit::MergeLayerDown:
		{
			const int upper = edit.layerIndex;
			const int lower = upper - 1;
			if (lower < 0 || upper >= (int)doc.layers.size()) return false;
			doc.layers[(std::size_t)lower] = edit.mergedLayer;
			doc.layers.erase(doc.layers.begin() + upper);
			for (std::size_t f = 0; f < doc.frames.size(); ++f)
			{
				JPPaintFrame &frame = doc.frames[f];
				if (upper >= (int)frame.layers.size()) continue;
				frame.layers[(std::size_t)lower] = f < edit.mergedLayerCels.size() ?
					edit.mergedLayerCels[f] : JPPaintLayer();
				frame.layers.erase(frame.layers.begin() + upper);
			}
			syncLayerArity(doc);
			doc.currentLayer = lower;
			bumpAllFrames(doc);
			return true;
		}
		default:
			return false;
		}
	}

	// Applies an edit in the UNDO direction.
	inline bool revertEdit(JPPaintDocument &doc, const JPPaintEdit &edit)
	{
		switch (edit.kind)
		{
		case JPPaintEdit::AddStroke:
		{
			std::vector<JPPaintStroke> *list =
				strokeListFor(doc, edit.frameIndex, edit.layerIndex);
			if (list == nullptr) return false;
			const int count = 1 + (int)edit.extraStrokes.size();
			const int at = edit.strokeIndex < 0 ?
				(int)list->size() - count : edit.strokeIndex;
			if (at < 0 || at + count > (int)list->size()) return false;
			list->erase(list->begin() + at, list->begin() + at + count);
			touchLayer(doc, edit.frameIndex, edit.layerIndex);
			return true;
		}
		case JPPaintEdit::ClearLayer:
		{
			std::vector<JPPaintStroke> *list =
				strokeListFor(doc, edit.frameIndex, edit.layerIndex);
			if (list == nullptr) return false;
			// The payload was captured from the same list the clear emptied.
			*list = edit.layer.sharedStrokes;
			touchLayer(doc, edit.frameIndex, edit.layerIndex);
			return true;
		}
		case JPPaintEdit::AddFrame:
		{
			if (!detail::validFrame(doc, edit.frameIndex)) return false;
			if (doc.frames.size() <= 1) return false;
			doc.frames.erase(doc.frames.begin() + edit.frameIndex);
			clampCurrentFrame(doc);
			return true;
		}
		case JPPaintEdit::DeleteFrame:
		{
			if (edit.frameIndex < 0 ||
				edit.frameIndex > (int)doc.frames.size()) return false;
			doc.frames.insert(doc.frames.begin() + edit.frameIndex, edit.frame);
			syncLayerArity(doc);
			clampCurrentFrame(doc);
			return true;
		}
		case JPPaintEdit::MoveFrame:
		{
			if (!detail::validFrame(doc, edit.toIndex)) return false;
			if (!detail::validFrame(doc, edit.fromIndex)) return false;
			JPPaintFrame moved = doc.frames[(std::size_t)edit.toIndex];
			doc.frames.erase(doc.frames.begin() + edit.toIndex);
			doc.frames.insert(doc.frames.begin() + edit.fromIndex, moved);
			clampCurrentFrame(doc);
			return true;
		}
		case JPPaintEdit::SetHold:
		{
			if (!detail::validFrame(doc, edit.frameIndex)) return false;
			doc.frames[(std::size_t)edit.frameIndex].hold =
				std::max(1, edit.previousValue);
			return true;
		}
		case JPPaintEdit::AddLayer:
		{
			// The inverse of AddLayer is DeleteLayer at the same index, so it
			// carries the same last-layer refusal.
			if (edit.layerIndex < 0 ||
				edit.layerIndex >= (int)doc.layers.size()) return false;
			if (doc.layers.size() <= 1) return false;
			doc.layers.erase(doc.layers.begin() + edit.layerIndex);
			for (JPPaintFrame &frame : doc.frames)
			{
				if (edit.layerIndex < (int)frame.layers.size())
				{
					frame.layers.erase(frame.layers.begin() + edit.layerIndex);
				}
			}
			syncLayerArity(doc);
			clampCurrentLayer(doc);
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::DeleteLayer:
		{
			if (edit.layerIndex < 0 ||
				edit.layerIndex > (int)doc.layers.size()) return false;
			doc.layers.insert(doc.layers.begin() + edit.layerIndex, edit.layer);
			// layerCels is in cel order and is what makes undoing a layer delete
			// restore the DRAWING rather than an empty layer.
			for (std::size_t f = 0; f < doc.frames.size(); ++f)
			{
				JPPaintFrame &frame = doc.frames[f];
				const int at = std::min(edit.layerIndex, (int)frame.layers.size());
				frame.layers.insert(frame.layers.begin() + at,
					f < edit.layerCels.size() ? edit.layerCels[f] : JPPaintLayer());
			}
			syncLayerArity(doc);
			clampCurrentLayer(doc);
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::MoveLayer:
		{
			const int count = (int)doc.layers.size();
			if (edit.fromIndex < 0 || edit.fromIndex >= count) return false;
			if (edit.toIndex < 0 || edit.toIndex >= count) return false;
			JPPaintLayerInfo moved = doc.layers[(std::size_t)edit.toIndex];
			doc.layers.erase(doc.layers.begin() + edit.toIndex);
			doc.layers.insert(doc.layers.begin() + edit.fromIndex, moved);
			for (JPPaintFrame &frame : doc.frames)
			{
				if (edit.toIndex >= (int)frame.layers.size()) continue;
				JPPaintLayer cel = frame.layers[(std::size_t)edit.toIndex];
				frame.layers.erase(frame.layers.begin() + edit.toIndex);
				const int at = std::min(edit.fromIndex, (int)frame.layers.size());
				frame.layers.insert(frame.layers.begin() + at, cel);
			}
			syncLayerArity(doc);
			clampCurrentLayer(doc);
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::SetLayerProps:
		{
			if (edit.layerIndex < 0 ||
				edit.layerIndex >= (int)doc.layers.size()) return false;
			JPPaintLayerInfo &target = doc.layers[(std::size_t)edit.layerIndex];
			target.name = edit.previousLayer.name;
			target.visible = edit.previousLayer.visible;
			target.locked = edit.previousLayer.locked;
			target.opacity = std::clamp(edit.previousLayer.opacity, 0.0f, 1.0f);
			target.blendMode = std::clamp(edit.previousLayer.blendMode, 0, 3);
			target.background = edit.previousLayer.background;
			target.sharedStrokes = edit.previousLayer.sharedStrokes;
			bumpAllFrames(doc);
			return true;
		}
		case JPPaintEdit::ReplaceStrokes:
		{
			std::vector<JPPaintStroke> *list =
				strokeListFor(doc, edit.frameIndex, edit.layerIndex);
			if (list == nullptr) return false;
			*list = edit.previousLayer.sharedStrokes;
			touchLayer(doc, edit.frameIndex, edit.layerIndex);
			return true;
		}
		case JPPaintEdit::MergeLayerDown:
		{
			const int upper = edit.layerIndex;
			const int lower = upper - 1;
			if (lower < 0 || lower >= (int)doc.layers.size()) return false;
			doc.layers[(std::size_t)lower] = edit.previousLayer;
			doc.layers.insert(doc.layers.begin() + upper, edit.layer);
			for (std::size_t f = 0; f < doc.frames.size(); ++f)
			{
				JPPaintFrame &frame = doc.frames[f];
				if (lower >= (int)frame.layers.size()) continue;
				frame.layers[(std::size_t)lower] = f < edit.previousLayerCels.size() ?
					edit.previousLayerCels[f] : JPPaintLayer();
				const int at = std::min(upper, (int)frame.layers.size());
				frame.layers.insert(frame.layers.begin() + at,
					f < edit.layerCels.size() ? edit.layerCels[f] : JPPaintLayer());
			}
			syncLayerArity(doc);
			doc.currentLayer = upper;
			bumpAllFrames(doc);
			return true;
		}
		default:
			return false;
		}
	}
}

// Bounded undo/redo history.
//
// Bounded TWICE on purpose: by entry count, so a long session of small edits
// stays cheap, and by total stored points, so a handful of enormous strokes
// cannot quietly hold hundreds of megabytes. Whichever bites first wins.
class JPPaintUndoRing
{
public:
	static constexpr std::size_t kMaxEntries = 64;
	static constexpr std::size_t kMaxPoints = 2000000;

	// Record an edit the caller has ALREADY applied to the document.
	void push(const JPPaintEdit &edit)
	{
		// Anything that was redoable is unreachable now that history has
		// branched, so drop it before accounting for the new entry.
		while (entries.size() > cursor)
		{
			points -= jp_paint::pointCount(entries.back());
			entries.pop_back();
		}
		entries.push_back(edit);
		points += jp_paint::pointCount(edit);
		++cursor;
		evict();
	}

	bool canUndo() const { return cursor > 0; }
	bool canRedo() const { return cursor < entries.size(); }

	bool undo(JPPaintDocument &doc)
	{
		if (!canUndo()) return false;
		if (!jp_paint::revertEdit(doc, entries[cursor - 1])) return false;
		--cursor;
		return true;
	}

	bool redo(JPPaintDocument &doc)
	{
		if (!canRedo()) return false;
		if (!jp_paint::applyEdit(doc, entries[cursor])) return false;
		++cursor;
		return true;
	}

	void clear()
	{
		entries.clear();
		cursor = 0;
		points = 0;
	}

	std::size_t size() const { return entries.size(); }
	std::size_t storedPoints() const { return points; }

private:
	// Drops the oldest entries. Those are the ones furthest from the cursor, so
	// eviction costs the user their oldest undo step rather than their newest.
	void evict()
	{
		while (!entries.empty() &&
			(entries.size() > kMaxEntries || points > kMaxPoints))
		{
			points -= jp_paint::pointCount(entries.front());
			entries.erase(entries.begin());
			if (cursor > 0) --cursor;
		}
	}

	std::vector<JPPaintEdit> entries;
	// entries[0, cursor) are applied to the document; entries[cursor, end) are
	// redoable.
	std::size_t cursor = 0;
	std::size_t points = 0;
};
