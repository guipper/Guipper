#include "jp_box_shader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace
{
	struct SvgPathToken
	{
		bool isCommand = false;
		char command = 0;
		float value = 0.0f;
	};

	std::vector<SvgPathToken> tokenizeSvgPath(const std::string &data)
	{
		std::vector<SvgPathToken> tokens;
		const char *cursor = data.c_str();
		while (*cursor != '\0')
		{
			if (std::isalpha(static_cast<unsigned char>(*cursor)))
			{
				SvgPathToken token;
				token.isCommand = true;
				token.command = *cursor++;
				tokens.push_back(token);
				continue;
			}
			if (std::isspace(static_cast<unsigned char>(*cursor)) ||
				*cursor == ',')
			{
				cursor++;
				continue;
			}
			char *end = nullptr;
			const float value = std::strtof(cursor, &end);
			if (end == cursor)
			{
				cursor++;
				continue;
			}
			SvgPathToken token;
			token.value = value;
			tokens.push_back(token);
			cursor = end;
		}
		return tokens;
	}

	bool readSvgAttribute(const std::string &tag,
		const std::string &name, std::string &value)
	{
		const size_t namePos = tag.find(name);
		if (namePos == std::string::npos)
			return false;
		const size_t equals = tag.find('=', namePos + name.size());
		if (equals == std::string::npos)
			return false;
		const size_t quote = tag.find_first_of("\"'", equals + 1);
		if (quote == std::string::npos)
			return false;
		const size_t end = tag.find(tag[quote], quote + 1);
		if (end == std::string::npos)
			return false;
		value = tag.substr(quote + 1, end - quote - 1);
		return true;
	}

	bool extractSvgPathData(const std::string &svg,
		const std::string &id, std::string &data)
	{
		size_t pathStart = std::string::npos;
		if (!id.empty())
		{
			const std::string doubleId = "id=\"" + id + "\"";
			const std::string singleId = "id='" + id + "'";
			size_t idPos = svg.find(doubleId);
			if (idPos == std::string::npos)
				idPos = svg.find(singleId);
			if (idPos != std::string::npos)
				pathStart = svg.rfind("<path", idPos);
		}
		if (pathStart == std::string::npos && id.empty())
			pathStart = svg.find("<path");
		if (pathStart == std::string::npos)
			return false;
		const size_t pathEnd = svg.find('>', pathStart);
		if (pathEnd == std::string::npos)
			return false;
		return readSvgAttribute(
			svg.substr(pathStart, pathEnd - pathStart + 1), "d", data);
	}

	ofRectangle parseSvgViewBox(const std::string &svg)
	{
		ofRectangle viewBox(0.0f, 0.0f, 1000.0f, 1000.0f);
		const size_t svgStart = svg.find("<svg");
		const size_t svgEnd = svg.find('>', svgStart);
		if (svgStart == std::string::npos || svgEnd == std::string::npos)
			return viewBox;
		std::string value;
		if (!readSvgAttribute(
			svg.substr(svgStart, svgEnd - svgStart + 1),
			"viewBox", value))
			return viewBox;
		std::replace(value.begin(), value.end(), ',', ' ');
		std::istringstream stream(value);
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		if (stream >> x >> y >> width >> height)
		{
			if (width > 0.0f && height > 0.0f)
				viewBox.set(x, y, width, height);
		}
		return viewBox;
	}

	bool parseSvgPath(const std::string &pathData,
		const ofRectangle &viewBox,
		std::vector<JPbox_shader::AdvancedMappingNode> &nodes,
		bool &closed, std::string &error)
	{
		const std::vector<SvgPathToken> tokens = tokenizeSvgPath(pathData);
		nodes.clear();
		closed = false;
		char command = 0;
		size_t index = 0;
		ofVec2f current(0.0f, 0.0f);
		ofVec2f subpathStart(0.0f, 0.0f);
		ofVec2f lastCubicControl(0.0f, 0.0f);
		ofVec2f lastQuadraticControl(0.0f, 0.0f);
		bool hasCubicControl = false;
		bool hasQuadraticControl = false;

		auto hasNumbers = [&](int count) {
			if (index + count > tokens.size())
				return false;
			for (int i = 0; i < count; i++)
				if (tokens[index + i].isCommand) return false;
			return true;
		};
		auto number = [&]() { return tokens[index++].value; };
		auto point = [&](bool relative) {
			const float x = number();
			const float y = number();
			ofVec2f value(x, y);
			return relative ? current + value : value;
		};
		auto addLine = [&](const ofVec2f &destination) {
			JPbox_shader::AdvancedMappingNode node;
			node.anchor = destination;
			node.inHandle = destination;
			node.outHandle = destination;
			nodes.push_back(node);
			current = destination;
			hasCubicControl = false;
			hasQuadraticControl = false;
		};
		auto addCubic = [&](const ofVec2f &control1,
			const ofVec2f &control2, const ofVec2f &destination) {
			if (nodes.empty()) addLine(current);
			nodes.back().outHandle = control1;
			nodes.back().smooth = true;
			JPbox_shader::AdvancedMappingNode node;
			node.anchor = destination;
			node.inHandle = control2;
			node.outHandle = destination;
			node.smooth = true;
			nodes.push_back(node);
			current = destination;
			lastCubicControl = control2;
			hasCubicControl = true;
			hasQuadraticControl = false;
		};

		while (index < tokens.size())
		{
			if (tokens[index].isCommand)
				command = tokens[index++].command;
			if (command == 0)
			{
				error = "SVG path has no drawing command";
				return false;
			}
			const bool relative = std::islower(
				static_cast<unsigned char>(command));
			const char upper = static_cast<char>(std::toupper(
				static_cast<unsigned char>(command)));
			if (upper == 'Z')
			{
				closed = true;
				current = subpathStart;
				command = 0;
				continue;
			}
			if (upper == 'M' && hasNumbers(2))
			{
				const ofVec2f destination = point(relative);
				if (!nodes.empty()) break;
				addLine(destination);
				subpathStart = destination;
				command = relative ? 'l' : 'L';
				continue;
			}
			if (upper == 'L' && hasNumbers(2))
			{
				addLine(point(relative));
				continue;
			}
			if (upper == 'H' && hasNumbers(1))
			{
				const float x = number();
				addLine(ofVec2f(relative ? current.x + x : x, current.y));
				continue;
			}
			if (upper == 'V' && hasNumbers(1))
			{
				const float y = number();
				addLine(ofVec2f(current.x,
					relative ? current.y + y : y));
				continue;
			}
			if (upper == 'C' && hasNumbers(6))
			{
				const ofVec2f control1 = point(relative);
				const ofVec2f control2 = point(relative);
				const ofVec2f destination = point(relative);
				addCubic(control1, control2, destination);
				continue;
			}
			if (upper == 'S' && hasNumbers(4))
			{
				const ofVec2f control1 = hasCubicControl ?
					current * 2.0f - lastCubicControl : current;
				const ofVec2f control2 = point(relative);
				const ofVec2f destination = point(relative);
				addCubic(control1, control2, destination);
				continue;
			}
			if (upper == 'Q' && hasNumbers(4))
			{
				const ofVec2f control = point(relative);
				const ofVec2f destination = point(relative);
				addCubic(current + (control - current) * (2.0f / 3.0f),
					destination + (control - destination) * (2.0f / 3.0f),
					destination);
				lastQuadraticControl = control;
				hasQuadraticControl = true;
				continue;
			}
			if (upper == 'T' && hasNumbers(2))
			{
				const ofVec2f control = hasQuadraticControl ?
					current * 2.0f - lastQuadraticControl : current;
				const ofVec2f destination = point(relative);
				addCubic(current + (control - current) * (2.0f / 3.0f),
					destination + (control - destination) * (2.0f / 3.0f),
					destination);
				lastQuadraticControl = control;
				hasQuadraticControl = true;
				continue;
			}
			if (upper == 'A' && hasNumbers(7))
			{
				for (int skip = 0; skip < 5; skip++) number();
				addLine(point(relative));
				continue;
			}
			error = "Unsupported or incomplete SVG path command";
			return false;
		}

		// A closed path's last segment returns to the start point, which leaves
		// a duplicate node carrying that segment's incoming handle. Fold it
		// back onto the first node, the way the path is actually drawn. Without
		// this our own exports come back one node bigger every round trip, and
		// a four corner surface parses as five nodes and is silently rejected.
		if (closed && nodes.size() >= 4)
		{
			const float tolerance = std::max(viewBox.width,
				viewBox.height) * 0.0001f;
			if (nodes.front().anchor.distance(nodes.back().anchor) <=
				tolerance)
			{
				nodes.front().inHandle = nodes.back().inHandle;
				nodes.front().smooth = nodes.front().smooth ||
					nodes.back().smooth;
				nodes.pop_back();
			}
		}

		if (nodes.size() < 3)
		{
			error = "SVG path needs at least three points";
			return false;
		}
		for (auto &node : nodes)
		{
			// Deliberately unclamped: geometry is allowed to sit outside the
			// canvas, and clamping here would silently collapse an off canvas
			// shape onto the edge on the way back in from our own export.
			auto normalize = [&](ofVec2f &value) {
				value.x = (value.x - viewBox.x) / viewBox.width;
				value.y = (value.y - viewBox.y) / viewBox.height;
			};
			normalize(node.anchor);
			normalize(node.inHandle);
			normalize(node.outHandle);
		}
		return true;
	}

	void appendXmlPoint(ofXml &parent, const std::string &name,
		const ofVec2f &point)
	{
		auto node = parent.appendChild(name);
		node.appendChild("x").set(point.x);
		node.appendChild("y").set(point.y);
	}

	ofVec2f readXmlPoint(const ofXml &parent,
		const std::string &name, const ofVec2f &fallback)
	{
		auto node = parent.getChild(name);
		if (!node || !node.getChild("x") || !node.getChild("y"))
			return fallback;
		return ofVec2f(node.getChild("x").getFloatValue(),
			node.getChild("y").getFloatValue());
	}

	void writeSvgPoint(std::ostringstream &stream, const ofVec2f &point)
	{
		stream << point.x * 1000.0f << ',' << point.y * 1000.0f;
	}
}

bool JPbox_shader::isAdvancedMappingShader() const
{
	return ofFilePath::getBaseName(dir) == "mapping_advanced";
}

// The inlet feeding a mapping layer, found by name. findIndexByName only ever
// returns an index it walked to, so the result needs no further bounds check.
int JPbox_shader::getAdvancedMappingInletIndex(int layerIndex) const
{
	if (layerIndex < 0 || layerIndex >= ADVANCED_MAPPING_LAYER_COUNT)
		return -1;
	return fbohandlergroup.findIndexByName(
		"textura" + ofToString(layerIndex + 1));
}

void JPbox_shader::initializeAdvancedMappingState()
{
	if (advancedMappingInitialized)
		return;
	for (AdvancedMappingLayer &layer : advancedMappingState.layers)
	{
		layer.corners = {
			ofVec2f(0.0f, 0.0f), ofVec2f(1.0f, 0.0f),
			ofVec2f(1.0f, 1.0f), ofVec2f(0.0f, 1.0f)};
		layer.edgeHandles = {
			ofVec2f(1.0f / 3.0f, 0.0f), ofVec2f(2.0f / 3.0f, 0.0f),
			ofVec2f(1.0f, 1.0f / 3.0f), ofVec2f(1.0f, 2.0f / 3.0f),
			ofVec2f(1.0f / 3.0f, 1.0f), ofVec2f(2.0f / 3.0f, 1.0f),
			ofVec2f(0.0f, 1.0f / 3.0f), ofVec2f(0.0f, 2.0f / 3.0f)};
	}
	advancedMappingMaskDirty.fill(true);
	advancedMappingInitialized = true;
}

JPbox_shader::AdvancedMappingState *JPbox_shader::getAdvancedMappingState()
{
	if (!isAdvancedMappingShader()) return nullptr;
	initializeAdvancedMappingState();
	return &advancedMappingState;
}

const JPbox_shader::AdvancedMappingState *
JPbox_shader::getAdvancedMappingState() const
{
	return isAdvancedMappingShader() && advancedMappingInitialized ?
		&advancedMappingState : nullptr;
}

void JPbox_shader::markAdvancedMappingMaskDirty(int layerIndex)
{
	if (layerIndex < 0)
		advancedMappingMaskDirty.fill(true);
	else if (layerIndex < ADVANCED_MAPPING_LAYER_COUNT)
		advancedMappingMaskDirty[layerIndex] = true;
}

void JPbox_shader::clearAdvancedMappingResources()
{
	for (ofFbo &mask : advancedMappingMasks)
	{
		if (mask.isAllocated()) mask.clear();
	}
	advancedMappingGuide.clear();
	advancedMappingMaskDirty.fill(true);
}

void JPbox_shader::rebuildAdvancedMappingMask(int layerIndex)
{
	if (!isAdvancedMappingShader() || layerIndex < 0 ||
		layerIndex >= ADVANCED_MAPPING_LAYER_COUNT)
		return;
	initializeAdvancedMappingState();
	const int width = std::max(1, static_cast<int>(fbo.getWidth()));
	const int height = std::max(1, static_cast<int>(fbo.getHeight()));
	if (!advancedMappingMasks[layerIndex].isAllocated() ||
		advancedMappingMasks[layerIndex].getWidth() != width ||
		advancedMappingMasks[layerIndex].getHeight() != height)
	{
		ofFbo::Settings settings;
		settings.width = width;
		settings.height = height;
		settings.internalformat = GL_R8;
		settings.textureTarget = GL_TEXTURE_2D;
		settings.useDepth = false;
		settings.useStencil = false;
		// The path edge is antialiased here, in the mask itself. The shader can
		// soften what lands in the FBO but cannot recover coverage that was
		// never rendered.
		settings.numSamples = 4;
		advancedMappingMasks[layerIndex].allocate(settings);
	}

	const AdvancedMappingLayer &layer =
		advancedMappingState.layers[layerIndex];
	advancedMappingMasks[layerIndex].begin();
	if (!layer.maskClosed || layer.mask.size() < 3)
	{
		ofClear(255, 255, 255, 255);
	}
	else
	{
		ofClear(0, 0, 0, 255);
		ofPath path;
		path.setFilled(true);
		path.setFillColor(ofColor::white);
		path.setPolyWindingMode(OF_POLY_WINDING_ODD);
		path.moveTo(layer.mask[0].anchor.x * width,
			layer.mask[0].anchor.y * height);
		for (size_t i = 0; i < layer.mask.size(); i++)
		{
			const AdvancedMappingNode &from = layer.mask[i];
			const AdvancedMappingNode &to =
				layer.mask[(i + 1) % layer.mask.size()];
			if (from.smooth || to.smooth)
			{
				path.bezierTo(from.outHandle.x * width,
					from.outHandle.y * height,
					to.inHandle.x * width,
					to.inHandle.y * height,
					to.anchor.x * width, to.anchor.y * height);
			}
			else
			{
				path.lineTo(to.anchor.x * width, to.anchor.y * height);
			}
		}
		path.close();
		path.draw();
	}
	advancedMappingMasks[layerIndex].end();
	advancedMappingMaskDirty[layerIndex] = false;
}

void JPbox_shader::updateAdvancedMappingUniforms()
{
	initializeAdvancedMappingState();
	for (int layerIndex = 0;
		 layerIndex < ADVANCED_MAPPING_LAYER_COUNT; layerIndex++)
	{
		const AdvancedMappingLayer &layer =
			advancedMappingState.layers[layerIndex];
		const std::string prefix = "layer" + ofToString(layerIndex + 1);
		// By name, not by position: reordering or adding a sampler in the
		// shader must not silently point a layer at the wrong input.
		const int inletIndex = getAdvancedMappingInletIndex(layerIndex);
		const bool connected = inletIndex >= 0 &&
			fbohandlergroup.getisPointerSet(inletIndex);
		shader.setUniform1i(prefix + "_connected", connected ? 1 : 0);
		shader.setUniform1i(prefix + "_fit", ofClamp(layer.fitMode, 0,
			ADVANCED_MAPPING_FIT_COUNT - 1));
		if (connected && advancedMappingMasks[layerIndex].isAllocated())
		{
			shader.setUniformTexture(prefix + "_mask",
				advancedMappingMasks[layerIndex].getTexture(), 5 + layerIndex);
		}

		float surface[24];
		for (int i = 0; i < 4; i++)
		{
			surface[i * 2] = layer.corners[i].x;
			surface[i * 2 + 1] = layer.corners[i].y;
		}
		for (int i = 0; i < 8; i++)
		{
			surface[(i + 4) * 2] = layer.edgeHandles[i].x;
			surface[(i + 4) * 2 + 1] = layer.edgeHandles[i].y;
		}
		shader.setUniform2fv(prefix + "_surface", surface, 12);

		// Seeded from a real point, not from the unit square: a layer moved off
		// canvas would otherwise report a box inflated to include 0..1, and the
		// shader seeds its Newton solve from this box.
		ofVec2f minimum = layer.corners[0];
		ofVec2f maximum = layer.corners[0];
		for (const ofVec2f &point : layer.corners)
		{
			minimum.x = std::min(minimum.x, point.x);
			minimum.y = std::min(minimum.y, point.y);
			maximum.x = std::max(maximum.x, point.x);
			maximum.y = std::max(maximum.y, point.y);
		}
		for (const ofVec2f &point : layer.edgeHandles)
		{
			minimum.x = std::min(minimum.x, point.x);
			minimum.y = std::min(minimum.y, point.y);
			maximum.x = std::max(maximum.x, point.x);
			maximum.y = std::max(maximum.y, point.y);
		}
		shader.setUniform4f(prefix + "_bounds",
			minimum.x, minimum.y, maximum.x, maximum.y);
	}
}

bool JPbox_shader::loadAdvancedMappingGuide(const std::string &path)
{
	if (!isAdvancedMappingShader()) return false;
	initializeAdvancedMappingState();
	advancedMappingGuide.clear();
	if (path.empty() || !advancedMappingGuide.load(path))
		return false;
	advancedMappingState.guideImagePath = path;
	advancedMappingState.guideVisible = true;
	return true;
}

bool JPbox_shader::hasAdvancedMappingGuide() const
{
	return advancedMappingGuide.isAllocated();
}

const ofImage *JPbox_shader::getAdvancedMappingGuide() const
{
	return hasAdvancedMappingGuide() ? &advancedMappingGuide : nullptr;
}

bool JPbox_shader::importAdvancedMappingSvg(int layerIndex,
	const std::string &path, std::string &error)
{
	if (!isAdvancedMappingShader() || layerIndex < 0 ||
		layerIndex >= ADVANCED_MAPPING_LAYER_COUNT)
	{
		error = "Invalid mapping layer";
		return false;
	}
	const std::string svg = ofBufferFromFile(path).getText();
	if (svg.empty())
	{
		error = "Could not read SVG";
		return false;
	}
	const ofRectangle viewBox = parseSvgViewBox(svg);
	std::string maskData;
	if (!extractSvgPathData(svg, "mask", maskData) &&
		!extractSvgPathData(svg, "", maskData))
	{
		error = "SVG has no path";
		return false;
	}
	std::vector<AdvancedMappingNode> mask;
	bool maskClosed = false;
	if (!parseSvgPath(maskData, viewBox, mask, maskClosed, error))
		return false;

	initializeAdvancedMappingState();
	AdvancedMappingLayer &layer = advancedMappingState.layers[layerIndex];
	layer.mask = mask;
	layer.maskClosed = maskClosed || mask.size() >= 3;

	std::string surfaceData;
	std::vector<AdvancedMappingNode> surface;
	bool surfaceClosed = false;
	std::string surfaceError;
	if (extractSvgPathData(svg, "surface", surfaceData) &&
		parseSvgPath(surfaceData, viewBox, surface,
			surfaceClosed, surfaceError) && surface.size() == 4)
	{
		for (int i = 0; i < 4; i++) layer.corners[i] = surface[i].anchor;
		layer.edgeHandles[0] = surface[0].outHandle;
		layer.edgeHandles[1] = surface[1].inHandle;
		layer.edgeHandles[2] = surface[1].outHandle;
		layer.edgeHandles[3] = surface[2].inHandle;
		layer.edgeHandles[4] = surface[3].inHandle;
		layer.edgeHandles[5] = surface[2].outHandle;
		layer.edgeHandles[6] = surface[0].inHandle;
		layer.edgeHandles[7] = surface[3].outHandle;
	}
	markAdvancedMappingMaskDirty(layerIndex);
	return true;
}

bool JPbox_shader::exportAdvancedMappingSvg(int layerIndex,
	const std::string &path, std::string &error) const
{
	if (!isAdvancedMappingShader() || !advancedMappingInitialized ||
		layerIndex < 0 || layerIndex >= ADVANCED_MAPPING_LAYER_COUNT)
	{
		error = "Invalid mapping layer";
		return false;
	}
	const AdvancedMappingLayer &layer = advancedMappingState.layers[layerIndex];
	std::ostringstream svg;
	svg << std::fixed << std::setprecision(3);
	svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
		<< "viewBox=\"0 0 1000 1000\">\n";
	svg << "  <path id=\"surface\" fill=\"none\" stroke=\"#00b8c8\" d=\"M ";
	writeSvgPoint(svg, layer.corners[0]);
	svg << " C "; writeSvgPoint(svg, layer.edgeHandles[0]);
	svg << ' '; writeSvgPoint(svg, layer.edgeHandles[1]);
	svg << ' '; writeSvgPoint(svg, layer.corners[1]);
	svg << " C "; writeSvgPoint(svg, layer.edgeHandles[2]);
	svg << ' '; writeSvgPoint(svg, layer.edgeHandles[3]);
	svg << ' '; writeSvgPoint(svg, layer.corners[2]);
	svg << " C "; writeSvgPoint(svg, layer.edgeHandles[5]);
	svg << ' '; writeSvgPoint(svg, layer.edgeHandles[4]);
	svg << ' '; writeSvgPoint(svg, layer.corners[3]);
	svg << " C "; writeSvgPoint(svg, layer.edgeHandles[7]);
	svg << ' '; writeSvgPoint(svg, layer.edgeHandles[6]);
	svg << ' '; writeSvgPoint(svg, layer.corners[0]);
	svg << " Z\"/>\n";
	svg << "  <path id=\"mask\" fill=\"#ffffff\" stroke=\"#ffbd4a\" d=\"";
	if (layer.mask.size() >= 3)
	{
		svg << "M "; writeSvgPoint(svg, layer.mask[0].anchor);
		for (size_t i = 0; i < layer.mask.size(); i++)
		{
			const AdvancedMappingNode &from = layer.mask[i];
			const AdvancedMappingNode &to =
				layer.mask[(i + 1) % layer.mask.size()];
			if (from.smooth || to.smooth)
			{
				svg << " C "; writeSvgPoint(svg, from.outHandle);
				svg << ' '; writeSvgPoint(svg, to.inHandle);
				svg << ' '; writeSvgPoint(svg, to.anchor);
			}
			else
			{
				svg << " L "; writeSvgPoint(svg, to.anchor);
			}
		}
		svg << " Z";
	}
	else
	{
		svg << "M 0,0 L 1000,0 L 1000,1000 L 0,1000 Z";
	}
	svg << "\"/>\n</svg>\n";
	const std::string output = svg.str();
	const ofBuffer outputBuffer(output.c_str(), output.size());
	if (!ofBufferToFile(path, outputBuffer))
	{
		error = "Could not write SVG";
		return false;
	}
	return true;
}

void JPbox_shader::saveCustomState(ofXml &boxNode) const
{
	if (!isAdvancedMappingShader() || !advancedMappingInitialized)
		return;
	auto root = boxNode.appendChild("advancedMapping");
	root.appendChild("selectedLayer").set(advancedMappingState.selectedLayer);
	root.appendChild("guideImagePath").set(advancedMappingState.guideImagePath);
	root.appendChild("guideVisible").set(advancedMappingState.guideVisible);
	root.appendChild("guideOpacity").set(advancedMappingState.guideOpacity);
	for (int layerIndex = 0;
		 layerIndex < ADVANCED_MAPPING_LAYER_COUNT; layerIndex++)
	{
		const AdvancedMappingLayer &layer =
			advancedMappingState.layers[layerIndex];
		auto layerNode = root.appendChild("layer");
		layerNode.appendChild("index").set(layerIndex);
		layerNode.appendChild("expanded").set(layer.inspectorExpanded);
		layerNode.appendChild("fit").set(layer.fitMode);
		auto surface = layerNode.appendChild("surface");
		for (int i = 0; i < 4; i++)
			appendXmlPoint(surface, "corner" + ofToString(i), layer.corners[i]);
		for (int i = 0; i < 8; i++)
			appendXmlPoint(surface, "handle" + ofToString(i), layer.edgeHandles[i]);
		auto mask = layerNode.appendChild("mask");
		mask.appendChild("closed").set(layer.maskClosed);
		for (const AdvancedMappingNode &point : layer.mask)
		{
			auto pointNode = mask.appendChild("point");
			appendXmlPoint(pointNode, "anchor", point.anchor);
			appendXmlPoint(pointNode, "in", point.inHandle);
			appendXmlPoint(pointNode, "out", point.outHandle);
			pointNode.appendChild("smooth").set(point.smooth);
		}
	}
}

void JPbox_shader::loadCustomState(const ofXml &boxNode)
{
	if (!isAdvancedMappingShader()) return;
	initializeAdvancedMappingState();
	auto root = boxNode.getChild("advancedMapping");
	if (!root) return;
	if (root.getChild("selectedLayer"))
		advancedMappingState.selectedLayer = ofClamp(
			root.getChild("selectedLayer").getIntValue(), 0,
			ADVANCED_MAPPING_LAYER_COUNT - 1);
	if (root.getChild("guideImagePath"))
		advancedMappingState.guideImagePath =
			root.getChild("guideImagePath").getValue();
	if (root.getChild("guideVisible"))
		advancedMappingState.guideVisible =
			root.getChild("guideVisible").getBoolValue();
	if (root.getChild("guideOpacity"))
		advancedMappingState.guideOpacity = ofClamp(
			root.getChild("guideOpacity").getFloatValue(), 0.0f, 1.0f);

	for (auto &layerNode : root.getChildren("layer"))
	{
		const int layerIndex = layerNode.getChild("index") ?
			layerNode.getChild("index").getIntValue() : -1;
		if (layerIndex < 0 || layerIndex >= ADVANCED_MAPPING_LAYER_COUNT)
			continue;
		AdvancedMappingLayer &layer = advancedMappingState.layers[layerIndex];
		if (layerNode.getChild("expanded"))
			layer.inspectorExpanded =
				layerNode.getChild("expanded").getBoolValue();
		// Absent in savefiles written before the fit modes existed, which is
		// exactly when stretch is the right answer.
		layer.fitMode = layerNode.getChild("fit") ?
			ofClamp(layerNode.getChild("fit").getIntValue(), 0,
				ADVANCED_MAPPING_FIT_COUNT - 1) :
			ADVANCED_MAPPING_FIT_STRETCH;
		auto surface = layerNode.getChild("surface");
		if (surface)
		{
			for (int i = 0; i < 4; i++)
				layer.corners[i] = readXmlPoint(surface,
					"corner" + ofToString(i), layer.corners[i]);
			for (int i = 0; i < 8; i++)
				layer.edgeHandles[i] = readXmlPoint(surface,
					"handle" + ofToString(i), layer.edgeHandles[i]);
		}
		auto mask = layerNode.getChild("mask");
		if (mask)
		{
			layer.mask.clear();
			layer.maskClosed = mask.getChild("closed") &&
				mask.getChild("closed").getBoolValue();
			for (auto &pointNode : mask.getChildren("point"))
			{
				AdvancedMappingNode point;
				point.anchor = readXmlPoint(pointNode, "anchor", ofVec2f());
				point.inHandle = readXmlPoint(pointNode, "in", point.anchor);
				point.outHandle = readXmlPoint(pointNode, "out", point.anchor);
				point.smooth = pointNode.getChild("smooth") &&
					pointNode.getChild("smooth").getBoolValue();
				layer.mask.push_back(point);
			}
		}
	}
	markAdvancedMappingMaskDirty();
	if (!advancedMappingState.guideImagePath.empty())
	{
		const bool visible = advancedMappingState.guideVisible;
		loadAdvancedMappingGuide(advancedMappingState.guideImagePath);
		advancedMappingState.guideVisible = visible;
	}
}

void JPbox_shader::copyCustomStateFrom(const JPbox *source)
{
	const JPbox_shader *sourceShader = dynamic_cast<const JPbox_shader *>(source);
	if (!isAdvancedMappingShader() || sourceShader == nullptr ||
		!sourceShader->isAdvancedMappingShader() ||
		!sourceShader->advancedMappingInitialized)
		return;
	initializeAdvancedMappingState();
	advancedMappingState = sourceShader->advancedMappingState;
	markAdvancedMappingMaskDirty();
	advancedMappingGuide.clear();
	if (!advancedMappingState.guideImagePath.empty())
	{
		const bool visible = advancedMappingState.guideVisible;
		loadAdvancedMappingGuide(advancedMappingState.guideImagePath);
		advancedMappingState.guideVisible = visible;
	}
}
