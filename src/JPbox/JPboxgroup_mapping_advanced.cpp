#include "JPboxgroup.h"

#include "../JPutils/jp_tooltip.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr float kAdvancedToolbarHeight = 30.0f;
	constexpr float kAdvancedHeaderHeight = 30.0f;
	constexpr float kAdvancedPanelTop =
		kAdvancedHeaderHeight + kAdvancedToolbarHeight;

	ofVec2f cubicPoint(const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &c, const ofVec2f &d, float t)
	{
		const float s = 1.0f - t;
		return a * s * s * s + b * 3.0f * s * s * t +
			c * 3.0f * s * t * t + d * t * t * t;
	}

	void drawCubic(const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &c, const ofVec2f &d,
		float x, float y, float width, float height)
	{
		ofPolyline line;
		for (int i = 0; i <= 28; i++)
		{
			const ofVec2f point = cubicPoint(a, b, c, d, i / 28.0f);
			line.addVertex(x + point.x * width, y + point.y * height);
		}
		line.draw();
	}

	ofVec2f clampMappingPoint(const ofVec2f &point)
	{
		return ofVec2f(ofClamp(point.x, 0.0f, 1.0f),
			ofClamp(point.y, 0.0f, 1.0f));
	}

	// The four surface edges, as (corner A, corner B, handle at 1/3, handle at
	// 2/3). Same winding the shader and projectAdvancedMappingPoint use.
	struct MappingEdge
	{
		int cornerA;
		int cornerB;
		int handleA;
		int handleB;
	};
	const MappingEdge kMappingEdges[4] = {
		{0, 1, 0, 1},  // top    c0 -> c1
		{1, 2, 2, 3},  // right  c1 -> c2
		{3, 2, 4, 5},  // bottom c3 -> c2
		{0, 3, 6, 7}}; // left   c0 -> c3

	// The two edges that meet at each corner.
	const int kCornerEdges[4][2] = {
		{0, 3}, {0, 1}, {1, 2}, {2, 3}};

	// A handle described in its own edge's frame: how far along the chord it
	// sits and how far off to the side, both relative to the chord length. A
	// handle resting exactly on the chord at its third has a zero offset, which
	// is what lets a straight edge stay straight when a corner is dragged.
	ofVec2f mappingHandleToEdgeFrame(const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &handle, float t)
	{
		const ofVec2f chord = b - a;
		const float length = chord.length();
		if (length < 0.0001f)
			return ofVec2f(0.0f, 0.0f);
		const ofVec2f along = chord / length;
		const ofVec2f normal(-along.y, along.x);
		const ofVec2f delta = handle - (a + chord * t);
		return ofVec2f(delta.dot(along) / length,
			delta.dot(normal) / length);
	}

	ofVec2f mappingHandleFromEdgeFrame(const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &offset, float t)
	{
		const ofVec2f chord = b - a;
		const float length = chord.length();
		const ofVec2f base = a + chord * t;
		if (length < 0.0001f)
			return base;
		const ofVec2f along = chord / length;
		const ofVec2f normal(-along.y, along.x);
		return base + along * (offset.x * length) +
			normal * (offset.y * length);
	}

	// Where a handle sits when its edge is a straight line: on the chord, at a
	// third and two thirds. The cubic then degenerates to that chord exactly.
	ofVec2f mappingStraightHandle(const ofVec2f &a, const ofVec2f &b, float t)
	{
		return a + (b - a) * t;
	}

	bool mappingLayerHasCurvedEdge(
		const JPbox_shader::AdvancedMappingLayer &layer)
	{
		for (const MappingEdge &edge : kMappingEdges)
		{
			const ofVec2f &a = layer.corners[edge.cornerA];
			const ofVec2f &b = layer.corners[edge.cornerB];
			if (layer.edgeHandles[edge.handleA].distance(
					mappingStraightHandle(a, b, 1.0f / 3.0f)) > 0.0005f ||
				layer.edgeHandles[edge.handleB].distance(
					mappingStraightHandle(a, b, 2.0f / 3.0f)) > 0.0005f)
				return true;
		}
		return false;
	}

	void mappingStraightenLayerEdges(
		JPbox_shader::AdvancedMappingLayer &layer)
	{
		for (const MappingEdge &edge : kMappingEdges)
		{
			const ofVec2f &a = layer.corners[edge.cornerA];
			const ofVec2f &b = layer.corners[edge.cornerB];
			layer.edgeHandles[edge.handleA] =
				mappingStraightHandle(a, b, 1.0f / 3.0f);
			layer.edgeHandles[edge.handleB] =
				mappingStraightHandle(a, b, 2.0f / 3.0f);
		}
	}

	// Outlines in normalized space, sampled exactly the way drawCubic draws
	// them, so what you can grab is always what you can see. Both are fed to
	// ofPolyline::inside, which wraps the last vertex to the first - the line
	// must NOT be closed explicitly, and must never be empty (inside() indexes
	// vertex 0 unguarded).
	void appendCubic(ofPolyline &line, const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &c, const ofVec2f &d)
	{
		for (int i = 1; i <= 28; i++)
		{
			const ofVec2f point = cubicPoint(a, b, c, d, i / 28.0f);
			line.addVertex(point.x, point.y, 0.0f);
		}
	}

	ofPolyline buildSurfaceOutline(
		const JPbox_shader::AdvancedMappingLayer &layer)
	{
		ofPolyline line;
		line.addVertex(layer.corners[0].x, layer.corners[0].y, 0.0f);
		for (const MappingEdge &edge : {kMappingEdges[0], kMappingEdges[1]})
			appendCubic(line, layer.corners[edge.cornerA],
				layer.edgeHandles[edge.handleA],
				layer.edgeHandles[edge.handleB],
				layer.corners[edge.cornerB]);
		// Bottom and left are stored c3->c2 and c0->c3, so walk them backwards
		// to keep the outline a single continuous loop.
		appendCubic(line, layer.corners[2], layer.edgeHandles[5],
			layer.edgeHandles[4], layer.corners[3]);
		appendCubic(line, layer.corners[3], layer.edgeHandles[7],
			layer.edgeHandles[6], layer.corners[0]);
		return line;
	}

	ofPolyline buildMaskOutline(
		const JPbox_shader::AdvancedMappingContour &contour)
	{
		ofPolyline line;
		if (contour.nodes.empty()) return line;
		line.addVertex(contour.nodes[0].anchor.x,
			contour.nodes[0].anchor.y, 0.0f);
		if (contour.nodes.size() == 1) return line;
		const size_t edgeCount = contour.closed ?
			contour.nodes.size() : contour.nodes.size() - 1;
		for (size_t i = 0; i < edgeCount; i++)
		{
			const auto &from = contour.nodes[i];
			const auto &to = contour.nodes[(i + 1) % contour.nodes.size()];
			if (from.smooth || to.smooth)
				appendCubic(line, from.anchor, from.outHandle,
					to.inHandle, to.anchor);
			else
				line.addVertex(to.anchor.x, to.anchor.y, 0.0f);
		}
		return line;
	}

	float closestCubicT(const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &c, const ofVec2f &d, const ofVec2f &point,
		float width, float height)
	{
		auto scaled = [&](const ofVec2f &value) {
			return ofVec2f(value.x * width, value.y * height);
		};
		float bestT = 0.0f;
		float bestDistance = std::numeric_limits<float>::max();
		for (int i = 0; i <= 64; i++)
		{
			const float t = i / 64.0f;
			const float distance = scaled(cubicPoint(a, b, c, d, t))
				.distance(scaled(point));
			if (distance < bestDistance)
			{
				bestDistance = distance;
				bestT = t;
			}
		}
		float range = 1.0f / 64.0f;
		for (int pass = 0; pass < 5; pass++)
		{
			const float left = std::max(0.0f, bestT - range);
			const float right = std::min(1.0f, bestT + range);
			for (int i = 0; i <= 8; i++)
			{
				const float t = ofLerp(left, right, i / 8.0f);
				const float distance = scaled(cubicPoint(a, b, c, d, t))
					.distance(scaled(point));
				if (distance < bestDistance)
				{
					bestDistance = distance;
					bestT = t;
				}
			}
			range *= 0.25f;
		}
		return bestT;
	}

	JPbox_shader::AdvancedMappingNode splitContourEdge(
		JPbox_shader::AdvancedMappingNode &from,
		JPbox_shader::AdvancedMappingNode &to, float t)
	{
		const ofVec2f p01 = from.anchor.getInterpolated(from.outHandle, t);
		const ofVec2f p12 = from.outHandle.getInterpolated(to.inHandle, t);
		const ofVec2f p23 = to.inHandle.getInterpolated(to.anchor, t);
		const ofVec2f p012 = p01.getInterpolated(p12, t);
		const ofVec2f p123 = p12.getInterpolated(p23, t);
		const ofVec2f point = p012.getInterpolated(p123, t);
		from.outHandle = p01;
		to.inHandle = p23;
		JPbox_shader::AdvancedMappingNode inserted;
		inserted.anchor = point;
		inserted.inHandle = p012;
		inserted.outHandle = p123;
		inserted.smooth = true;
		return inserted;
	}

	bool outlineContains(const ofPolyline &line, const ofVec2f &point)
	{
		return line.size() >= 3 && line.inside(point.x, point.y);
	}

	float pointSegmentDistance(const ofVec2f &point, const ofVec2f &a,
		const ofVec2f &b)
	{
		const ofVec2f segment = b - a;
		const float lengthSquared = segment.dot(segment);
		const float t = lengthSquared > 0.000001f ?
			ofClamp((point - a).dot(segment) / lengthSquared, 0.0f, 1.0f) : 0.0f;
		return point.distance(a + segment * t);
	}

	bool segmentsIntersect(const ofVec2f &a, const ofVec2f &b,
		const ofVec2f &c, const ofVec2f &d)
	{
		auto cross = [](const ofVec2f &u, const ofVec2f &v) {
			return u.x * v.y - u.y * v.x;
		};
		const ofVec2f r = b - a;
		const ofVec2f s = d - c;
		const float denominator = cross(r, s);
		if (std::abs(denominator) < 0.00001f) return false;
		const float t = cross(c - a, s) / denominator;
		const float u = cross(c - a, r) / denominator;
		return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
	}

	bool polylineTouchesRectangle(const ofPolyline &line,
		const ofRectangle &rectangle, bool closed)
	{
		if (line.size() == 0) return false;
		for (const auto &vertex : line)
			if (rectangle.inside(vertex.x, vertex.y)) return true;
		const ofVec2f corners[4] = {
			{rectangle.x, rectangle.y}, {rectangle.getRight(), rectangle.y},
			{rectangle.getRight(), rectangle.getBottom()},
			{rectangle.x, rectangle.getBottom()}};
		if (closed && line.size() >= 3)
			for (const ofVec2f &corner : corners)
				if (line.inside(corner.x, corner.y)) return true;
		const size_t edgeCount = closed ? line.size() : line.size() - 1;
		for (size_t i = 0; i < edgeCount; i++)
		{
			const ofVec2f a(line[i].x, line[i].y);
			const ofVec2f b(line[(i + 1) % line.size()].x,
				line[(i + 1) % line.size()].y);
			for (int side = 0; side < 4; side++)
				if (segmentsIntersect(a, b, corners[side], corners[(side + 1) % 4]))
					return true;
		}
		return false;
	}

	// Corner i pairs with corner 3 - i, its diagonal opposite. A scale drag
	// pins that opposite corner, so the ordering here is load bearing.
	void mappingBoxCorners(const ofRectangle &box, ofVec2f corners[4])
	{
		corners[0].set(box.x, box.y);
		corners[1].set(box.getRight(), box.y);
		corners[2].set(box.x, box.getBottom());
		corners[3].set(box.getRight(), box.getBottom());
	}

	void drawDashedLine(const ofVec2f &a, const ofVec2f &b, float dash)
	{
		const float length = a.distance(b);
		if (length < 0.0001f) return;
		const ofVec2f step = (b - a) / length;
		for (float travelled = 0.0f; travelled < length; travelled += dash * 2.0f)
			ofDrawLine(a + step * travelled,
				a + step * std::min(travelled + dash, length));
	}

	void drawCloseIcon(const ofRectangle &bounds, bool hovered)
	{
		ofSetColor(hovered ? COL_ACCENT_RED : COL_TEXT_SECONDARY);
		ofSetLineWidth(1.5f);
		ofDrawLine(bounds.x + 5.0f, bounds.y + 5.0f,
			bounds.getRight() - 5.0f, bounds.getBottom() - 5.0f);
		ofDrawLine(bounds.getRight() - 5.0f, bounds.y + 5.0f,
			bounds.x + 5.0f, bounds.getBottom() - 5.0f);
	}
}

bool JPboxgroup::isAdvancedMappingShaderBox(JPbox *box) const
{
	JPbox_shader *shaderBox = dynamic_cast<JPbox_shader *>(box);
	return shaderBox != nullptr && shaderBox->isAdvancedMappingShader();
}

JPbox_shader *JPboxgroup::getAdvancedMappingEditBox()
{
	return dynamic_cast<JPbox_shader *>(getMappingEditBox());
}

int JPboxgroup::getAdvancedMappingParameterLayer(const string &name) const
{
	if (name.size() < 8 || name.rfind("layer", 0) != 0 ||
		name[6] != '_')
		return -1;
	const int layerIndex = name[5] - '1';
	return layerIndex >= 0 &&
		layerIndex < JPbox_shader::ADVANCED_MAPPING_LAYER_COUNT ?
		layerIndex : -1;
}

ofVec2f JPboxgroup::projectAdvancedMappingPoint(
	const JPbox_shader::AdvancedMappingLayer &layer,
	const ofVec2f &uv) const
{
	const ofVec2f top = cubicPoint(layer.corners[0],
		layer.edgeHandles[0], layer.edgeHandles[1],
		layer.corners[1], uv.x);
	const ofVec2f bottom = cubicPoint(layer.corners[3],
		layer.edgeHandles[4], layer.edgeHandles[5],
		layer.corners[2], uv.x);
	const ofVec2f left = cubicPoint(layer.corners[0],
		layer.edgeHandles[6], layer.edgeHandles[7],
		layer.corners[3], uv.y);
	const ofVec2f right = cubicPoint(layer.corners[1],
		layer.edgeHandles[2], layer.edgeHandles[3],
		layer.corners[2], uv.y);
	const ofVec2f topLinear = layer.corners[0] * (1.0f - uv.x) +
		layer.corners[1] * uv.x;
	const ofVec2f bottomLinear = layer.corners[3] * (1.0f - uv.x) +
		layer.corners[2] * uv.x;
	const ofVec2f bilinear = topLinear * (1.0f - uv.y) + bottomLinear * uv.y;
	return top * (1.0f - uv.y) + bottom * uv.y +
		left * (1.0f - uv.x) + right * uv.x - bilinear;
}

ofRectangle JPboxgroup::getAdvancedMappingToolbarBounds(
	AdvancedMappingToolbarAction action) const
{
	const float gap = 3.0f;
	// 8, not 11: with thirteen buttons the row has to stay inside the 420 px
	// minimum panel width (rightmost edge lands at 403).
	const float groupGap = 8.0f;
	const float buttonWidth = 25.0f;
	const float buttonHeight = 22.0f;
	// Buttons of the same function sit together; each group boundary adds a
	// wider gap, so the row reads as: pick a layer, pick a tool, shape what it
	// selected, reference image, files. Relies on the enum being in that order.
	const int group =
		action <= ADVANCED_MAPPING_LAYER_4 ? 0 :
		action <= ADVANCED_MAPPING_TOOL_MOVE ? 1 :
		action <= ADVANCED_MAPPING_FIT ? 2 :
		action <= ADVANCED_MAPPING_GUIDE ? 3 : 4;
	return ofRectangle(
		mappingPanelX + 10.0f +
			static_cast<int>(action) * (buttonWidth + gap) +
			group * groupGap,
		mappingPanelY + kAdvancedHeaderHeight +
			(kAdvancedToolbarHeight - buttonHeight) * 0.5f,
		buttonWidth, buttonHeight);
}

ofRectangle JPboxgroup::getAdvancedMappingHeaderActionBounds(
	bool newShape) const
{
	const ofRectangle guides =
		getMappingPanelActionBounds(MAPPING_PANEL_GUIDES);
	const float width = newShape ? 25.0f : 42.0f;
	const float gap = 5.0f;
	const float right = newShape ?
		guides.x - gap : guides.x - gap - 25.0f - gap;
	return ofRectangle(right - width,
		mappingPanelY + (kAdvancedHeaderHeight - 18.0f) * 0.5f,
		width, 18.0f);
}

bool JPboxgroup::advancedMappingBezierActive(
	const JPbox_shader::AdvancedMappingLayer &layer) const
{
	// Off by default, so a fresh surface behaves as a plain corner pin and a
	// corner drag cannot bend an edge. A layer that already carries a curve -
	// from a savefile or an SVG import - switches its handles back on by
	// itself, so opening older work never hides geometry that is really there.
	return advancedMappingBezierEnabled || mappingLayerHasCurvedEdge(layer);
}

bool JPboxgroup::getAdvancedMappingMoveBox(
	const JPbox_shader::AdvancedMappingLayer &layer, ofRectangle &box) const
{
	if (advancedMappingMoveTarget == ADVANCED_MAPPING_TARGET_SURFACE)
	{
		box = buildSurfaceOutline(layer).getBoundingBox();
		return box.width > 0.0001f && box.height > 0.0001f;
	}
	bool found = false;
	for (int contourIndex : advancedMappingSelectedMaskContours)
	{
		if (contourIndex < 0 ||
			contourIndex >= static_cast<int>(layer.masks.size())) continue;
		const ofPolyline outline = buildMaskOutline(layer.masks[contourIndex]);
		if (outline.size() == 0) continue;
		const ofRectangle contourBox = outline.getBoundingBox();
		if (!found) box = contourBox;
		else box.growToInclude(contourBox);
		found = true;
	}
	if (!found) return false;
	if (box.width <= 0.0001f)
	{
		box.x -= 0.005f;
		box.width = 0.01f;
	}
	if (box.height <= 0.0001f)
	{
		box.y -= 0.005f;
		box.height = 0.01f;
	}
	return true;
}

ofVec2f JPboxgroup::getAdvancedMappingRotationHandle(
	const ofRectangle &box, const ofRectangle &preview) const
{
	auto screen = [&](const ofVec2f &point) {
		return ofVec2f(
			preview.x + (0.5f + (point.x - advancedMappingViewCenter.x) *
				advancedMappingViewZoom) * preview.width,
			preview.y + (0.5f + (point.y - advancedMappingViewCenter.y) *
				advancedMappingViewZoom) * preview.height);
	};
	const ofVec2f corner = screen(ofVec2f(box.getRight(), box.y));
	const float contentTop = mappingPanelY + kAdvancedPanelTop + 12.0f;
	const float contentBottom = mappingPanelY + mappingPanelH - 12.0f;
	const float direction = corner.y - 30.0f >= contentTop ? -1.0f : 1.0f;
	return ofVec2f(ofClamp(corner.x + 20.0f,
		mappingPanelX + 13.0f, mappingPanelX + mappingPanelW - 13.0f),
		ofClamp(corner.y + direction * 24.0f, contentTop, contentBottom));
}

void JPboxgroup::drawAdvancedMappingParameterHeaders(JPbox *box)
{
	JPbox_shader *shaderBox = dynamic_cast<JPbox_shader *>(box);
	if (shaderBox == nullptr || !shaderBox->isAdvancedMappingShader())
		return;
	JPbox_shader::AdvancedMappingState *state =
		shaderBox->getAdvancedMappingState();
	if (state == nullptr) return;

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	for (const InspectorParameterGroupHeader &header :
		advancedMappingParameterHeaders)
	{
		if (header.layerIndex < 0 ||
			header.layerIndex >= JPbox_shader::ADVANCED_MAPPING_LAYER_COUNT)
			continue;
		const bool expanded =
			state->layers[header.layerIndex].inspectorExpanded;
		const bool hovered = header.bounds.inside(
			ofGetMouseX(), ofGetMouseY());
		if (hovered)
		{
			ofSetColor(ofColor(COL_BG_HOVER, 170));
			ofDrawRectRounded(header.bounds, 3.0f);
		}
		const float centerY = header.bounds.getCenter().y;
		const float chevronX = header.bounds.x + 10.0f;
		ofSetColor(expanded || hovered ?
			COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
		ofSetLineWidth(1.4f);
		if (expanded)
		{
			ofDrawLine(chevronX - 4.0f, centerY - 2.0f,
				chevronX, centerY + 2.0f);
			ofDrawLine(chevronX, centerY + 2.0f,
				chevronX + 4.0f, centerY - 2.0f);
		}
		else
		{
			ofDrawLine(chevronX - 2.0f, centerY - 4.0f,
				chevronX + 2.0f, centerY);
			ofDrawLine(chevronX + 2.0f, centerY,
				chevronX - 2.0f, centerY + 4.0f);
		}
		const string label = "TEXTURE " +
			ofToString(header.layerIndex + 1);
		ofSetColor(hovered ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		jp_constants::p_font.drawString(label,
			header.bounds.x + 24.0f, centerY + 4.0f);

		string source = "Not connected";
		if (header.layerIndex < box->fbohandlergroup.getSize() &&
			box->fbohandlergroup.getisPointerSet(header.layerIndex))
		{
			source = box->fbohandlergroup.getFboName(header.layerIndex);
		}
		while (source.size() > 1 &&
			jp_constants::p_font.stringWidth(source) >
			header.bounds.width * 0.42f)
			source.pop_back();
		ofSetColor(source == "Not connected" ?
			COL_TEXT_MUTED : COL_ACCENT_CYAN);
		jp_constants::p_font.drawString(source,
			header.bounds.getRight() -
				jp_constants::p_font.stringWidth(source) - 7.0f,
			centerY + 4.0f);
		ofSetColor(ofColor(COL_BORDER_MUTED, 100));
		ofDrawLine(header.bounds.x + 2.0f, header.bounds.getBottom(),
			header.bounds.getRight() - 2.0f, header.bounds.getBottom());
		jp_tooltip::draw(expanded ?
			"Collapse texture parameters" : "Expand texture parameters",
			header.bounds.x, header.bounds.y,
			header.bounds.width, header.bounds.height);
	}
	ofPopStyle();
}

bool JPboxgroup::handleAdvancedMappingParameterHeaderClick(JPbox *box)
{
	if (!inspectorBodyContains(ofGetMouseX(), ofGetMouseY())) return false;
	JPbox_shader *shaderBox = dynamic_cast<JPbox_shader *>(box);
	if (shaderBox == nullptr || !shaderBox->isAdvancedMappingShader())
		return false;
	JPbox_shader::AdvancedMappingState *state =
		shaderBox->getAdvancedMappingState();
	if (state == nullptr) return false;
	for (const InspectorParameterGroupHeader &header :
		advancedMappingParameterHeaders)
	{
		if (header.bounds.inside(ofGetMouseX(), ofGetMouseY()))
		{
			auto &layer = state->layers[header.layerIndex];
			layer.inspectorExpanded = !layer.inspectorExpanded;
			setControllers();
			return true;
		}
	}
	return false;
}

void JPboxgroup::drawAdvancedMappingOverlay(float x, float y,
	float width, float height, bool includeHandles, bool interactive)
{
	JPbox_shader *box = getAdvancedMappingEditBox();
	if (box == nullptr) return;
	JPbox_shader::AdvancedMappingState *state =
		box->getAdvancedMappingState();
	if (state == nullptr) return;
	const int layerIndex = ofClamp(state->selectedLayer, 0,
		JPbox_shader::ADVANCED_MAPPING_LAYER_COUNT - 1);
	const auto &layer = state->layers[layerIndex];
	const float zoom = interactive ? advancedMappingViewZoom : 1.0f;
	const ofVec2f center = interactive ? advancedMappingViewCenter :
		ofVec2f(0.5f, 0.5f);
	const float canvasX = x + width * 0.5f - center.x * width * zoom;
	const float canvasY = y + height * 0.5f - center.y * height * zoom;
	const float canvasW = width * zoom;
	const float canvasH = height * zoom;
	auto screen = [&](const ofVec2f &point) {
		return ofVec2f(canvasX + point.x * canvasW,
			canvasY + point.y * canvasH);
	};

	ofPushStyle();
	ofNoFill();
	ofSetLineWidth(1.0f);
	if (mappingGridVisible)
	{
		for (int lineIndex = 1; lineIndex < 10; lineIndex++)
		{
			const float position = lineIndex / 10.0f;
			ofSetColor(lineIndex == 5 ?
				ofColor(COL_ACCENT_CYAN, 210) :
				ofColor(COL_TEXT_PRIMARY, 90));
			ofPolyline horizontal;
			ofPolyline vertical;
			for (int segment = 0; segment <= 32; segment++)
			{
				const float value = segment / 32.0f;
				const ofVec2f horizontalPoint = screen(projectAdvancedMappingPoint(
					layer, ofVec2f(value, position)));
				const ofVec2f verticalPoint = screen(projectAdvancedMappingPoint(
					layer, ofVec2f(position, value)));
				horizontal.addVertex(horizontalPoint.x, horizontalPoint.y, 0.0f);
				vertical.addVertex(verticalPoint.x, verticalPoint.y, 0.0f);
			}
			horizontal.draw();
			vertical.draw();
		}
	}

	if (mappingGuidesVisible || includeHandles)
	{
		ofSetColor(COL_ACCENT_CYAN);
		ofSetLineWidth(2.0f);
		drawCubic(layer.corners[0], layer.edgeHandles[0],
			layer.edgeHandles[1], layer.corners[1], canvasX, canvasY, canvasW, canvasH);
		drawCubic(layer.corners[1], layer.edgeHandles[2],
			layer.edgeHandles[3], layer.corners[2], canvasX, canvasY, canvasW, canvasH);
		drawCubic(layer.corners[3], layer.edgeHandles[4],
			layer.edgeHandles[5], layer.corners[2], canvasX, canvasY, canvasW, canvasH);
		drawCubic(layer.corners[0], layer.edgeHandles[6],
			layer.edgeHandles[7], layer.corners[3], canvasX, canvasY, canvasW, canvasH);

		for (int contourIndex = 0;
			contourIndex < static_cast<int>(layer.masks.size()); contourIndex++)
		{
			const auto &contour = layer.masks[contourIndex];
			if (contour.nodes.size() < 2) continue;
			const bool selected = std::find(
				advancedMappingSelectedMaskContours.begin(),
				advancedMappingSelectedMaskContours.end(), contourIndex) !=
				advancedMappingSelectedMaskContours.end();
			ofSetColor(selected ? COL_ACCENT_GOLD :
				ofColor(COL_ACCENT_GOLD, 135));
			ofSetLineWidth(selected ? 2.0f : 1.3f);
			const size_t edgeCount = contour.closed ?
				contour.nodes.size() : contour.nodes.size() - 1;
			for (size_t i = 0; i < edgeCount; i++)
			{
				const auto &from = contour.nodes[i];
				const auto &to = contour.nodes[(i + 1) % contour.nodes.size()];
				if (from.smooth || to.smooth)
					drawCubic(from.anchor, from.outHandle,
						to.inHandle, to.anchor,
						canvasX, canvasY, canvasW, canvasH);
				else
					ofDrawLine(screen(from.anchor), screen(to.anchor));
			}
		}

		if (includeHandles)
		{
			if (advancedMappingTool == ADVANCED_MAPPING_MOVE && interactive)
			{
				// Every point as a small dim dot: enough to read the shape's
				// structure, too small to read as a grab target, because this
				// tool only ever moves whole shapes.
				ofFill();
				ofSetColor(ofColor(COL_ACCENT_CYAN, 130));
				for (int corner = 0; corner < 4; corner++)
					ofDrawCircle(screen(layer.corners[corner]), 2.5f);
				ofSetColor(ofColor(COL_ACCENT_GOLD, 130));
				for (const auto &contour : layer.masks)
					for (const auto &node : contour.nodes)
						ofDrawCircle(screen(node.anchor), 2.5f);
				ofNoFill();

				const bool maskTarget = advancedMappingMoveTarget ==
					ADVANCED_MAPPING_TARGET_MASK;
				ofRectangle box;
				if (getAdvancedMappingMoveBox(layer, box))
				{
					ofSetColor(maskTarget ? COL_ACCENT_GOLD : COL_ACCENT_CYAN);
					ofSetLineWidth(3.0f);
					if (maskTarget)
					{
						for (int contourIndex : advancedMappingSelectedMaskContours)
						{
							if (contourIndex < 0 || contourIndex >=
								static_cast<int>(layer.masks.size())) continue;
							ofPolyline emphasis;
							const ofPolyline outline = buildMaskOutline(
								layer.masks[contourIndex]);
							for (const auto &vertex : outline)
							{
								const ofVec2f point = screen(
									ofVec2f(vertex.x, vertex.y));
								emphasis.addVertex(point.x, point.y, 0.0f);
							}
							if (layer.masks[contourIndex].closed) emphasis.close();
							emphasis.draw();
						}
					}
					else
					{
						ofPolyline emphasis;
						for (const auto &vertex : buildSurfaceOutline(layer))
						{
							const ofVec2f point = screen(
								ofVec2f(vertex.x, vertex.y));
							emphasis.addVertex(point.x, point.y, 0.0f);
						}
						emphasis.close();
						emphasis.draw();
					}

					ofVec2f handles[4];
					mappingBoxCorners(box, handles);
					for (int i = 0; i < 4; i++)
						handles[i] = screen(handles[i]);
					ofSetColor(ofColor(COL_TEXT_PRIMARY, 130));
					ofSetLineWidth(1.0f);
					drawDashedLine(handles[0], handles[1], 5.0f);
					drawDashedLine(handles[1], handles[3], 5.0f);
					drawDashedLine(handles[3], handles[2], 5.0f);
					drawDashedLine(handles[2], handles[0], 5.0f);
					ofFill();
					for (int i = 0; i < 4; i++)
					{
						ofSetColor(COL_BG_DARK);
						ofDrawRectangle(handles[i].x - 4.0f,
							handles[i].y - 4.0f, 8.0f, 8.0f);
						ofSetColor(maskTarget ?
							COL_ACCENT_GOLD : COL_ACCENT_CYAN);
						ofDrawRectangle(handles[i].x - 3.0f,
							handles[i].y - 3.0f, 6.0f, 6.0f);
					}
					if (maskTarget)
					{
						const ofVec2f topRight = screen(
							ofVec2f(box.getRight(), box.y));
						const ofVec2f rotation =
							getAdvancedMappingRotationHandle(box,
								getMappingPanelPreviewRect());
						ofSetColor(ofColor(COL_ACCENT_GOLD, 180));
						ofSetLineWidth(1.2f);
						ofDrawLine(topRight, rotation);
						ofNoFill();
						ofDrawCircle(rotation, 7.0f);
						ofFill();
						ofDrawTriangle(rotation.x + 4.0f, rotation.y - 4.0f,
							rotation.x + 8.0f, rotation.y - 5.0f,
							rotation.x + 6.0f, rotation.y - 1.0f);
						ofNoFill();
					}
					ofNoFill();
				}
			}
			else if (advancedMappingTool == ADVANCED_MAPPING_MESH ||
				advancedMappingTool == ADVANCED_MAPPING_MOVE)
			{
				// A render window showing the move tool falls in here rather
				// than into the pen branch below: corners read as calibration
				// marks, big mask anchors read as editor chrome.
				// The bezier handles only appear once the user asks for them,
				// so the default cage is four corners and nothing else.
				const bool bezier = advancedMappingBezierActive(layer);
				const int cornerHandles[4][2] = {
					{0, 6}, {1, 2}, {3, 5}, {4, 7}};
				for (int corner = 0; corner < 4; corner++)
				{
					for (int side = 0; bezier && side < 2; side++)
					{
						const int handle = cornerHandles[corner][side];
						ofSetColor(ofColor(COL_TEXT_SECONDARY, 145));
						ofDrawLine(screen(layer.corners[corner]),
							screen(layer.edgeHandles[handle]));
						ofSetColor(COL_TEXT_SECONDARY);
						ofFill();
						ofDrawCircle(screen(layer.edgeHandles[handle]), 4.0f);
						ofNoFill();
					}
					ofSetColor(COL_ACCENT_CYAN);
					ofFill();
					ofDrawCircle(screen(layer.corners[corner]), 8.0f);
					ofSetColor(COL_BG_DARK);
					ofDrawCircle(screen(layer.corners[corner]), 3.0f);
					ofNoFill();
				}

				// Mask points, smaller than in the pen tool so the surface stays
				// the focus, but visible because they can be selected here.
				for (int contourIndex = 0;
					contourIndex < static_cast<int>(layer.masks.size()); contourIndex++)
				{
					const auto &contour = layer.masks[contourIndex];
					for (int i = 0; i < static_cast<int>(contour.nodes.size()); i++)
					{
						const bool selected = contourIndex ==
							advancedMappingSelectedMaskContour &&
							i == advancedMappingSelectedMaskNode;
						ofSetColor(selected ? COL_ACCENT_GOLD :
							ofColor(COL_ACCENT_GOLD, 150));
						ofFill();
						ofDrawCircle(screen(contour.nodes[i].anchor),
							selected ? 5.0f : 3.5f);
						ofNoFill();
					}
				}
			}
			else
			{
				for (int contourIndex = 0;
					contourIndex < static_cast<int>(layer.masks.size()); contourIndex++)
				{
					const auto &contour = layer.masks[contourIndex];
					for (int i = 0; i < static_cast<int>(contour.nodes.size()); i++)
					{
						const auto &node = contour.nodes[i];
						const bool selected = contourIndex ==
							advancedMappingSelectedMaskContour &&
							i == advancedMappingSelectedMaskNode;
						if (node.smooth && selected)
						{
							ofSetColor(ofColor(COL_TEXT_SECONDARY, 150));
							ofDrawLine(screen(node.anchor), screen(node.inHandle));
							ofDrawLine(screen(node.anchor), screen(node.outHandle));
							ofSetColor(COL_TEXT_SECONDARY);
							ofFill();
							ofDrawCircle(screen(node.inHandle), 4.0f);
							ofDrawCircle(screen(node.outHandle), 4.0f);
						}
						ofSetColor(selected ? COL_ACCENT_GOLD :
							(contourIndex == advancedMappingSelectedMaskContour ?
							COL_TEXT_PRIMARY : ofColor(COL_TEXT_PRIMARY, 145)));
						ofFill();
						ofDrawCircle(screen(node.anchor), selected ? 7.0f : 5.0f);
					}
				}
			}
		}
	}
	if (interactive && advancedMappingDragKind ==
		ADVANCED_MAPPING_DRAG_MASK_MARQUEE)
	{
		ofRectangle marquee(advancedMappingMarqueeStart,
			advancedMappingMarqueeEnd.x - advancedMappingMarqueeStart.x,
			advancedMappingMarqueeEnd.y - advancedMappingMarqueeStart.y);
		marquee.standardize();
		ofFill();
		ofSetColor(ofColor(COL_ACCENT_GOLD, 30));
		ofDrawRectangle(marquee);
		ofNoFill();
		ofSetColor(COL_ACCENT_GOLD);
		ofSetLineWidth(1.2f);
		ofDrawRectangle(marquee);
	}
	ofPopStyle();
}

void JPboxgroup::drawAdvancedMappingPanel()
{
	JPbox_shader *box = getAdvancedMappingEditBox();
	if (box == nullptr) return;
	JPbox_shader::AdvancedMappingState *state =
		box->getAdvancedMappingState();
	if (state == nullptr) return;
	clampMappingPanelLayout();

	const ofRectangle preview = getMappingPanelPreviewRect();
	const ofRectangle guides = getMappingPanelActionBounds(MAPPING_PANEL_GUIDES);
	const ofRectangle grid = getMappingPanelActionBounds(MAPPING_PANEL_GRID);
	const ofRectangle render =
		getMappingPanelActionBounds(MAPPING_PANEL_RENDER_GUIDES);
	const ofRectangle close = getMappingPanelActionBounds(MAPPING_PANEL_CLOSE);
	const ofRectangle newShape = getAdvancedMappingHeaderActionBounds(true);
	const ofRectangle fitView = getAdvancedMappingHeaderActionBounds(false);
	const auto &selectedLayer = state->layers[state->selectedLayer];
	const bool canAddShape = advancedMappingSelectedMaskContour >= 0 &&
		advancedMappingSelectedMaskContour <
			static_cast<int>(selectedLayer.masks.size()) &&
		selectedLayer.masks[advancedMappingSelectedMaskContour].closed;
	const bool closeHovered = close.inside(ofGetMouseX(), ofGetMouseY());

	ofPushStyle();
	ofEnableAlphaBlending();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(0, 105);
	ofDrawRectRounded(mappingPanelX + 3.0f, mappingPanelY + 4.0f,
		mappingPanelW, mappingPanelH, 6.0f);
	ofSetColor(COL_BG_TAB, 248);
	ofDrawRectRounded(mappingPanelX, mappingPanelY,
		mappingPanelW, mappingPanelH, 6.0f);
	ofSetColor(COL_BG_PANEL, 250);
	ofDrawRectRounded(mappingPanelX, mappingPanelY,
		mappingPanelW, kAdvancedPanelTop, 6.0f);
	ofNoFill();
	ofSetColor(COL_ACCENT_CYAN);
	ofSetLineWidth(1.6f);
	ofDrawRectRounded(mappingPanelX, mappingPanelY,
		mappingPanelW, mappingPanelH, 6.0f);
	ofFill();

	string title = "MAP+ - " + box->name;
	const float titleMax = fitView.x - mappingPanelX - 20.0f;
	while (title.size() > 1 &&
		jp_constants::p_font.stringWidth(title) > titleMax)
		title.pop_back();
	ofSetColor(COL_ACCENT_CYAN);
	jp_constants::p_font.drawString(title,
		mappingPanelX + 10.0f, mappingPanelY + 21.0f);

	auto actionBackground = [&](const ofRectangle &bounds, bool active) {
		if (bounds.inside(ofGetMouseX(), ofGetMouseY()))
		{
			ofSetColor(active ? ofColor(COL_ACCENT_CYAN, 75) :
				ofColor(COL_BG_HOVER, 220));
			ofDrawRectRounded(bounds, 3.0f);
		}
	};
	actionBackground(guides, mappingGuidesVisible);
	actionBackground(grid, mappingGridVisible);
	actionBackground(render, mappingRenderGuidesVisible);
	actionBackground(fitView, advancedMappingViewZoom > 1.0001f);
	if (canAddShape) actionBackground(newShape, false);
	if (closeHovered)
	{
		ofSetColor(ofColor(COL_ACCENT_RED, 135));
		ofDrawRectRounded(close, 3.0f);
	}

	ofNoFill();
	ofSetLineWidth(1.3f);
	ofSetColor(mappingGuidesVisible ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	ofDrawRectangle(guides.x + 4.0f, guides.y + 4.0f,
		guides.width - 8.0f, guides.height - 8.0f);
	ofSetColor(mappingGridVisible ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	ofDrawRectangle(grid.x + 4.0f, grid.y + 4.0f,
		grid.width - 8.0f, grid.height - 8.0f);
	ofDrawLine(grid.getCenter().x, grid.y + 4.0f,
		grid.getCenter().x, grid.getBottom() - 4.0f);
	ofDrawLine(grid.x + 4.0f, grid.getCenter().y,
		grid.getRight() - 4.0f, grid.getCenter().y);
	ofSetColor(mappingRenderGuidesVisible ?
		COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	ofDrawRectangle(render.x + 3.0f, render.y + 5.0f,
		render.width - 6.0f, render.height - 8.0f);
	drawCloseIcon(close, closeHovered);
	ofFill();

	ofSetColor(canAddShape ? COL_TEXT_SECONDARY : ofColor(COL_TEXT_DIM, 95));
	ofSetLineWidth(1.4f);
	ofDrawLine(newShape.getCenter().x - 5.0f, newShape.getCenter().y,
		newShape.getCenter().x + 5.0f, newShape.getCenter().y);
	ofDrawLine(newShape.getCenter().x, newShape.getCenter().y - 5.0f,
		newShape.getCenter().x, newShape.getCenter().y + 5.0f);
	const string zoomLabel = ofToString(
		static_cast<int>(std::round(advancedMappingViewZoom * 100.0f))) + "%";
	ofSetColor(advancedMappingViewZoom > 1.0001f ?
		COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	jp_constants::p_font.drawString(zoomLabel,
		fitView.getCenter().x - jp_constants::p_font.stringWidth(zoomLabel) * 0.5f,
		fitView.getCenter().y + 4.0f);
	for (int actionIndex = 0;
		 actionIndex < ADVANCED_MAPPING_TOOLBAR_COUNT; actionIndex++)
	{
		const auto action = static_cast<AdvancedMappingToolbarAction>(actionIndex);
		const ofRectangle bounds = getAdvancedMappingToolbarBounds(action);
		bool active = actionIndex == state->selectedLayer;
		bool disabled = false;
		if (action == ADVANCED_MAPPING_TOOL_PEN)
			active = advancedMappingTool == ADVANCED_MAPPING_PEN ||
				(advancedMappingTool == ADVANCED_MAPPING_MOVE &&
				advancedMappingMoveTarget == ADVANCED_MAPPING_TARGET_MASK);
		else if (action == ADVANCED_MAPPING_TOOL_MESH)
			active = advancedMappingTool == ADVANCED_MAPPING_MESH ||
				(advancedMappingTool == ADVANCED_MAPPING_MOVE &&
				advancedMappingMoveTarget == ADVANCED_MAPPING_TARGET_SURFACE);
		else if (action == ADVANCED_MAPPING_TOOL_MOVE)
			active = advancedMappingTool == ADVANCED_MAPPING_MOVE;
		else if (action == ADVANCED_MAPPING_BEZIER)
			active = advancedMappingBezierActive(
				state->layers[state->selectedLayer]);
		else if (action == ADVANCED_MAPPING_SMOOTH)
		{
			const auto &layer = state->layers[state->selectedLayer];
			const bool validContour = advancedMappingSelectedMaskContour >= 0 &&
				advancedMappingSelectedMaskContour <
					static_cast<int>(layer.masks.size());
			const bool hasSelection = validContour &&
				advancedMappingSelectedMaskNode >= 0 &&
				advancedMappingSelectedMaskNode < static_cast<int>(
					layer.masks[advancedMappingSelectedMaskContour].nodes.size());
			active = hasSelection &&
				layer.masks[advancedMappingSelectedMaskContour]
					.nodes[advancedMappingSelectedMaskNode].smooth;
			// With no point selected there is nothing to smooth, so show that
			// rather than looking like a button that silently does nothing.
			disabled = !hasSelection;
		}
		else if (action == ADVANCED_MAPPING_FIT)
			active = state->layers[state->selectedLayer].fitMode !=
				JPbox_shader::ADVANCED_MAPPING_FIT_STRETCH;
		else if (action == ADVANCED_MAPPING_GUIDE)
			active = state->guideVisible && box->hasAdvancedMappingGuide();
		const bool hovered = !disabled &&
			bounds.inside(ofGetMouseX(), ofGetMouseY());
		const bool maskTargetIndicator = action == ADVANCED_MAPPING_TOOL_PEN &&
			advancedMappingTool == ADVANCED_MAPPING_MOVE &&
			advancedMappingMoveTarget == ADVANCED_MAPPING_TARGET_MASK;
		if (active || hovered)
		{
			ofSetColor(active ? (maskTargetIndicator ?
				ofColor(COL_ACCENT_GOLD, 70) :
				ofColor(COL_ACCENT_CYAN_DARK, 225)) :
				ofColor(COL_BG_HOVER, 220));
			ofDrawRectRounded(bounds, 3.0f);
		}
		ofSetColor(disabled ? ofColor(COL_TEXT_DIM, 110) :
			(active ? (maskTargetIndicator ? COL_ACCENT_GOLD :
				COL_TEXT_PRIMARY) :
			(hovered ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY)));
		ofSetLineWidth(1.3f);
		const ofVec2f center = bounds.getCenter();
		if (actionIndex <= ADVANCED_MAPPING_LAYER_4)
		{
			const string label = "T" + ofToString(actionIndex + 1);
			jp_constants::p_font.drawString(label,
				center.x - jp_constants::p_font.stringWidth(label) / 2.0f,
				center.y + 4.0f);
		}
		else if (action == ADVANCED_MAPPING_TOOL_PEN)
		{
			ofNoFill();
			ofDrawTriangle(center.x - 5.0f, center.y + 5.0f,
				center.x + 5.0f, center.y - 5.0f,
				center.x + 2.0f, center.y + 7.0f);
			ofFill();
		}
		else if (action == ADVANCED_MAPPING_TOOL_MESH)
		{
			ofNoFill();
			ofDrawRectangle(center.x - 6.0f, center.y - 6.0f, 12.0f, 12.0f);
			ofDrawLine(center.x, center.y - 6.0f, center.x, center.y + 6.0f);
			ofDrawLine(center.x - 6.0f, center.y, center.x + 6.0f, center.y);
			ofFill();
		}
		else if (action == ADVANCED_MAPPING_TOOL_MOVE)
		{
			// Four way arrow. Must have its own branch: the final else draws
			// the SVG arrow, so a missing case here is a silently wrong icon.
			ofSetLineWidth(1.4f);
			ofDrawLine(center.x - 7.0f, center.y, center.x + 7.0f, center.y);
			ofDrawLine(center.x, center.y - 7.0f, center.x, center.y + 7.0f);
			ofFill();
			ofDrawTriangle(center.x - 7.0f, center.y,
				center.x - 3.5f, center.y - 2.5f,
				center.x - 3.5f, center.y + 2.5f);
			ofDrawTriangle(center.x + 7.0f, center.y,
				center.x + 3.5f, center.y - 2.5f,
				center.x + 3.5f, center.y + 2.5f);
			ofDrawTriangle(center.x, center.y - 7.0f,
				center.x - 2.5f, center.y - 3.5f,
				center.x + 2.5f, center.y - 3.5f);
			ofDrawTriangle(center.x, center.y + 7.0f,
				center.x - 2.5f, center.y + 3.5f,
				center.x + 2.5f, center.y + 3.5f);
		}
		else if (action == ADVANCED_MAPPING_BEZIER)
		{
			// A curve with its two control handles - the thing this button
			// hands you. Deliberately unlike the smooth icon beside it.
			ofNoFill();
			ofBezier(center.x - 7.0f, center.y + 5.0f,
				center.x - 1.0f, center.y + 5.0f,
				center.x + 1.0f, center.y - 5.0f,
				center.x + 7.0f, center.y - 5.0f);
			ofDrawLine(center.x - 7.0f, center.y + 5.0f,
				center.x - 1.0f, center.y + 5.0f);
			ofDrawLine(center.x + 7.0f, center.y - 5.0f,
				center.x + 1.0f, center.y - 5.0f);
			ofFill();
			ofDrawCircle(center.x - 1.0f, center.y + 5.0f, 1.7f);
			ofDrawCircle(center.x + 1.0f, center.y - 5.0f, 1.7f);
		}
		else if (action == ADVANCED_MAPPING_SMOOTH)
		{
			// A hard corner and the arc that rounds it off, which is what
			// smoothing a mask point does.
			ofNoFill();
			ofSetLineWidth(1.0f);
			ofDrawLine(center.x - 6.0f, center.y + 6.0f,
				center.x - 6.0f, center.y - 6.0f);
			ofDrawLine(center.x - 6.0f, center.y - 6.0f,
				center.x + 6.0f, center.y - 6.0f);
			ofSetLineWidth(1.6f);
			ofBezier(center.x - 6.0f, center.y + 6.0f,
				center.x - 6.0f, center.y - 2.0f,
				center.x - 2.0f, center.y - 6.0f,
				center.x + 6.0f, center.y - 6.0f);
			ofFill();
		}
		else if (action == ADVANCED_MAPPING_FIT)
		{
			// The quad as an outline, the artwork as a bar inside it: flush for
			// stretch, letterboxed for contain, overflowing for cover.
			const int fit = state->layers[state->selectedLayer].fitMode;
			ofFill();
			if (fit == JPbox_shader::ADVANCED_MAPPING_FIT_CONTAIN)
				ofDrawRectangle(center.x - 7.0f, center.y - 3.0f, 14.0f, 6.0f);
			else if (fit == JPbox_shader::ADVANCED_MAPPING_FIT_COVER)
				ofDrawRectangle(center.x - 4.0f, center.y - 8.0f, 8.0f, 16.0f);
			else
				ofDrawRectangle(center.x - 7.0f, center.y - 6.0f, 14.0f, 12.0f);
			ofNoFill();
			ofSetLineWidth(1.3f);
			ofDrawRectangle(center.x - 7.0f, center.y - 6.0f, 14.0f, 12.0f);
			ofFill();
		}
		else if (action == ADVANCED_MAPPING_GUIDE)
		{
			ofNoFill();
			ofDrawRectangle(center.x - 7.0f, center.y - 6.0f, 14.0f, 12.0f);
			ofDrawTriangle(center.x - 5.0f, center.y + 4.0f,
				center.x - 1.0f, center.y,
				center.x + 5.0f, center.y + 4.0f);
			ofFill();
		}
		else
		{
			const bool importing = action == ADVANCED_MAPPING_SVG_IMPORT;
			ofDrawLine(center.x, center.y + (importing ? -6.0f : 6.0f),
				center.x, center.y + (importing ? 4.0f : -4.0f));
			const float tipY = center.y + (importing ? 4.0f : -4.0f);
			ofDrawLine(center.x, tipY, center.x - 4.0f,
				tipY + (importing ? -4.0f : 4.0f));
			ofDrawLine(center.x, tipY, center.x + 4.0f,
				tipY + (importing ? -4.0f : 4.0f));
		}
	}

	ofSetColor(COL_BG_DARK);
	ofDrawRectangle(preview);
	const ofRectangle canvas(
		preview.x + preview.width * 0.5f - advancedMappingViewCenter.x *
			preview.width * advancedMappingViewZoom,
		preview.y + preview.height * 0.5f - advancedMappingViewCenter.y *
			preview.height * advancedMappingViewZoom,
		preview.width * advancedMappingViewZoom,
		preview.height * advancedMappingViewZoom);
	// Geometry is allowed off canvas, so the overlay has to be clipped or it
	// paints over the toolbar and out past the panel. Scissor here at the call
	// site, never inside drawAdvancedMappingOverlay - the render window calls
	// that too, in another GL context, where clipping to this rect is wrong.
	// The y inversion below holds only because the panel draws to the default
	// framebuffer with no live transform (JPboxgroup::draw pops its matrix
	// before drawMappingPanel, and ofApp adds none).
	const ofRectangle clip(mappingPanelX + 1.0f,
		mappingPanelY + kAdvancedPanelTop,
		mappingPanelW - 2.0f,
		mappingPanelH - kAdvancedPanelTop - 1.0f);
	GLint previousScissor[4];
	GLint viewport[4];
	const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_SCISSOR_BOX, previousScissor);
	glGetIntegerv(GL_VIEWPORT, viewport);
	// Pixels per screen coordinate, read from the live viewport rather than
	// assumed to be 1, so this stays correct on a hidpi surface.
	const float pixelScale = ofGetHeight() > 0 ?
		viewport[3] / static_cast<float>(ofGetHeight()) : 1.0f;
	glEnable(GL_SCISSOR_TEST);
	glScissor(viewport[0] + static_cast<GLint>(std::floor(clip.x * pixelScale)),
		viewport[1] + static_cast<GLint>(std::floor(
			(ofGetHeight() - (clip.y + clip.height)) * pixelScale)),
		std::max(0, static_cast<GLint>(std::ceil(clip.width * pixelScale))),
		std::max(0, static_cast<GLint>(std::ceil(clip.height * pixelScale))));

	if (state->guideVisible && box->hasAdvancedMappingGuide())
	{
		ofSetColor(255);
		box->getAdvancedMappingGuide()->draw(canvas);
		ofSetColor(255, static_cast<int>(255.0f *
			(1.0f - state->guideOpacity * 0.55f)));
	}
	else
	{
		ofSetColor(255);
	}
	box->fbo.draw(canvas);

	drawAdvancedMappingOverlay(preview.x, preview.y,
		preview.width, preview.height, mappingGuidesVisible, true);

	glScissor(previousScissor[0], previousScissor[1],
		previousScissor[2], previousScissor[3]);
	if (!scissorWasEnabled) glDisable(GL_SCISSOR_TEST);

	ofSetColor(COL_ACCENT_CYAN, 200);
	ofDrawLine(mappingPanelX + mappingPanelW - 18.0f,
		mappingPanelY + mappingPanelH - 4.0f,
		mappingPanelX + mappingPanelW - 4.0f,
		mappingPanelY + mappingPanelH - 18.0f);

	jp_tooltip::draw("Toggle mapping borders and points",
		guides.x, guides.y, guides.width, guides.height);
	jp_tooltip::draw("Toggle curved calibration grid",
		grid.x, grid.y, grid.width, grid.height);
	jp_tooltip::draw(mappingRenderGuidesVisible ?
		"Hide mapping borders in render windows" :
		"Show mapping borders in render windows",
		render.x, render.y, render.width, render.height);
	jp_tooltip::draw("Close mapping editor",
		close.x, close.y, close.width, close.height);
	jp_tooltip::draw(canAddShape ?
		"Start another independent mask shape" :
		"Close the selected mask shape before adding another",
		newShape.x, newShape.y, newShape.width, newShape.height);
	jp_tooltip::draw("Fit mapping view to the preview",
		fitView.x, fitView.y, fitView.width, fitView.height);
	for (int layer = 0; layer < 4; layer++)
	{
		const ofRectangle bounds = getAdvancedMappingToolbarBounds(
			static_cast<AdvancedMappingToolbarAction>(layer));
		jp_tooltip::draw("Edit texture " + ofToString(layer + 1) + " mapping",
			bounds.x, bounds.y, bounds.width, bounds.height);
	}
	auto tooltip = [&](AdvancedMappingToolbarAction action,
		const string &text) {
		const ofRectangle bounds = getAdvancedMappingToolbarBounds(action);
		jp_tooltip::draw(text, bounds.x, bounds.y,
			bounds.width, bounds.height);
	};
	tooltip(ADVANCED_MAPPING_TOOL_MESH,
		advancedMappingTool == ADVANCED_MAPPING_MOVE ?
		"Select the surface as the MOVE target" :
		"Edit mapping surface corners");
	tooltip(ADVANCED_MAPPING_TOOL_PEN,
		advancedMappingTool == ADVANCED_MAPPING_MOVE ?
		"Select masks as the MOVE target" :
		"Draw masks; click a closed edge to insert a point");
	tooltip(ADVANCED_MAPPING_TOOL_MOVE,
		advancedMappingTool == ADVANCED_MAPPING_MOVE ?
		"Leave MOVE and return to the highlighted target editor" :
		"Move the highlighted surface or mask target");
	tooltip(ADVANCED_MAPPING_BEZIER,
		advancedMappingBezierActive(state->layers[state->selectedLayer]) ?
		"Hide bezier handles and straighten the surface edges" :
		"Show bezier handles to curve the surface edges");
	{
		const auto &layer = state->layers[state->selectedLayer];
		const bool validContour = advancedMappingSelectedMaskContour >= 0 &&
			advancedMappingSelectedMaskContour <
				static_cast<int>(layer.masks.size());
		const bool hasSelection = validContour &&
			advancedMappingSelectedMaskNode >= 0 &&
			advancedMappingSelectedMaskNode < static_cast<int>(
				layer.masks[advancedMappingSelectedMaskContour].nodes.size());
		tooltip(ADVANCED_MAPPING_SMOOTH, hasSelection ?
			"Toggle smooth selected mask point" :
			"Click a mask point first, then smooth it");
	}
	{
		const int fit = state->layers[state->selectedLayer].fitMode;
		tooltip(ADVANCED_MAPPING_FIT,
			fit == JPbox_shader::ADVANCED_MAPPING_FIT_CONTAIN ?
				"Source fit: contain - click for cover" :
			fit == JPbox_shader::ADVANCED_MAPPING_FIT_COVER ?
				"Source fit: cover - click for stretch" :
				"Source fit: stretch - click for contain");
	}
	tooltip(ADVANCED_MAPPING_GUIDE,
		box->hasAdvancedMappingGuide() ?
		"Toggle guide photo; right click to replace" : "Load guide photo");
	tooltip(ADVANCED_MAPPING_SVG_IMPORT, "Import mask and surface SVG");
	tooltip(ADVANCED_MAPPING_SVG_EXPORT, "Export mask and surface SVG");
	ofPopStyle();
}

void JPboxgroup::markAdvancedMappingChanged(JPbox_shader *box,
	int layerIndex, bool maskChanged)
{
	if (box == nullptr) return;
	if (maskChanged) box->markAdvancedMappingMaskDirty(layerIndex);
	markMappingParameterChanged();
	if (isCueDraftMode()) updateCueDraftGraph();
}

bool JPboxgroup::updateAdvancedMappingMousePressed(int mouseButton)
{
	JPbox_shader *box = getAdvancedMappingEditBox();
	if (box == nullptr || !mouseOverMappingPanel()) return false;
	auto *state = box->getAdvancedMappingState();
	if (state == nullptr) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());

	if (mouseOverMappingPanelCloseIcon() &&
		mouseButton == OF_MOUSE_BUTTON_LEFT)
	{
		endMappingEdit();
		return true;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		getMappingPanelActionBounds(MAPPING_PANEL_GUIDES).inside(mouse))
	{
		toggleMappingGuides();
		return true;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		getMappingPanelActionBounds(MAPPING_PANEL_GRID).inside(mouse))
	{
		toggleMappingGrid();
		return true;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		getMappingPanelActionBounds(MAPPING_PANEL_RENDER_GUIDES).inside(mouse))
	{
		toggleMappingRenderGuides();
		return true;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		getAdvancedMappingHeaderActionBounds(false).inside(mouse))
	{
		advancedMappingViewZoom = 1.0f;
		advancedMappingViewCenter.set(0.5f, 0.5f);
		return true;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		getAdvancedMappingHeaderActionBounds(true).inside(mouse))
	{
		auto &layer = state->layers[state->selectedLayer];
		if (advancedMappingSelectedMaskContour >= 0 &&
			advancedMappingSelectedMaskContour <
				static_cast<int>(layer.masks.size()) &&
			layer.masks[advancedMappingSelectedMaskContour].closed)
		{
			layer.masks.push_back(JPbox_shader::AdvancedMappingContour());
			advancedMappingSelectedMaskContour =
				static_cast<int>(layer.masks.size()) - 1;
			advancedMappingSelectedMaskContours.assign(1,
				advancedMappingSelectedMaskContour);
			advancedMappingSelectedMaskNode = -1;
			advancedMappingTool = ADVANCED_MAPPING_PEN;
			mappingGuidesVisible = true;
		}
		return true;
	}
	for (int actionIndex = 0;
		 actionIndex < ADVANCED_MAPPING_TOOLBAR_COUNT; actionIndex++)
	{
		const auto action = static_cast<AdvancedMappingToolbarAction>(actionIndex);
		if (!getAdvancedMappingToolbarBounds(action).inside(mouse)) continue;
		if (actionIndex <= ADVANCED_MAPPING_LAYER_4 &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			state->selectedLayer = actionIndex;
			advancedMappingSelectedMaskContour =
				state->layers[actionIndex].masks.empty() ? -1 : 0;
			advancedMappingSelectedMaskContours.clear();
			advancedMappingSelectedMaskNode = -1;
			// The new layer may have no mask at all, and a move target left
			// pointing at one would leave the tool with nothing to show.
			advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_SURFACE;
			return true;
		}
		if (action == ADVANCED_MAPPING_TOOL_MESH &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			if (advancedMappingTool == ADVANCED_MAPPING_MOVE)
				advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_SURFACE;
			else
				advancedMappingTool = ADVANCED_MAPPING_MESH;
			mappingGuidesVisible = true;
			return true;
		}
		if (action == ADVANCED_MAPPING_TOOL_PEN &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			if (advancedMappingTool == ADVANCED_MAPPING_MOVE)
			{
				advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_MASK;
				if (advancedMappingSelectedMaskContours.empty() &&
					advancedMappingSelectedMaskContour >= 0)
					advancedMappingSelectedMaskContours.assign(1,
						advancedMappingSelectedMaskContour);
			}
			else
				advancedMappingTool = ADVANCED_MAPPING_PEN;
			mappingGuidesVisible = true;
			return true;
		}
		if (action == ADVANCED_MAPPING_TOOL_MOVE &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			const AdvancedMappingTool previousTool = advancedMappingTool;
			if (previousTool == ADVANCED_MAPPING_MOVE)
			{
				advancedMappingTool = advancedMappingMoveTarget ==
					ADVANCED_MAPPING_TARGET_MASK ? ADVANCED_MAPPING_PEN :
					ADVANCED_MAPPING_MESH;
				mappingGuidesVisible = true;
				return true;
			}
			advancedMappingTool = ADVANCED_MAPPING_MOVE;
			if (previousTool == ADVANCED_MAPPING_PEN)
			{
				advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_MASK;
				if (advancedMappingSelectedMaskContour >= 0)
					advancedMappingSelectedMaskContours.assign(1,
						advancedMappingSelectedMaskContour);
			}
			else if (previousTool == ADVANCED_MAPPING_MESH)
				advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_SURFACE;
			// Without this the overlay draws nothing and there is no shape to
			// aim at, which makes the tool look broken.
			mappingGuidesVisible = true;
			return true;
		}
		if (action == ADVANCED_MAPPING_BEZIER &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			auto &layer = state->layers[state->selectedLayer];
			if (advancedMappingBezierActive(layer))
			{
				// Turning the handles off has to straighten the edges too,
				// otherwise a curve would stay in the render with no handle
				// left on screen to undo it.
				advancedMappingBezierEnabled = false;
				if (mappingLayerHasCurvedEdge(layer))
				{
					mappingStraightenLayerEdges(layer);
					markAdvancedMappingChanged(box, state->selectedLayer,
						false);
				}
			}
			else
			{
				// The handles live in the mesh tool, so go there - otherwise
				// the button would appear to do nothing from the pen.
				advancedMappingBezierEnabled = true;
				advancedMappingTool = ADVANCED_MAPPING_MESH;
				mappingGuidesVisible = true;
			}
			return true;
		}
		if (action == ADVANCED_MAPPING_SMOOTH &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			auto &layer = state->layers[state->selectedLayer];
			if (advancedMappingSelectedMaskContour < 0 ||
				advancedMappingSelectedMaskContour >=
					static_cast<int>(layer.masks.size())) return true;
			auto &contour = layer.masks[advancedMappingSelectedMaskContour];
			const int selected = advancedMappingSelectedMaskNode;
			if (selected >= 0 && selected < static_cast<int>(contour.nodes.size()))
			{
				auto &node = contour.nodes[selected];
				node.smooth = !node.smooth;
				if (node.smooth && contour.nodes.size() > 1)
				{
					const int previous = selected > 0 ? selected - 1 :
						(contour.closed ? static_cast<int>(contour.nodes.size()) - 1 : selected);
					const int next = selected + 1 < static_cast<int>(contour.nodes.size()) ?
						selected + 1 : (contour.closed ? 0 : selected);
					const ofVec2f tangent =
						(contour.nodes[next].anchor - contour.nodes[previous].anchor) * 0.18f;
					node.inHandle = clampMappingPoint(node.anchor - tangent);
					node.outHandle = clampMappingPoint(node.anchor + tangent);
				}
				else if (!node.smooth)
				{
					node.inHandle = node.anchor;
					node.outHandle = node.anchor;
				}
				markAdvancedMappingChanged(box, state->selectedLayer, true);
			}
			return true;
		}
		if (action == ADVANCED_MAPPING_FIT &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			// Cycles stretch -> contain -> cover. Stretch stays reachable
			// because it is what existing compositions were built on.
			auto &layer = state->layers[state->selectedLayer];
			layer.fitMode = (layer.fitMode + 1) %
				JPbox_shader::ADVANCED_MAPPING_FIT_COUNT;
			markAdvancedMappingChanged(box, state->selectedLayer, false);
			return true;
		}
		if (action == ADVANCED_MAPPING_GUIDE)
		{
			const bool replace = mouseButton == OF_MOUSE_BUTTON_RIGHT ||
				!box->hasAdvancedMappingGuide();
			if (replace)
			{
				jp_constants::systemDialog_open = true;
				ofFileDialogResult result = ofSystemLoadDialog("Select mapping guide photo");
				jp_constants::systemDialog_open = false;
				if (result.bSuccess)
					box->loadAdvancedMappingGuide(result.getPath());
			}
			else if (mouseButton == OF_MOUSE_BUTTON_LEFT)
			{
				state->guideVisible = !state->guideVisible;
			}
			return true;
		}
		if (action == ADVANCED_MAPPING_SVG_IMPORT &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			jp_constants::systemDialog_open = true;
			ofFileDialogResult result = ofSystemLoadDialog("Import mapping SVG");
			jp_constants::systemDialog_open = false;
			if (result.bSuccess)
			{
				string error;
				if (box->importAdvancedMappingSvg(state->selectedLayer,
					result.getPath(), error))
				{
					advancedMappingSelectedMaskContour =
						state->layers[state->selectedLayer].masks.empty() ? -1 : 0;
					advancedMappingSelectedMaskContours.clear();
					advancedMappingSelectedMaskNode = -1;
					markAdvancedMappingChanged(box, state->selectedLayer, true);
				}
				else
					ofLogWarning("mapping_advanced") << error;
			}
			return true;
		}
		if (action == ADVANCED_MAPPING_SVG_EXPORT &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			jp_constants::systemDialog_open = true;
			ofFileDialogResult result = ofSystemSaveDialog(
				box->name + "_texture" + ofToString(state->selectedLayer + 1) + ".svg",
				"Export mapping SVG");
			jp_constants::systemDialog_open = false;
			if (result.bSuccess)
			{
				string error;
				if (!box->exportAdvancedMappingSvg(state->selectedLayer,
					result.getPath(), error))
					ofLogWarning("mapping_advanced") << error;
			}
			return true;
		}
		return true;
	}

	if (mouseButton == OF_MOUSE_BUTTON_LEFT && mouseOverMappingPanelResizeHandle())
	{
		mappingPanelPointerCaptured = true;
		mappingPanelResizing = true;
		mappingPanelDragging = false;
		mappingPanelDragStartMouse = mouse;
		mappingPanelResizeStartSize.set(mappingPanelW, mappingPanelH);
		return true;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT && mouseOverMappingPanelHeader())
	{
		mappingPanelPointerCaptured = true;
		mappingPanelDragging = true;
		mappingPanelResizing = false;
		mappingPanelDragStartMouse = mouse;
		mappingPanelDragStartPos.set(mappingPanelX, mappingPanelY);
		return true;
	}

	const ofRectangle preview = getMappingPanelPreviewRect();
	if (preview.inside(mouse) && mouseButton == OF_MOUSE_BUTTON_MIDDLE)
	{
		mappingPanelPointerCaptured = true;
		advancedMappingViewPanning = true;
		advancedMappingRightPanPending = false;
		advancedMappingViewPanStartMouse = mouse;
		advancedMappingViewPanStartCenter = advancedMappingViewCenter;
		return true;
	}
	// The move tool reaches past the preview into the letterbox margin. A
	// shape can be parked off canvas and there is no undo, so the margin is
	// the only way back to something that has been dragged out of sight.
	const ofRectangle content(mappingPanelX + 1.0f,
		mappingPanelY + kAdvancedPanelTop,
		mappingPanelW - 2.0f,
		mappingPanelH - kAdvancedPanelTop - 1.0f);
	const bool inGrabArea = advancedMappingTool == ADVANCED_MAPPING_MOVE ?
		content.inside(mouse) : preview.inside(mouse);
	if (!inGrabArea) return true;
	auto &layer = state->layers[state->selectedLayer];
	auto screen = [&](const ofVec2f &point) {
		return ofVec2f(
			preview.x + (0.5f + (point.x - advancedMappingViewCenter.x) *
				advancedMappingViewZoom) * preview.width,
			preview.y + (0.5f + (point.y - advancedMappingViewCenter.y) *
				advancedMappingViewZoom) * preview.height);
	};
	const ofVec2f rawUv(
		advancedMappingViewCenter.x +
			((mouse.x - preview.x) / preview.width - 0.5f) /
			advancedMappingViewZoom,
		advancedMappingViewCenter.y +
			((mouse.y - preview.y) / preview.height - 0.5f) /
			advancedMappingViewZoom);
	const ofVec2f uv = clampMappingPoint(rawUv);
	if (preview.inside(mouse) && mouseButton == OF_MOUSE_BUTTON_RIGHT)
	{
		mappingPanelPointerCaptured = true;
		advancedMappingViewPanning = false;
		advancedMappingRightPanPending = true;
		advancedMappingViewPanStartMouse = mouse;
		advancedMappingViewPanStartCenter = advancedMappingViewCenter;
		advancedMappingPendingDeleteContour = -1;
		advancedMappingPendingDeleteNode = -1;
		if (advancedMappingTool == ADVANCED_MAPPING_PEN)
		{
			for (int contourIndex = static_cast<int>(layer.masks.size()) - 1;
				contourIndex >= 0 && advancedMappingPendingDeleteNode < 0;
				contourIndex--)
				for (int nodeIndex = static_cast<int>(
					layer.masks[contourIndex].nodes.size()) - 1;
					nodeIndex >= 0; nodeIndex--)
					if (screen(layer.masks[contourIndex].nodes[nodeIndex].anchor)
						.distance(mouse) <= 12.0f)
					{
						advancedMappingPendingDeleteContour = contourIndex;
						advancedMappingPendingDeleteNode = nodeIndex;
						break;
					}
		}
		return true;
	}

	if (advancedMappingTool == ADVANCED_MAPPING_MOVE)
	{
		if (mouseButton != OF_MOUSE_BUTTON_LEFT) return true;
		mappingPanelPointerCaptured = true;
		advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
		advancedMappingDragIndex = -1;
		advancedMappingDragSnapshot = layer;
		advancedMappingDragPreview = preview;
		advancedMappingDragStartUv = rawUv;
		advancedMappingDragLayer = state->selectedLayer;
		advancedMappingDragContours = advancedMappingSelectedMaskContours;

		ofRectangle box;
		if (getAdvancedMappingMoveBox(layer, box))
		{
			if (advancedMappingMoveTarget == ADVANCED_MAPPING_TARGET_MASK)
			{
				const ofVec2f rotation =
					getAdvancedMappingRotationHandle(box, preview);
				if (rotation.distance(mouse) <= 12.0f)
				{
					advancedMappingDragKind =
						ADVANCED_MAPPING_DRAG_ROTATE_SHAPES;
					advancedMappingRotationPivot = box.getCenter();
					const ofVec2f pivotScreen = screen(
						advancedMappingRotationPivot);
					advancedMappingRotationStartAngle = std::atan2(
						mouse.y - pivotScreen.y, mouse.x - pivotScreen.x);
					return true;
				}
			}
			ofVec2f handles[4];
			mappingBoxCorners(box, handles);
			for (int i = 0; i < 4; i++)
			{
				if (screen(handles[i]).distance(mouse) > 11.0f) continue;
				advancedMappingDragKind = ADVANCED_MAPPING_DRAG_SCALE_SHAPE;
				advancedMappingScaleAnchor = handles[3 - i];
				advancedMappingScaleHandle = handles[i];
				return true;
			}
		}

		if (advancedMappingMoveTarget == ADVANCED_MAPPING_TARGET_SURFACE)
		{
			if (outlineContains(buildSurfaceOutline(layer), rawUv))
				advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MOVE_SHAPE;
			return true;
		}

		int hitMask = -1;
		for (int contourIndex = static_cast<int>(layer.masks.size()) - 1;
			contourIndex >= 0 && hitMask < 0; contourIndex--)
		{
			const auto &contour = layer.masks[contourIndex];
			const ofPolyline outline = buildMaskOutline(contour);
			if (contour.closed && outlineContains(outline, rawUv))
				hitMask = contourIndex;
			for (size_t i = 0; hitMask < 0 && i < outline.size(); i++)
				if (screen(ofVec2f(outline[i].x, outline[i].y))
					.distance(mouse) <= 8.0f)
					hitMask = contourIndex;
			for (size_t i = 1; hitMask < 0 && i < outline.size(); i++)
				if (pointSegmentDistance(mouse,
					screen(ofVec2f(outline[i - 1].x, outline[i - 1].y)),
					screen(ofVec2f(outline[i].x, outline[i].y))) <= 8.0f)
					hitMask = contourIndex;
			if (hitMask < 0 && contour.closed && outline.size() > 2 &&
				pointSegmentDistance(mouse,
					screen(ofVec2f(outline[outline.size() - 1].x,
						outline[outline.size() - 1].y)),
					screen(ofVec2f(outline[0].x, outline[0].y))) <= 8.0f)
				hitMask = contourIndex;
		}
		const bool shift = ofGetKeyPressed(OF_KEY_SHIFT);
		if (hitMask >= 0)
		{
			auto selected = std::find(advancedMappingSelectedMaskContours.begin(),
				advancedMappingSelectedMaskContours.end(), hitMask);
			if (shift)
			{
				if (selected == advancedMappingSelectedMaskContours.end())
					advancedMappingSelectedMaskContours.push_back(hitMask);
				else
					advancedMappingSelectedMaskContours.erase(selected);
				advancedMappingDragContours = advancedMappingSelectedMaskContours;
				advancedMappingSelectedMaskContour =
					advancedMappingSelectedMaskContours.empty() ? -1 :
					advancedMappingSelectedMaskContours.back();
				advancedMappingSelectedMaskNode = -1;
				return true;
			}
			if (selected == advancedMappingSelectedMaskContours.end())
				advancedMappingSelectedMaskContours.assign(1, hitMask);
			advancedMappingSelectedMaskContour = hitMask;
			advancedMappingSelectedMaskNode = -1;
			advancedMappingDragContours = advancedMappingSelectedMaskContours;
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MOVE_SHAPE;
			return true;
		}

		advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_MARQUEE;
		advancedMappingMarqueeStart = mouse;
		advancedMappingMarqueeEnd = mouse;
		advancedMappingMarqueeAdditive = shift;
		return true;
	}

	if (advancedMappingTool == ADVANCED_MAPPING_PEN)
	{
		int hitContour = -1;
		int hitAnchor = -1;
		for (int contourIndex = static_cast<int>(layer.masks.size()) - 1;
			contourIndex >= 0 && hitAnchor < 0; contourIndex--)
			for (int i = static_cast<int>(layer.masks[contourIndex].nodes.size()) - 1;
				i >= 0; i--)
				if (screen(layer.masks[contourIndex].nodes[i].anchor)
					.distance(mouse) <= 12.0f)
				{
					hitContour = contourIndex;
					hitAnchor = i;
					break;
				}
		if (mouseButton != OF_MOUSE_BUTTON_LEFT) return true;
		mappingPanelPointerCaptured = true;
		advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
		advancedMappingDragIndex = -1;
		if (advancedMappingSelectedMaskContour >= 0 &&
			advancedMappingSelectedMaskContour < static_cast<int>(layer.masks.size()) &&
			advancedMappingSelectedMaskNode >= 0 &&
			advancedMappingSelectedMaskNode < static_cast<int>(layer.masks[
				advancedMappingSelectedMaskContour].nodes.size()) &&
			layer.masks[advancedMappingSelectedMaskContour]
				.nodes[advancedMappingSelectedMaskNode].smooth)
		{
			auto &selected = layer.masks[advancedMappingSelectedMaskContour]
				.nodes[advancedMappingSelectedMaskNode];
			if (screen(selected.inHandle).distance(mouse) <= 11.0f)
			{
				advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_IN;
				advancedMappingDragIndex = advancedMappingSelectedMaskNode;
				advancedMappingDragContour = advancedMappingSelectedMaskContour;
				return true;
			}
			if (screen(selected.outHandle).distance(mouse) <= 11.0f)
			{
				advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_OUT;
				advancedMappingDragIndex = advancedMappingSelectedMaskNode;
				advancedMappingDragContour = advancedMappingSelectedMaskContour;
				return true;
			}
		}
		if (hitAnchor >= 0)
		{
			auto &contour = layer.masks[hitContour];
			advancedMappingSelectedMaskContour = hitContour;
			if (hitAnchor == 0 && !contour.closed && contour.nodes.size() >= 3)
			{
				contour.closed = true;
				advancedMappingSelectedMaskNode = 0;
				markAdvancedMappingChanged(box, state->selectedLayer, true);
				return true;
			}
			advancedMappingSelectedMaskNode = hitAnchor;
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_ANCHOR;
			advancedMappingDragIndex = hitAnchor;
			advancedMappingDragContour = hitContour;
			return true;
		}

		// A click near any closed edge inserts a point without changing its
		// outline. Curves are split exactly with De Casteljau subdivision.
		int edgeContour = -1;
		int edgeIndex = -1;
		float edgeT = 0.0f;
		float edgeDistance = 9.0f;
		for (int contourIndex = 0;
			contourIndex < static_cast<int>(layer.masks.size()); contourIndex++)
		{
			auto &contour = layer.masks[contourIndex];
			if (!contour.closed || contour.nodes.size() < 3) continue;
			for (int i = 0; i < static_cast<int>(contour.nodes.size()); i++)
			{
				const auto &from = contour.nodes[i];
				const auto &to = contour.nodes[(i + 1) % contour.nodes.size()];
				float t = 0.0f;
				ofVec2f point;
				if (from.smooth || to.smooth)
				{
					t = closestCubicT(from.anchor, from.outHandle,
						to.inHandle, to.anchor, rawUv,
						preview.width * advancedMappingViewZoom,
						preview.height * advancedMappingViewZoom);
					point = cubicPoint(from.anchor, from.outHandle,
						to.inHandle, to.anchor, t);
				}
				else
				{
					const ofVec2f segment = to.anchor - from.anchor;
					const float lengthSquared = segment.dot(segment);
					t = lengthSquared > 0.000001f ? ofClamp(
						(rawUv - from.anchor).dot(segment) / lengthSquared,
						0.0f, 1.0f) : 0.0f;
					point = from.anchor + segment * t;
				}
				const float distance = screen(point).distance(mouse);
				if (distance < edgeDistance)
				{
					edgeDistance = distance;
					edgeContour = contourIndex;
					edgeIndex = i;
					edgeT = t;
				}
			}
		}
		if (edgeContour >= 0)
		{
			auto &contour = layer.masks[edgeContour];
			const int next = (edgeIndex + 1) % contour.nodes.size();
			JPbox_shader::AdvancedMappingNode inserted;
			if (contour.nodes[edgeIndex].smooth || contour.nodes[next].smooth)
				inserted = splitContourEdge(contour.nodes[edgeIndex],
					contour.nodes[next], edgeT);
			else
			{
				inserted.anchor = contour.nodes[edgeIndex].anchor +
					(contour.nodes[next].anchor - contour.nodes[edgeIndex].anchor) * edgeT;
				inserted.inHandle = inserted.anchor;
				inserted.outHandle = inserted.anchor;
			}
			const int insertAt = edgeIndex + 1;
			contour.nodes.insert(contour.nodes.begin() + insertAt, inserted);
			advancedMappingSelectedMaskContour = edgeContour;
			advancedMappingSelectedMaskNode = insertAt;
			markAdvancedMappingChanged(box, state->selectedLayer, true);
			return true;
		}

		if (layer.masks.empty())
		{
			layer.masks.push_back(JPbox_shader::AdvancedMappingContour());
			advancedMappingSelectedMaskContour = 0;
		}
		if (advancedMappingSelectedMaskContour >= 0 &&
			advancedMappingSelectedMaskContour < static_cast<int>(layer.masks.size()) &&
			!layer.masks[advancedMappingSelectedMaskContour].closed)
		{
			auto &contour = layer.masks[advancedMappingSelectedMaskContour];
			JPbox_shader::AdvancedMappingNode node;
			node.anchor = uv;
			node.inHandle = uv;
			node.outHandle = uv;
			contour.nodes.push_back(node);
			advancedMappingSelectedMaskNode =
				static_cast<int>(contour.nodes.size()) - 1;
			markAdvancedMappingChanged(box, state->selectedLayer, true);
		}
		return true;
	}

	if (mouseButton != OF_MOUSE_BUTTON_LEFT) return true;
	mappingPanelPointerCaptured = true;
	advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
	advancedMappingDragIndex = -1;
	for (int i = 0; i < 4; i++)
	{
		if (screen(layer.corners[i]).distance(mouse) <= 14.0f)
		{
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_SURFACE_CORNER;
			advancedMappingDragIndex = i;
			return true;
		}
	}
	// Only grabbable while the bezier handles are actually on screen; with them
	// hidden these would be invisible hotspots sitting on the edges.
	const bool bezier = advancedMappingBezierActive(layer);
	for (int i = 0; bezier && i < 8; i++)
	{
		if (screen(layer.edgeHandles[i]).distance(mouse) <= 11.0f)
		{
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_SURFACE_HANDLE;
			advancedMappingDragIndex = i;
			return true;
		}
	}
	// Mask points are selectable from the mesh tool too, after the surface has
	// had first refusal on the click. Selection used to happen only in the pen
	// tool, which left the smooth button doing nothing at all in mesh mode.
	for (int contourIndex = static_cast<int>(layer.masks.size()) - 1;
		contourIndex >= 0; contourIndex--)
	{
		for (int i = static_cast<int>(layer.masks[contourIndex].nodes.size()) - 1;
			i >= 0; i--)
		{
			if (screen(layer.masks[contourIndex].nodes[i].anchor)
				.distance(mouse) <= 11.0f)
			{
				advancedMappingSelectedMaskContour = contourIndex;
				advancedMappingSelectedMaskNode = i;
				advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_ANCHOR;
				advancedMappingDragIndex = i;
				advancedMappingDragContour = contourIndex;
				return true;
			}
		}
	}
	return true;
}

bool JPboxgroup::updateAdvancedMappingMouseDragged(int mouseButton)
{
	if (!mappingPanelPointerCaptured) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	if (advancedMappingRightPanPending)
	{
		if (mouseButton != OF_MOUSE_BUTTON_RIGHT) return false;
		if (mouse.distance(advancedMappingViewPanStartMouse) > 4.0f)
		{
			advancedMappingRightPanPending = false;
			advancedMappingViewPanning = true;
			advancedMappingPendingDeleteContour = -1;
			advancedMappingPendingDeleteNode = -1;
		}
		else return true;
	}
	if (advancedMappingViewPanning)
	{
		if (mouseButton != OF_MOUSE_BUTTON_RIGHT &&
			mouseButton != OF_MOUSE_BUTTON_MIDDLE) return false;
		const ofRectangle preview = getMappingPanelPreviewRect();
		if (preview.width <= 0.0f || preview.height <= 0.0f) return true;
		const ofVec2f delta = mouse - advancedMappingViewPanStartMouse;
		advancedMappingViewCenter = advancedMappingViewPanStartCenter -
			ofVec2f(delta.x / (preview.width * advancedMappingViewZoom),
				delta.y / (preview.height * advancedMappingViewZoom));
		clampAdvancedMappingView();
		return true;
	}
	if (mouseButton != OF_MOUSE_BUTTON_LEFT) return false;
	const ofVec2f delta = mouse - mappingPanelDragStartMouse;
	if (mappingPanelDragging)
	{
		mappingPanelX = mappingPanelDragStartPos.x + delta.x;
		mappingPanelY = mappingPanelDragStartPos.y + delta.y;
		clampMappingPanelLayout();
		return true;
	}
	if (mappingPanelResizing)
	{
		mappingPanelW = mappingPanelResizeStartSize.x + delta.x;
		mappingPanelH = mappingPanelResizeStartSize.y + delta.y;
		clampMappingPanelLayout();
		return true;
	}
	JPbox_shader *box = getAdvancedMappingEditBox();
	if (box == nullptr) return true;
	auto *state = box->getAdvancedMappingState();
	if (state == nullptr) return true;
	const ofRectangle preview = getMappingPanelPreviewRect();
	if (preview.width <= 0.0f || preview.height <= 0.0f) return true;
	const ofVec2f uv = clampMappingPoint(ofVec2f(
		advancedMappingViewCenter.x +
			((mouse.x - preview.x) / preview.width - 0.5f) /
			advancedMappingViewZoom,
		advancedMappingViewCenter.y +
			((mouse.y - preview.y) / preview.height - 0.5f) /
			advancedMappingViewZoom));
	auto &layer = state->layers[state->selectedLayer];
	if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_MARQUEE)
	{
		advancedMappingMarqueeEnd = mouse;
		return true;
	}
	if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MOVE_SHAPE ||
		advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SCALE_SHAPE ||
		advancedMappingDragKind == ADVANCED_MAPPING_DRAG_ROTATE_SHAPES)
	{
		// Everything is written as snapshot + transform, never accumulated, so
		// a dropped or replayed mouse frame still lands in the same place. The
		// mask FBO rebuild and the cue draft graph both run on every drag
		// frame, and either can hand us a freshly rebuilt layer.
		if (advancedMappingDragLayer != state->selectedLayer ||
			advancedMappingDragSnapshot.masks.size() != layer.masks.size())
			return true;
		const ofRectangle &start = advancedMappingDragPreview;
		if (start.width <= 0.0f || start.height <= 0.0f) return true;
		// Deliberately unclamped and measured against the press time preview
		// rect: shapes may leave the canvas, and clampMappingPanelLayout can
		// resize the panel underneath a drag.
		const ofVec2f dragUv(
			advancedMappingViewCenter.x +
				((mouse.x - start.x) / start.width - 0.5f) /
				advancedMappingViewZoom,
			advancedMappingViewCenter.y +
				((mouse.y - start.y) / start.height - 0.5f) /
				advancedMappingViewZoom);
		const auto &snapshot = advancedMappingDragSnapshot;
		const bool maskTarget = advancedMappingMoveTarget ==
			ADVANCED_MAPPING_TARGET_MASK;

		ofVec2f offset(0.0f, 0.0f);
		float scale = 1.0f;
		float rotation = 0.0f;
		if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SCALE_SHAPE)
		{
			// Project the cursor onto the box diagonal. One scalar for both
			// axes, so the aspect ratio is locked by construction; done in
			// screen pixels because that is what the cursor is aiming in.
			const auto screenAtStart = [&](const ofVec2f &point) {
				return ofVec2f(
					start.x + (0.5f + (point.x - advancedMappingViewCenter.x) *
						advancedMappingViewZoom) * start.width,
					start.y + (0.5f + (point.y - advancedMappingViewCenter.y) *
						advancedMappingViewZoom) * start.height);
			};
			const ofVec2f anchor = screenAtStart(advancedMappingScaleAnchor);
			const ofVec2f grabbed = screenAtStart(advancedMappingScaleHandle);
			const ofVec2f diagonal = grabbed - anchor;
			const float lengthSquared = diagonal.dot(diagonal);
			if (lengthSquared < 0.0001f) return true;
			scale = std::max((mouse - anchor).dot(diagonal) / lengthSquared,
				0.02f);
		}
		else if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MOVE_SHAPE)
		{
			offset = dragUv - advancedMappingDragStartUv;
		}
		else
		{
			const auto screenAtStart = [&](const ofVec2f &point) {
				return ofVec2f(
					start.x + (0.5f + (point.x - advancedMappingViewCenter.x) *
						advancedMappingViewZoom) * start.width,
					start.y + (0.5f + (point.y - advancedMappingViewCenter.y) *
						advancedMappingViewZoom) * start.height);
			};
			const ofVec2f pivotScreen = screenAtStart(advancedMappingRotationPivot);
			rotation = std::atan2(mouse.y - pivotScreen.y,
				mouse.x - pivotScreen.x) - advancedMappingRotationStartAngle;
			if (ofGetKeyPressed(OF_KEY_SHIFT))
			{
				const float increment = PI / 12.0f;
				rotation = std::round(rotation / increment) * increment;
			}
		}
		const ofVec2f pivot = advancedMappingScaleAnchor;
		auto place = [&](const ofVec2f &point) {
			if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SCALE_SHAPE)
				return pivot + (point - pivot) * scale;
			if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_ROTATE_SHAPES)
			{
				const ofVec2f relative(
					(point.x - advancedMappingRotationPivot.x) * start.width,
					(point.y - advancedMappingRotationPivot.y) * start.height);
				const float cosine = std::cos(rotation);
				const float sine = std::sin(rotation);
				return advancedMappingRotationPivot + ofVec2f(
					(relative.x * cosine - relative.y * sine) / start.width,
					(relative.x * sine + relative.y * cosine) / start.height);
			}
			return point + offset;
		};

		if (maskTarget)
		{
			for (int contourIndex : advancedMappingDragContours)
			{
				if (contourIndex < 0 || contourIndex >=
					static_cast<int>(layer.masks.size()) || contourIndex >=
					static_cast<int>(snapshot.masks.size()) ||
					layer.masks[contourIndex].nodes.size() !=
						snapshot.masks[contourIndex].nodes.size()) continue;
				auto &contour = layer.masks[contourIndex];
				const auto &source = snapshot.masks[contourIndex];
				for (size_t i = 0; i < contour.nodes.size(); i++)
				{
					contour.nodes[i].anchor = place(source.nodes[i].anchor);
					contour.nodes[i].inHandle = place(source.nodes[i].inHandle);
					contour.nodes[i].outHandle = place(source.nodes[i].outHandle);
				}
			}
		}
		else
		{
			for (int i = 0; i < 4; i++)
				layer.corners[i] = place(snapshot.corners[i]);
			for (int i = 0; i < 8; i++)
				layer.edgeHandles[i] = place(snapshot.edgeHandles[i]);
		}
		markAdvancedMappingChanged(box, state->selectedLayer, maskTarget);
		return true;
	}
	if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_ANCHOR &&
		advancedMappingDragContour >= 0 &&
		advancedMappingDragContour < static_cast<int>(layer.masks.size()) &&
		advancedMappingDragIndex >= 0 &&
		advancedMappingDragIndex < static_cast<int>(
			layer.masks[advancedMappingDragContour].nodes.size()))
	{
		auto &node = layer.masks[advancedMappingDragContour]
			.nodes[advancedMappingDragIndex];
		const ofVec2f movement = uv - node.anchor;
		node.anchor = uv;
		node.inHandle = clampMappingPoint(node.inHandle + movement);
		node.outHandle = clampMappingPoint(node.outHandle + movement);
		markAdvancedMappingChanged(box, state->selectedLayer, true);
	}
	else if ((advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_IN ||
		advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_OUT) &&
		advancedMappingDragContour >= 0 &&
		advancedMappingDragContour < static_cast<int>(layer.masks.size()) &&
		advancedMappingDragIndex >= 0 &&
		advancedMappingDragIndex < static_cast<int>(
			layer.masks[advancedMappingDragContour].nodes.size()))
	{
		auto &node = layer.masks[advancedMappingDragContour]
			.nodes[advancedMappingDragIndex];
		if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_IN)
		{
			node.inHandle = uv;
			if (node.smooth)
				node.outHandle = clampMappingPoint(node.anchor * 2.0f - uv);
		}
		else
		{
			node.outHandle = uv;
			if (node.smooth)
				node.inHandle = clampMappingPoint(node.anchor * 2.0f - uv);
		}
		markAdvancedMappingChanged(box, state->selectedLayer, true);
	}
	else if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SURFACE_CORNER &&
		advancedMappingDragIndex >= 0 && advancedMappingDragIndex < 4)
	{
		const int corner = advancedMappingDragIndex;
		// Move the whole edge with the corner rather than dragging only the
		// nearest handle along: that left the far handle of each edge behind and
		// bowed a straight edge as soon as a corner was touched. Capturing each
		// handle in its edge's frame keeps a straight edge exactly straight, and
		// carries an existing curve along with the corner. A curve steep enough
		// that the new handle would land outside the preview still flattens,
		// because the handles stay clamped to it just as when dragged by hand.
		// With the handles off there is no curve to carry, and rebuilding from
		// the edge frame would leave a hair of rounding behind on every drag
		// until it read as a curve. Snap those edges straight instead.
		if (!advancedMappingBezierActive(layer))
		{
			layer.corners[corner] = uv;
			mappingStraightenLayerEdges(layer);
			markAdvancedMappingChanged(box, state->selectedLayer, false);
			return true;
		}
		ofVec2f offsets[2][2];
		for (int side = 0; side < 2; side++)
		{
			const MappingEdge &edge =
				kMappingEdges[kCornerEdges[corner][side]];
			offsets[side][0] = mappingHandleToEdgeFrame(
				layer.corners[edge.cornerA], layer.corners[edge.cornerB],
				layer.edgeHandles[edge.handleA], 1.0f / 3.0f);
			offsets[side][1] = mappingHandleToEdgeFrame(
				layer.corners[edge.cornerA], layer.corners[edge.cornerB],
				layer.edgeHandles[edge.handleB], 2.0f / 3.0f);
		}
		layer.corners[corner] = uv;
		for (int side = 0; side < 2; side++)
		{
			const MappingEdge &edge =
				kMappingEdges[kCornerEdges[corner][side]];
			layer.edgeHandles[edge.handleA] = clampMappingPoint(
				mappingHandleFromEdgeFrame(layer.corners[edge.cornerA],
					layer.corners[edge.cornerB], offsets[side][0],
					1.0f / 3.0f));
			layer.edgeHandles[edge.handleB] = clampMappingPoint(
				mappingHandleFromEdgeFrame(layer.corners[edge.cornerA],
					layer.corners[edge.cornerB], offsets[side][1],
					2.0f / 3.0f));
		}
		markAdvancedMappingChanged(box, state->selectedLayer, false);
	}
	else if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SURFACE_HANDLE &&
		advancedMappingDragIndex >= 0 && advancedMappingDragIndex < 8)
	{
		layer.edgeHandles[advancedMappingDragIndex] = uv;
		markAdvancedMappingChanged(box, state->selectedLayer, false);
	}
	return true;
}

bool JPboxgroup::updateAdvancedMappingMouseReleased(int mouseButton)
{
	if (!mappingPanelPointerCaptured) return false;
	JPbox_shader *box = getAdvancedMappingEditBox();
	auto *state = box != nullptr ? box->getAdvancedMappingState() : nullptr;
	if (mouseButton == OF_MOUSE_BUTTON_RIGHT)
	{
		if (!advancedMappingViewPanning && !advancedMappingRightPanPending)
			return false;
		if (advancedMappingRightPanPending && state != nullptr &&
			advancedMappingPendingDeleteContour >= 0 &&
			advancedMappingPendingDeleteContour < static_cast<int>(
				state->layers[state->selectedLayer].masks.size()))
		{
			auto &layer = state->layers[state->selectedLayer];
			const int contourIndex = advancedMappingPendingDeleteContour;
			auto &contour = layer.masks[contourIndex];
			if (advancedMappingPendingDeleteNode >= 0 &&
				advancedMappingPendingDeleteNode <
					static_cast<int>(contour.nodes.size()))
			{
				contour.nodes.erase(contour.nodes.begin() +
					advancedMappingPendingDeleteNode);
				if (contour.nodes.size() < 3) contour.closed = false;
				const bool removedContour = contour.nodes.empty();
				if (removedContour)
				{
					layer.masks.erase(layer.masks.begin() + contourIndex);
					advancedMappingSelectedMaskContours.erase(std::remove(
						advancedMappingSelectedMaskContours.begin(),
						advancedMappingSelectedMaskContours.end(), contourIndex),
						advancedMappingSelectedMaskContours.end());
					for (int &selected : advancedMappingSelectedMaskContours)
						if (selected > contourIndex) selected--;
				}
				else
				{
					advancedMappingSelectedMaskContour = contourIndex;
					advancedMappingSelectedMaskContours.assign(1, contourIndex);
				}
				if (removedContour)
					advancedMappingSelectedMaskContour =
						advancedMappingSelectedMaskContours.empty() ? -1 :
						advancedMappingSelectedMaskContours.back();
				advancedMappingSelectedMaskNode = -1;
				markAdvancedMappingChanged(box, state->selectedLayer, true);
			}
		}
	}
	else if (mouseButton == OF_MOUSE_BUTTON_MIDDLE)
	{
		if (!advancedMappingViewPanning) return false;
	}
	else if (mouseButton == OF_MOUSE_BUTTON_LEFT)
	{
		if (advancedMappingViewPanning) return false;
		if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_MARQUEE &&
			state != nullptr)
		{
			ofRectangle marquee(advancedMappingMarqueeStart,
				advancedMappingMarqueeEnd.x - advancedMappingMarqueeStart.x,
				advancedMappingMarqueeEnd.y - advancedMappingMarqueeStart.y);
			marquee.standardize();
			if (!advancedMappingMarqueeAdditive)
				advancedMappingSelectedMaskContours.clear();
			if (marquee.width > 3.0f || marquee.height > 3.0f)
			{
				const ofRectangle preview = getMappingPanelPreviewRect();
				auto screen = [&](const ofVec2f &point) {
					return ofVec2f(preview.x + (0.5f +
						(point.x - advancedMappingViewCenter.x) *
						advancedMappingViewZoom) * preview.width,
						preview.y + (0.5f +
						(point.y - advancedMappingViewCenter.y) *
						advancedMappingViewZoom) * preview.height);
				};
				const auto &layer = state->layers[state->selectedLayer];
				for (int contourIndex = 0; contourIndex <
					static_cast<int>(layer.masks.size()); contourIndex++)
				{
					ofPolyline screenOutline;
					for (const auto &vertex : buildMaskOutline(
						layer.masks[contourIndex]))
					{
						const ofVec2f point = screen(
							ofVec2f(vertex.x, vertex.y));
						screenOutline.addVertex(point.x, point.y, 0.0f);
					}
					if (!polylineTouchesRectangle(screenOutline, marquee,
						layer.masks[contourIndex].closed)) continue;
					if (std::find(advancedMappingSelectedMaskContours.begin(),
						advancedMappingSelectedMaskContours.end(), contourIndex) ==
						advancedMappingSelectedMaskContours.end())
						advancedMappingSelectedMaskContours.push_back(contourIndex);
				}
			}
			advancedMappingSelectedMaskContour =
				advancedMappingSelectedMaskContours.empty() ? -1 :
				advancedMappingSelectedMaskContours.back();
			advancedMappingSelectedMaskNode = -1;
		}
	}
	else return false;
	advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
	advancedMappingDragIndex = -1;
	advancedMappingDragLayer = -1;
	advancedMappingDragContour = -1;
	advancedMappingDragContours.clear();
	advancedMappingDragSnapshot.masks.clear();
	advancedMappingViewPanning = false;
	advancedMappingRightPanPending = false;
	advancedMappingPendingDeleteContour = -1;
	advancedMappingPendingDeleteNode = -1;
	mappingPanelDragging = false;
	mappingPanelResizing = false;
	mappingPanelPointerCaptured = false;
	clampMappingPanelLayout();
	return true;
}

void JPboxgroup::clampAdvancedMappingView()
{
	advancedMappingViewZoom = ofClamp(advancedMappingViewZoom, 1.0f, 16.0f);
	const ofRectangle preview = getMappingPanelPreviewRect();
	if (preview.width <= 0.0f || preview.height <= 0.0f) return;
	const float keepX = std::min(32.0f, preview.width * 0.45f);
	const float keepY = std::min(32.0f, preview.height * 0.45f);
	advancedMappingViewCenter.x = ofClamp(advancedMappingViewCenter.x,
		(-0.5f + keepX / preview.width) / advancedMappingViewZoom,
		1.0f + (0.5f - keepX / preview.width) /
			advancedMappingViewZoom);
	advancedMappingViewCenter.y = ofClamp(advancedMappingViewCenter.y,
		(-0.5f + keepY / preview.height) / advancedMappingViewZoom,
		1.0f + (0.5f - keepY / preview.height) /
			advancedMappingViewZoom);
}

bool JPboxgroup::updateAdvancedMappingMouseScrolled(
	int x, int y, float scrollY)
{
	if (scrollY == 0.0f || getAdvancedMappingEditBox() == nullptr)
		return false;
	const ofRectangle preview = getMappingPanelPreviewRect();
	if (!preview.inside(x, y)) return false;
	const float oldZoom = advancedMappingViewZoom;
	const ofVec2f pointer((x - preview.x) / preview.width,
		(y - preview.y) / preview.height);
	const ofVec2f anchoredUv = advancedMappingViewCenter +
		ofVec2f((pointer.x - 0.5f) / oldZoom,
			(pointer.y - 0.5f) / oldZoom);
	advancedMappingViewZoom = ofClamp(oldZoom *
		(scrollY > 0.0f ? 1.15f : 1.0f / 1.15f), 1.0f, 16.0f);
	advancedMappingViewCenter = anchoredUv -
		ofVec2f((pointer.x - 0.5f) / advancedMappingViewZoom,
			(pointer.y - 0.5f) / advancedMappingViewZoom);
	clampAdvancedMappingView();
	return true;
}
