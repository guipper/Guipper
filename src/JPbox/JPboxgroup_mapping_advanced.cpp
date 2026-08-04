#include "JPboxgroup.h"

#include "../JPutils/jp_tooltip.h"

#include <algorithm>
#include <cmath>

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
		const JPbox_shader::AdvancedMappingLayer &layer)
	{
		ofPolyline line;
		if (layer.mask.size() < 3) return line;
		line.addVertex(layer.mask[0].anchor.x, layer.mask[0].anchor.y, 0.0f);
		for (size_t i = 0; i < layer.mask.size(); i++)
		{
			const auto &from = layer.mask[i];
			const auto &to = layer.mask[(i + 1) % layer.mask.size()];
			if (from.smooth || to.smooth)
				appendCubic(line, from.anchor, from.outHandle,
					to.inHandle, to.anchor);
			else
				line.addVertex(to.anchor.x, to.anchor.y, 0.0f);
		}
		return line;
	}

	bool outlineContains(const ofPolyline &line, const ofVec2f &point)
	{
		return line.size() >= 3 && line.inside(point.x, point.y);
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
	ofPolyline outline =
		advancedMappingMoveTarget == ADVANCED_MAPPING_TARGET_MASK ?
		buildMaskOutline(layer) : buildSurfaceOutline(layer);
	if (outline.size() < 3) return false;
	box = outline.getBoundingBox();
	return box.width > 0.0001f && box.height > 0.0001f;
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
	auto screen = [&](const ofVec2f &point) {
		return ofVec2f(x + point.x * width, y + point.y * height);
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
			layer.edgeHandles[1], layer.corners[1], x, y, width, height);
		drawCubic(layer.corners[1], layer.edgeHandles[2],
			layer.edgeHandles[3], layer.corners[2], x, y, width, height);
		drawCubic(layer.corners[3], layer.edgeHandles[4],
			layer.edgeHandles[5], layer.corners[2], x, y, width, height);
		drawCubic(layer.corners[0], layer.edgeHandles[6],
			layer.edgeHandles[7], layer.corners[3], x, y, width, height);

		if (layer.mask.size() >= 2)
		{
			ofSetColor(COL_ACCENT_GOLD);
			ofSetLineWidth(1.6f);
			const size_t edgeCount = layer.maskClosed ?
				layer.mask.size() : layer.mask.size() - 1;
			for (size_t i = 0; i < edgeCount; i++)
			{
				const auto &from = layer.mask[i];
				const auto &to = layer.mask[(i + 1) % layer.mask.size()];
				if (from.smooth || to.smooth)
					drawCubic(from.anchor, from.outHandle,
						to.inHandle, to.anchor, x, y, width, height);
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
				for (const auto &node : layer.mask)
					ofDrawCircle(screen(node.anchor), 2.5f);
				ofNoFill();

				const bool maskTarget = advancedMappingMoveTarget ==
					ADVANCED_MAPPING_TARGET_MASK;
				ofRectangle box;
				if (getAdvancedMappingMoveBox(layer, box))
				{
					ofPolyline outline = maskTarget ?
						buildMaskOutline(layer) : buildSurfaceOutline(layer);
					ofPolyline emphasis;
					for (const auto &vertex : outline)
						emphasis.addVertex(
							x + vertex.x * width, y + vertex.y * height, 0.0f);
					emphasis.close();
					ofSetColor(maskTarget ? COL_ACCENT_GOLD : COL_ACCENT_CYAN);
					ofSetLineWidth(3.0f);
					emphasis.draw();

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
				for (int i = 0; i < static_cast<int>(layer.mask.size()); i++)
				{
					ofSetColor(i == advancedMappingSelectedMaskNode ?
						COL_ACCENT_GOLD : ofColor(COL_ACCENT_GOLD, 150));
					ofFill();
					ofDrawCircle(screen(layer.mask[i].anchor),
						i == advancedMappingSelectedMaskNode ? 5.0f : 3.5f);
					ofNoFill();
				}
			}
			else
			{
				for (int i = 0; i < static_cast<int>(layer.mask.size()); i++)
				{
					const auto &node = layer.mask[i];
					if (node.smooth && i == advancedMappingSelectedMaskNode)
					{
						ofSetColor(ofColor(COL_TEXT_SECONDARY, 150));
						ofDrawLine(screen(node.anchor), screen(node.inHandle));
						ofDrawLine(screen(node.anchor), screen(node.outHandle));
						ofSetColor(COL_TEXT_SECONDARY);
						ofFill();
						ofDrawCircle(screen(node.inHandle), 4.0f);
						ofDrawCircle(screen(node.outHandle), 4.0f);
					}
					ofSetColor(i == advancedMappingSelectedMaskNode ?
						COL_ACCENT_GOLD : COL_TEXT_PRIMARY);
					ofFill();
					ofDrawCircle(screen(node.anchor),
						i == advancedMappingSelectedMaskNode ? 7.0f : 5.0f);
				}
			}
		}
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
	const float titleMax = guides.x - mappingPanelX - 20.0f;
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

	for (int actionIndex = 0;
		 actionIndex < ADVANCED_MAPPING_TOOLBAR_COUNT; actionIndex++)
	{
		const auto action = static_cast<AdvancedMappingToolbarAction>(actionIndex);
		const ofRectangle bounds = getAdvancedMappingToolbarBounds(action);
		bool active = actionIndex == state->selectedLayer;
		bool disabled = false;
		if (action == ADVANCED_MAPPING_TOOL_PEN)
			active = advancedMappingTool == ADVANCED_MAPPING_PEN;
		else if (action == ADVANCED_MAPPING_TOOL_MESH)
			active = advancedMappingTool == ADVANCED_MAPPING_MESH;
		else if (action == ADVANCED_MAPPING_TOOL_MOVE)
			active = advancedMappingTool == ADVANCED_MAPPING_MOVE;
		else if (action == ADVANCED_MAPPING_BEZIER)
			active = advancedMappingBezierActive(
				state->layers[state->selectedLayer]);
		else if (action == ADVANCED_MAPPING_SMOOTH)
		{
			const auto &mask = state->layers[state->selectedLayer].mask;
			const bool hasSelection = advancedMappingSelectedMaskNode >= 0 &&
				advancedMappingSelectedMaskNode < static_cast<int>(mask.size());
			active = hasSelection &&
				mask[advancedMappingSelectedMaskNode].smooth;
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
		if (active || hovered)
		{
			ofSetColor(active ? ofColor(COL_ACCENT_CYAN_DARK, 225) :
				ofColor(COL_BG_HOVER, 220));
			ofDrawRectRounded(bounds, 3.0f);
		}
		ofSetColor(disabled ? ofColor(COL_TEXT_DIM, 110) :
			(active ? COL_TEXT_PRIMARY :
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
	if (state->guideVisible && box->hasAdvancedMappingGuide())
	{
		ofSetColor(255);
		box->getAdvancedMappingGuide()->draw(preview);
		ofSetColor(255, static_cast<int>(255.0f *
			(1.0f - state->guideOpacity * 0.55f)));
	}
	else
	{
		ofSetColor(255);
	}
	box->fbo.draw(preview);

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
	tooltip(ADVANCED_MAPPING_TOOL_MESH, "Edit mapping surface corners");
	tooltip(ADVANCED_MAPPING_TOOL_PEN, "Draw and edit mask path");
	tooltip(ADVANCED_MAPPING_TOOL_MOVE,
		"Move or resize the whole surface or mask");
	tooltip(ADVANCED_MAPPING_BEZIER,
		advancedMappingBezierActive(state->layers[state->selectedLayer]) ?
		"Hide bezier handles and straighten the surface edges" :
		"Show bezier handles to curve the surface edges");
	{
		const auto &mask = state->layers[state->selectedLayer].mask;
		const bool hasSelection = advancedMappingSelectedMaskNode >= 0 &&
			advancedMappingSelectedMaskNode < static_cast<int>(mask.size());
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

	for (int actionIndex = 0;
		 actionIndex < ADVANCED_MAPPING_TOOLBAR_COUNT; actionIndex++)
	{
		const auto action = static_cast<AdvancedMappingToolbarAction>(actionIndex);
		if (!getAdvancedMappingToolbarBounds(action).inside(mouse)) continue;
		if (actionIndex <= ADVANCED_MAPPING_LAYER_4 &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			state->selectedLayer = actionIndex;
			advancedMappingSelectedMaskNode = -1;
			// The new layer may have no mask at all, and a move target left
			// pointing at one would leave the tool with nothing to show.
			advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_SURFACE;
			return true;
		}
		if (action == ADVANCED_MAPPING_TOOL_MESH &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			advancedMappingTool = ADVANCED_MAPPING_MESH;
			mappingGuidesVisible = true;
			return true;
		}
		if (action == ADVANCED_MAPPING_TOOL_PEN &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			advancedMappingTool = ADVANCED_MAPPING_PEN;
			mappingGuidesVisible = true;
			return true;
		}
		if (action == ADVANCED_MAPPING_TOOL_MOVE &&
			mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			advancedMappingTool = ADVANCED_MAPPING_MOVE;
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
			const int selected = advancedMappingSelectedMaskNode;
			if (selected >= 0 && selected < static_cast<int>(layer.mask.size()))
			{
				auto &node = layer.mask[selected];
				node.smooth = !node.smooth;
				if (node.smooth && layer.mask.size() > 1)
				{
					const int previous = selected > 0 ? selected - 1 :
						(layer.maskClosed ? static_cast<int>(layer.mask.size()) - 1 : selected);
					const int next = selected + 1 < static_cast<int>(layer.mask.size()) ?
						selected + 1 : (layer.maskClosed ? 0 : selected);
					const ofVec2f tangent =
						(layer.mask[next].anchor - layer.mask[previous].anchor) * 0.18f;
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
					markAdvancedMappingChanged(box, state->selectedLayer, true);
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
		return ofVec2f(preview.x + point.x * preview.width,
			preview.y + point.y * preview.height);
	};
	const ofVec2f rawUv((mouse.x - preview.x) / preview.width,
		(mouse.y - preview.y) / preview.height);
	const ofVec2f uv = clampMappingPoint(rawUv);

	if (advancedMappingTool == ADVANCED_MAPPING_MOVE)
	{
		if (mouseButton != OF_MOUSE_BUTTON_LEFT) return true;
		// Captured even on a miss: drag and release both bail without it, and
		// the click would leak downstream.
		mappingPanelPointerCaptured = true;
		advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
		advancedMappingDragIndex = -1;
		advancedMappingDragSnapshot = layer;
		advancedMappingDragPreview = preview;
		advancedMappingDragStartUv = rawUv;
		advancedMappingDragLayer = state->selectedLayer;

		// Scale handles first, so one sitting outside its shape is still
		// grabbable, then the mask, then the surface underneath it.
		ofRectangle box;
		if (getAdvancedMappingMoveBox(layer, box))
		{
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
		if (outlineContains(buildMaskOutline(layer), rawUv))
		{
			advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_MASK;
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MOVE_SHAPE;
		}
		else if (outlineContains(buildSurfaceOutline(layer), rawUv))
		{
			advancedMappingMoveTarget = ADVANCED_MAPPING_TARGET_SURFACE;
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MOVE_SHAPE;
		}
		return true;
	}

	if (advancedMappingTool == ADVANCED_MAPPING_PEN)
	{
		int hitAnchor = -1;
		for (int i = 0; i < static_cast<int>(layer.mask.size()); i++)
			if (screen(layer.mask[i].anchor).distance(mouse) <= 12.0f)
				hitAnchor = i;
		if (mouseButton == OF_MOUSE_BUTTON_RIGHT && hitAnchor >= 0)
		{
			layer.mask.erase(layer.mask.begin() + hitAnchor);
			if (layer.mask.size() < 3) layer.maskClosed = false;
			advancedMappingSelectedMaskNode = -1;
			markAdvancedMappingChanged(box, state->selectedLayer, true);
			return true;
		}
		if (mouseButton != OF_MOUSE_BUTTON_LEFT) return true;
		mappingPanelPointerCaptured = true;
		advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
		advancedMappingDragIndex = -1;
		if (advancedMappingSelectedMaskNode >= 0 &&
			advancedMappingSelectedMaskNode < static_cast<int>(layer.mask.size()) &&
			layer.mask[advancedMappingSelectedMaskNode].smooth)
		{
			auto &selected = layer.mask[advancedMappingSelectedMaskNode];
			if (screen(selected.inHandle).distance(mouse) <= 11.0f)
			{
				advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_IN;
				advancedMappingDragIndex = advancedMappingSelectedMaskNode;
				return true;
			}
			if (screen(selected.outHandle).distance(mouse) <= 11.0f)
			{
				advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_OUT;
				advancedMappingDragIndex = advancedMappingSelectedMaskNode;
				return true;
			}
		}
		if (hitAnchor >= 0)
		{
			if (hitAnchor == 0 && !layer.maskClosed && layer.mask.size() >= 3)
			{
				layer.maskClosed = true;
				advancedMappingSelectedMaskNode = 0;
				markAdvancedMappingChanged(box, state->selectedLayer, true);
				return true;
			}
			advancedMappingSelectedMaskNode = hitAnchor;
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_ANCHOR;
			advancedMappingDragIndex = hitAnchor;
			return true;
		}
		if (!layer.maskClosed)
		{
			JPbox_shader::AdvancedMappingNode node;
			node.anchor = uv;
			node.inHandle = uv;
			node.outHandle = uv;
			layer.mask.push_back(node);
			advancedMappingSelectedMaskNode =
				static_cast<int>(layer.mask.size()) - 1;
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
	for (int i = static_cast<int>(layer.mask.size()) - 1; i >= 0; i--)
	{
		if (screen(layer.mask[i].anchor).distance(mouse) <= 11.0f)
		{
			advancedMappingSelectedMaskNode = i;
			advancedMappingDragKind = ADVANCED_MAPPING_DRAG_MASK_ANCHOR;
			advancedMappingDragIndex = i;
			return true;
		}
	}
	return true;
}

bool JPboxgroup::updateAdvancedMappingMouseDragged(int mouseButton)
{
	if (mouseButton != OF_MOUSE_BUTTON_LEFT ||
		!mappingPanelPointerCaptured) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
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
		(mouse.x - preview.x) / preview.width,
		(mouse.y - preview.y) / preview.height));
	auto &layer = state->layers[state->selectedLayer];
	if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MOVE_SHAPE ||
		advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SCALE_SHAPE)
	{
		// Everything is written as snapshot + transform, never accumulated, so
		// a dropped or replayed mouse frame still lands in the same place. The
		// mask FBO rebuild and the cue draft graph both run on every drag
		// frame, and either can hand us a freshly rebuilt layer.
		if (advancedMappingDragLayer != state->selectedLayer ||
			advancedMappingDragSnapshot.mask.size() != layer.mask.size())
			return true;
		const ofRectangle &start = advancedMappingDragPreview;
		if (start.width <= 0.0f || start.height <= 0.0f) return true;
		// Deliberately unclamped and measured against the press time preview
		// rect: shapes may leave the canvas, and clampMappingPanelLayout can
		// resize the panel underneath a drag.
		const ofVec2f dragUv((mouse.x - start.x) / start.width,
			(mouse.y - start.y) / start.height);
		const auto &snapshot = advancedMappingDragSnapshot;
		const bool maskTarget = advancedMappingMoveTarget ==
			ADVANCED_MAPPING_TARGET_MASK;

		ofVec2f offset(0.0f, 0.0f);
		float scale = 1.0f;
		if (advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SCALE_SHAPE)
		{
			// Project the cursor onto the box diagonal. One scalar for both
			// axes, so the aspect ratio is locked by construction; done in
			// screen pixels because that is what the cursor is aiming in.
			const ofVec2f anchor(start.x +
					advancedMappingScaleAnchor.x * start.width,
				start.y + advancedMappingScaleAnchor.y * start.height);
			const ofVec2f grabbed(start.x +
					advancedMappingScaleHandle.x * start.width,
				start.y + advancedMappingScaleHandle.y * start.height);
			const ofVec2f diagonal = grabbed - anchor;
			const float lengthSquared = diagonal.dot(diagonal);
			if (lengthSquared < 0.0001f) return true;
			scale = std::max((mouse - anchor).dot(diagonal) / lengthSquared,
				0.02f);
		}
		else
		{
			offset = dragUv - advancedMappingDragStartUv;
		}
		const ofVec2f pivot = advancedMappingScaleAnchor;
		auto place = [&](const ofVec2f &point) {
			return advancedMappingDragKind == ADVANCED_MAPPING_DRAG_SCALE_SHAPE ?
				pivot + (point - pivot) * scale : point + offset;
		};

		if (maskTarget)
		{
			for (size_t i = 0; i < layer.mask.size(); i++)
			{
				layer.mask[i].anchor = place(snapshot.mask[i].anchor);
				layer.mask[i].inHandle = place(snapshot.mask[i].inHandle);
				layer.mask[i].outHandle = place(snapshot.mask[i].outHandle);
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
		advancedMappingDragIndex >= 0 &&
		advancedMappingDragIndex < static_cast<int>(layer.mask.size()))
	{
		auto &node = layer.mask[advancedMappingDragIndex];
		const ofVec2f movement = uv - node.anchor;
		node.anchor = uv;
		node.inHandle = clampMappingPoint(node.inHandle + movement);
		node.outHandle = clampMappingPoint(node.outHandle + movement);
		markAdvancedMappingChanged(box, state->selectedLayer, true);
	}
	else if ((advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_IN ||
		advancedMappingDragKind == ADVANCED_MAPPING_DRAG_MASK_OUT) &&
		advancedMappingDragIndex >= 0 &&
		advancedMappingDragIndex < static_cast<int>(layer.mask.size()))
	{
		auto &node = layer.mask[advancedMappingDragIndex];
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
	if (mouseButton != OF_MOUSE_BUTTON_LEFT ||
		!mappingPanelPointerCaptured) return false;
	advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
	advancedMappingDragIndex = -1;
	advancedMappingDragLayer = -1;
	advancedMappingDragSnapshot.mask.clear();
	mappingPanelDragging = false;
	mappingPanelResizing = false;
	mappingPanelPointerCaptured = false;
	clampMappingPanelLayout();
	return true;
}
