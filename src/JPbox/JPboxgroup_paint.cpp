#include "JPboxgroup.h"

#include "../JPgui/jp_button.h"
#include "../JPgui/jp_gl_state.h"
#include "../JPutils/jp_help_content.h"
#include "../JPutils/jp_textfield.h"
#include "../JPutils/jp_tooltip.h"

#include <algorithm>
#include <cmath>

// The paint canvas editor.
//
// Structured after JPboxgroup_mapping_advanced.cpp - a separate translation
// unit, no header of its own, every declaration in JPboxgroup.h - because that
// is the shape this class's editors already take and a second convention would
// be one more thing to keep in step.
//
// Two deliberate departures from the mapping editor, both fixing things it got
// wrong rather than copying them:
//   * the draw is wrapped in a jp_pointer::Scope, so a dropdown or modal drawn
//     over the panel cannot light up the toolbar underneath it;
//   * the panel takes the keyboard, so DEL deletes a CEL instead of falling
//     through to ofApp and deleting the box being drawn on.

namespace
{
	bool isPointInPolygon(const ofVec2f &p, const std::vector<ofVec2f> &polygon)
	{
		if (polygon.size() < 3) return false;
		bool inside = false;
		std::size_t j = polygon.size() - 1;
		for (std::size_t i = 0; i < polygon.size(); ++i)
		{
			if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
				(p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x))
			{
				inside = !inside;
			}
			j = i;
		}
		return inside;
	}

	ofRectangle paintPointBounds(const std::vector<JPPaintPoint> &points,
		float padding = 0.0f)
	{
		if (points.empty()) return ofRectangle();
		float minX = points[0].x, maxX = minX;
		float minY = points[0].y, maxY = minY;
		for (const JPPaintPoint &point : points)
		{
			minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
			minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
		}
		return ofRectangle(minX - padding, minY - padding,
			maxX - minX + padding * 2.0f,
			maxY - minY + padding * 2.0f);
	}

	bool rectanglesOverlap(const ofRectangle &a, const ofRectangle &b)
	{
		return a.getRight() >= b.x && b.getRight() >= a.x &&
			a.getBottom() >= b.y && b.getBottom() >= a.y;
	}

	float pointSegmentDistanceSquared(const ofVec2f &point,
		const ofVec2f &a, const ofVec2f &b)
	{
		const ofVec2f delta = b - a;
		const float lengthSquared = delta.lengthSquared();
		if (lengthSquared <= 1.0e-12f) return point.squareDistance(a);
		const float t = ofClamp((point - a).dot(delta) / lengthSquared, 0.0f, 1.0f);
		return point.squareDistance(a + delta * t);
	}

	bool segmentsIntersect(const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &c, const ofVec2f &d)
	{
		auto cross = [](const ofVec2f &u, const ofVec2f &v) {
			return u.x * v.y - u.y * v.x;
		};
		const ofVec2f ab = b - a;
		const ofVec2f cd = d - c;
		const float denominator = cross(ab, cd);
		if (std::abs(denominator) <= 1.0e-12f) return false;
		const float t = cross(c - a, cd) / denominator;
		const float u = cross(c - a, ab) / denominator;
		return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
	}

	bool strokeTouchesLasso(const JPPaintStroke &stroke,
		const std::vector<ofVec2f> &lasso, const ofRectangle &lassoBounds,
		float canvasAspect)
	{
		if (stroke.points.empty() || lasso.size() < 3) return false;
		float maxWidth = 1.0f;
		for (const JPPaintPoint &point : stroke.points)
			maxWidth = std::max(maxWidth, point.width);
		const float conservativeRadius = stroke.size * maxWidth *
			std::max(1.0f, canvasAspect);
		if (!rectanglesOverlap(paintPointBounds(stroke.points, conservativeRadius),
			lassoBounds)) return false;

		for (const JPPaintPoint &point : stroke.points)
			if (isPointInPolygon(ofVec2f(point.x, point.y), lasso)) return true;

		if (stroke.tool == (int)JPPaintTool::Lasso)
		{
			std::vector<ofVec2f> polygon;
			polygon.reserve(stroke.points.size());
			for (const JPPaintPoint &point : stroke.points)
				polygon.push_back(ofVec2f(point.x, point.y));
			if (isPointInPolygon(lasso.front(), polygon)) return true;
		}

		const std::size_t strokeSegments = stroke.points.size() > 1 ?
			stroke.points.size() - 1 : 0;
		for (std::size_t i = 0; i < strokeSegments; ++i)
		{
			const ofVec2f a(stroke.points[i].x, stroke.points[i].y);
			const ofVec2f b(stroke.points[i + 1].x, stroke.points[i + 1].y);
			const float radius = stroke.size * std::max(1.0f, canvasAspect) *
				std::max(stroke.points[i].width, stroke.points[i + 1].width);
			for (std::size_t p = 0; p + 1 < lasso.size(); ++p)
			{
				if (segmentsIntersect(a, b, lasso[p], lasso[p + 1]) ||
					pointSegmentDistanceSquared(lasso[p], a, b) <= radius * radius)
					return true;
			}
		}

		if (stroke.points.size() == 1)
		{
			const ofVec2f center(stroke.points[0].x, stroke.points[0].y);
			const float radius = stroke.size * std::max(1.0f, canvasAspect) *
				stroke.points[0].width;
			for (std::size_t p = 0; p + 1 < lasso.size(); ++p)
				if (pointSegmentDistanceSquared(center, lasso[p], lasso[p + 1]) <=
					radius * radius) return true;
		}
		return false;
	}

	JPPaintClip makePaintClip(const std::vector<ofVec2f> &lasso,
		bool inverted)
	{
		JPPaintClip clip;
		clip.inverted = inverted;
		clip.points.reserve(lasso.size());
		for (const ofVec2f &point : lasso)
			clip.points.push_back(JPPaintPoint{point.x, point.y, 1.0f});
		return clip;
	}

	// Split a selection non-destructively. Both halves retain the complete
	// original vector and receive complementary raster-time clips. This keeps
	// caps, joins, pressure, rectangles, ellipses and filled lasso shapes
	// pixel-identical instead of rebuilding them from severed point lists.
	void cutStrokesWithLasso(JPbox_paint *box,
		const std::vector<ofVec2f> &lasso,
		std::vector<int> &outSelected,
		ofRectangle &outBounds)
	{
		const int cel = std::clamp(box->document().currentFrame, 0,
			(int)box->document().frames.size() - 1);
		const std::vector<JPPaintStroke> *listPtr =
			jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
		if (listPtr == nullptr || lasso.size() < 3) return;

		// Remove earlier selection pairs that were never transformed. Otherwise
		// their invisible copies retain the full source vector, get split again,
		// and reveal the old lasso path when this new selection is moved.
		const std::vector<JPPaintStroke> orig =
			jp_paint::collapseComplementaryStrokes(*listPtr);
		std::vector<JPPaintStroke> result;
		result.reserve(orig.size() * 2);
		outSelected.clear();
		outBounds = ofRectangle();
		std::vector<JPPaintPoint> lassoPoints;
		lassoPoints.reserve(lasso.size());
		for (const ofVec2f &point : lasso)
			lassoPoints.push_back(JPPaintPoint{point.x, point.y, 1.0f});
		const ofRectangle lassoBounds = paintPointBounds(lassoPoints);

		for (const JPPaintStroke &s : orig)
		{
			// Bucket fills are commands whose result depends on all preceding
			// pixels; moving their seed is not equivalent to moving those pixels.
			// Leave them untouched until fills are materialised in the document.
			if (s.points.empty() || s.tool == (int)JPPaintTool::Fill ||
				s.clips.size() >= 8 ||
				!strokeTouchesLasso(s, lasso, lassoBounds, box->canvasAspect()))
			{
				result.push_back(s);
				continue;
			}

			JPPaintStroke outside = s;
			outside.clips.push_back(makePaintClip(lasso, true));
			result.push_back(std::move(outside));

			JPPaintStroke inside = s;
			inside.clips.push_back(makePaintClip(lasso, false));
			result.push_back(std::move(inside));
			outSelected.push_back((int)result.size() - 1);
		}

		// Keep every pair at the original z position. Moving all selected halves
		// to the tail would change translucent overlaps and eraser ordering even
		// before the user moved the selection.
		if (outSelected.empty()) return;
		outBounds = lassoBounds;
		box->replaceStrokes(cel, box->currentLayer(), result);
	}



	ofVec2f rotatePointAround(const ofVec2f &p, const ofVec2f &center, float angleDeg, float aspect)
	{
		float angleRad = ofDegToRad(angleDeg);
		float cosA = std::cos(angleRad);
		float sinA = std::sin(angleRad);
		float dx = (p.x - center.x) * aspect;
		float dy = p.y - center.y;
		float rx = dx * cosA - dy * sinA;
		float ry = dx * sinA + dy * cosA;
		return ofVec2f(
			center.x + rx / aspect,
			center.y + ry
		);
	}

	template <typename Transform>
	void transformStrokeCoordinates(JPPaintStroke &stroke, Transform transform)
	{
		for (JPPaintPoint &point : stroke.points)
		{
			const ofVec2f changed = transform(ofVec2f(point.x, point.y));
			point.x = changed.x;
			point.y = changed.y;
		}
		for (JPPaintClip &clip : stroke.clips)
		{
			for (JPPaintPoint &point : clip.points)
			{
				const ofVec2f changed = transform(ofVec2f(point.x, point.y));
				point.x = changed.x;
				point.y = changed.y;
			}
		}
	}

	constexpr float kHeaderHeight = 30.0f;
	constexpr float kToolbarHeight = 34.0f;
	constexpr float kTransportHeight = 30.0f;
	constexpr float kPanelPadding = 8.0f;
	// The timeline is an Aseprite style grid: rows are layers, columns are
	// frames. Cells sit flush against each other with 1px separators rather than
	// gaps, which is what makes it read as a grid instead of a row of cards.
	constexpr float kTimelineHeaderHeight = 16.0f;
	constexpr float kTimelineCompositeHeight = 34.0f;
	constexpr float kTimelineRowHeight = 24.0f;
	constexpr float kTimelineGutterWidth = 132.0f;
	constexpr float kTimelineCellWidth = 34.0f;
	constexpr float kMinPanelWidth = 620.0f;
	constexpr float kMinPanelHeight = 460.0f;
	// Toolbar order is a UI concern and deliberately NOT JPPaintTool's numeric
	// order - those ints live in savefiles. Rearranging or adding buttons here
	// can never reach them.
	const JPPaintTool kToolbarTools[] = {
		JPPaintTool::Brush, JPPaintTool::Eraser, JPPaintTool::Line,
		JPPaintTool::Rect, JPPaintTool::Ellipse, JPPaintTool::Lasso,
		JPPaintTool::Fill, JPPaintTool::LassoSelect};
	constexpr int kToolbarToolCount =
		(int)(sizeof(kToolbarTools) / sizeof(kToolbarTools[0]));

	constexpr float kToolButton = 26.0f;
	constexpr float kToolGap = 3.0f;
	constexpr float kGroupGap = 10.0f;
	constexpr float kSizeSliderWidth = 96.0f;
	constexpr float kSwatchWidth = 34.0f;
	constexpr float kActionWidth = 34.0f;
	constexpr float kPickerWidth = 184.0f;
	// Room for the hex field under the swatch rows.
	constexpr float kPickerHeight = 206.0f;
	// Two rows of eight. Enough for a real palette without making the popover
	// taller than the toolbar it hangs from.
	constexpr int kPaletteColumns = 8;
	constexpr int kPaletteRows = 2;
	constexpr int kPaletteSize = kPaletteColumns * kPaletteRows;
	constexpr float kSwatchSize = 16.0f;
	constexpr float kSwatchPitch = 19.0f;
	// Brush radius as a fraction of canvas width. The floor is a hairline at
	// 1080p; the ceiling still lets one dab cover a third of the canvas.
	constexpr float kMinBrush = 0.0008f;
	constexpr float kMaxBrush = 0.16f;

	float panelTop(float y) { return y + kHeaderHeight; }

	// The size slider is logarithmic: a linear one spends most of its travel in
	// sizes nobody uses and makes fine work at the small end impossible.
	float brushFromSlider(float t)
	{
		const float clamped = ofClamp(t, 0.0f, 1.0f);
		return kMinBrush * std::pow(kMaxBrush / kMinBrush, clamped);
	}

	float sliderFromBrush(float size)
	{
		const float clamped = ofClamp(size, kMinBrush, kMaxBrush);
		return std::log(clamped / kMinBrush) / std::log(kMaxBrush / kMinBrush);
	}

	// The chord a lasso will close on release. Dashed so it reads as "not drawn
	// yet" rather than as part of the mark.
	void drawDashedLine(const ofVec2f &from, const ofVec2f &to, float dash)
	{
		const float length = from.distance(to);
		if (length < 0.01f) return;
		const ofVec2f step = (to - from) / length;
		for (float travelled = 0.0f; travelled < length; travelled += dash * 2.0f)
		{
			const ofVec2f a = from + step * travelled;
			const ofVec2f b = from + step * std::min(travelled + dash, length);
			ofDrawLine(a.x, a.y, b.x, b.y);
		}
	}

	void drawCloseGlyph(const ofRectangle &bounds, bool hovered)
	{
		ofPushStyle();
		ofSetColor(hovered ? COL_ACCENT_RED : COL_TEXT_SECONDARY);
		ofSetLineWidth(1.6f);
		const float inset = 5.0f;
		ofDrawLine(bounds.x + inset, bounds.y + inset,
			bounds.getRight() - inset, bounds.getBottom() - inset);
		ofDrawLine(bounds.getRight() - inset, bounds.y + inset,
			bounds.x + inset, bounds.getBottom() - inset);
		ofSetLineWidth(1.0f);
		ofPopStyle();
	}

	// A transparent canvas has to READ as transparent, or a user painting white
	// on nothing cannot tell it apart from white on white.
	void drawCheckerboard(const ofRectangle &rect)
	{
		const float cell = 10.0f;
		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(58, 58, 62);
		ofDrawRectangle(rect);
		ofSetColor(48, 48, 52);
		int row = 0;
		for (float y = rect.y; y < rect.getBottom(); y += cell, ++row)
		{
			const float h = std::min(cell, rect.getBottom() - y);
			int column = 0;
			for (float x = rect.x; x < rect.getRight(); x += cell, ++column)
			{
				if (((row + column) & 1) == 0) continue;
				ofDrawRectangle(x, y, std::min(cell, rect.getRight() - x), h);
			}
		}
		ofPopStyle();
	}
}

// ------------------------------------------------------------------ identity

bool JPboxgroup::isPaintBox(JPbox *box) const
{
	return dynamic_cast<JPbox_paint *>(box) != nullptr;
}

JPbox_paint *JPboxgroup::getPaintEditBox()
{
	// An INDEX plus a group path, never a pointer. The box can be deleted,
	// reordered or replaced by a cue draft between two frames; re-resolving
	// every time is what makes all three safe without a single guard.
	if (!paintEditActive) return nullptr;
	if (paintTargetIndex != getCurrentViewSelectedIndex()) return nullptr;
	if (paintTargetGroupPath != activeGroupPath) return nullptr;
	return dynamic_cast<JPbox_paint *>(getInspectorBox());
}

const JPbox_paint *JPboxgroup::getPaintEditBox() const
{
	return const_cast<JPboxgroup *>(this)->getPaintEditBox();
}

bool JPboxgroup::isPaintEditActive() const
{
	return paintEditActive;
}

bool JPboxgroup::togglePaintEdit()
{
	if (paintEditActive)
	{
		endPaintEdit();
		return true;
	}
	if (!isPaintBox(getInspectorBox())) return false;

	paintTargetIndex = getCurrentViewSelectedIndex();
	paintTargetGroupPath = activeGroupPath;
	paintEditActive = true;
	paintViewZoom = 1.0f;
	paintViewCenter.set(0.5f, 0.5f);
	paintViewPanning = false;
	paintDragMode = PAINT_DRAG_NONE;
	paintPickerOpen = false;
	paintFilmstripScroll = 0.0f;
	if (paintPanelW < kMinPanelWidth || paintPanelH < kMinPanelHeight)
	{
		setupDefaultPaintPanelLayout();
	}
	clampPaintPanelLayout();
	return true;
}

void JPboxgroup::endPaintEdit()
{
	// A stroke in flight is discarded rather than committed: closing the panel
	// mid-drag is a cancel, and committing would leave a mark the user never
	// finished making.
	if (JPbox_paint *box = getPaintEditBox())
	{
		box->liveStrokeActive = false;
		box->liveStroke = JPPaintStroke();
	}
	paintEditActive = false;
	paintTargetIndex = -1;
	paintTargetGroupPath.clear();
	paintPanelDragging = false;
	paintPanelResizing = false;
	paintPanelPointerCaptured = false;
	paintViewPanning = false;
	paintViewPanButton = -1;
	paintDragMode = PAINT_DRAG_NONE;
	paintDragCelFrom = -1;
	paintDragCelTo = -1;
	paintPickerOpen = false;
	paintHelpOpen = false;
	paintHelpScroll = 0.0f;
	cancelPaintHex();
	cancelPaintLayerRename();
}

void JPboxgroup::dismissPaintTopLayer()
{
	if (!paintEditActive) return;
	// Topmost first, and a focused field is the innermost thing of all.
	if (paintRenamingLayer >= 0) { cancelPaintLayerRename(); return; }
	if (paintHexFocus) { cancelPaintHex(); return; }
	// The shortcuts dialog is a modal, so it outranks the picker.
	if (paintHelpOpen)
	{
		closePaintHelp();
		return;
	}
	if (paintPickerOpen)
	{
		paintPickerOpen = false;
		return;
	}
	if (paintSelectionActive)
	{
		clearPaintSelection();
		return;
	}
	endPaintEdit();
}

void JPboxgroup::markPaintChanged()
{
	markCueDraftDirty(cueSelectedIndex(), CUE_DIRTY_PARAMS);
	if (isCueDraftMode()) updateCueDraftGraph();
}

// -------------------------------------------------------------------- layout

void JPboxgroup::setupDefaultPaintPanelLayout()
{
	paintPanelW = std::max(kMinPanelWidth, std::min(760.0f, ofGetWidth() * 0.5f));
	paintPanelH = std::max(kMinPanelHeight, std::min(640.0f, ofGetHeight() * 0.72f));
	paintPanelX = 24.0f;
	paintPanelY = tabBarOffsetY + 48.0f;
}

void JPboxgroup::clampPaintPanelLayout()
{
	const float margin = 8.0f;
	const float topMargin = tabBarOffsetY + 40.0f;
	const float maxWidth = std::max(kMinPanelWidth, ofGetWidth() - margin * 2.0f);
	const float maxHeight = std::max(kMinPanelHeight,
		ofGetHeight() - topMargin - margin);
	paintPanelW = ofClamp(paintPanelW, kMinPanelWidth, maxWidth);
	paintPanelH = ofClamp(paintPanelH, kMinPanelHeight, maxHeight);
	paintPanelX = ofClamp(paintPanelX, margin,
		std::max(margin, ofGetWidth() - paintPanelW - margin));
	paintPanelY = ofClamp(paintPanelY, topMargin,
		std::max(topMargin, ofGetHeight() - paintPanelH - margin));
	clampPaintView();
	clampPaintTimelineScroll();
}

void JPboxgroup::setPaintPanelLayout(float x, float y, float w, float h)
{
	paintPanelX = x;
	paintPanelY = y;
	paintPanelW = w;
	paintPanelH = h;
	clampPaintPanelLayout();
}

void JPboxgroup::getPaintPanelLayout(float &x, float &y, float &w, float &h) const
{
	x = paintPanelX;
	y = paintPanelY;
	w = paintPanelW;
	h = paintPanelH;
}

ofRectangle JPboxgroup::getPaintPanelBounds() const
{
	// An EMPTY rect when closed: JPSurfaceStack::blockedAt skips zero area
	// rects, which is how a closed surface stops blocking clicks.
	if (!paintEditActive) return ofRectangle();
	ofRectangle bounds(paintPanelX, paintPanelY, paintPanelW, paintPanelH);
	if (paintPickerOpen)
	{
		// The picker is not its own surface. Growing the panel's bounds to
		// cover it is what blocks clicks behind the popover, and the panel's
		// close lambda dismisses the picker first - so ESC still peels one
		// layer at a time without a second z-order constant.
		bounds = bounds.getUnion(getPaintPickerBounds());
	}
	return bounds;
}

bool JPboxgroup::paintPanKeyHeld() const
{
	// Read directly, NOT through isSpacePanHeld/spacePanAllowed: those consult
	// wantsKeyCapture(), which is already true whenever this panel is open, so
	// they can never fire here. Same shape as the mapping editor's shift read.
	//
	// Ctrl rather than space because space is play/pause in this panel. Cmd is
	// accepted too so the gesture is the same on a Mac keyboard.
	return ofGetKeyPressed(OF_KEY_CONTROL) || ofGetKeyPressed(OF_KEY_COMMAND);
}

bool JPboxgroup::mouseOverPaintPanel() const
{
	return paintEditActive &&
		getPaintPanelBounds().inside((float)ofGetMouseX(), (float)ofGetMouseY());
}

bool JPboxgroup::mouseOverPaintPanelHeader() const
{
	return paintEditActive && ofGetMouseX() >= paintPanelX &&
		ofGetMouseX() <= paintPanelX + paintPanelW &&
		ofGetMouseY() >= paintPanelY &&
		ofGetMouseY() <= paintPanelY + kHeaderHeight;
}

bool JPboxgroup::isPaintHelpOpen() const
{
	return paintEditActive && paintHelpOpen;
}

void JPboxgroup::closePaintHelp()
{
	paintHelpOpen = false;
	paintHelpScroll = 0.0f;
}

void JPboxgroup::setHelpLanguageProvider(std::function<int()> provider)
{
	helpLanguageProvider = std::move(provider);
}

ofRectangle JPboxgroup::getPaintHelpRect() const
{
	if (!isPaintHelpOpen()) return ofRectangle();
	// Centred on the PANEL, not the window, like the MIDI conflict prompt - it
	// belongs to this panel and reads as part of it.
	const float width = std::min(560.0f, std::max(320.0f, paintPanelW - 40.0f));
	const float height = std::min(430.0f, std::max(200.0f, paintPanelH - 60.0f));
	return ofRectangle(paintPanelX + (paintPanelW - width) * 0.5f,
		paintPanelY + (paintPanelH - height) * 0.5f, width, height);
}

ofRectangle JPboxgroup::getPaintHelpCloseBounds() const
{
	const ofRectangle bounds = getPaintHelpRect();
	if (bounds.width <= 0.0f) return ofRectangle();
	return ofRectangle(bounds.getRight() - 26.0f, bounds.y + 7.0f, 18.0f, 18.0f);
}

ofRectangle JPboxgroup::getPaintHelpIconBounds() const
{
	const float size = 18.0f;
	// Immediately left of the panel's close cross.
	return ofRectangle(paintPanelX + paintPanelW - 10.0f - size * 2.0f - 6.0f,
		paintPanelY + (kHeaderHeight - size) * 0.5f, size, size);
}

ofRectangle JPboxgroup::getPaintPanelCloseBounds() const
{
	const float size = 18.0f;
	return ofRectangle(paintPanelX + paintPanelW - 10.0f - size,
		paintPanelY + (kHeaderHeight - size) * 0.5f, size, size);
}

bool JPboxgroup::mouseOverPaintPanelResizeHandle() const
{
	const float grip = 16.0f;
	return paintEditActive &&
		ofRectangle(paintPanelX + paintPanelW - grip,
			paintPanelY + paintPanelH - grip, grip, grip)
			.inside((float)ofGetMouseX(), (float)ofGetMouseY());
}

ofRectangle JPboxgroup::getPaintToolBounds(int tool) const
{
	const float y = panelTop(paintPanelY) + (kToolbarHeight - kToolButton) * 0.5f;
	return ofRectangle(paintPanelX + kPanelPadding +
		tool * (kToolButton + kToolGap), y, kToolButton, kToolButton);
}

ofRectangle JPboxgroup::getPaintSizeSliderBounds() const
{
	const ofRectangle last = getPaintToolBounds(kToolbarToolCount - 1);
	return ofRectangle(last.getRight() + kGroupGap, last.y + 3.0f,
		kSizeSliderWidth, kToolButton - 6.0f);
}

ofRectangle JPboxgroup::getPaintColorSwatchBounds() const
{
	const ofRectangle slider = getPaintSizeSliderBounds();
	return ofRectangle(slider.getRight() + kGroupGap,
		getPaintToolBounds(0).y, kSwatchWidth, kToolButton);
}

ofRectangle JPboxgroup::getPaintQuickSwatchBounds(int index) const
{
	const ofRectangle swatch = getPaintColorSwatchBounds();
	const float size = 18.0f;
	const float gap = 4.0f;
	const float startX = swatch.getRight() + 6.0f;
	const float y = swatch.y + (swatch.height - size) * 0.5f;
	return ofRectangle(startX + (float)index * (size + gap), y, size, size);
}

ofRectangle JPboxgroup::getPaintActionBounds(int action) const
{
	// Right aligned, indexed FROM the right, so adding one later shifts the
	// group instead of colliding with the tools on the left.
	const int fromRight = PAINT_ACTION_COUNT - 1 - action;
	return ofRectangle(paintPanelX + paintPanelW - kPanelPadding - kActionWidth -
		fromRight * (kActionWidth + kToolGap),
		getPaintToolBounds(0).y, kActionWidth, kToolButton);
}

float JPboxgroup::paintTimelineHeight() const
{
	const JPbox_paint *box = getPaintEditBox();
	const int layers = box != nullptr ?
		(int)box->document().layers.size() : 1;
	const float wanted = kTimelineHeaderHeight + kTimelineCompositeHeight +
		(float)layers * kTimelineRowHeight + 4.0f;
	// Capped so a tall stack scrolls instead of eating the canvas it is there to
	// serve. Without this, twelve layers would leave nothing to draw on.
	const float cap = std::max(120.0f, paintPanelH * 0.45f);
	return std::min(wanted, cap);
}

ofRectangle JPboxgroup::getPaintTimelineBounds() const
{
	const float height = paintTimelineHeight();
	return ofRectangle(paintPanelX + kPanelPadding,
		paintPanelY + paintPanelH - kPanelPadding - height,
		paintPanelW - kPanelPadding * 2.0f, height);
}

ofRectangle JPboxgroup::getPaintTimelineGutterBounds() const
{
	const ofRectangle timeline = getPaintTimelineBounds();
	return ofRectangle(timeline.x, timeline.y,
		std::min(kTimelineGutterWidth, timeline.width * 0.5f), timeline.height);
}

ofRectangle JPboxgroup::getPaintTimelineGridBounds() const
{
	const ofRectangle timeline = getPaintTimelineBounds();
	const ofRectangle gutter = getPaintTimelineGutterBounds();
	return ofRectangle(gutter.getRight() + 1.0f, timeline.y,
		std::max(1.0f, timeline.getRight() - gutter.getRight() - 1.0f),
		timeline.height);
}

// Where the layer rows begin - below the frame numbers and the composite row.
float JPboxgroup::paintLayerRowsTop() const
{
	return getPaintTimelineBounds().y + kTimelineHeaderHeight +
		kTimelineCompositeHeight;
}

ofRectangle JPboxgroup::getPaintTransportBounds(int slot) const
{
	const float top = getPaintTimelineBounds().y - kTransportHeight + 3.0f;
	const float height = kTransportHeight - 8.0f;
	float x = paintPanelX + kPanelPadding;
	// Widths in slot order. Laid out by accumulation rather than by a formula
	// because they genuinely differ - a play icon and a "PING" label are not
	// the same size and forcing them to be wastes the row.
	static const float widths[PAINT_TRANSPORT_COUNT] =
		{24.0f, 30.0f, 24.0f, 24.0f, 52.0f, 50.0f, 66.0f, 42.0f};
	for (int i = 0; i < PAINT_TRANSPORT_COUNT; ++i)
	{
		if (i == slot) return ofRectangle(x, top, widths[i], height);
		x += widths[i] +
			(i == PAINT_TRANSPORT_DIRECTION ? kGroupGap : kToolGap);
	}
	return ofRectangle();
}

ofRectangle JPboxgroup::getPaintCanvasArea() const
{
	const float top = panelTop(paintPanelY) + kToolbarHeight + kPanelPadding;
	const float bottom = getPaintTimelineBounds().y - kTransportHeight;
	// Still the ONE function that owns the split, so the timeline's variable
	// height is the only thing anything else has to know about.
	return ofRectangle(paintPanelX + kPanelPadding, top,
		paintPanelW - kPanelPadding * 2.0f,
		std::max(40.0f, bottom - top - kPanelPadding));
}

JPViewTransform JPboxgroup::paintView() const
{
	JPViewTransform view;
	const JPbox_paint *box = getPaintEditBox();
	view.preview = jp_view::fit(getPaintCanvasArea(),
		box != nullptr ? box->canvasAspect() : 16.0f / 9.0f);
	view.zoom = paintViewZoom;
	view.center = paintViewCenter;
	return view;
}

void JPboxgroup::clampPaintView()
{
	paintViewZoom = ofClamp(paintViewZoom, 1.0f, 24.0f);
	// At zoom 1 the canvas exactly fills the preview, so any pan would show
	// dead space on one side. Above it, allow travel but never so far that the
	// canvas leaves the viewport entirely.
	const float slack = 0.5f - 0.5f / paintViewZoom;
	paintViewCenter.x = ofClamp(paintViewCenter.x, 0.5f - slack, 0.5f + slack);
	paintViewCenter.y = ofClamp(paintViewCenter.y, 0.5f - slack, 0.5f + slack);
}

ofRectangle JPboxgroup::getPaintPickerBounds() const
{
	const ofRectangle swatch = getPaintColorSwatchBounds();
	float x = swatch.x;
	float y = swatch.getBottom() + 4.0f;
	// Keep the popover on screen even when the panel is parked against an edge.
	x = ofClamp(x, 4.0f, std::max(4.0f, ofGetWidth() - kPickerWidth - 4.0f));
	y = ofClamp(y, 4.0f, std::max(4.0f, ofGetHeight() - kPickerHeight - 4.0f));
	return ofRectangle(x, y, kPickerWidth, kPickerHeight);
}

ofRectangle JPboxgroup::getPaintSwatchBounds(int index) const
{
	if (index < 0 || index >= kPaletteSize) return ofRectangle();
	const ofRectangle picker = getPaintPickerBounds();
	const float top = picker.y + 8.0f + 120.0f + 8.0f;
	const int column = index % kPaletteColumns;
	const int row = index / kPaletteColumns;
	return ofRectangle(picker.x + 8.0f + column * kSwatchPitch,
		top + row * kSwatchPitch, kSwatchSize, kSwatchSize);
}

ofRectangle JPboxgroup::getPaintPaletteAddBounds() const
{
	const ofRectangle picker = getPaintPickerBounds();
	return ofRectangle(picker.x + 8.0f + kPaletteColumns * kSwatchPitch,
		picker.y + 8.0f + 120.0f + 8.0f, kSwatchSize, kSwatchSize);
}

// ------------------------------------------------------------ timeline cells

ofRectangle JPboxgroup::getPaintFrameHeaderBounds(int frame) const
{
	const ofRectangle grid = getPaintTimelineGridBounds();
	return ofRectangle(grid.x + (float)frame * kTimelineCellWidth -
		paintFilmstripScroll, grid.y, kTimelineCellWidth,
		kTimelineHeaderHeight);
}

ofRectangle JPboxgroup::getPaintCompositeCellBounds(int frame) const
{
	const ofRectangle header = getPaintFrameHeaderBounds(frame);
	return ofRectangle(header.x, header.getBottom(), header.width,
		kTimelineCompositeHeight);
}

ofRectangle JPboxgroup::getPaintCellBounds(int frame, int row) const
{
	const ofRectangle header = getPaintFrameHeaderBounds(frame);
	return ofRectangle(header.x,
		paintLayerRowsTop() + (float)row * kTimelineRowHeight -
			paintTimelineScrollY, header.width, kTimelineRowHeight);
}

ofRectangle JPboxgroup::getPaintGutterRowBounds(int row) const
{
	const ofRectangle gutter = getPaintTimelineGutterBounds();
	return ofRectangle(gutter.x,
		paintLayerRowsTop() + (float)row * kTimelineRowHeight -
			paintTimelineScrollY, gutter.width, kTimelineRowHeight);
}

ofRectangle JPboxgroup::getPaintLayerEyeBounds(int row) const
{
	const ofRectangle bounds = getPaintGutterRowBounds(row);
	return ofRectangle(bounds.x + 3.0f, bounds.y + 2.0f, 15.0f, 15.0f);
}

ofRectangle JPboxgroup::getPaintLayerBadgeBounds(int row) const
{
	const ofRectangle bounds = getPaintGutterRowBounds(row);
	return ofRectangle(bounds.getRight() - 42.0f, bounds.y + 3.0f, 22.0f, 13.0f);
}

ofRectangle JPboxgroup::getPaintLayerDeleteBounds(int row) const
{
	const ofRectangle bounds = getPaintGutterRowBounds(row);
	return ofRectangle(bounds.getRight() - 17.0f, bounds.y + 3.0f, 14.0f, 13.0f);
}

ofRectangle JPboxgroup::getPaintLayerOpacityBounds(int row) const
{
	const ofRectangle bounds = getPaintGutterRowBounds(row);
	return ofRectangle(bounds.x + 3.0f, bounds.getBottom() - 6.0f,
		bounds.width - 6.0f, 4.0f);
}

ofRectangle JPboxgroup::getPaintAddCelBounds() const
{
	const JPbox_paint *box = getPaintEditBox();
	const int count = box != nullptr ? (int)box->document().frames.size() : 0;
	// The header cell just past the last frame, so "add a frame" is where the
	// next frame will appear.
	return getPaintFrameHeaderBounds(count);
}

ofRectangle JPboxgroup::getPaintLayerNameBounds(int row) const
{
	const ofRectangle bounds = getPaintGutterRowBounds(row);
	const ofRectangle eye = getPaintLayerEyeBounds(row);
	const ofRectangle badge = getPaintLayerBadgeBounds(row);
	// Between the eye and the BG badge, and above the opacity bar: the strip that
	// shows the name is the strip that starts a rename.
	return ofRectangle(eye.getRight() + 2.0f, bounds.y + 1.0f,
		std::max(10.0f, badge.x - eye.getRight() - 4.0f), 16.0f);
}

ofRectangle JPboxgroup::getPaintHexFieldBounds() const
{
	const ofRectangle picker = getPaintPickerBounds();
	// Under the two swatch rows.
	const float top = picker.y + 8.0f + 120.0f + 8.0f +
		kPaletteRows * kSwatchPitch + 3.0f;
	return ofRectangle(picker.x + 8.0f, top, picker.width - 16.0f, 18.0f);
}

ofRectangle JPboxgroup::getPaintLayerAddBounds() const
{
	const ofRectangle gutter = getPaintTimelineGutterBounds();
	return ofRectangle(gutter.getRight() - 4.0f - 14.0f, gutter.y + 1.0f,
		14.0f, 14.0f);
}

int JPboxgroup::paintLayerAtRow(int row) const
{
	const JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return -1;
	const int count = (int)box->document().layers.size();
	if (row < 0 || row >= count) return -1;
	// Row 0 is the TOP of the stack, which is the LAST document index - index 0
	// is the bottom, because compositing walks forward.
	return count - 1 - row;
}

int JPboxgroup::paintLayerRowAtScreen(const ofVec2f &mouse) const
{
	const JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return -1;
	const ofRectangle gutter = getPaintTimelineGutterBounds();
	const float rowsTop = paintLayerRowsTop();
	for (int row = 0; row < (int)box->document().layers.size(); ++row)
	{
		const ofRectangle bounds = getPaintGutterRowBounds(row);
		// A row scrolled out of the timeline is not clickable, the same way it is
		// not drawn.
		if (bounds.getBottom() > gutter.getBottom() + 0.5f) break;
		if (bounds.y < rowsTop - 0.5f) continue;
		if (bounds.inside(mouse)) return row;
	}
	return -1;
}

int JPboxgroup::paintCelAtScreen(const ofVec2f &mouse) const
{
	const JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return -1;
	const ofRectangle grid = getPaintTimelineGridBounds();
	for (int frame = 0; frame < (int)box->document().frames.size(); ++frame)
	{
		const ofRectangle header = getPaintFrameHeaderBounds(frame);
		if (header.getRight() < grid.x || header.x > grid.getRight()) continue;
		// The number row and the composite thumbnail under it are one target:
		// both mean "this frame".
		if (header.inside(mouse)) return frame;
		if (getPaintCompositeCellBounds(frame).inside(mouse)) return frame;
	}
	return -1;
}

bool JPboxgroup::paintCellAtScreen(const ofVec2f &mouse, int &frame,
	int &row) const
{
	const JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return false;
	const int foundRow = paintLayerRowAtScreen(
		ofVec2f(getPaintTimelineGutterBounds().getCenter().x, mouse.y));
	if (foundRow < 0) return false;
	const ofRectangle grid = getPaintTimelineGridBounds();
	for (int f = 0; f < (int)box->document().frames.size(); ++f)
	{
		const ofRectangle cell = getPaintCellBounds(f, foundRow);
		if (cell.getRight() < grid.x || cell.x > grid.getRight()) continue;
		if (!cell.inside(mouse)) continue;
		frame = f;
		row = foundRow;
		return true;
	}
	return false;
}

void JPboxgroup::clampPaintTimelineScroll()
{
	const JPbox_paint *box = getPaintEditBox();
	if (box == nullptr)
	{
		paintFilmstripScroll = 0.0f;
		paintTimelineScrollY = 0.0f;
		return;
	}
	// One column of slack past the last frame, for the add button.
	const float contentWidth =
		(float)(box->document().frames.size() + 1) * kTimelineCellWidth;
	paintFilmstripScroll = ofClamp(paintFilmstripScroll, 0.0f,
		std::max(0.0f, contentWidth - getPaintTimelineGridBounds().width));

	const float contentHeight =
		(float)box->document().layers.size() * kTimelineRowHeight;
	const float visibleHeight = std::max(0.0f,
		getPaintTimelineBounds().getBottom() - paintLayerRowsTop());
	paintTimelineScrollY = ofClamp(paintTimelineScrollY, 0.0f,
		std::max(0.0f, contentHeight - visibleHeight));
}

// --------------------------------------------------------------------- draw

void JPboxgroup::drawPaintCanvas(JPbox_paint *box)
{
	const JPViewTransform view = paintView();
	const ofRectangle area = getPaintCanvasArea();
	const ofRectangle canvas = view.canvasRect();
	const JPPaintDocument &doc = box->document();
	const int current = box->currentCel();
	const int count = (int)doc.frames.size();

	// Every cel about to be drawn is rasterized HERE, before the scissor is
	// armed. A rebuild binds its own framebuffer while the scissor box is still
	// set to this panel's rectangle, which would clip the rebuild to a corner
	// of the cel and cache the result. warmCel also forces the MSAA resolve,
	// which is subject to the scissor too - see the note in ensureRaster.
	box->warmCel(current);
	// Resolved out here for the same reason the cels are: asking a framebuffer
	// for its texture can trigger a multisample resolve blit, and a blit obeys
	// the scissor. This one belongs to ANOTHER box, so our clip must not touch
	// it.
	ofTexture *referenceTex = nullptr;
	if (paintReferenceVisible)
	{
		if (ofFbo *reference = box->referenceFbo())
		{
			referenceTex = &reference->getTexture();
		}
	}
	for (int offset = 1; offset <= doc.onionBefore; ++offset)
	{
		if (current - offset >= 0) box->warmCel(current - offset);
	}
	for (int offset = 1; offset <= doc.onionAfter; ++offset)
	{
		if (current + offset < count) box->warmCel(current + offset);
	}

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(COL_BG_DARK);
	ofDrawRectangle(area);
	{
		// Zooming pushes the canvas past the viewport on purpose, so it has to
		// be clipped or it paints over the toolbar and the filmstrip.
		jp_gl::ScopedScissor clip(area);

		drawCheckerboard(canvas);
		if (doc.bgA > 0.0f)
		{
			ofSetColor((int)(doc.bgR * 255), (int)(doc.bgG * 255),
				(int)(doc.bgB * 255), (int)(doc.bgA * 255));
			ofDrawRectangle(canvas);
		}

		// The reference is a tracing aid: dim, under everything, and never part
		// of what the box outputs.
		if (referenceTex != nullptr)
		{
			ofEnableAlphaBlending();
			ofSetColor(255, 255, 255, 90);
			referenceTex->draw(canvas);
		}

		// Onion skins: warm behind, cool ahead, fading with distance. The two
		// hues are what make a ghost readable as past or future at a glance.
		//
		// The tint's RGB is scaled by the fade as well as its alpha. Cels are
		// premultiplied, so scaling only the alpha would leave the colour at
		// full strength and every ghost would come out near-opaque - the onion
		// opacity setting would do almost nothing.
		auto ghostTint = [](int r, int g, int b, float fade) {
			return ofColor((int)(r * fade), (int)(g * fade), (int)(b * fade),
				(int)(255.0f * fade));
		};
		ofEnableAlphaBlending();
		for (int offset = doc.onionBefore; offset >= 1; --offset)
		{
			const int index = current - offset;
			if (index < 0) continue;
			const float fade = doc.onionOpacity / (float)offset;
			box->drawCel(index, canvas.x, canvas.y, canvas.width, canvas.height,
				ghostTint(255, 120, 90, fade));
		}
		for (int offset = doc.onionAfter; offset >= 1; --offset)
		{
			const int index = current + offset;
			if (index >= count) continue;
			const float fade = doc.onionOpacity / (float)offset;
			box->drawCel(index, canvas.x, canvas.y, canvas.width, canvas.height,
				ghostTint(90, 160, 255, fade));
		}

		const bool shifting = paintSelectionActive && (paintSelectionDragging || paintSelectionRotating || paintSelectionScaling);
		bool drewSelectionPreview = false;
		if (shifting)
		{
			const int cel = std::clamp(box->document().currentFrame, 0, (int)box->document().frames.size() - 1);
			const std::vector<JPPaintStroke> *list = jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
			if (list != nullptr)
			{
				std::vector<JPPaintStroke> previewStrokes = *list;
				ofVec2f centerVal = paintSelectionBounds.getCenter();
				float aspect = canvas.height > 0.0f ? (canvas.width / canvas.height) : 1.0f;
				for (int idx : paintSelectedStrokeIndices)
				{
					if (idx >= 0 && idx < (int)previewStrokes.size())
					{
						transformStrokeCoordinates(previewStrokes[(std::size_t)idx], [&](const ofVec2f &point) {
							ofVec2f p = point;
							if (paintSelectionScaling)
							{
								p.x = centerVal.x + (p.x - centerVal.x) * paintSelectionScale;
								p.y = centerVal.y + (p.y - centerVal.y) * paintSelectionScale;
							}
							if (paintSelectionRotating)
							{
								p = rotatePointAround(p, centerVal, paintSelectionRotation, aspect);
							}
							if (paintSelectionDragging)
							{
								p += paintSelectionDragOffset;
							}
							return p;
						});
					}
				}
				box->drawCelPreview(current, box->currentLayer(), previewStrokes,
					canvas.x, canvas.y, canvas.width, canvas.height,
					ofColor(255, 255, 255, 255));
				drewSelectionPreview = true;
			}
		}

		if (!drewSelectionPreview)
		{
			box->drawCel(current, canvas.x, canvas.y, canvas.width, canvas.height,
				ofColor(255, 255, 255, 255));
		}

		if (box->liveStrokeActive && !box->liveStroke.points.empty())
		{
			const JPPaintStroke &live = box->liveStroke;
			if (live.tool == (int)JPPaintTool::LassoSelect)
			{
				// Bypass preview: selection outline is drawn below
			}
			else if (live.tool == (int)JPPaintTool::Lasso)
			{
				// An OUTLINE, not the fill. The shared raster renderer closes and
				// fills a Lasso unconditionally, which is right for a committed
				// stroke and wrong for one still being drawn - it filled the gap
				// the user had not drawn yet, every frame. Previewing is a UI
				// concern, so it is done here rather than by teaching the
				// rasterizer about a mode it should not have.
				const JPViewTransform view = paintView();
				ofPushStyle();
				ofNoFill();
				// A fixed screen width: this is a guide, so it must not scale with
				// the brush.
				ofSetLineWidth(2.0f);
				ofSetColor((int)(live.r * 255), (int)(live.g * 255),
					(int)(live.b * 255), 235);
				ofPolyline sweep;
				for (const JPPaintPoint &point : live.points)
				{
					const ofVec2f at = view.toScreen(ofVec2f(point.x, point.y));
					sweep.addVertex(at.x, at.y);
				}
				sweep.draw();
				const ofVec2f first =
					view.toScreen(ofVec2f(live.points.front().x,
						live.points.front().y));
				if (live.points.size() >= 2)
				{
					const ofVec2f last =
						view.toScreen(ofVec2f(live.points.back().x,
							live.points.back().y));
					ofSetColor((int)(live.r * 255), (int)(live.g * 255),
						(int)(live.b * 255), 130);
					ofSetLineWidth(1.5f);
					drawDashedLine(last, first, 5.0f);
				}
				// The point it will snap to.
				ofFill();
				ofSetColor(COL_TEXT_PRIMARY);
				ofDrawCircle(first.x, first.y, 3.0f);
				ofSetLineWidth(1.0f);
				ofPopStyle();
			}
			else
			{
				// An eraser has no colour to preview with, so it shows as a dark
				// translucent ribbon - visible against a drawing, and unmistakably
				// not a mark being added.
				if (live.erase) ofSetColor(20, 20, 24, 150);
				else ofSetColor((int)(live.r * 255), (int)(live.g * 255),
					(int)(live.b * 255), (int)(live.a * 255));
				box->drawStrokePreview(live, canvas.x, canvas.y,
					canvas.width, canvas.height);
			}
		}

		// Draw active selection outline!
		if (paintSelectionActive && !paintSelectionPath.empty())
		{
			const JPViewTransform view = paintView();
			ofPushStyle();
			ofNoFill();
			ofSetLineWidth(1.0f);
			ofSetColor(COL_ACCENT_CYAN, 240);
			ofPolyline selectionLine;
			ofVec2f centerVal = paintSelectionBounds.getCenter();
			float aspect = view.canvasRect().height > 0.0f ? (view.canvasRect().width / view.canvasRect().height) : 1.0f;
			for (const auto &p : paintSelectionPath)
			{
				ofVec2f pt = p;
				if (paintSelectionScaling)
				{
					pt.x = centerVal.x + (pt.x - centerVal.x) * paintSelectionScale;
					pt.y = centerVal.y + (pt.y - centerVal.y) * paintSelectionScale;
				}
				if (paintSelectionRotating)
				{
					pt = rotatePointAround(pt, centerVal, paintSelectionRotation, aspect);
				}
				if (paintSelectionDragging)
				{
					pt += paintSelectionDragOffset;
				}
				const ofVec2f scr = view.toScreen(pt);
				selectionLine.addVertex(scr.x, scr.y);
			}
			
			// Draw dashed outline
			const auto &vertices = selectionLine.getVertices();
			for (std::size_t i = 0; i < vertices.size() - 1; ++i)
			{
				drawDashedLine(vertices[i], vertices[i + 1], 4.0f);
			}
			
			// Draw rotation handle
			ofVec2f topCenter = ofVec2f(centerVal.x, centerVal.y + (paintSelectionBounds.y - centerVal.y) * (paintSelectionScaling ? paintSelectionScale : 1.0f));
			if (paintSelectionRotating)
			{
				topCenter = rotatePointAround(topCenter, centerVal, paintSelectionRotation, aspect);
			}
			if (paintSelectionDragging)
			{
				topCenter += paintSelectionDragOffset;
			}
			ofVec2f centerScr = view.toScreen(centerVal + (paintSelectionDragging ? paintSelectionDragOffset : ofVec2f(0.0f, 0.0f)));
			ofVec2f topCenterScr = view.toScreen(topCenter);
			ofVec2f dir = (topCenterScr - centerScr).getNormalized();
			if (dir.lengthSquared() == 0.0f) dir.set(0.0f, -1.0f);
			ofVec2f handleScr = topCenterScr + dir * 20.0f;
			
			ofSetLineWidth(1.0f);
			drawDashedLine(topCenterScr, handleScr, 3.0f);
			ofFill();
			const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
			const bool handleHovered = mouse.distance(handleScr) < 8.0f;
			ofSetColor(handleHovered || paintSelectionRotating ? COL_ACCENT_CYAN : COL_TEXT_PRIMARY);
			ofDrawCircle(handleScr.x, handleScr.y, 4.0f);
			ofNoFill();
			ofSetColor(COL_ACCENT_CYAN, 240);
			ofDrawCircle(handleScr.x, handleScr.y, 4.0f);
			
			// Draw 4 scale handles on the corners
			auto getCornerScr = [&](float x, float y) {
				ofVec2f pt = centerVal + (ofVec2f(x, y) - centerVal) * (paintSelectionScaling ? paintSelectionScale : 1.0f);
				if (paintSelectionRotating)
				{
					pt = rotatePointAround(pt, centerVal, paintSelectionRotation, aspect);
				}
				if (paintSelectionDragging)
				{
					pt += paintSelectionDragOffset;
				}
				return view.toScreen(pt);
			};
			
			ofVec2f tlScr = getCornerScr(paintSelectionBounds.x, paintSelectionBounds.y);
			ofVec2f trScr = getCornerScr(paintSelectionBounds.getRight(), paintSelectionBounds.y);
			ofVec2f blScr = getCornerScr(paintSelectionBounds.x, paintSelectionBounds.getBottom());
			ofVec2f brScr = getCornerScr(paintSelectionBounds.getRight(), paintSelectionBounds.getBottom());
			
			ofFill();
			ofSetColor(COL_TEXT_PRIMARY);
			ofDrawRectangle(tlScr.x - 3, tlScr.y - 3, 6, 6);
			ofDrawRectangle(trScr.x - 3, trScr.y - 3, 6, 6);
			ofDrawRectangle(blScr.x - 3, blScr.y - 3, 6, 6);
			ofDrawRectangle(brScr.x - 3, brScr.y - 3, 6, 6);
			
			ofNoFill();
			ofSetColor(COL_ACCENT_CYAN, 240);
			ofDrawRectangle(tlScr.x - 3, tlScr.y - 3, 6, 6);
			ofDrawRectangle(trScr.x - 3, trScr.y - 3, 6, 6);
			ofDrawRectangle(blScr.x - 3, blScr.y - 3, 6, 6);
			ofDrawRectangle(brScr.x - 3, brScr.y - 3, 6, 6);
			
			ofPopStyle();
		}

		// If currently drawing a selection.
		if (box->liveStrokeActive && !box->liveStroke.points.empty() &&
			paintTool == (int)JPPaintTool::LassoSelect)
		{
			const JPViewTransform view = paintView();
			ofPushStyle();
			ofNoFill();
			ofSetLineWidth(1.0f);
			ofSetColor(COL_ACCENT_CYAN, 180);
			ofPolyline selectionLine;
			for (const auto &point : box->liveStroke.points)
			{
				const ofVec2f scr = view.toScreen(ofVec2f(point.x, point.y));
				selectionLine.addVertex(scr.x, scr.y);
			}
			selectionLine.close();
			
			// Draw dashed outline
			const auto &vertices = selectionLine.getVertices();
			for (std::size_t i = 0; i < vertices.size() - 1; ++i)
			{
				drawDashedLine(vertices[i], vertices[i + 1], 4.0f);
			}
			ofPopStyle();
		}
	}

	ofNoFill();
	ofSetColor(COL_BORDER_MUTED);
	ofDrawRectangle(canvas);
	ofSetColor(COL_BORDER_DEFAULT);
	ofDrawRectangle(area);
	ofFill();
	ofPopStyle();
}

void JPboxgroup::drawPaintToolbar(JPbox_paint *box)
{
	static const char *tooltips[] = {
		"Pincel (B)", "Borrador (E)", "Línea (L)", "Rectángulo (R)", "Elipse (O)",
		"Pluma / Pen: cierra y rellena al soltar (P)",
		"Rellenar región - el slider define la tolerancia (G)",
		"Selección libre (S): recorta sin deformar y selecciona el interior; arrastrá para mover, manija superior para rotar, esquinas para escalar, DEL para borrar, D/Alt+arrastrá para duplicar"};

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	for (int slot = 0; slot < kToolbarToolCount; ++slot)
	{
		const JPPaintTool tool = kToolbarTools[slot];
		const ofRectangle bounds = getPaintToolBounds(slot);
		const bool active = (int)tool == paintTool;
		const bool over = jp_button::hovered(bounds);
		ofSetColor(active ? ofColor(COL_ACCENT_CYAN, 200) :
			(over ? COL_BG_HOVER : COL_BG_BUTTON));
		ofDrawRectRounded(bounds, 3.0f);
		ofNoFill();
		ofSetColor(active || over ? COL_ACCENT_CYAN : COL_BORDER_DEFAULT);
		ofDrawRectRounded(bounds, 3.0f);
		ofFill();

		const float cx = bounds.getCenter().x;
		const float cy = bounds.getCenter().y;
		ofSetColor(active ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		ofSetLineWidth(1.5f);
		if (tool == JPPaintTool::Brush)
		{
			ofDrawLine(cx - 5, cy + 5, cx + 3, cy - 4);
			ofDrawTriangle(cx + 2, cy - 6, cx + 6, cy - 6, cx + 5, cy - 1);
			ofDrawCircle(cx - 6, cy + 6, 1.6f);
		}
		else if (tool == JPPaintTool::Eraser)
		{
			ofPushMatrix();
			ofTranslate(cx, cy);
			ofRotateDeg(-35.0f);
			ofDrawRectRounded(-6, -4, 12, 8, 2);
			ofPopMatrix();
		}
		else if (tool == JPPaintTool::Line)
		{
			ofDrawLine(cx - 6, cy + 5, cx + 6, cy - 5);
		}
		else if (tool == JPPaintTool::Rect)
		{
			ofNoFill();
			ofDrawRectangle(cx - 6, cy - 4, 12, 9);
			ofFill();
		}
		else if (tool == JPPaintTool::Ellipse)
		{
			ofNoFill();
			ofDrawEllipse(cx, cy, 13, 10);
			ofFill();
		}
		else if (tool == JPPaintTool::Lasso)
		{
			// A loop that does not quite meet, plus the dot it will snap to.
			ofNoFill();
			ofPolyline loop;
			for (int i = 0; i <= 22; ++i)
			{
				const float a = ofDegToRad(35.0f + i * (300.0f / 22.0f));
				loop.addVertex(cx + std::cos(a) * 6.5f, cy + std::sin(a) * 5.5f);
			}
			loop.draw();
			ofFill();
			ofDrawCircle(cx + std::cos(ofDegToRad(35.0f)) * 6.5f,
				cy + std::sin(ofDegToRad(35.0f)) * 5.5f, 1.8f);
		}
		else if (tool == JPPaintTool::Fill)
		{
			// Bucket: a tipped pail with a drop under it.
			ofPushMatrix();
			ofTranslate(cx, cy - 1.0f);
			ofRotateDeg(-28.0f);
			ofDrawTriangle(-6, -4, 6, -4, 0, 5);
			ofPopMatrix();
			ofDrawCircle(cx + 5.0f, cy + 6.0f, 1.8f);
		}

		else if (tool == JPPaintTool::LassoSelect)
		{
			ofNoFill();
			ofPolyline loop;
			for (int i = 0; i <= 22; ++i)
			{
				const float a = ofDegToRad(35.0f + i * (300.0f / 22.0f));
				loop.addVertex(cx + std::cos(a) * 6.5f, cy + std::sin(a) * 5.5f);
			}
			const auto &vertices = loop.getVertices();
			for (std::size_t i = 0; i < vertices.size() - 1; i += 2) {
				ofDrawLine(vertices[i], vertices[i+1]);
			}
			ofFill();
		}
		ofSetLineWidth(1.0f);
		jp_tooltip::draw(tooltips[slot], bounds.x, bounds.y,
			bounds.width, bounds.height);
	}

	// Brush size: a trough with a filled proportion and a dab preview, so the
	// number that matters (how fat is the mark) is shown rather than described.
	const ofRectangle slider = getPaintSizeSliderBounds();
	const bool fillSelected = paintTool == (int)JPPaintTool::Fill;
	const float t = fillSelected ? paintFillTolerance
		: sliderFromBrush(paintBrushSize);
	ofSetColor(COL_SLIDER_TROUGH);
	ofDrawRectRounded(slider, 3.0f);
	ofSetColor(COL_ACCENT_CYAN_DIM);
	ofDrawRectRounded(slider.x, slider.y, slider.width * t, slider.height, 3.0f);
	ofNoFill();
	ofSetColor(jp_button::hovered(slider) ? COL_ACCENT_CYAN : COL_BORDER_DEFAULT);
	ofDrawRectRounded(slider, 3.0f);
	ofFill();
	// The knob IS the dab preview - it shows the size at the place the size is
	// set, instead of spending another control on saying the same thing. For the
	// bucket there is no dab, so the knob is a plain marker.
	ofSetColor(COL_TEXT_PRIMARY);
	ofDrawCircle(slider.x + slider.width * t, slider.getCenter().y,
		fillSelected ? 3.0f
			: ofClamp(paintBrushSize * 200.0f, 2.0f, slider.height * 0.5f));
	jp_tooltip::draw(fillSelected ?
		"Tolerancia de relleno - cuánto puede variar la región desde el píxel cliqueado" :
		"Tamaño del pincel ( [ y ] )",
		slider.x, slider.y, slider.width, slider.height);

	const ofRectangle swatch = getPaintColorSwatchBounds();
	ofSetColor(30, 30, 34);
	ofDrawRectRounded(swatch, 3.0f);
	// Over a checker, so an alpha below 1 is visible as alpha.
	drawCheckerboard(ofRectangle(swatch.x + 2, swatch.y + 2,
		swatch.width - 4, swatch.height - 4));
	ofSetColor(paintColor);
	ofDrawRectangle(swatch.x + 2, swatch.y + 2, swatch.width - 4, swatch.height - 4);
	ofNoFill();
	ofSetColor(paintPickerOpen || jp_button::hovered(swatch) ?
		COL_ACCENT_CYAN : COL_BORDER_DEFAULT);
	ofDrawRectRounded(swatch, 3.0f);
	ofFill();
	jp_tooltip::draw("Color del pincel", swatch.x, swatch.y,
		swatch.width, swatch.height);

	// Draw the first 6 saved colors from the palette next to the picker swatch
	for (int i = 0; i < std::min(6, (int)paintPalette.size()); ++i)
	{
		const ofRectangle qBounds = getPaintQuickSwatchBounds(i);
		ofSetColor(30, 30, 34);
		ofDrawRectRounded(qBounds, 2.0f);
		
		// Checkerboard for transparency
		drawCheckerboard(ofRectangle(qBounds.x + 1, qBounds.y + 1, qBounds.width - 2, qBounds.height - 2));
		
		ofSetColor(paintPalette[(std::size_t)i]);
		ofDrawRectangle(qBounds.x + 1, qBounds.y + 1, qBounds.width - 2, qBounds.height - 2);
		
		ofNoFill();
		const bool over = jp_button::hovered(qBounds);
		ofSetColor(over ? COL_ACCENT_CYAN : COL_BORDER_DEFAULT);
		ofDrawRectRounded(qBounds, 2.0f);
		ofFill();
		
		jp_tooltip::draw("Usar color guardado", qBounds.x, qBounds.y, qBounds.width, qBounds.height);
	}

	static const char *actionTips[PAINT_ACTION_COUNT] =
		{"Deshacer (Ctrl+Z)", "Rehacer (Ctrl+Shift+Z)"};
	for (int action = 0; action < PAINT_ACTION_COUNT; ++action)
	{
		const ofRectangle bounds = getPaintActionBounds(action);
		const bool enabled =
			action == PAINT_ACTION_UNDO ? box->canUndo() :
			action == PAINT_ACTION_REDO ? box->canRedo() : true;
		jp_button::draw(bounds, "", false, enabled, COL_ACCENT_CYAN);
		
		// Draw icon inside the button bounds
		const float cx = bounds.getCenter().x;
		const float cy = bounds.getCenter().y;
		const bool over = enabled && jp_button::hovered(bounds);
		ofSetColor(!enabled ? COL_TEXT_MUTED : (over ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY));
		
		ofPushStyle();
		if (action == PAINT_ACTION_UNDO)
		{
			// Left curved arrow
			ofDrawTriangle(cx - 5.0f, cy, cx, cy - 4.5f, cx, cy + 4.5f);
			ofNoFill();
			ofSetLineWidth(1.6f);
			ofPolyline path;
			path.addVertex(cx, cy);
			path.addVertex(cx + 3.0f, cy - 2.5f);
			path.addVertex(cx + 6.0f, cy);
			path.addVertex(cx + 6.0f, cy + 4.0f);
			path.draw();
		}
		else if (action == PAINT_ACTION_REDO)
		{
			// Right curved arrow
			ofDrawTriangle(cx + 5.0f, cy, cx, cy - 4.5f, cx, cy + 4.5f);
			ofNoFill();
			ofSetLineWidth(1.6f);
			ofPolyline path;
			path.addVertex(cx, cy);
			path.addVertex(cx - 3.0f, cy - 2.5f);
			path.addVertex(cx - 6.0f, cy);
			path.addVertex(cx - 6.0f, cy + 4.0f);
			path.draw();
		}
		ofPopStyle();

		jp_tooltip::draw(actionTips[action], bounds.x, bounds.y,
			bounds.width, bounds.height);
	}
	ofPopStyle();
}

void JPboxgroup::drawPaintTransport(JPbox_paint *box)
{
	JPMediaState &state = box->mediaState();
	const JPPaintDocument &doc = box->document();

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	for (int slot = 0; slot < PAINT_TRANSPORT_COUNT; ++slot)
	{
		const ofRectangle bounds = getPaintTransportBounds(slot);
		if (bounds.width <= 0.0f) continue;
		const bool over = jp_button::hovered(bounds);
		bool active = false;
		if (slot == PAINT_TRANSPORT_PLAY) active = state.playing;
		else if (slot == PAINT_TRANSPORT_DIRECTION) active = state.reverse;
		else if (slot == PAINT_TRANSPORT_REFERENCE) active = paintReferenceVisible;

		ofSetColor(active ? ofColor(COL_ACCENT_GREEN, 190) :
			(over ? COL_BG_HOVER : COL_BG_BUTTON));
		ofDrawRectRounded(bounds, 3.0f);
		ofNoFill();
		ofSetColor(active ? COL_ACCENT_GREEN :
			(over ? COL_BORDER_HOVER : COL_BORDER_DEFAULT));
		ofDrawRectRounded(bounds, 3.0f);
		ofFill();

		const float cx = bounds.getCenter().x;
		const float cy = bounds.getCenter().y;
		ofSetColor(active ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		string label;
		string tip;
		if (slot == PAINT_TRANSPORT_PREV)
		{
			ofDrawTriangle(cx - 3, cy, cx + 4, cy - 5, cx + 4, cy + 5);
			ofDrawRectangle(cx - 6, cy - 5, 1.6f, 10);
			tip = "Celda anterior ( , )";
		}
		else if (slot == PAINT_TRANSPORT_PLAY)
		{
			if (state.playing)
			{
				ofDrawRectangle(cx - 4, cy - 5, 3, 10);
				ofDrawRectangle(cx + 1, cy - 5, 3, 10);
			}
			else ofDrawTriangle(cx - 3, cy - 6, cx + 5, cy, cx - 3, cy + 6);
			tip = state.playing ? "Pausa (Espacio)" : "Reproducir (Espacio)";
		}
		else if (slot == PAINT_TRANSPORT_NEXT)
		{
			ofDrawTriangle(cx + 3, cy, cx - 4, cy - 5, cx - 4, cy + 5);
			ofDrawRectangle(cx + 5, cy - 5, 1.6f, 10);
			tip = "Siguiente celda ( . )";
		}
		else if (slot == PAINT_TRANSPORT_DIRECTION)
		{
			// Same arrow the inspector transport draws, so the two controls for
			// one value do not look like two different features.
			const float sign = active ? -1.0f : 1.0f;
			ofDrawLine(cx - 7 * sign, cy, cx + 7 * sign, cy);
			ofDrawTriangle(cx + 7 * sign, cy,
				cx + 2 * sign, cy - 4, cx + 2 * sign, cy + 4);
			tip = active ? "Reproducir adelante" : "Reproducir atrás";
		}
		else if (slot == PAINT_TRANSPORT_FPS)
		{
			label = ofToString(doc.fps, 0) + " fps";
			tip = "Fotogramas por segundo - arrastra para cambiar";
		}
		else if (slot == PAINT_TRANSPORT_LOOP)
		{
			static const char *loops[] = {"ONCE", "LOOP", "PING"};
			label = loops[std::clamp((int)state.loopMode, 0, 2)];
			tip = "Modo de reproducción (loop)";
		}
		else if (slot == PAINT_TRANSPORT_ONION)
		{
			label = "ONION " + ofToString(doc.onionBefore);
			tip = "Rango de cebolla (Shift+O)";
		}
		else
		{
			label = "REF";
			tip = "Mostrar entrada de referencia";
		}
		if (!label.empty())
		{
			ofSetColor(active ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
			jp_constants::p2_font.drawString(label,
				cx - jp_constants::p2_font.stringWidth(label) * 0.5f, cy + 3.0f);
		}
		jp_tooltip::draw(tip, bounds.x, bounds.y, bounds.width, bounds.height);
	}
	ofPopStyle();
}

void JPboxgroup::drawPaintTimeline(JPbox_paint *box)
{
	const ofRectangle timeline = getPaintTimelineBounds();
	const ofRectangle gutter = getPaintTimelineGutterBounds();
	const ofRectangle grid = getPaintTimelineGridBounds();
	const JPPaintDocument &doc = box->document();
	const int frameCount = (int)doc.frames.size();
	const int layerCount = (int)doc.layers.size();
	const int playing = box->currentCel();
	const int currentLayerIndex = box->currentLayer();

	// Every composite thumbnail that is about to be drawn is rasterized HERE,
	// before any scissor is armed: a rebuild binds its own framebuffer and
	// resolves multisamples, and a blit obeys the scissor box.
	for (int frame = 0; frame < frameCount; ++frame)
	{
		const ofRectangle cell = getPaintCompositeCellBounds(frame);
		if (cell.getRight() < grid.x || cell.x > grid.getRight()) continue;
		box->warmThumb(frame);
	}

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(COL_BG_DARK);
	ofDrawRectRounded(timeline, 4.0f);

	// ------------------------------------------------------------- the gutter
	{
		jp_gl::ScopedScissor clip(gutter);
		ofSetColor(COL_TEXT_DIM);
		jp_constants::p2_font.drawString("LAYERS", gutter.x + 5.0f,
			gutter.y + 12.0f);

		const ofRectangle add = getPaintLayerAddBounds();
		const bool addOver = jp_button::hovered(add);
		ofSetColor(addOver ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
		ofSetLineWidth(1.5f);
		ofDrawLine(add.getCenter().x - 4, add.getCenter().y,
			add.getCenter().x + 4, add.getCenter().y);
		ofDrawLine(add.getCenter().x, add.getCenter().y - 4,
			add.getCenter().x, add.getCenter().y + 4);
		ofSetLineWidth(1.0f);

		for (int row = 0; row < layerCount; ++row)
		{
			const int index = paintLayerAtRow(row);
			if (index < 0) continue;
			const JPPaintLayerInfo &info = doc.layers[(std::size_t)index];
			const ofRectangle bounds = getPaintGutterRowBounds(row);
			if (bounds.getBottom() < paintLayerRowsTop()) continue;
			if (bounds.y > timeline.getBottom()) break;
			const bool selected = index == currentLayerIndex;
			const bool over = jp_button::hovered(bounds);
			const bool dropTarget = paintDragMode == PAINT_DRAG_LAYER &&
				paintDragLayerTo == index && paintDragLayerFrom != index;

			ofSetColor(selected ? ofColor(COL_ACCENT_CYAN, 55) :
				(over ? COL_BG_HOVER : COL_BG_BUTTON));
			ofDrawRectangle(bounds);

			// The eye is an outline when hidden with a slash through it, a filled
			// pupil when shown - the state has to read at a glance from 15px.
			const ofRectangle eye = getPaintLayerEyeBounds(row);
			ofSetColor(info.visible ? COL_ACCENT_CYAN : COL_TEXT_DARK);
			ofNoFill();
			ofSetLineWidth(1.3f);
			ofDrawEllipse(eye.getCenter().x, eye.getCenter().y, 13.0f, 8.0f);
			ofSetLineWidth(1.0f);
			ofFill();
			if (info.visible)
			{
				ofDrawCircle(eye.getCenter().x, eye.getCenter().y, 2.6f);
			}
			else
			{
				ofDrawLine(eye.x + 1, eye.getBottom() - 2,
					eye.getRight() - 1, eye.y + 2);
			}

			string label = info.name.empty() ?
				("Layer " + ofToString(index + 1)) : info.name;
			const float labelLimit = getPaintLayerBadgeBounds(row).x -
				(eye.getRight() + 4.0f) - 4.0f;
			while (label.size() > 1 &&
				jp_constants::p2_font.stringWidth(label) > labelLimit)
			{
				label.pop_back();
			}
			if (paintRenamingLayer == index)
			{
				const ofRectangle field = getPaintLayerNameBounds(row);
				ofSetColor(COL_BG_INPUT);
				ofDrawRectangle(field);
				ofNoFill();
				ofSetColor(COL_ACCENT_CYAN);
				ofDrawRectangle(field);
				ofFill();
				ofSetColor(COL_TEXT_PRIMARY);
				jp_constants::p2_font.drawString(paintRenameBuffer, field.x + 3.0f,
					field.getCenter().y + 3.5f);
				jp_textfield::drawCaret(jp_constants::p2_font, paintRenameBuffer,
					paintRenameCursor, field.x + 3.0f, field.getCenter().y, 11.0f);
			}
			else
			{
				ofSetColor(selected ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
				jp_constants::p2_font.drawString(label, eye.getRight() + 4.0f,
					bounds.y + 13.0f);
			}

			// GOLD for the background badge: the same "applies beyond what you are
			// looking at" meaning the cue system uses gold for.
			const ofRectangle badge = getPaintLayerBadgeBounds(row);
			const bool badgeOver = jp_button::hovered(badge);
			ofSetColor(info.background ? ofColor(COL_ACCENT_GOLD, 200) :
				(badgeOver ? COL_BG_HOVER : ofColor(COL_BG_INPUT, 160)));
			ofDrawRectRounded(badge, 2.0f);
			ofSetColor(info.background ? COL_BG_DARK :
				(badgeOver ? COL_ACCENT_GOLD : COL_TEXT_MUTED));
			jp_constants::p2_font.drawString("BG",
				badge.getCenter().x -
					jp_constants::p2_font.stringWidth("BG") * 0.5f,
				badge.getCenter().y + 3.5f);

			if (layerCount > 1)
			{
				const ofRectangle trash = getPaintLayerDeleteBounds(row);
				const bool deleteOver = jp_button::hovered(trash);
				ofSetColor(deleteOver ? COL_ACCENT_RED : COL_TEXT_MUTED);
				
				const float tx = trash.getCenter().x;
				const float ty = trash.getCenter().y;
				
				ofPushStyle();
				ofSetLineWidth(1.0f);
				// Lid
				ofDrawLine(tx - 4, ty - 4, tx + 4, ty - 4);
				ofDrawLine(tx - 2, ty - 5, tx + 2, ty - 5);
				// Body
				ofDrawLine(tx - 3, ty - 3, tx - 3, ty + 4);
				ofDrawLine(tx + 3, ty - 3, tx + 3, ty + 4);
				ofDrawLine(tx - 3, ty + 4, tx + 3, ty + 4);
				// Lines inside
				ofDrawLine(tx - 1, ty - 1, tx - 1, ty + 2);
				ofDrawLine(tx + 1, ty - 1, tx + 1, ty + 2);
				ofPopStyle();
			}

			const ofRectangle opacity = getPaintLayerOpacityBounds(row);
			ofSetColor(COL_SLIDER_TROUGH);
			ofDrawRectangle(opacity);
			ofSetColor(info.visible ? COL_ACCENT_CYAN_DIM : COL_TEXT_DARK);
			ofDrawRectangle(opacity.x, opacity.y,
				opacity.width * ofClamp(info.opacity, 0.0f, 1.0f),
				opacity.height);

			ofNoFill();
			ofSetLineWidth(dropTarget ? 2.0f : 1.0f);
			ofSetColor(dropTarget ? COL_ACCENT_GOLD :
				selected ? COL_ACCENT_CYAN : COL_BORDER_MUTED);
			ofDrawRectangle(bounds);
			ofSetLineWidth(1.0f);
			ofFill();
		}
	}

	// --------------------------------------------------------------- the grid
	{
		jp_gl::ScopedScissor clip(grid);
		for (int frame = 0; frame < frameCount; ++frame)
		{
			const ofRectangle header = getPaintFrameHeaderBounds(frame);
			if (header.getRight() < grid.x || header.x > grid.getRight()) continue;
			const bool isCurrent = frame == doc.currentFrame;
			const bool isPlaying = frame == playing && frame != doc.currentFrame;
			const bool dropTarget = paintDragMode == PAINT_DRAG_CEL &&
				paintDragCelTo == frame && paintDragCelFrom != frame;

			// The whole column is tinted for the selected frame, so the playhead
			// reads down the grid rather than only in the header.
			if (isCurrent || isPlaying)
			{
				ofSetColor(isCurrent ? ofColor(COL_ACCENT_CYAN, 34)
					: ofColor(COL_ACCENT_GREEN, 30));
				ofDrawRectangle(header.x, header.y, header.width,
					timeline.getBottom() - header.y);
			}

			ofSetColor(dropTarget ? COL_ACCENT_GOLD :
				isCurrent ? COL_TEXT_PRIMARY :
				isPlaying ? COL_ACCENT_GREEN : COL_TEXT_SECONDARY);
			const string number = ofToString(frame + 1);
			jp_constants::p2_font.drawString(number,
				header.getCenter().x -
					jp_constants::p2_font.stringWidth(number) * 0.5f,
				header.y + 11.0f);

			const ofRectangle composite = getPaintCompositeCellBounds(frame);
			const ofRectangle inner(composite.x + 1, composite.y + 1,
				composite.width - 2, composite.height - 2);
			ofSetColor(COL_BG_BUTTON);
			ofDrawRectangle(inner);
			if (jp_paint::celStrokeCount(doc, frame) > 0)
			{
				// Letterboxed, not stretched: the cell is square and the canvas
				// is not, so drawing straight into it squashed every drawing.
				box->drawThumb(frame,
					jp_view::fit(inner, box->canvasAspect()));
			}
			const int hold = jp_paint::holdOf(doc.frames[(std::size_t)frame]);
			if (hold > 1)
			{
				ofSetColor(COL_ACCENT_GOLD);
				const string holdLabel = "x" + ofToString(hold);
				jp_constants::p2_font.drawString(holdLabel,
					composite.getRight() -
						jp_constants::p2_font.stringWidth(holdLabel) - 3.0f,
					composite.getBottom() - 3.0f);
			}
			ofNoFill();
			ofSetColor(isCurrent ? COL_ACCENT_CYAN : COL_BORDER_MUTED);
			ofDrawRectangle(composite.x + 1, composite.y + 1,
				composite.width - 2, composite.height - 2);
			ofFill();
		}

		// Cells. A marker, not a thumbnail: at 34px a thin stroke would be a
		// smudge, and a per-(frame, layer) thumbnail cache would be invalidated
		// wholesale by frame.revision anyway.
		for (int row = 0; row < layerCount; ++row)
		{
			const int index = paintLayerAtRow(row);
			if (index < 0) continue;
			const JPPaintLayerInfo &info = doc.layers[(std::size_t)index];
			const ofRectangle first = getPaintCellBounds(0, row);
			if (first.getBottom() < paintLayerRowsTop()) continue;
			if (first.y > timeline.getBottom()) break;

			if (info.background)
			{
				// ONE band, not a marker per frame: these strokes genuinely are
				// shared, and six identical markers would imply six cels.
				const ofRectangle last = getPaintCellBounds(
					std::max(0, frameCount - 1), row);
				const ofRectangle band(first.x + 2.0f, first.y + 5.0f,
					std::max(4.0f, last.getRight() - first.x - 4.0f),
					first.height - 10.0f);
				// The column rules still run through a shared row, so the grid
				// does not appear to stop at it.
				ofSetColor(ofColor(COL_BORDER_MUTED, 70));
				for (int frame = 0; frame < frameCount; ++frame)
				{
					const ofRectangle cell = getPaintCellBounds(frame, row);
					if (cell.getRight() < grid.x) continue;
					if (cell.x > grid.getRight()) break;
					ofDrawLine(cell.getRight(), cell.y, cell.getRight(),
						cell.getBottom());
				}
				const bool empty = info.sharedStrokes.empty();
				ofSetColor(empty ? ofColor(COL_ACCENT_GOLD, 45)
					: ofColor(COL_ACCENT_GOLD, 150));
				ofDrawRectRounded(band, 3.0f);
				ofSetColor(empty ? COL_TEXT_DARK : COL_BG_DARK);
				const string shared = "shared on every frame";
				if (jp_constants::p2_font.stringWidth(shared) < band.width - 8.0f)
				{
					jp_constants::p2_font.drawString(shared, band.x + 5.0f,
						band.getCenter().y + 3.5f);
				}
				continue;
			}

			for (int frame = 0; frame < frameCount; ++frame)
			{
				const ofRectangle cell = getPaintCellBounds(frame, row);
				if (cell.getRight() < grid.x || cell.x > grid.getRight()) continue;
				const std::vector<JPPaintStroke> *strokes =
					jp_paint::strokeListFor(doc, frame, index);
				const bool filled = strokes != nullptr && !strokes->empty();
				const bool selected = frame == doc.currentFrame &&
					index == currentLayerIndex;

				if (filled)
				{
					const float side = std::min(cell.width, cell.height) - 12.0f;
					ofSetColor(info.visible ? ofColor(COL_ACCENT_CYAN, 180)
						: ofColor(COL_TEXT_MUTED, 150));
					ofDrawRectRounded(
						cell.getCenter().x - side * 0.5f,
						cell.getCenter().y - side * 0.5f, side, side, 2.0f);
				}
				if (selected)
				{
					ofNoFill();
					ofSetLineWidth(1.5f);
					ofSetColor(COL_ACCENT_CYAN);
					ofDrawRectangle(cell.x + 1, cell.y + 1, cell.width - 2,
						cell.height - 2);
					ofSetLineWidth(1.0f);
					ofFill();
				}
				// A 1px separator, which is what makes it read as a grid.
				ofSetColor(ofColor(COL_BORDER_MUTED, 90));
				ofDrawLine(cell.getRight(), cell.y, cell.getRight(),
					cell.getBottom());
			}
		}

		const ofRectangle add = getPaintAddCelBounds();
		if (add.x < grid.getRight())
		{
			const bool addOver = jp_button::hovered(add);
			ofSetColor(addOver ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
			ofSetLineWidth(1.5f);
			ofDrawLine(add.getCenter().x - 4, add.getCenter().y,
				add.getCenter().x + 4, add.getCenter().y);
			ofDrawLine(add.getCenter().x, add.getCenter().y - 4,
				add.getCenter().x, add.getCenter().y + 4);
			ofSetLineWidth(1.0f);
		}
	}

	// The rule between the gutter and the grid, and the frame after the header.
	ofSetColor(ofColor(COL_BORDER_MUTED, 140));
	ofDrawLine(grid.x - 1.0f, timeline.y, grid.x - 1.0f, timeline.getBottom());
	ofDrawLine(timeline.x, paintLayerRowsTop(), timeline.getRight(),
		paintLayerRowsTop());
	ofNoFill();
	ofSetColor(COL_BORDER_DEFAULT);
	ofDrawRectRounded(timeline, 4.0f);
	ofFill();
	ofPopStyle();

	const ofRectangle addCel = getPaintAddCelBounds();
	jp_tooltip::draw("Añadir cuadro (N), o Shift para duplicar (D)",
		addCel.x, addCel.y, addCel.width, addCel.height);
	const ofRectangle addLayer = getPaintLayerAddBounds();
	jp_tooltip::draw("Añadir capa sobre la actual", addLayer.x, addLayer.y,
		addLayer.width, addLayer.height);
	const int hoveredRow = paintLayerRowAtScreen(
		ofVec2f((float)ofGetMouseX(), (float)ofGetMouseY()));
	if (hoveredRow >= 0)
	{
		const ofRectangle bounds = getPaintGutterRowBounds(hoveredRow);
		jp_tooltip::draw("Clic para seleccionar, arrastrar para reordenar, tacho de basura para eliminar. "
			"El ojo la oculta, la barra es su opacidad, BG la dibuja en todos los cuadros",
			bounds.x, bounds.y, bounds.width, bounds.height);
	}
}

void JPboxgroup::drawPaintPicker()
{
	const ofRectangle bounds = getPaintPickerBounds();
	// The popover has to own the pointer above the panel body, or the toolbar
	// underneath it lights up through it.
	jp_pointer::Scope pickerScope(jp_pointer::kDropdown);

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(0, 0, 0, 70);
	ofDrawRectRounded(bounds.x + 3, bounds.y + 3, bounds.width, bounds.height, 5.0f);
	ofSetColor(COL_BG_PANEL, 252);
	ofDrawRectRounded(bounds, 5.0f);
	ofNoFill();
	ofSetColor(COL_ACCENT_CYAN);
	ofDrawRectRounded(bounds, 5.0f);
	ofFill();

	const ofRectangle square(bounds.x + 8, bounds.y + 8, 120, 120);
	const ofRectangle hue(square.getRight() + 8, square.y, 16, 120);
	const ofRectangle alpha(hue.getRight() + 8, square.y, 16, 120);

	// Saturation across, value down, in 12px cells. Fine enough to read as a
	// gradient and cheap enough not to matter; a shader for one small square
	// would be a whole asset to load and keep in step.
	const int steps = 12;
	for (int sx = 0; sx < steps; ++sx)
	{
		for (int sy = 0; sy < steps; ++sy)
		{
			ofFloatColor cell;
			cell.setHsb(paintPickerHue, (sx + 0.5f) / steps,
				1.0f - (sy + 0.5f) / steps);
			ofSetColor(cell);
			ofDrawRectangle(square.x + square.width * sx / steps,
				square.y + square.height * sy / steps,
				square.width / steps + 1.0f, square.height / steps + 1.0f);
		}
	}
	ofNoFill();
	ofSetColor(paintPickerVal > 0.5f ? ofColor(20) : ofColor(240));
	ofDrawCircle(square.x + paintPickerSat * square.width,
		square.y + (1.0f - paintPickerVal) * square.height, 4.5f);
	ofSetColor(COL_BORDER_MUTED);
	ofDrawRectangle(square);
	ofFill();

	for (int i = 0; i < (int)hue.height; ++i)
	{
		ofFloatColor band;
		band.setHsb(i / hue.height, 1.0f, 1.0f);
		ofSetColor(band);
		ofDrawRectangle(hue.x, hue.y + i, hue.width, 1.0f);
	}
	ofSetColor(COL_TEXT_PRIMARY);
	ofDrawRectangle(hue.x - 2, hue.y + paintPickerHue * hue.height - 1,
		hue.width + 4, 2.0f);

	drawCheckerboard(alpha);
	for (int i = 0; i < (int)alpha.height; ++i)
	{
		ofFloatColor band = paintColor;
		band.a = i / alpha.height;
		ofSetColor(band);
		ofDrawRectangle(alpha.x, alpha.y + i, alpha.width, 1.0f);
	}
	ofSetColor(COL_TEXT_PRIMARY);
	ofDrawRectangle(alpha.x - 2, alpha.y + paintColor.a * alpha.height - 1,
		alpha.width + 4, 2.0f);

	for (int i = 0; i < kPaletteSize; ++i)
	{
		const ofRectangle cell = getPaintSwatchBounds(i);
		ofSetColor(COL_BG_INPUT);
		ofDrawRectangle(cell);
		if (i < (int)paintPalette.size())
		{
			// Over a checker, so a swatch with alpha reads as having alpha.
			drawCheckerboard(cell);
			ofSetColor(paintPalette[(std::size_t)i]);
			ofDrawRectangle(cell);
		}
		ofNoFill();
		ofSetColor(jp_button::hovered(cell) ? COL_ACCENT_CYAN : COL_BORDER_MUTED);
		ofDrawRectangle(cell);
		ofFill();
	}

	const ofRectangle hex = getPaintHexFieldBounds();
	const bool hexOver = jp_button::hovered(hex);
	ofSetColor(paintHexFocus ? COL_BG_INPUT :
		(hexOver ? COL_BG_HOVER : ofColor(COL_BG_INPUT, 170)));
	ofDrawRectRounded(hex, 3.0f);
	ofNoFill();
	ofSetColor(paintHexFocus ? COL_ACCENT_CYAN :
		(hexOver ? COL_BORDER_HOVER : COL_BORDER_MUTED));
	ofDrawRectRounded(hex, 3.0f);
	ofFill();
	{
		// While focused it shows what is being typed; otherwise the live colour,
		// so the field doubles as a readout.
		const string shown = paintHexFocus ? paintHexBuffer :
			jp_paint::formatHexColor(paintColor.r, paintColor.g, paintColor.b,
				paintColor.a);
		ofSetColor(paintHexFocus ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		jp_constants::p2_font.drawString(shown, hex.x + 6.0f,
			hex.getCenter().y + 3.5f);
		if (paintHexFocus)
		{
			jp_textfield::drawCaret(jp_constants::p2_font, paintHexBuffer,
				paintHexCursor, hex.x + 6.0f, hex.getCenter().y, 12.0f);
		}
	}

	const ofRectangle add = getPaintPaletteAddBounds();
	const bool addFull = (int)paintPalette.size() >= kPaletteSize;
	const bool addOver = !addFull && jp_button::hovered(add);
	ofSetColor(addOver ? COL_BG_HOVER : COL_BG_BUTTON);
	ofDrawRectangle(add);
	ofNoFill();
	ofSetColor(addFull ? COL_BORDER_MUTED :
		(addOver ? COL_ACCENT_CYAN : COL_BORDER_DEFAULT));
	ofDrawRectangle(add);
	ofFill();
	ofSetColor(addFull ? COL_TEXT_DARK :
		(addOver ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY));
	ofSetLineWidth(1.4f);
	ofDrawLine(add.getCenter().x - 4, add.getCenter().y,
		add.getCenter().x + 4, add.getCenter().y);
	ofDrawLine(add.getCenter().x, add.getCenter().y - 4,
		add.getCenter().x, add.getCenter().y + 4);
	ofSetLineWidth(1.0f);
	ofPopStyle();

	// Drawn outside the pushStyle so the tooltip is not affected by it.
	jp_tooltip::draw(addFull ? "La paleta está llena" : "Guardar este color en la paleta",
		add.x, add.y, add.width, add.height);
	jp_tooltip::draw("Escribe un color hex - #RGB, #RRGGBB o #RRGGBBAA. Enter aplica, Esc cancela",
		hex.x, hex.y, hex.width, hex.height);
}

void JPboxgroup::drawPaintHelp()
{
	const ofRectangle bounds = getPaintHelpRect();
	if (bounds.width <= 0.0f) return;
	// The prompt owns the pointer while it is up. Without this its own close
	// button is drawn at the panel body's layer, and the modal rule - a modal
	// blocks the whole window for everything below it - then blocks that button.
	jp_pointer::Scope promptScope(jp_pointer::kPrompt);
	const int language = 1;

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	// Scrim over the PANEL only, not the window: this belongs to the panel.
	ofSetColor(0, 0, 0, 150);
	ofDrawRectangle(paintPanelX, paintPanelY, paintPanelW, paintPanelH);
	ofSetColor(ofColor(COL_BG_PANEL, 250));
	ofDrawRectRounded(bounds, 6.0f);
	ofNoFill();
	ofSetColor(COL_ACCENT_CYAN);
	ofSetLineWidth(1.5f);
	ofDrawRectRounded(bounds, 6.0f);
	ofSetLineWidth(1.0f);
	ofFill();

	const string title = language == 0 ? "DRAWING AND ANIMATION KEYS"
		: "TECLAS DE DIBUJO Y ANIMACION";
	ofSetColor(COL_ACCENT_CYAN);
	jp_constants::p_font.drawString(title, bounds.x + 14.0f, bounds.y + 20.0f);
	ofSetColor(ofColor(COL_BORDER_MUTED, 150));
	ofDrawLine(bounds.x + 14.0f, bounds.y + 30.0f,
		bounds.getRight() - 14.0f, bounds.y + 30.0f);

	const ofRectangle close = getPaintHelpCloseBounds();
	drawCloseGlyph(close, jp_button::hovered(close));

	// Greedy wrap, local rather than borrowed: ofApp's help renderer is welded to
	// jp_screen::frame() and shares one layout cache with the HELP screen, which a
	// second consumer at a different size would thrash.
	auto wrap = [](const string &text, float maxWidth) {
		vector<string> lines;
		const vector<string> words = ofSplitString(text, " ", true, true);
		string line;
		for (const string &word : words)
		{
			const string candidate = line.empty() ? word : line + " " + word;
			if (!line.empty() &&
				jp_constants::p2_font.stringWidth(candidate) > maxWidth)
			{
				lines.push_back(line);
				line = word;
			}
			else line = candidate;
		}
		if (!line.empty()) lines.push_back(line);
		return lines;
	};

	const ofRectangle body(bounds.x + 14.0f, bounds.y + 38.0f,
		bounds.width - 28.0f, bounds.height - 50.0f);
	const float keysWidth = 120.0f;
	const float descX = body.x + keysWidth + 10.0f;
	const float descWidth = std::max(80.0f, body.getRight() - descX);

	// Measured first so the scroll can be clamped before anything is painted -
	// otherwise the last wheel notch shows an empty page for one frame.
	float contentHeight = 0.0f;
	for (const jp_help::Line &line : jp_help::table())
	{
		if (line.scope != jp_help::Scope::Paint) continue;
		if (line.kind == jp_help::Kind::Note)
		{
			contentHeight +=
				(float)wrap(jp_help::text(line, language), body.width).size() * 13.0f
				+ 6.0f;
			continue;
		}
		contentHeight +=
			std::max(1.0f, (float)wrap(jp_help::text(line, language),
				descWidth).size()) * 13.0f + 4.0f;
	}
	paintHelpScroll = ofClamp(paintHelpScroll, 0.0f,
		std::max(0.0f, contentHeight - body.height));

	{
		jp_gl::ScopedScissor clip(body);
		float y = body.y - paintHelpScroll;
		for (const jp_help::Line &line : jp_help::table())
		{
			// H() and GAP() are always Scope::Global, so a scope filter yields
			// entries and notes only - which is all a key list needs.
			if (line.scope != jp_help::Scope::Paint) continue;
			const string text = jp_help::text(line, language);
			if (line.kind == jp_help::Kind::Note)
			{
				const vector<string> lines = wrap(text, body.width);
				ofSetColor(COL_TEXT_DIM);
				for (const string &row : lines)
				{
					jp_constants::p2_font.drawString(row, body.x, y + 10.0f);
					y += 13.0f;
				}
				y += 6.0f;
				continue;
			}
			const vector<string> lines = wrap(text, descWidth);
			ofSetColor(COL_TEXT_PRIMARY);
			jp_constants::p2_font.drawString(line.keys, body.x, y + 10.0f);
			ofSetColor(COL_TEXT_SECONDARY);
			for (std::size_t i = 0; i < lines.size(); ++i)
			{
				jp_constants::p2_font.drawString(lines[i], descX,
					y + 10.0f + (float)i * 13.0f);
			}
			y += std::max(1.0f, (float)lines.size()) * 13.0f + 4.0f;
		}
	}

	if (contentHeight > body.height)
	{
		ofSetColor(COL_TEXT_MUTED);
		const string hint = language == 0 ? "scroll for more" : "rueda para ver mas";
		jp_constants::p2_font.drawString(hint,
			bounds.getRight() - 14.0f - jp_constants::p2_font.stringWidth(hint),
			bounds.getBottom() - 6.0f);
	}
	ofPopStyle();
}

void JPboxgroup::drawPaintPanel()
{
	if (!paintEditActive) return;
	JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return;

	// The mapping panel omits this and its toolbar still highlights under an
	// open dropdown. Scoping the whole draw is what makes jp_button::hovered
	// and every other pointer test here respect what is stacked above.
	jp_pointer::Scope pointerScope(jp_pointer::kPaintPanel);

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);

	ofSetColor(0, 0, 0, 70);
	ofDrawRectRounded(paintPanelX + 4, paintPanelY + 4,
		paintPanelW, paintPanelH, 6.0f);
	ofSetColor(COL_BG_PANEL, 250);
	ofDrawRectRounded(paintPanelX, paintPanelY, paintPanelW, paintPanelH, 6.0f);
	ofSetColor(COL_BG_TAB);
	ofDrawRectRounded(paintPanelX, paintPanelY, paintPanelW,
		kHeaderHeight + kToolbarHeight, 6.0f);
	ofDrawRectangle(paintPanelX, paintPanelY + kHeaderHeight + kToolbarHeight - 6.0f,
		paintPanelW, 6.0f);
	ofNoFill();
	ofSetColor(COL_ACCENT_CYAN);
	ofDrawRectRounded(paintPanelX, paintPanelY, paintPanelW, paintPanelH, 6.0f);
	ofFill();

	string title = "PAINT - " + box->name;
	const float titleLimit = getPaintPanelCloseBounds().x - paintPanelX - 22.0f;
	while (title.size() > 4 &&
		jp_constants::p_font.stringWidth(title + "..") > titleLimit)
	{
		title.pop_back();
	}
	ofSetColor(COL_TEXT_PRIMARY);
	jp_constants::p_font.drawString(title, paintPanelX + 12.0f,
		paintPanelY + kHeaderHeight * 0.5f + 4.0f);

	const ofRectangle helpIcon = getPaintHelpIconBounds();
	const bool helpOver = jp_button::hovered(helpIcon);
	ofSetColor(paintHelpOpen || helpOver ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	jp_constants::p_font.drawString("?",
		helpIcon.getCenter().x - jp_constants::p_font.stringWidth("?") * 0.5f,
		helpIcon.getCenter().y + 5.0f);
	jp_tooltip::draw("Atajos de dibujo y animación", helpIcon.x, helpIcon.y,
		helpIcon.width, helpIcon.height);

	const ofRectangle close = getPaintPanelCloseBounds();
	drawCloseGlyph(close, jp_button::hovered(close));
	jp_tooltip::draw("Cerrar (Esc)", close.x, close.y, close.width, close.height);

	drawPaintToolbar(box);
	drawPaintCanvas(box);
	drawPaintTransport(box);
	drawPaintTimeline(box);

	// Resize grip: three chevrons in the corner, the same affordance the
	// mapping panel uses.
	ofSetColor(mouseOverPaintPanelResizeHandle() ?
		COL_ACCENT_CYAN : COL_BORDER_MUTED);
	for (int i = 0; i < 3; ++i)
	{
		const float offset = 4.0f + i * 4.0f;
		ofDrawLine(paintPanelX + paintPanelW - offset,
			paintPanelY + paintPanelH - 4.0f,
			paintPanelX + paintPanelW - 4.0f,
			paintPanelY + paintPanelH - offset);
	}

	// Last, so they are over everything they overlap. The modal outranks the
	// popover, so it paints after it.
	if (paintPickerOpen) drawPaintPicker();
	if (paintHelpOpen) drawPaintHelp();
	ofPopStyle();
}

// -------------------------------------------------------------------- stroke

void JPboxgroup::beginPaintStroke(JPbox_paint *box, const ofVec2f &uv)
{
	JPPaintStroke stroke;
	stroke.r = paintColor.r;
	stroke.g = paintColor.g;
	stroke.b = paintColor.b;
	stroke.a = paintColor.a;
	stroke.size = paintBrushSize;
	stroke.erase = paintTool == (int)JPPaintTool::Eraser;
	stroke.tool = paintTool;
	stroke.points.push_back(JPPaintPoint{uv.x, uv.y, 1.0f});
	box->liveStroke = stroke;
	box->liveStrokeActive = true;
	paintStrokeStartUv = uv;
}

void JPboxgroup::extendPaintStroke(JPbox_paint *box, const ofVec2f &uv)
{
	if (!box->liveStrokeActive) return;
	std::vector<JPPaintPoint> &points = box->liveStroke.points;

	if (paintTool == (int)JPPaintTool::Brush ||
		paintTool == (int)JPPaintTool::Eraser ||
		paintTool == (int)JPPaintTool::Lasso ||
		paintTool == (int)JPPaintTool::LassoSelect)
	{
		// A sample that has not moved adds a point and a join circle for
		// nothing - the previous dab already covers that pixel.
		if (!points.empty())
		{
			const float dx = uv.x - points.back().x;
			const float dy = uv.y - points.back().y;
			if (dx * dx + dy * dy < 1.0e-8f) return;
		}
		points.push_back(JPPaintPoint{uv.x, uv.y, 1.0f});
		return;
	}

	// Shape tools are REBUILT from the two corners every drag frame rather than
	// accumulated, so a dropped or replayed mouse frame lands in the same place.
	// Same discipline the mapping editor's snapshot+transform uses.
	points.clear();
	const float x0 = paintStrokeStartUv.x, y0 = paintStrokeStartUv.y;
	const float x1 = uv.x, y1 = uv.y;
	if (paintTool == (int)JPPaintTool::Line)
	{
		points.push_back(JPPaintPoint{x0, y0, 1.0f});
		points.push_back(JPPaintPoint{x1, y1, 1.0f});
	}
	else if (paintTool == (int)JPPaintTool::Rect)
	{
		points.push_back(JPPaintPoint{x0, y0, 1.0f});
		points.push_back(JPPaintPoint{x1, y0, 1.0f});
		points.push_back(JPPaintPoint{x1, y1, 1.0f});
		points.push_back(JPPaintPoint{x0, y1, 1.0f});
		points.push_back(JPPaintPoint{x0, y0, 1.0f});
	}
	else
	{
		// A polyline, not a special primitive: keeping every tool a plain list
		// of points is what lets one renderer, one undo entry and one XML
		// encoding serve all of them.
		const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
		const float rx = std::abs(x1 - x0) * 0.5f, ry = std::abs(y1 - y0) * 0.5f;
		const int segments = 64;
		for (int i = 0; i <= segments; ++i)
		{
			const float angle = TWO_PI * (float)i / (float)segments;
			points.push_back(JPPaintPoint{cx + std::cos(angle) * rx,
				cy + std::sin(angle) * ry, 1.0f});
		}
	}
}

void JPboxgroup::endPaintStroke(JPbox_paint *box)
{
	if (!box->liveStrokeActive) return;
	JPPaintStroke stroke = box->liveStroke;
	box->liveStrokeActive = false;
	box->liveStroke = JPPaintStroke();
	if (stroke.points.empty()) return;

	// LassoSelect is UI-only; its live stroke is consumed by
	// endSelectionDrawing / cutStrokesWithLasso and must NEVER reach commitStroke.
	if (stroke.tool == (int)JPPaintTool::LassoSelect) return;

	if (stroke.tool == (int)JPPaintTool::Lasso && stroke.points.size() >= 3)
	{
		// Close the ring to its start point. This is what the tool IS: the user
		// draws an open sweep and lets go, and the gap is bridged for them.
		stroke.points.push_back(stroke.points.front());
	}
	if (stroke.tool == (int)JPPaintTool::Brush ||
		stroke.tool == (int)JPPaintTool::Eraser ||
		stroke.tool == (int)JPPaintTool::Lasso)
	{
		// Half a pixel at the render resolution: below what anyone can see, and
		// it typically removes most of the raw mouse samples. Shape tools are
		// already minimal and simplifying a small ellipse would flatten it.
		const float epsilon = 0.5f /
			std::max(1.0f, (float)jp_constants::renderWidth);
		jp_paint::simplify(stroke.points, epsilon);
	}
	box->commitStroke(stroke);
	markPaintChanged();
}

void JPboxgroup::endSelectionDrawing(JPbox_paint *box)
{
	if (!box->liveStrokeActive) return;
	JPPaintStroke stroke = box->liveStroke;
	box->liveStrokeActive = false;
	box->liveStroke = JPPaintStroke();
	
	if (stroke.points.size() < 3) return;
	
	paintSelectionPath.clear();
	for (const auto &pt : stroke.points)
	{
		paintSelectionPath.push_back(ofVec2f(pt.x, pt.y));
	}
	if (paintSelectionPath.size() >= 3)
	{
		paintSelectionPath.push_back(paintSelectionPath.front());
	}
	
	if (!paintSelectionPath.empty())
	{
		float minX = paintSelectionPath[0].x;
		float maxX = minX;
		float minY = paintSelectionPath[0].y;
		float maxY = minY;
		for (const auto &p : paintSelectionPath)
		{
			minX = std::min(minX, p.x);
			maxX = std::max(maxX, p.x);
			minY = std::min(minY, p.y);
			maxY = std::max(maxY, p.y);
		}
		paintSelectionBounds = ofRectangle(minX, minY, maxX - minX, maxY - minY);
		paintSelectionActive = true;
	}
	
	paintSelectedStrokeIndices.clear();
}

void JPboxgroup::clearPaintSelection()
{
	paintSelectionActive = false;
	paintSelectionPath.clear();
	paintSelectedStrokeIndices.clear();
	paintSelectionBounds = ofRectangle();
	paintSelectionDragging = false;
	paintSelectionDragOffset.set(0.0f, 0.0f);
	paintSelectionRotating = false;
	paintSelectionRotation = 0.0f;
	paintSelectionScaling = false;
	paintSelectionScale = 1.0f;
}

void JPboxgroup::moveSelectedStrokes(JPbox_paint *box, const ofVec2f &offset)
{
	if (paintSelectedStrokeIndices.empty()) return;
	const int cel = std::clamp(box->document().currentFrame, 0, (int)box->document().frames.size() - 1);
	const std::vector<JPPaintStroke> *list = jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
	if (list == nullptr) return;
	
	std::vector<JPPaintStroke> newList = *list;
	for (int idx : paintSelectedStrokeIndices)
	{
		if (idx >= 0 && idx < (int)newList.size())
		{
			transformStrokeCoordinates(newList[idx],
				[&](const ofVec2f &point) { return point + offset; });
		}
	}
	
	box->replaceStrokes(cel, box->currentLayer(), newList);
	
	for (auto &p : paintSelectionPath)
	{
		p += offset;
	}
	paintSelectionBounds.x += offset.x;
	paintSelectionBounds.y += offset.y;
}

void JPboxgroup::rotateSelectedStrokes(JPbox_paint *box, float angle)
{
	if (paintSelectedStrokeIndices.empty() || std::abs(angle) < 0.01f) return;
	const int cel = std::clamp(box->document().currentFrame, 0, (int)box->document().frames.size() - 1);
	const std::vector<JPPaintStroke> *list = jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
	if (list == nullptr) return;
	
	std::vector<JPPaintStroke> newList = *list;
	ofVec2f center = paintSelectionBounds.getCenter();
	float aspect = paintView().canvasRect().height > 0.0f ? (paintView().canvasRect().width / paintView().canvasRect().height) : 1.0f;
	
	for (int idx : paintSelectedStrokeIndices)
	{
		if (idx >= 0 && idx < (int)newList.size())
		{
			transformStrokeCoordinates(newList[idx], [&](const ofVec2f &point) {
				return rotatePointAround(point, center, angle, aspect);
			});
		}
	}
	
	box->replaceStrokes(cel, box->currentLayer(), newList);
	
	for (auto &p : paintSelectionPath)
	{
		p = rotatePointAround(p, center, angle, aspect);
	}
	
	float minX = paintSelectionPath[0].x;
	float maxX = minX;
	float minY = paintSelectionPath[0].y;
	float maxY = minY;
	for (const auto &p : paintSelectionPath)
	{
		minX = std::min(minX, p.x);
		maxX = std::max(maxX, p.x);
		minY = std::min(minY, p.y);
		maxY = std::max(maxY, p.y);
	}
	paintSelectionBounds = ofRectangle(minX, minY, maxX - minX, maxY - minY);
}

void JPboxgroup::scaleSelectedStrokes(JPbox_paint *box, float scaleFactor)
{
	if (paintSelectedStrokeIndices.empty() || std::abs(scaleFactor - 1.0f) < 0.001f) return;
	const int cel = std::clamp(box->document().currentFrame, 0, (int)box->document().frames.size() - 1);
	const std::vector<JPPaintStroke> *list = jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
	if (list == nullptr) return;
	
	std::vector<JPPaintStroke> newList = *list;
	ofVec2f center = paintSelectionBounds.getCenter();
	for (int idx : paintSelectedStrokeIndices)
	{
		if (idx >= 0 && idx < (int)newList.size())
		{
			transformStrokeCoordinates(newList[idx], [&](const ofVec2f &point) {
				return ofVec2f(center.x + (point.x - center.x) * scaleFactor,
					center.y + (point.y - center.y) * scaleFactor);
			});
		}
	}
	
	box->replaceStrokes(cel, box->currentLayer(), newList);
	
	for (auto &p : paintSelectionPath)
	{
		p.x = center.x + (p.x - center.x) * scaleFactor;
		p.y = center.y + (p.y - center.y) * scaleFactor;
	}
	
	float minX = paintSelectionPath[0].x;
	float maxX = minX;
	float minY = paintSelectionPath[0].y;
	float maxY = minY;
	for (const auto &p : paintSelectionPath)
	{
		minX = std::min(minX, p.x);
		maxX = std::max(maxX, p.x);
		minY = std::min(minY, p.y);
		maxY = std::max(maxY, p.y);
	}
	paintSelectionBounds = ofRectangle(minX, minY, maxX - minX, maxY - minY);
}

void JPboxgroup::duplicateSelectedStrokes(JPbox_paint *box)
{
	if (paintSelectedStrokeIndices.empty()) return;
	const int cel = std::clamp(box->document().currentFrame, 0, (int)box->document().frames.size() - 1);
	const std::vector<JPPaintStroke> *list = jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
	if (list == nullptr) return;
	
	std::vector<JPPaintStroke> newList = *list;
	std::vector<int> newSelectedIndices;
	
	for (int idx : paintSelectedStrokeIndices)
	{
		if (idx >= 0 && idx < (int)newList.size())
		{
			JPPaintStroke dup = newList[idx];
			transformStrokeCoordinates(dup, [](const ofVec2f &point) {
				return point + ofVec2f(0.02f, 0.02f);
			});
			newList.push_back(dup);
			newSelectedIndices.push_back((int)newList.size() - 1);
		}
	}
	
	box->replaceStrokes(cel, box->currentLayer(), newList);
	
	for (auto &p : paintSelectionPath)
	{
		p += 0.02f;
	}
	paintSelectionBounds.x += 0.02f;
	paintSelectionBounds.y += 0.02f;
	
	paintSelectedStrokeIndices = newSelectedIndices;
}

void JPboxgroup::deleteSelectedStrokes(JPbox_paint *box)
{
	if (paintSelectedStrokeIndices.empty()) return;
	const int cel = std::clamp(box->document().currentFrame, 0, (int)box->document().frames.size() - 1);
	const std::vector<JPPaintStroke> *list = jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
	if (list == nullptr) return;
	
	std::vector<JPPaintStroke> newList;
	for (int i = 0; i < (int)list->size(); ++i)
	{
		if (std::find(paintSelectedStrokeIndices.begin(), paintSelectedStrokeIndices.end(), i) == paintSelectedStrokeIndices.end())
		{
			newList.push_back((*list)[i]);
		}
	}
	
	box->replaceStrokes(cel, box->currentLayer(), newList);
	
	paintSelectionActive = false;
	paintSelectedStrokeIndices.clear();
	paintSelectionPath.clear();
}

void JPboxgroup::addPaintPaletteColor()
{
	if ((int)paintPalette.size() >= kPaletteSize) return;
	for (const ofFloatColor &existing : paintPalette)
	{
		// A near-duplicate would spend one of sixteen slots saying what another
		// slot already says.
		if (std::abs(existing.r - paintColor.r) < 0.004f &&
			std::abs(existing.g - paintColor.g) < 0.004f &&
			std::abs(existing.b - paintColor.b) < 0.004f &&
			std::abs(existing.a - paintColor.a) < 0.004f)
		{
			return;
		}
	}
	paintPalette.push_back(paintColor);
	savePaintPalette();
}

void JPboxgroup::removePaintPaletteColor(int index)
{
	if (index < 0 || index >= (int)paintPalette.size()) return;
	paintPalette.erase(paintPalette.begin() + index);
	savePaintPalette();
}

void JPboxgroup::loadPaintPalette()
{
	paintPalette.clear();
	const string path = ofToDataPath("paint_palette.xml");
	if (!ofFile(path).exists()) return;
	ofXml xml;
	if (!xml.load(path)) return;
	// Flat root of <color> rows, the same shape shader_favorites.xml uses.
	for (auto &node : xml.find("/color"))
	{
		const vector<string> parts = ofSplitString(node.getValue(), " ", true, true);
		if (parts.size() < 4) continue;
		ofFloatColor colour(ofToFloat(parts[0]), ofToFloat(parts[1]),
			ofToFloat(parts[2]), ofToFloat(parts[3]));
		colour.r = ofClamp(colour.r, 0.0f, 1.0f);
		colour.g = ofClamp(colour.g, 0.0f, 1.0f);
		colour.b = ofClamp(colour.b, 0.0f, 1.0f);
		colour.a = ofClamp(colour.a, 0.0f, 1.0f);
		paintPalette.push_back(colour);
		if ((int)paintPalette.size() >= kPaletteSize) break;
	}
}

void JPboxgroup::savePaintPalette() const
{
	ofXml xml;
	for (const ofFloatColor &colour : paintPalette)
	{
		xml.appendChild("color").set(
			ofToString(colour.r, 5) + " " + ofToString(colour.g, 5) + " " +
			ofToString(colour.b, 5) + " " + ofToString(colour.a, 5));
	}
	xml.save(ofToDataPath("paint_palette.xml"));
}

bool JPboxgroup::paintTextCaptureActive() const
{
	return paintEditActive && (paintHexFocus || paintRenamingLayer >= 0);
}

void JPboxgroup::commitPaintHex()
{
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	// A rejected string leaves the colour alone - a half-typed field must not
	// half-apply.
	if (jp_paint::parseHexColor(paintHexBuffer, r, g, b, a))
	{
		paintColor.set(r, g, b, a);
		paintPickerHue = paintColor.getHue();
		paintPickerSat = paintColor.getSaturation();
		paintPickerVal = paintColor.getBrightness();
	}
	cancelPaintHex();
}

void JPboxgroup::cancelPaintHex()
{
	paintHexFocus = false;
	paintHexBuffer.clear();
	paintHexCursor = 0;
	paintHexSelectAll = false;
}

void JPboxgroup::beginPaintLayerRename(int layerIndex)
{
	JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return;
	if (layerIndex < 0 ||
		layerIndex >= (int)box->document().layers.size()) return;
	cancelPaintHex();
	paintRenamingLayer = layerIndex;
	paintRenameBuffer =
		box->document().layers[(std::size_t)layerIndex].name;
	paintRenameCursor = (int)paintRenameBuffer.size();
	// Selected on entry, so typing replaces rather than appends - what every
	// rename in every file manager does.
	paintRenameSelectAll = true;
}

void JPboxgroup::commitPaintLayerRename()
{
	JPbox_paint *box = getPaintEditBox();
	if (box != nullptr && paintRenamingLayer >= 0 &&
		paintRenamingLayer < (int)box->document().layers.size())
	{
		JPPaintLayerInfo props =
			box->document().layers[(std::size_t)paintRenamingLayer];
		// An empty name would leave a nameless row, so it falls back to the
		// generated one rather than being accepted.
		props.name = paintRenameBuffer.empty() ?
			("Layer " + ofToString(props.id + 1)) : paintRenameBuffer;
		box->setLayerProps(paintRenamingLayer, props);
		markPaintChanged();
	}
	cancelPaintLayerRename();
}

void JPboxgroup::cancelPaintLayerRename()
{
	paintRenamingLayer = -1;
	paintRenameBuffer.clear();
	paintRenameCursor = 0;
	paintRenameSelectAll = false;
}

// Returns true when a focused field consumed the key.
bool JPboxgroup::paintHandleTextKey(int key)
{
	if (paintHexFocus)
	{
		if (key == OF_KEY_RETURN || key == '\r') { commitPaintHex(); return true; }
		if (key == OF_KEY_ESC) { cancelPaintHex(); return true; }
		jp_textfield::handleKey(paintHexBuffer, paintHexCursor, key, false, false,
			&paintHexSelectAll);
		return true;
	}
	if (paintRenamingLayer >= 0)
	{
		if (key == OF_KEY_RETURN || key == '\r')
		{
			commitPaintLayerRename();
			return true;
		}
		if (key == OF_KEY_ESC) { cancelPaintLayerRename(); return true; }
		jp_textfield::handleKey(paintRenameBuffer, paintRenameCursor, key, false,
			false, &paintRenameSelectAll);
		return true;
	}
	return false;
}

void JPboxgroup::applyPaintPickerColor()
{
	const float alpha = paintColor.a;
	paintColor.setHsb(paintPickerHue, paintPickerSat, paintPickerVal);
	// setHsb rewrites alpha, and the alpha strip is edited independently.
	paintColor.a = alpha;
}

bool JPboxgroup::handlePaintPickerPressed(const ofVec2f &mouse)
{
	const ofRectangle bounds = getPaintPickerBounds();
	const ofRectangle square(bounds.x + 8, bounds.y + 8, 120, 120);
	const ofRectangle hue(square.getRight() + 8, square.y, 16, 120);
	const ofRectangle alpha(hue.getRight() + 8, square.y, 16, 120);

	if (square.inside(mouse))
	{
		paintDragMode = PAINT_DRAG_PICKER_SV;
		updatePaintPickerFromDrag(mouse);
		return true;
	}
	if (hue.inside(mouse))
	{
		paintDragMode = PAINT_DRAG_PICKER_HUE;
		updatePaintPickerFromDrag(mouse);
		return true;
	}
	if (alpha.inside(mouse))
	{
		paintDragMode = PAINT_DRAG_PICKER_ALPHA;
		updatePaintPickerFromDrag(mouse);
		return true;
	}
	if (getPaintPaletteAddBounds().inside(mouse))
	{
		addPaintPaletteColor();
		return true;
	}
	for (int i = 0; i < (int)paintPalette.size(); ++i)
	{
		if (!getPaintSwatchBounds(i).inside(mouse)) continue;
		paintColor = paintPalette[(std::size_t)i];
		// Keep the HSB controls showing the colour that is actually selected.
		paintPickerHue = paintColor.getHue();
		paintPickerSat = paintColor.getSaturation();
		paintPickerVal = paintColor.getBrightness();
		return true;
	}
	return true;
}

void JPboxgroup::updatePaintPickerFromDrag(const ofVec2f &mouse)
{
	const ofRectangle bounds = getPaintPickerBounds();
	const ofRectangle square(bounds.x + 8, bounds.y + 8, 120, 120);
	if (paintDragMode == PAINT_DRAG_PICKER_SV)
	{
		paintPickerSat = ofClamp((mouse.x - square.x) / square.width, 0.0f, 1.0f);
		paintPickerVal = ofClamp(
			1.0f - (mouse.y - square.y) / square.height, 0.0f, 1.0f);
		applyPaintPickerColor();
	}
	else if (paintDragMode == PAINT_DRAG_PICKER_HUE)
	{
		paintPickerHue = ofClamp((mouse.y - square.y) / square.height, 0.0f, 1.0f);
		applyPaintPickerColor();
	}
	else if (paintDragMode == PAINT_DRAG_PICKER_ALPHA)
	{
		paintColor.a = ofClamp((mouse.y - square.y) / square.height, 0.0f, 1.0f);
	}
}

// --------------------------------------------------------------------- mouse

bool JPboxgroup::update_paintMousePressed(int mouseButton)
{
	JPbox_paint *box = getPaintEditBox();
	if (box == nullptr || !mouseOverPaintPanel()) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());

	if (paintHelpOpen)
	{
		// A modal swallows everything while it is up, including a click on the
		// panel behind it - anything else and the panel would act on a click the
		// user aimed at the dialog.
		paintPanelPointerCaptured = true;
		if (getPaintHelpCloseBounds().inside(mouse)) closePaintHelp();
		else if (!getPaintHelpRect().inside(mouse)) closePaintHelp();
		return true;
	}

	if (paintPickerOpen)
	{
		if (getPaintPickerBounds().inside(mouse))
		{
			paintPanelPointerCaptured = true;
			if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
				getPaintHexFieldBounds().inside(mouse))
			{
				if (!paintHexFocus)
				{
					paintHexFocus = true;
					paintHexBuffer = jp_paint::formatHexColor(paintColor.r,
						paintColor.g, paintColor.b, paintColor.a);
					paintHexCursor = (int)paintHexBuffer.size();
					paintHexSelectAll = true;
				}
				return true;
			}
			// A press anywhere else in the popover leaves the field, applying
			// whatever was typed rather than discarding it silently.
			if (paintHexFocus) commitPaintHex();
			if (mouseButton == OF_MOUSE_BUTTON_RIGHT)
			{
				// Removing a swatch is handled HERE, not in the generic
				// right-click branch below - that one deletes a CEL, and the
				// popover has to win inside its own rectangle.
				for (int i = 0; i < (int)paintPalette.size(); ++i)
				{
					if (!getPaintSwatchBounds(i).inside(mouse)) continue;
					removePaintPaletteColor(i);
					break;
				}
				return true;
			}
			handlePaintPickerPressed(mouse);
			return true;
		}
		// A press anywhere else dismisses the popover AND does what it would
		// normally do - dismissing and swallowing would cost a second click.
		paintPickerOpen = false;
	}

	paintPanelPointerCaptured = true;
	paintDragMode = PAINT_DRAG_NONE;

	// Clicking away from a rename applies it, the way every rename field does.
	// Done before any hit test so the press still does whatever it was aimed at.
	if (paintRenamingLayer >= 0)
	{
		const int renamingRow = paintLayerRowAtScreen(mouse);
		if (renamingRow < 0 || paintLayerAtRow(renamingRow) != paintRenamingLayer)
		{
			commitPaintLayerRename();
		}
	}

	// Read ONCE, here, and never again for the rest of the gesture - the drag
	// path keys off paintViewPanning instead. Re-reading mid-drag would let a
	// stroke that had already started turn into a pan, leaving a half-committed
	// live stroke behind.
	const bool panArmed = mouseButton == OF_MOUSE_BUTTON_MIDDLE ||
		(mouseButton == OF_MOUSE_BUTTON_LEFT && paintPanKeyHeld());
	if (panArmed && getPaintCanvasArea().inside(mouse))
	{
		paintViewPanning = true;
		paintViewPanButton = mouseButton;
		paintViewPanStartMouse = mouse;
		paintViewPanStartCenter = paintViewCenter;
		return true;
	}
	if (mouseButton == OF_MOUSE_BUTTON_MIDDLE) return true;

	if (mouseButton == OF_MOUSE_BUTTON_RIGHT &&
		ofGetKeyPressed(OF_KEY_ALT) && getPaintCanvasArea().inside(mouse))
	{
		// Eyedropper. Alt is the sampling modifier in every paint program, and it
		// works without leaving the tool in hand - which is the whole point.
		ofFloatColor sampled;
		if (box->sampleColor(paintView().toUv(mouse).x,
			paintView().toUv(mouse).y, sampled))
		{
			paintColor = sampled;
			paintPickerHue = paintColor.getHue();
			paintPickerSat = paintColor.getSaturation();
			paintPickerVal = paintColor.getBrightness();
		}
		return true;
	}

	if (mouseButton != OF_MOUSE_BUTTON_LEFT) return true;

	if (getPaintHelpIconBounds().inside(mouse))
	{
		paintHelpOpen = true;
		paintHelpScroll = 0.0f;
		return true;
	}
	if (getPaintPanelCloseBounds().inside(mouse))
	{
		endPaintEdit();
		return true;
	}
	if (mouseOverPaintPanelResizeHandle())
	{
		paintPanelResizing = true;
		paintPanelDragStartMouse = mouse;
		paintPanelResizeStartSize.set(paintPanelW, paintPanelH);
		return true;
	}
	if (mouseOverPaintPanelHeader())
	{
		paintPanelDragging = true;
		paintPanelDragStartMouse = mouse;
		paintPanelDragStartPos.set(paintPanelX, paintPanelY);
		return true;
	}

	for (int slot = 0; slot < kToolbarToolCount; ++slot)
	{
		if (!getPaintToolBounds(slot).inside(mouse)) continue;
		paintTool = (int)kToolbarTools[slot];
		return true;
	}

	const ofRectangle slider = getPaintSizeSliderBounds();
	if (slider.inside(mouse))
	{
		paintDragMode = PAINT_DRAG_SIZE;
		const float t = ofClamp(
			(mouse.x - slider.x) / std::max(1.0f, slider.width), 0.0f, 1.0f);
		if (paintTool == (int)JPPaintTool::Fill) paintFillTolerance = t;
		else paintBrushSize = brushFromSlider(t);
		return true;
	}
	if (getPaintColorSwatchBounds().inside(mouse))
	{
		paintPickerOpen = !paintPickerOpen;
		if (paintPickerOpen)
		{
			paintPickerHue = paintColor.getHue();
			paintPickerSat = paintColor.getSaturation();
			paintPickerVal = paintColor.getBrightness();
		}
		return true;
	}
	for (int i = 0; i < std::min(6, (int)paintPalette.size()); ++i)
	{
		if (getPaintQuickSwatchBounds(i).inside(mouse))
		{
			paintColor = paintPalette[(std::size_t)i];
			paintPickerHue = paintColor.getHue();
			paintPickerSat = paintColor.getSaturation();
			paintPickerVal = paintColor.getBrightness();
			return true;
		}
	}
	for (int action = 0; action < PAINT_ACTION_COUNT; ++action)
	{
		if (!getPaintActionBounds(action).inside(mouse)) continue;
		const bool changed = action == PAINT_ACTION_UNDO ?
			box->undo() : box->redo();
		if (changed)
		{
			clearPaintSelection();
			clampPaintTimelineScroll();
			markPaintChanged();
		}
		return true;
	}

	JPMediaState &state = box->mediaState();
	for (int slot = 0; slot < PAINT_TRANSPORT_COUNT; ++slot)
	{
		if (!getPaintTransportBounds(slot).inside(mouse)) continue;
		if (slot == PAINT_TRANSPORT_PREV) box->setCurrentCel(box->currentCel() - 1);
		else if (slot == PAINT_TRANSPORT_PLAY) state.playing = !state.playing;
		else if (slot == PAINT_TRANSPORT_NEXT) box->setCurrentCel(box->currentCel() + 1);
		else if (slot == PAINT_TRANSPORT_FPS)
		{
			paintDragMode = PAINT_DRAG_FPS;
			paintPanelDragStartMouse = mouse;
			paintPanelDragStartPos.set(box->document().fps, 0.0f);
		}
		else if (slot == PAINT_TRANSPORT_DIRECTION)
			jp_media::toggleDirection(state);
		else if (slot == PAINT_TRANSPORT_LOOP)
			jp_media::cycleLoopMode(state);
		else if (slot == PAINT_TRANSPORT_ONION)
		{
			// One control cycling both sides: separate before/after spinners
			// would be four more hit targets for a setting nobody tunes
			// asymmetrically.
			JPPaintDocument &doc = box->document();
			const int next = (doc.onionBefore + 1) % 4;
			doc.onionBefore = next;
			doc.onionAfter = next;
		}
		else paintReferenceVisible = !paintReferenceVisible;
		markPaintChanged();
		return true;
	}

	if (getPaintLayerAddBounds().inside(mouse))
	{
		box->addLayer();
		markPaintChanged();
		return true;
	}
	{
		const int row = paintLayerRowAtScreen(mouse);
		if (row >= 0)
		{
			const int index = paintLayerAtRow(row);
			if (index >= 0)
			{
				if (getPaintLayerEyeBounds(row).inside(mouse))
				{
					JPPaintLayerInfo props = box->document()
						.layers[(std::size_t)index];
					props.visible = !props.visible;
					box->setLayerProps(index, props);
				}
				else if (getPaintLayerBadgeBounds(row).inside(mouse))
				{
					box->toggleLayerBackground(index);
				}
				else if (box->document().layers.size() > 1 && getPaintLayerDeleteBounds(row).inside(mouse))
				{
					box->deleteLayer(index);
					markPaintChanged();
				}
				else if (getPaintLayerOpacityBounds(row).inside(mouse))
				{
					paintDragMode = PAINT_DRAG_LAYER_OPACITY;
					paintDragLayerFrom = index;
					const ofRectangle bar = getPaintLayerOpacityBounds(row);
					JPPaintLayerInfo props = box->document()
						.layers[(std::size_t)index];
					props.opacity = ofClamp(
						(mouse.x - bar.x) / std::max(1.0f, bar.width), 0.0f, 1.0f);
					box->setLayerProps(index, props);
				}
				else
				{
					const uint64_t now = ofGetSystemTimeMillis();
					const bool doubleClicked = row == paintLastClickRow &&
						now - paintLastClickMillis < duration_mouseclick;
					paintLastClickMillis = now;
					paintLastClickRow = row;
					// The panel consumes its own presses, so JPboxgroup's
					// isDoubleClick is never computed for them - hence the local
					// pair. Only the NAME strip starts a rename, so a
					// double-click meant as two selections does not.
					if (doubleClicked && getPaintLayerNameBounds(row).inside(mouse))
					{
						beginPaintLayerRename(index);
					}
					else
					{
						box->setCurrentLayer(index);
						paintDragMode = PAINT_DRAG_LAYER;
						paintDragLayerFrom = index;
						paintDragLayerTo = index;
					}
				}
				markPaintChanged();
			}
			return true;
		}
		// Dead space in the column still belongs to the column.
		if (getPaintTimelineGutterBounds().inside(mouse)) return true;
	}
	if (getPaintAddCelBounds().inside(mouse))
	{
		box->addCel(ofGetKeyPressed(OF_KEY_SHIFT));
		clampPaintTimelineScroll();
		markPaintChanged();
		return true;
	}
	{
		// A grid cell selects the frame AND the layer - that is the whole point of
		// a grid. It does NOT start a drag: dragging reorders, and reordering
		// lives on the frame header and the gutter where the thing being moved is
		// unambiguous.
		int cellFrame = -1;
		int cellRow = -1;
		if (paintCellAtScreen(mouse, cellFrame, cellRow))
		{
			const int index = paintLayerAtRow(cellRow);
			if (index >= 0)
			{
				box->setCurrentCel(cellFrame);
				box->setCurrentLayer(index);
				markPaintChanged();
			}
			return true;
		}
	}
	const int cel = paintCelAtScreen(mouse);
	if (cel >= 0)
	{
		box->setCurrentCel(cel);
		paintDragMode = PAINT_DRAG_CEL;
		paintDragCelFrom = cel;
		paintDragCelTo = cel;
		markPaintChanged();
		return true;
	}
	if (getPaintTimelineBounds().inside(mouse)) return true;

	if (getPaintCanvasArea().inside(mouse))
	{
		// Drawing while the transport runs would put the mark on whichever cel
		// the playhead happened to reach, so painting stops playback rather
		// than fighting it.
		if (state.playing)
		{
			state.playing = false;
			box->setCurrentCel(box->currentCel());
		}
		if (paintTool == (int)JPPaintTool::Fill)
		{
			// A bucket is a click, not a gesture: it commits immediately and
			// there is no live stroke to preview.
			//
			// Capped because every fill on a cel costs a full readback and a
			// whole-canvas scan EVERY time that cel is re-rasterized. Sixteen is
			// generous for real use and keeps a pathological cel editable.
			int existing = 0;
			const std::vector<JPPaintStroke> *list = jp_paint::strokeListFor(
				box->document(), box->document().currentFrame,
				box->currentLayer());
			if (list != nullptr)
			{
				for (const JPPaintStroke &s : *list)
				{
					if (s.tool == (int)JPPaintTool::Fill) ++existing;
				}
			}
			if (existing >= 16)
			{
				ofLogWarning("JPboxgroup") <<
					"paint: this cel already carries 16 fills; flatten or clear "
					"it before adding another";
				return true;
			}
			JPPaintStroke fill;
			fill.r = paintColor.r;
			fill.g = paintColor.g;
			fill.b = paintColor.b;
			fill.a = paintColor.a;
			fill.tool = (int)JPPaintTool::Fill;
			fill.tolerance = paintFillTolerance;
			const ofVec2f uv = paintView().toUv(mouse);
			// Off-canvas has no region to flood.
			if (uv.x < 0.0f || uv.y < 0.0f || uv.x > 1.0f || uv.y > 1.0f)
			{
				return true;
			}
			fill.points.push_back(JPPaintPoint{uv.x, uv.y, 1.0f});
			box->commitStroke(fill);
			markPaintChanged();
			return true;
		}
		if (paintTool == (int)JPPaintTool::LassoSelect)
		{
			const ofVec2f uv = paintView().toUv(mouse);
			bool clickedInside = false;
			bool clickedRotate = false;
			bool clickedScale = false;
			const ofVec2f centerVal = paintSelectionBounds.getCenter();
			
			if (paintSelectionActive)
			{
				const JPViewTransform view = paintView();
				float aspect = view.canvasRect().height > 0.0f ? (view.canvasRect().width / view.canvasRect().height) : 1.0f;
				
				auto getCornerScr = [&](float x, float y) {
					ofVec2f pt = centerVal + (ofVec2f(x, y) - centerVal) * (paintSelectionScaling ? paintSelectionScale : 1.0f);
					if (paintSelectionRotating)
					{
						pt = rotatePointAround(pt, centerVal, paintSelectionRotation, aspect);
					}
					if (paintSelectionDragging)
					{
						pt += paintSelectionDragOffset;
					}
					return view.toScreen(pt);
				};
				
				ofVec2f tlScr = getCornerScr(paintSelectionBounds.x, paintSelectionBounds.y);
				ofVec2f trScr = getCornerScr(paintSelectionBounds.getRight(), paintSelectionBounds.y);
				ofVec2f blScr = getCornerScr(paintSelectionBounds.x, paintSelectionBounds.getBottom());
				ofVec2f brScr = getCornerScr(paintSelectionBounds.getRight(), paintSelectionBounds.getBottom());
				
				if (mouse.distance(tlScr) < 8.0f || mouse.distance(trScr) < 8.0f ||
					mouse.distance(blScr) < 8.0f || mouse.distance(brScr) < 8.0f)
				{
					clickedScale = true;
				}
				else
				{
					ofVec2f topCenter = ofVec2f(centerVal.x, centerVal.y + (paintSelectionBounds.y - centerVal.y) * (paintSelectionScaling ? paintSelectionScale : 1.0f));
					if (paintSelectionRotating)
					{
						topCenter = rotatePointAround(topCenter, centerVal, paintSelectionRotation, aspect);
					}
					if (paintSelectionDragging)
					{
						topCenter += paintSelectionDragOffset;
					}
					ofVec2f centerScr = view.toScreen(centerVal + (paintSelectionDragging ? paintSelectionDragOffset : ofVec2f(0.0f, 0.0f)));
					ofVec2f topCenterScr = view.toScreen(topCenter);
					ofVec2f dir = (topCenterScr - centerScr).getNormalized();
					if (dir.lengthSquared() == 0.0f) dir.set(0.0f, -1.0f);
					ofVec2f handleScr = topCenterScr + dir * 20.0f;
					
					if (mouse.distance(handleScr) < 12.0f)
					{
						clickedRotate = true;
					}
					else
					{
						clickedInside = isPointInPolygon(uv, paintSelectionPath);
					}
				}
			}

			if (clickedScale)
			{
				paintSelectionScaling = true;
				const JPViewTransform view = paintView();
				const ofVec2f centerScr = view.toScreen(centerVal + (paintSelectionDragging ? paintSelectionDragOffset : ofVec2f(0.0f, 0.0f)));
				paintSelectionScaleStartDist = mouse.distance(centerScr);
				if (paintSelectionScaleStartDist < 1.0f) paintSelectionScaleStartDist = 1.0f;
				paintSelectionScale = 1.0f;
			}
			else if (clickedRotate)
			{
				paintSelectionRotating = true;
				const JPViewTransform view = paintView();
				const ofVec2f centerScr = view.toScreen(centerVal + (paintSelectionDragging ? paintSelectionDragOffset : ofVec2f(0.0f, 0.0f)));
				paintSelectionRotateStartAngle = ofRadToDeg(atan2(mouse.y - centerScr.y, mouse.x - centerScr.x)) - paintSelectionRotation;
			}
			else if (clickedInside)
			{
				paintSelectionDragging = true;
				paintSelectionDragStartUv = uv;
				paintSelectionDragOffset.set(0.0f, 0.0f);
				if (ofGetKeyPressed(OF_KEY_ALT))
				{
					duplicateSelectedStrokes(box);
				}
			}
			else
			{
				// Clicking outside confirms the previous selection and immediately
				// starts another one while the selection tool remains active.
				clearPaintSelection();
				paintDragMode = PAINT_DRAG_STROKE;
				beginPaintStroke(box, uv);
			}
			return true;
		}

		paintDragMode = PAINT_DRAG_STROKE;
		beginPaintStroke(box, paintView().toUv(mouse));
		return true;
	}
	return true;
}

bool JPboxgroup::update_paintMouseDragged(int mouseButton)
{
	if (!paintPanelPointerCaptured) return false;
	JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());

	if (paintViewPanning)
	{
		// Not the matching button: a pan must not be driven or ended by a
		// button other than the one that armed it.
		if (mouseButton != paintViewPanButton) return false;
		const JPViewTransform view = paintView();
		const ofVec2f delta = mouse - paintViewPanStartMouse;
		paintViewCenter = paintViewPanStartCenter - ofVec2f(
			delta.x / std::max(1.0f, view.preview.width * paintViewZoom),
			delta.y / std::max(1.0f, view.preview.height * paintViewZoom));
		clampPaintView();
		return true;
	}
	// A gesture that began in this panel belongs to it for its whole life, so a
	// button driving no gesture is still CONSUMED rather than handed back. This
	// returned false, and JPboxgroup::update_mouseDragged then panned the graph
	// with it - a right-drag inside the panel moved the canvas behind it.
	if (mouseButton != OF_MOUSE_BUTTON_LEFT) return true;
 
	if (paintSelectionScaling)
	{
		const JPViewTransform view = paintView();
		const ofVec2f centerVal = paintSelectionBounds.getCenter();
		const ofVec2f centerScr = view.toScreen(centerVal + (paintSelectionDragging ? paintSelectionDragOffset : ofVec2f(0.0f, 0.0f)));
		float currentDist = mouse.distance(centerScr);
		paintSelectionScale = currentDist / paintSelectionScaleStartDist;
		if (paintSelectionScale < 0.05f) paintSelectionScale = 0.05f;
		markPaintChanged();
		return true;
	}

	if (paintSelectionRotating)
	{
		const JPViewTransform view = paintView();
		const ofVec2f centerVal = paintSelectionBounds.getCenter();
		const ofVec2f centerScr = view.toScreen(centerVal);
		float currentAngle = ofRadToDeg(atan2(mouse.y - centerScr.y, mouse.x - centerScr.x));
		paintSelectionRotation = currentAngle - paintSelectionRotateStartAngle;
		markPaintChanged();
		return true;
	}

	if (paintSelectionDragging)
	{
		const ofVec2f uv = paintView().toUv(mouse);
		paintSelectionDragOffset = uv - paintSelectionDragStartUv;
		markPaintChanged();
		return true;
	}

	if (paintPanelDragging)
	{
		const ofVec2f delta = mouse - paintPanelDragStartMouse;
		paintPanelX = paintPanelDragStartPos.x + delta.x;
		paintPanelY = paintPanelDragStartPos.y + delta.y;
		clampPaintPanelLayout();
		return true;
	}
	if (paintPanelResizing)
	{
		const ofVec2f delta = mouse - paintPanelDragStartMouse;
		paintPanelW = paintPanelResizeStartSize.x + delta.x;
		paintPanelH = paintPanelResizeStartSize.y + delta.y;
		clampPaintPanelLayout();
		return true;
	}

	switch (paintDragMode)
	{
	case PAINT_DRAG_STROKE:
		extendPaintStroke(box, paintView().toUv(mouse));
		return true;
	case PAINT_DRAG_SIZE:
	{
		const ofRectangle slider = getPaintSizeSliderBounds();
		const float t = ofClamp(
			(mouse.x - slider.x) / std::max(1.0f, slider.width), 0.0f, 1.0f);
		if (paintTool == (int)JPPaintTool::Fill) paintFillTolerance = t;
		else paintBrushSize = brushFromSlider(t);
		return true;
	}
	case PAINT_DRAG_FPS:
	{
		// Absolute from the press value, not accumulated, so the rate does not
		// depend on how many mouse events arrived.
		const float delta = (mouse.x - paintPanelDragStartMouse.x) * 0.15f;
		box->document().fps = ofClamp(
			std::round(paintPanelDragStartPos.x + delta), 1.0f, 60.0f);
		return true;
	}
	case PAINT_DRAG_CEL:
	{
		const int over = paintCelAtScreen(mouse);
		if (over >= 0) paintDragCelTo = over;
		return true;
	}
	case PAINT_DRAG_LAYER:
	{
		const int row = paintLayerRowAtScreen(mouse);
		if (row >= 0)
		{
			const int index = paintLayerAtRow(row);
			if (index >= 0) paintDragLayerTo = index;
		}
		return true;
	}
	case PAINT_DRAG_LAYER_OPACITY:
	{
		if (paintDragLayerFrom < 0) return true;
		// Absolute from the bar, not accumulated, so the value cannot depend on
		// how many mouse events arrived.
		int row = -1;
		for (int r = 0; r < (int)box->document().layers.size(); ++r)
		{
			if (paintLayerAtRow(r) == paintDragLayerFrom) { row = r; break; }
		}
		if (row < 0) return true;
		const ofRectangle bar = getPaintLayerOpacityBounds(row);
		JPPaintLayerInfo props =
			box->document().layers[(std::size_t)paintDragLayerFrom];
		props.opacity = ofClamp(
			(mouse.x - bar.x) / std::max(1.0f, bar.width), 0.0f, 1.0f);
		box->setLayerProps(paintDragLayerFrom, props);
		return true;
	}
	case PAINT_DRAG_PICKER_SV:
	case PAINT_DRAG_PICKER_HUE:
	case PAINT_DRAG_PICKER_ALPHA:
		updatePaintPickerFromDrag(mouse);
		return true;
	default:
		return true;
	}
}

bool JPboxgroup::update_paintMouseReleased(int mouseButton)
{
	if (!paintPanelPointerCaptured) return false;
	JPbox_paint *box = getPaintEditBox();

	if (paintViewPanning)
	{
		if (mouseButton != paintViewPanButton) return false;
		paintViewPanning = false;
		paintViewPanButton = -1;
		paintPanelPointerCaptured = false;
		return true;
	}
	if (mouseButton != OF_MOUSE_BUTTON_LEFT)
	{
		// A right-click delete is done at press; nothing to finish here, but
		// the latch still has to clear or the next drag is ignored.
		paintPanelPointerCaptured = false;
		return true;
	}

	if (box != nullptr)
	{
		if (paintTool == (int)JPPaintTool::LassoSelect)
		{
			if (paintSelectionScaling)
			{
				if (std::abs(paintSelectionScale - 1.0f) > 0.01f)
					scaleSelectedStrokes(box, paintSelectionScale);
				paintSelectionScaling = false;
				paintSelectionScale = 1.0f;
			}
			else if (paintSelectionRotating)
			{
				if (std::abs(paintSelectionRotation) > 0.01f)
					rotateSelectedStrokes(box, paintSelectionRotation);
				paintSelectionRotating = false;
				paintSelectionRotation = 0.0f;
			}
			else if (paintSelectionDragging)
			{
				if (paintSelectionDragOffset.lengthSquared() > 0.0f)
					moveSelectedStrokes(box, paintSelectionDragOffset);
				paintSelectionDragging = false;
				paintSelectionDragOffset.set(0.0f, 0.0f);
			}
			else if (paintDragMode == PAINT_DRAG_STROKE)
			{
				// Selection cuts non-destructively and keeps the interior active.
				endSelectionDrawing(box);
				if (!paintSelectionPath.empty())
				{
					cutStrokesWithLasso(box, paintSelectionPath,
						paintSelectedStrokeIndices, paintSelectionBounds);
					paintSelectionActive = !paintSelectedStrokeIndices.empty();
					paintSelectionDragging = false;
					paintSelectionRotating = false;
					paintSelectionScaling = false;
					paintSelectionRotation = 0.0f;
					paintSelectionScale = 1.0f;
					paintSelectionDragOffset.set(0.0f, 0.0f);
				}
			}
			markPaintChanged();
		}
		else if (paintDragMode == PAINT_DRAG_STROKE) endPaintStroke(box);
		else if (paintDragMode == PAINT_DRAG_CEL &&
			paintDragCelFrom >= 0 && paintDragCelTo >= 0 &&
			paintDragCelFrom != paintDragCelTo)
		{
			box->moveCel(paintDragCelFrom, paintDragCelTo);
			markPaintChanged();
		}
		else if (paintDragMode == PAINT_DRAG_LAYER &&
			paintDragLayerFrom >= 0 && paintDragLayerTo >= 0 &&
			paintDragLayerFrom != paintDragLayerTo)
		{
			box->moveLayer(paintDragLayerFrom, paintDragLayerTo);
			markPaintChanged();
		}
		else if (paintDragMode == PAINT_DRAG_FPS ||
			paintDragMode == PAINT_DRAG_SIZE ||
			paintDragMode == PAINT_DRAG_LAYER_OPACITY)
		{
			markPaintChanged();
		}
	}

	paintDragMode = PAINT_DRAG_NONE;
	paintDragCelFrom = -1;
	paintDragCelTo = -1;
	paintDragLayerFrom = -1;
	paintDragLayerTo = -1;
	paintPanelDragging = false;
	paintPanelResizing = false;
	paintPanelPointerCaptured = false;
	clampPaintPanelLayout();
	return true;
}

bool JPboxgroup::update_paintMouseScrolled(int x, int y, float scrollY)
{
	if (!paintEditActive || getPaintEditBox() == nullptr) return false;
	const ofVec2f mouse((float)x, (float)y);

	if (paintHelpOpen)
	{
		// Consumed whether or not the pointer is over the dialog: a modal must not
		// let the canvas zoom underneath it.
		paintHelpScroll -= scrollY * 40.0f;
		return true;
	}

	if (getPaintTimelineBounds().inside(mouse))
	{
		// Over the gutter the wheel walks the layer stack; over the grid it walks
		// the frames, which is the axis each side is showing.
		if (getPaintTimelineGutterBounds().inside(mouse))
		{
			paintTimelineScrollY -= scrollY * 30.0f;
		}
		else
		{
			paintFilmstripScroll -= scrollY * 40.0f;
		}
		clampPaintTimelineScroll();
		return true;
	}
	if (!getPaintCanvasArea().inside(mouse)) return false;
	if (scrollY == 0.0f) return false;

	// Cursor anchored: the point under the pointer stays under the pointer,
	// which is the only zoom that lets you work into a detail.
	const JPViewTransform view = paintView();
	const ofVec2f anchor = view.toUv(mouse);
	paintViewZoom = ofClamp(paintViewZoom * (scrollY > 0.0f ? 1.15f : 1.0f / 1.15f),
		1.0f, 24.0f);
	JPViewTransform zoomed = paintView();
	const ofVec2f after = zoomed.toUv(mouse);
	paintViewCenter += anchor - after;
	clampPaintView();
	return true;
}

// ------------------------------------------------------------------ keyboard

bool JPboxgroup::paintWantsKeyCapture() const
{
	return paintEditActive && getPaintEditBox() != nullptr;
}

bool JPboxgroup::paintUndoShortcut(bool redo)
{
	JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return false;
	// Consumed but ignored while typing: undoing a brush stroke because somebody
	// reached for Ctrl+Z in a text field would be a nasty surprise.
	if (paintTextCaptureActive()) return true;
	const bool changed = redo ? box->redo() : box->undo();
	if (changed)
	{
		// Selection is a document edit too. Undo restores the unsplit strokes;
		// keeping the floating lasso would leave stale indices addressing the
		// pre-undo list and make the operation look as if it had not reverted.
		clearPaintSelection();
		clampPaintTimelineScroll();
		markPaintChanged();
	}
	// Consumed either way: with the panel open, Ctrl+Z means "undo a stroke"
	// even when there is nothing left to undo. Falling through to the global
	// clipboard handlers would be a surprise.
	return true;
}

bool JPboxgroup::paintSelectAllShortcut()
{
	JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return false;
	// Consume the chord without touching the canvas while a field or modal owns
	// input, just like undo does.
	if (paintTextCaptureActive() || paintHelpOpen) return true;

	const int cel = std::clamp(box->document().currentFrame, 0,
		(int)box->document().frames.size() - 1);
	const std::vector<JPPaintStroke> *list =
		jp_paint::strokeListFor(box->document(), cel, box->currentLayer());
	clearPaintSelection();
	if (list == nullptr) return true;

	for (int i = 0; i < (int)list->size(); ++i)
	{
		const JPPaintStroke &stroke = (*list)[(std::size_t)i];
		// A fill is a replay command tied to its seed and preceding pixels, not
		// movable geometry. It remains in place when selecting the whole layer.
		if (!stroke.points.empty() && stroke.tool != (int)JPPaintTool::Fill)
			paintSelectedStrokeIndices.push_back(i);
	}
	if (paintSelectedStrokeIndices.empty()) return true;

	paintTool = (int)JPPaintTool::LassoSelect;
	paintSelectionPath = {
		ofVec2f(0.0f, 0.0f), ofVec2f(1.0f, 0.0f),
		ofVec2f(1.0f, 1.0f), ofVec2f(0.0f, 1.0f),
		ofVec2f(0.0f, 0.0f)};
	paintSelectionBounds = ofRectangle(0.0f, 0.0f, 1.0f, 1.0f);
	paintSelectionActive = true;
	return true;
}

void JPboxgroup::paintKeyPressed(int key)
{
	JPbox_paint *box = getPaintEditBox();
	if (box == nullptr) return;
	// The dialog is modal: a shortcut must not fire on the drawing behind it. ESC
	// still reaches it, through the surface stack rather than through here.
	if (paintHelpOpen) return;
	// A focused field owns the keyboard. Without this, typing a hex digit would
	// swap the brush and typing a layer name would delete cels.
	if (paintHandleTextKey(key)) return;
	if ((key == OF_KEY_RETURN || key == '\r') && paintSelectionActive)
	{
		clearPaintSelection();
		return;
	}
	JPPaintDocument &doc = box->document();
	JPMediaState &state = box->mediaState();
	bool changed = true;

	switch (key)
	{
	case 'b': case 'B': paintTool = (int)JPPaintTool::Brush; break;
	case 'e': case 'E': paintTool = (int)JPPaintTool::Eraser; break;
	case 'l': case 'L': paintTool = (int)JPPaintTool::Line; break;
	case 'r': case 'R': paintTool = (int)JPPaintTool::Rect; break;
	case 'o': paintTool = (int)JPPaintTool::Ellipse; break;
	case 'p': paintTool = (int)JPPaintTool::Lasso; break;
	case 's': case 'S': paintTool = (int)JPPaintTool::LassoSelect; break;

	// Shift+, and Shift+. - the layer parallel to , and . stepping cels.
	case '<': box->setCurrentLayer(box->currentLayer() - 1); break;
	case '>': box->setCurrentLayer(box->currentLayer() + 1); break;
	case 'g': case 'G': paintTool = (int)JPPaintTool::Fill; break;
	case 'O':
	{
		const int next = (doc.onionBefore + 1) % 4;
		doc.onionBefore = next;
		doc.onionAfter = next;
		break;
	}
	case '[':
		paintBrushSize = brushFromSlider(
			sliderFromBrush(paintBrushSize) - 0.05f);
		break;
	case ']':
		paintBrushSize = brushFromSlider(
			sliderFromBrush(paintBrushSize) + 0.05f);
		break;
	case ',': case OF_KEY_LEFT: box->setCurrentCel(box->currentCel() - 1); break;
	case '.': case OF_KEY_RIGHT: box->setCurrentCel(box->currentCel() + 1); break;
	case ' ': state.playing = !state.playing; break;
	case 'n': case 'N': box->addCel(false); clampPaintTimelineScroll(); break;
	case 'd': case 'D':
		if (paintSelectionActive)
		{
			duplicateSelectedStrokes(box);
		}
		else
		{
			box->addCel(true);
			clampPaintTimelineScroll();
		}
		break;
	// THE reason this panel takes the keyboard. Without it this key reaches
	// ofApp::keyPressed and deletes the box being drawn on, taking the whole
	// drawing with it - which is what happens today with the mapping panel open.
	case OF_KEY_DEL: case OF_KEY_BACKSPACE:
		if (paintSelectionActive)
		{
			deleteSelectedStrokes(box);
		}
		else if (ofGetKeyPressed(OF_KEY_SHIFT))
		{
			box->deleteCel(doc.currentFrame);
			clampPaintTimelineScroll();
		}
		else
		{
			box->clearCurrentLayer();
		}
		break;
	case '-':
		box->setCelHold(doc.currentFrame,
			jp_paint::holdOf(doc.frames[(std::size_t)doc.currentFrame]) - 1);
		break;
	case '=': case '+':
		box->setCelHold(doc.currentFrame,
			jp_paint::holdOf(doc.frames[(std::size_t)doc.currentFrame]) + 1);
		break;
	default:
		changed = false;
		break;
	}
	if (changed) markPaintChanged();
}
