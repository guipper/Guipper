#include "jp_box_paint.h"

#include "../JPgui/jp_gl_state.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include <FreeImage.h>

namespace
{
	std::string withExtension(const std::string &path, const std::string &extension)
	{
		if (path.size() >= extension.size() &&
			path.substr(path.size() - extension.size()) == extension) return path;
		return path + extension;
	}

	void setGifMetadata(FIBITMAP *page, const char *key, FREE_IMAGE_MDTYPE type,
		const void *value, DWORD bytes)
	{
		FITAG *tag = FreeImage_CreateTag();
		if (tag == nullptr) return;
		FreeImage_SetTagKey(tag, key);
		FreeImage_SetTagType(tag, type);
		FreeImage_SetTagCount(tag, 1);
		FreeImage_SetTagLength(tag, bytes);
		FreeImage_SetTagValue(tag, value);
		FreeImage_SetMetadata(FIMD_ANIMATION, page, key, tag);
		FreeImage_DeleteTag(tag);
	}
	// A stroke that needs no scratch pass: fully opaque and not erasing, so
	// overlapping geometry cannot double-blend visibly.
	//
	// A Fill is never simple however opaque it is - it has to read back whatever
	// is underneath it, which cannot happen inside a batch's framebuffer bind.
	bool isSimpleStroke(const JPPaintStroke &stroke)
	{
		if (stroke.tool == (int)JPPaintTool::Fill) return false;
		// The selection tool stores a temporary lasso outline in the live stroke
		// buffer but are NEVER committed to the document as drawable marks.
		// Guard here as a second line of defence in case one leaks.
		if (stroke.tool == (int)JPPaintTool::LassoSelect ||
			stroke.tool == (int)JPPaintTool::RectSelect) return false;
		if (!stroke.clips.empty()) return false;
		return !stroke.erase && stroke.a >= 0.999f &&
			stroke.hardness >= 0.999f;
	}

	int channel(float value)
	{
		return (int)std::lround(ofClamp(value, 0.0f, 1.0f) * 255.0f);
	}

	// Cels are stored PREMULTIPLIED, so compositing into one uses ONE rather
	// than SRC_ALPHA for the colour term.
	//
	// This is not a preference. With straight alpha, drawing a 40% stroke onto a
	// transparent-black cel gives rgb = 0.4*colour, a = 0.4; compositing THAT
	// over the background multiplies the colour by 0.4 a second time, so the
	// mark lands at 16% of its colour over a darkened background - muddy rather
	// than translucent. The same double-darkening puts a dark fringe on the
	// antialiased edge of every opaque stroke, because MSAA coverage reaches the
	// buffer as partial alpha.
	void beginPremultipliedBlend()
	{
		ofEnableAlphaBlending();
		glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA,
			GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	// Erase scales BOTH colour and alpha by 1-src.a, which is the correct
	// premultiplied erase: a half erase halves the mark's contribution instead
	// of darkening it toward black.
	void beginEraseBlend()
	{
		ofEnableAlphaBlending();
		glBlendFuncSeparate(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA,
			GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
	}

	void beginLayerBlend(int mode)
	{
		ofEnableAlphaBlending();
		switch (mode)
		{
		case 1: // multiply
			glBlendFuncSeparate(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA,
				GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case 2: // screen
			glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR,
				GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case 3: // additive colour, ordinary alpha coverage
			glBlendFuncSeparate(GL_ONE, GL_ONE,
				GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			break;
		default:
			beginPremultipliedBlend();
			break;
		}
	}

	// Back to what the rest of the program assumes. ofEnableAlphaBlending
	// resets the function as well as the enable, so this is a real restore.
	void endCustomBlend()
	{
		ofEnableAlphaBlending();
	}

	const char *kUnpremultiplyVert =
		"#version 150\n"
		"uniform mat4 modelViewProjectionMatrix;\n"
		"in vec4 position;\n"
		"void main() { gl_Position = modelViewProjectionMatrix * position; }\n";

	// gl_FragCoord over resolution: the same convention every shader in
	// bin/data/shaders uses, so the orientation matches theirs instead of
	// needing a y flip of its own.
	const char *kUnpremultiplyFrag =
		"#version 150\n"
		"uniform sampler2D src;\n"
		"uniform vec2 resolution;\n"
		"uniform vec4 bgColor;\n"
		"uniform float opacity;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"  vec4 p = texture(src, gl_FragCoord.xy / resolution);\n"
		"  vec4 b = vec4(bgColor.rgb * bgColor.a, bgColor.a);\n"
		"  vec4 o = p + b * (1.0 - p.a);\n"
		"  o *= opacity;\n"
		"  fragColor = o.a > 0.0005 ? vec4(o.rgb / o.a, o.a) : vec4(0.0);\n"
		"}\n";
}

JPbox_paint::JPbox_paint() {}
JPbox_paint::~JPbox_paint() {}

void JPbox_paint::reload() {}

void JPbox_paint::setup(string _dir, string _name)
{
	JPbox::setup(_dir, _name);

	// APPEND ONLY, and the order has to match ParamSlot. See the comment there
	// for why the transport is deliberately absent.
	parameters.addFloatValue(1.0, "opacity");
	parameters.addFloatValue(0.0, "playhead");
	parameters.addBoolValue(false, "scrub");

	tipo = PAINTBOX;

	// One inlet, and it is a tracing reference only - never composited into the
	// output. Named "reference" rather than "textura1" so the inspector's input
	// row says what it is for.
	fbohandlergroup.addFbohandler("reference");
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	doc = JPPaintDocument();
	doc.frames[0].id = 0;
	doc.layers[0].id = 0;
	doc.layers[0].name = "Layer 1";
	jp_paint::syncLayerArity(doc);
	playback = JPMediaState();
	// A drawing does not start playing at you. Every other media box has a
	// source that is already moving; this one starts as a still canvas.
	playback.playing = false;
	playback.loopMode = JPMediaLoopMode::Loop;
	playheadTicks = 0.0f;
	lastClockTime = ofGetElapsedTimef();

	rasters.assign(kRasterSlots, RasterSlot());
	history.clear();
}

// ---------------------------------------------------------------- lifecycle

void JPbox_paint::update()
{
	JPbox::update();
	// ALWAYS, outside the render gate: the schedule throttles pixels, not
	// clocks. A paint box three quarters skipped still has to keep time or its
	// animation would run at a quarter speed whenever it went off the
	// dependency path.
	advancePlayback();
	if (shouldRenderThisFrame())
	{
		updateFBO();
	}
	setfbohandler_nodepos();
}

void JPbox_paint::updateFBO()
{
	jp_gl::ScopedNoScissor noClip;
	if (tryPassThroughFBO())
	{
		return;
	}
	if (!onoff.boolValue)
	{
		JPbox::updateFBO();
		return;
	}

	const int cel = currentCel();
	// ensureRaster binds a framebuffer, so it has to run before anything else
	// binds one - nesting framebuffer binds is illegal.
	ofFbo &raster = ensureRaster(cel);
	const bool live = liveStrokeActive && !liveStroke.points.empty();

	if (!live)
	{
		compositeToOutput(raster, opacityParam());
		return;
	}

	// A stroke in flight is merged into a scratch copy rather than committed to
	// the cel, so dragging the mouse never re-rasterizes the cel.
	//
	// At the CEL's resolution, not the output's. The two are the same by
	// default, but a canvas sized away from the project resolution would
	// otherwise resize this scratch on every commit and back again on the next
	// drag - a full-resolution reallocation twice per stroke, which hitches
	// while drawing - and would rasterize the live stroke at a different
	// resolution from the committed one.
	ensureScratch(raster.getWidth(), raster.getHeight());
	scratch.begin();
	ofClear(0, 0, 0, 0);
	ofEnableAlphaBlending();
	ofSetColor(255, 255, 255, 255);
	renderStrokeGeometry(liveStroke, scratch.getWidth(), scratch.getHeight());
	scratch.end();

	if (!composite.isAllocated() ||
		(int)composite.getWidth() != (int)raster.getWidth() ||
		(int)composite.getHeight() != (int)raster.getHeight())
	{
		ofFbo::Settings settings;
		settings.width = std::max(1, (int)raster.getWidth());
		settings.height = std::max(1, (int)raster.getHeight());
		settings.internalformat = GL_RGBA8;
		settings.textureTarget = GL_TEXTURE_2D;
		settings.useDepth = false;
		settings.useStencil = false;
		composite.allocate(settings);
	}

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	composite.begin();
	ofClear(0, 0, 0, 0);
	beginPremultipliedBlend();
	ofSetColor(255, 255, 255, 255);
	if (raster.isAllocated())
	{
		raster.draw(0, 0, composite.getWidth(), composite.getHeight());
	}
	if (liveStroke.erase)
	{
		beginEraseBlend();
		ofSetColor(255, 255, 255, channel(liveStroke.a));
	}
	else
	{
		ofSetColor(channel(liveStroke.r * liveStroke.a),
			channel(liveStroke.g * liveStroke.a),
			channel(liveStroke.b * liveStroke.a), channel(liveStroke.a));
	}
	scratch.draw(0, 0, composite.getWidth(), composite.getHeight());
	endCustomBlend();
	composite.end();
	ofPopStyle();

	compositeToOutput(composite, opacityParam());

	// Deliberately absent: the reference inlet and the onion ghosts. Both are
	// working aids for the editor, not part of what this box produces.
}

void JPbox_paint::draw()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255);
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
	ofSetColor(COL_TEXT_PRIMARY, 255);

	draw_inlets();
	ofSetColor(COL_TEXT_PRIMARY, 255);
}

void JPbox_paint::clear()
{
	JPbox::clear();
	for (RasterSlot &slot : rasters)
	{
		if (slot.fbo.isAllocated())
		{
			slot.fbo.clear();
			slot.fbo.destroy();
		}
		slot.valid = false;
	}
	thumbs.clear();
	if (scratch.isAllocated())
	{
		scratch.clear();
		scratch.destroy();
	}
	if (composite.isAllocated())
	{
		composite.clear();
		composite.destroy();
	}
	if (selectionPreview.isAllocated())
	{
		selectionPreview.clear();
		selectionPreview.destroy();
	}
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}

void JPbox_paint::setfbohandler_nodepos()
{
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		fbohandlergroup.setPos(x - width / 2, y, i);
	}
}

// ------------------------------------------------------------------ rasters

void JPbox_paint::ensureScratch(float width, float height)
{
	const int w = std::max(1, (int)width);
	const int h = std::max(1, (int)height);
	if (scratch.isAllocated() &&
		(int)scratch.getWidth() == w && (int)scratch.getHeight() == h)
	{
		return;
	}
	// 4x MSAA so a diagonal stroke edge is antialiased in the scratch itself.
	// Nothing downstream can recover coverage that was never rendered - the
	// same reasoning the advanced mapping mask uses.
	ofFbo::Settings settings;
	settings.width = w;
	settings.height = h;
	settings.internalformat = GL_RGBA8;
	settings.textureTarget = GL_TEXTURE_2D;
	settings.useDepth = false;
	settings.useStencil = true;
	settings.numSamples = 4;
	scratch.allocate(settings);
}

void JPbox_paint::invalidateRasters()
{
	for (RasterSlot &slot : rasters)
	{
		slot.valid = false;
		slot.frameId = -1;
	}
	for (ThumbSlot &slot : thumbs)
	{
		slot.valid = false;
		slot.frameId = -1;
	}
}

// Committing a stroke bumps the cel's revision, which is what invalidates the
// cache - so without this every stroke on a cel costs a replay of every stroke
// already there, and drawing gets slower the longer you draw. Source-over
// premultiplied compositing is associative, so a stroke drawn onto the finished
// cel lands exactly where a replay would have put it. The live-stroke path in
// updateFBO already relies on the same identity.
//
// Only taken when the new stroke cannot interact with anything it was not
// already sitting on top of:
//   - the layer has to be the last thing this cel draws, so nothing composites
//     over the new stroke afterwards;
//   - visible, at full opacity and normal blend, or the layer's contribution is
//     not a plain source-over of its own strokes;
//   - not an eraser, which must reach only its own layer;
//   - not a bucket fill, which reads its target back and would flood through
//     the layers underneath instead of within its own;
//   - not a shared background layer, whose strokes appear on every cel, so one
//     cached raster is not the only one that just went stale.
// Anything else leaves the cache invalid and takes the replay.
void JPbox_paint::appendStrokesToRasterCache(int celIndex, int layerIndex,
	const std::vector<JPPaintStroke> &strokes)
{
	if (rasters.empty() || strokes.empty()) return;
	for (const JPPaintStroke &stroke : strokes)
	{
		if (stroke.points.empty()) return;
		if (stroke.erase || stroke.tool == (int)JPPaintTool::Fill) return;
	}
	if (celIndex < 0 || celIndex >= (int)doc.frames.size()) return;
	if (layerIndex < 0 || layerIndex >= (int)doc.layers.size()) return;

	const JPPaintLayerInfo &info = doc.layers[(std::size_t)layerIndex];
	if (info.background || !info.visible) return;
	if (info.blendMode != 0 || info.opacity < 0.998f) return;
	for (int above = layerIndex + 1; above < (int)doc.layers.size(); ++above)
	{
		const JPPaintLayerInfo &other = doc.layers[(std::size_t)above];
		if (!other.visible || other.opacity <= 0.002f) continue;
		const std::vector<JPPaintStroke> *list =
			jp_paint::strokeListFor(doc, celIndex, above);
		if (list != nullptr && !list->empty()) return;
	}

	const JPPaintFrame &frame = doc.frames[(std::size_t)celIndex];
	const int targetW = std::max(16, doc.canvasWidth);
	const int targetH = std::max(16, doc.canvasHeight);
	for (RasterSlot &slot : rasters)
	{
		// Exactly one revision behind: the slot holds this cel as it was
		// immediately before this stroke, and nothing else has happened since.
		if (!slot.valid || slot.frameId != frame.id) continue;
		if (slot.revision + 1 != frame.revision) continue;
		if (!slot.fbo.isAllocated()) continue;
		if ((int)slot.fbo.getWidth() != targetW ||
			(int)slot.fbo.getHeight() != targetH) continue;

		// Through paintStrokeList, not paintStroke, so the strokes take the
		// SAME path a replay would take for them. A cache that drew them any
		// other way would make the drawing change the moment the slot was
		// evicted.
		jp_gl::ScopedNoScissor noClip;
		paintStrokeList(slot.fbo, strokes);
		slot.revision = frame.revision;
		return;
	}
}

ofFbo &JPbox_paint::ensureRaster(int celIndex)
{
	// A cache MISS rebuilds, which binds a framebuffer. This is reached from
	// inside the editor panel's canvas clip, so the guard is what keeps a window
	// space scissor from cropping the cel.
	// A cache MISS rebuilds, and a rebuild binds a framebuffer. This is reached
	// from inside the editor panel's canvas clip, so without the guard the
	// rebuild - the ofClear included - is cropped to that clip in the wrong
	// coordinate space, and the cel keeps stale content outside it.
	jp_gl::ScopedNoScissor noClip;
	if (rasters.empty())
	{
		rasters.assign(kRasterSlots, RasterSlot());
	}
	jp_paint::clampCurrentFrame(doc);
	const int index = std::clamp(celIndex, 0, (int)doc.frames.size() - 1);
	const JPPaintFrame &frame = doc.frames[(std::size_t)index];
	const int targetW = std::max(16, doc.canvasWidth);
	const int targetH = std::max(16, doc.canvasHeight);

	++rasterClock;

	// A hit needs the same cel, the same edit counter AND the same size: a
	// render resolution change has to repaint rather than stretch stale pixels.
	for (RasterSlot &slot : rasters)
	{
		if (!slot.valid || slot.frameId != frame.id) continue;
		if (slot.revision != frame.revision) continue;
		if (!slot.fbo.isAllocated()) continue;
		if ((int)slot.fbo.getWidth() != targetW ||
			(int)slot.fbo.getHeight() != targetH) continue;
		slot.lastUsed = rasterClock;
		return slot.fbo;
	}

	// Miss. Take an unused slot if there is one, otherwise the least recently
	// used - the cel the user is least likely to be looking at.
	RasterSlot *victim = &rasters[0];
	for (RasterSlot &slot : rasters)
	{
		if (!slot.valid)
		{
			victim = &slot;
			break;
		}
		if (slot.lastUsed < victim->lastUsed)
		{
			victim = &slot;
		}
	}

	if (!victim->fbo.isAllocated() ||
		(int)victim->fbo.getWidth() != targetW ||
		(int)victim->fbo.getHeight() != targetH)
	{
		ofFbo::Settings settings;
		settings.width = targetW;
		settings.height = targetH;
		settings.internalformat = GL_RGBA8;
		settings.textureTarget = GL_TEXTURE_2D;
		settings.useDepth = false;
		settings.useStencil = false;
		settings.numSamples = 4;
		victim->fbo.allocate(settings);
	}

	rebuildRaster(victim->fbo, doc, index);
	// Force the MSAA resolve NOW, while the scissor is still suspended.
	//
	// This is the whole reason the cel looked cropped. These framebuffers are
	// multisampled, so ofFbo defers a glBlitFramebuffer from the sample buffer
	// to the texture until something asks for the texture - which is inside
	// drawCel, inside the panel's canvas clip. A blit IS subject to the scissor
	// test, so the resolve copied only the clipped rectangle and the rest of the
	// cel stayed blank. The filmstrip clip is 56px tall, which is why the
	// thumbnails came out empty altogether.
	victim->fbo.getTexture();
	victim->frameId = frame.id;
	victim->revision = frame.revision;
	victim->lastUsed = rasterClock;
	victim->valid = true;
	return victim->fbo;
}

void JPbox_paint::ensureLayerScratch(float width, float height)
{
	const int w = std::max(1, (int)width);
	const int h = std::max(1, (int)height);
	if (layerScratch.isAllocated() &&
		(int)layerScratch.getWidth() == w && (int)layerScratch.getHeight() == h)
	{
		return;
	}
	ofFbo::Settings settings;
	settings.width = w;
	settings.height = h;
	settings.internalformat = GL_RGBA8;
	settings.textureTarget = GL_TEXTURE_2D;
	settings.useDepth = false;
	settings.useStencil = false;
	settings.numSamples = 4;
	layerScratch.allocate(settings);
}

void JPbox_paint::rebuildRaster(ofFbo &target, const JPPaintDocument &document,
	int celIndex, int overrideLayerIndex,
	const std::vector<JPPaintStroke> *overrideStrokes)
{
	const float w = target.getWidth();
	const float h = target.getHeight();

	target.begin();
	ofClear(0, 0, 0, 0);
	target.end();

	for (int layerIndex = 0; layerIndex < (int)document.layers.size();
		++layerIndex)
	{
		const JPPaintLayerInfo &info = document.layers[(std::size_t)layerIndex];
		if (!info.visible || info.opacity <= 0.002f) continue;
		const std::vector<JPPaintStroke> *strokes =
			layerIndex == overrideLayerIndex && overrideStrokes != nullptr ?
			overrideStrokes : jp_paint::strokeListFor(document, celIndex, layerIndex);
		if (strokes == nullptr || strokes->empty()) continue;

		// EVERY layer goes through the scratch, even a fully opaque one. It costs
		// one blit and buys consistent semantics: an eraser reaches only its own
		// layer, and a bucket floods within its own layer rather than through
		// whatever happens to sit underneath it.
		ensureLayerScratch(w, h);
		layerScratch.begin();
		ofClear(0, 0, 0, 0);
		layerScratch.end();
		paintStrokeList(layerScratch, *strokes);

		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		target.begin();
		beginLayerBlend(info.blendMode);
		// Premultiplied, so scaling by the layer's opacity means scaling ALL FOUR
		// channels. Scaling only alpha would leave the colour at full strength and
		// the opacity control would barely do anything - the same mistake the
		// onion ghosts made before they were fixed.
		const int k = channel(info.opacity);
		ofSetColor(k, k, k, k);
		layerScratch.draw(0, 0, w, h);
		endCustomBlend();
		target.end();
		ofPopStyle();
	}
}

void JPbox_paint::paintStrokeList(ofFbo &target,
	const std::vector<JPPaintStroke> &strokes)
{
	const float w = target.getWidth();
	const float h = target.getHeight();

	// Reduce adjacent complementary pairs recursively. Recursive reduction is
	// important after selecting an already clipped area: four nested pieces can
	// collapse to two and then back to the one unchanged source stroke.
	std::vector<JPPaintStroke> collapsed;
	const std::vector<JPPaintStroke> *renderStrokes = &strokes;
	if (std::any_of(strokes.begin(), strokes.end(),
		[](const JPPaintStroke &stroke) { return !stroke.clips.empty(); }))
	{
		collapsed = jp_paint::collapseComplementaryStrokes(strokes);
		renderStrokes = &collapsed;
	}
	const std::vector<JPPaintStroke> &renderList = *renderStrokes;

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	std::size_t i = 0;
	while (i < renderList.size())
	{
		if (isSimpleStroke(renderList[i]))
		{
			// Batch the whole run of simple strokes into ONE framebuffer bind.
			// Opaque non-erasing strokes are the common case by a wide margin,
			// and a bind per stroke would dominate a rebuild.
			target.begin();
			while (i < renderList.size() && isSimpleStroke(renderList[i]))
			{
				const JPPaintStroke &stroke = renderList[i];
				// Re-armed per stroke, not once for the batch:
				// renderStrokeGeometry ends in ofPopStyle, which restores the
				// blend MODE and so throws away glBlendFuncSeparate. Without
				// this every stroke after the first composites straight.
				beginPremultipliedBlend();
				// Opaque, so rgb*a == rgb: already premultiplied. MSAA turns
				// edge coverage into partial alpha at resolve time, which is
				// exactly the premultiplied edge we want.
				ofSetColor(channel(stroke.r), channel(stroke.g),
					channel(stroke.b), 255);
				renderStrokeGeometry(stroke, w, h);
				++i;
			}
			endCustomBlend();
			target.end();
			continue;
		}
		if (renderList[i].tool == (int)JPPaintTool::Fill)
		{
			paintFillStroke(target, renderList[i]);
		}
		else
		{
			paintStroke(target, renderList[i]);
		}
		++i;
	}
	ofEnableAlphaBlending();
	ofPopStyle();
}

void JPbox_paint::paintStroke(ofFbo &target, const JPPaintStroke &stroke)
{
	if (stroke.points.empty()) return;

	// Filmstrip scale. The self-overlap seam the scratch pass exists to remove
	// is sub-pixel in a 46px thumbnail, and going through the scratch here would
	// resize it from the render resolution and back on every cel edit - an 8MB
	// reallocation twice per stroke, which hitches while drawing.
	if (target.getWidth() < 256.0f && stroke.clips.empty() &&
		stroke.hardness >= 0.999f)
	{
		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		target.begin();
		if (stroke.erase)
		{
			beginEraseBlend();
			ofSetColor(255, 255, 255, channel(stroke.a));
		}
		else
		{
			beginPremultipliedBlend();
			ofSetColor(channel(stroke.r * stroke.a), channel(stroke.g * stroke.a),
				channel(stroke.b * stroke.a), channel(stroke.a));
		}
		renderStrokeGeometry(stroke, target.getWidth(), target.getHeight());
		endCustomBlend();
		target.end();
		ofPopStyle();
		return;
	}

	// A translucent stroke that crosses itself would double-blend at the
	// overlap and show a dark seam, because the ribbon and its join circles are
	// separate pieces of geometry. Rendering the stroke OPAQUE into a scratch
	// buffer and compositing that once at the stroke's alpha is what real paint
	// programs do, and it is the only way to get one coverage value per pixel
	// out of overlapping geometry.
	ensureScratch(target.getWidth(), target.getHeight());
	scratch.begin();
	ofClear(0, 0, 0, 0);
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
	const bool clipped = beginStrokeClip(stroke,
		scratch.getWidth(), scratch.getHeight());
	const bool softBrush = stroke.hardness < 0.999f &&
		(stroke.tool == (int)JPPaintTool::Brush ||
		 stroke.tool == (int)JPPaintTool::Eraser);
	if (!softBrush)
	{
		ofEnableAlphaBlending();
		ofSetColor(255, 255, 255, 255);
		renderStrokeGeometry(stroke, scratch.getWidth(), scratch.getHeight());
	}
	else
	{
		// Nested silhouettes approximate a radial feather without changing the
		// vector path. The alpha sequence makes accumulated coverage progress
		// linearly from the outside to the opaque core.
		const int steps = 16;
		for (int i = 0; i < steps; ++i)
		{
			JPPaintStroke pass = stroke;
			const float u = (float)i / (float)(steps - 1);
			pass.size *= ofLerp(1.0f, std::max(0.025f, stroke.hardness), u);
			beginPremultipliedBlend();
			const int k = channel(1.0f / (float)(steps - i));
			ofSetColor(k, k, k, k);
			renderStrokeGeometry(pass, scratch.getWidth(), scratch.getHeight());
		}
	}
	if (clipped) endStrokeClip();
	scratch.end();

	target.begin();
	if (stroke.erase)
	{
		beginEraseBlend();
		ofSetColor(255, 255, 255, channel(stroke.a));
		scratch.draw(0, 0, target.getWidth(), target.getHeight());
	}
	else
	{
		beginPremultipliedBlend();
		// The scratch holds coverage in every channel, so scaling rgb by the
		// stroke's alpha here is what makes the result premultiplied:
		// rgb = coverage*alpha*colour, a = coverage*alpha.
		ofSetColor(channel(stroke.r * stroke.a), channel(stroke.g * stroke.a),
			channel(stroke.b * stroke.a), channel(stroke.a));
		scratch.draw(0, 0, target.getWidth(), target.getHeight());
	}
	endCustomBlend();
	target.end();
}

bool JPbox_paint::materializeFill(float u, float v, float tolerance,
	const ofFloatColor &color)
{
	jp_gl::ScopedNoScissor noClip;
	jp_paint::clampCurrentLayer(doc);
	jp_paint::clampCurrentFrame(doc);
	if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f) return false;
	const int layerIndex = doc.currentLayer;
	if (doc.layers[(std::size_t)layerIndex].locked) return false;
	const int cel = doc.currentFrame;
	const std::vector<JPPaintStroke> *strokes =
		jp_paint::strokeListFor(doc, cel, layerIndex);
	if (strokes == nullptr) return false;

	const int w = std::max(16, doc.canvasWidth);
	const int h = std::max(16, doc.canvasHeight);
	// The layer ALONE, which is what a bucket floods within: reusing the cel
	// raster would let the region leak across whatever happens to sit on the
	// layers underneath it.
	ensureLayerScratch((float)w, (float)h);
	layerScratch.begin();
	ofClear(0, 0, 0, 0);
	layerScratch.end();
	paintStrokeList(layerScratch, *strokes);
	layerScratch.readToPixels(fillReadback);
	if ((int)fillReadback.getWidth() != w ||
		(int)fillReadback.getHeight() != h) return false;
	if (fillReadback.getNumChannels() < 4) return false;

	// Same row convention paintFillStroke and sampleColor use: readToPixels
	// hands rows back in the order the strokes were rendered in, so v maps
	// straight to a row.
	const int seedX = (int)std::lround(u * (float)(w - 1));
	const int seedY = (int)std::lround(v * (float)(h - 1));
	const int tol =
		(int)std::lround(ofClamp(tolerance, 0.0f, 1.0f) * 255.0f);
	if (jp_paint::floodFill(fillReadback.getData(), w, h, seedX, seedY, tol,
		fillMask) == 0) return false;

	std::vector<std::vector<JPPaintPoint>> contours;
	// Half a pixel: takes the staircase off a traced edge without moving it
	// anywhere the eye can follow at the resolution it was traced at.
	jp_paint::traceMaskContours(fillMask, w, h, contours, 0.5f / (float)w);
	if (contours.empty()) return false;

	JPPaintStroke region;
	region.tool = (int)JPPaintTool::Region;
	region.r = color.r;
	region.g = color.g;
	region.b = color.b;
	region.a = color.a;
	region.tolerance = ofClamp(tolerance, 0.0f, 1.0f);
	region.points = contours.front();
	region.contours.assign(contours.begin() + 1, contours.end());
	commitStroke(region);
	return true;
}

// The OLD bucket: a seed and a tolerance, re-flooded on every rebuild. Kept
// because savefiles contain it. New fills are materialised into geometry by
// materializeFill instead - see JPPaintStroke::contours for why.
void JPbox_paint::paintFillStroke(ofFbo &target, const JPPaintStroke &stroke)
{
	if (stroke.points.empty()) return;
	const int w = std::max(1, (int)target.getWidth());
	const int h = std::max(1, (int)target.getHeight());

	// Read what is already there. The target must NOT be bound: reading a bound
	// framebuffer gives stale contents, and getTexture resolves the multisample
	// buffer on the way out - which is why this whole path runs inside
	// ensureRaster's ScopedNoScissor.
	target.readToPixels(fillReadback);
	if ((int)fillReadback.getWidth() != w || (int)fillReadback.getHeight() != h)
	{
		return;
	}
	if (fillReadback.getNumChannels() < 4)
	{
		ofLogWarning("JPbox_paint") << name <<
			": fill needs an RGBA readback, got " <<
			fillReadback.getNumChannels() << " channels";
		return;
	}

	// readToPixels hands back rows in the SAME order the strokes were rendered
	// in, so a seed at uv.y maps straight to row uv.y*h. Do not "fix" this with a
	// flip - the mask is uploaded and drawn through the same path, so the two
	// cancel, and only the seed has to agree.
	const int seedX = (int)std::lround(stroke.points[0].x * (float)(w - 1));
	const int seedY = (int)std::lround(stroke.points[0].y * (float)(h - 1));
	const int tolerance = (int)std::lround(ofClamp(stroke.tolerance, 0.0f, 1.0f) * 255.0f);
	const std::size_t filled = jp_paint::floodFill(fillReadback.getData(), w, h,
		seedX, seedY, tolerance, fillMask);
	if (filled == 0) return;

	// Premultiplied straight away, so the composite below is one plain blend.
	if ((int)fillPixels.getWidth() != w || (int)fillPixels.getHeight() != h ||
		fillPixels.getNumChannels() != 4)
	{
		fillPixels.allocate(w, h, OF_PIXELS_RGBA);
	}
	const unsigned char red = (unsigned char)channel(stroke.r * stroke.a);
	const unsigned char green = (unsigned char)channel(stroke.g * stroke.a);
	const unsigned char blue = (unsigned char)channel(stroke.b * stroke.a);
	const unsigned char alpha = (unsigned char)channel(stroke.a);
	unsigned char *out = fillPixels.getData();
	for (std::size_t p = 0; p < fillMask.size(); ++p)
	{
		const std::size_t o = p * 4;
		if (fillMask[p] != 0)
		{
			out[o] = red; out[o + 1] = green; out[o + 2] = blue; out[o + 3] = alpha;
		}
		else
		{
			out[o] = out[o + 1] = out[o + 2] = out[o + 3] = 0;
		}
	}
	fillTexture.loadData(fillPixels);

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	target.begin();
	if (stroke.erase)
	{
		beginEraseBlend();
		ofSetColor(255, 255, 255, 255);
	}
	else
	{
		beginPremultipliedBlend();
		// The texture already carries the colour, so no tint here.
		ofSetColor(255, 255, 255, 255);
	}
	fillTexture.draw(0, 0, (float)w, (float)h);
	endCustomBlend();
	target.end();
	ofPopStyle();
}

void JPbox_paint::renderStrokeGeometry(const JPPaintStroke &stroke,
	float width, float height)
{
	const std::vector<JPPaintPoint> &points = stroke.points;
	if (points.empty()) return;
	// LassoSelect is a UI-only tool: its outline is drawn by
	// the panel overlay, not the rasterizer, and must never produce marks.
	if (stroke.tool == (int)JPPaintTool::LassoSelect ||
		stroke.tool == (int)JPPaintTool::RectSelect) return;

	// Sizes are normalized to canvas WIDTH only, so a brush dab stays round on
	// a non-square canvas instead of turning elliptical.
	const float base = std::max(0.5f, stroke.size * width);

	ofPushStyle();
	ofFill();
	// Enough segments that a fat brush does not look like a stop sign, capped
	// so a very fat one does not cost thousands of triangles per dab.
	ofSetCircleResolution((int)ofClamp(base * 1.5f, 12.0f, 64.0f));

	if (jp_paint::isAreaTool(stroke.tool))
	{
		// A closed, filled area. ofPath is right here for exactly the reason it
		// is wrong for a ribbon: there is no width to vary and no join to notch,
		// only an area to fill.
		ofPath path = strokeAreaPath(stroke, width, height);
		path.setFillColor(ofGetStyle().color);
		path.draw();
		ofPopStyle();
		return;
	}

	if (points.size() == 1)
	{
		ofDrawCircle(points[0].x * width, points[0].y * height,
			base * points[0].width);
		ofPopStyle();
		return;
	}

	// ofPath::setStrokeWidth gives a constant width with no round caps or
	// joins, so the stroke would step visibly wherever the width changed and
	// show notches at every corner. A ribbon of quads plus a circle at each
	// point is cheaper and handles hairpins without special cases.
	ofMesh ribbon;
	ribbon.setMode(OF_PRIMITIVE_TRIANGLES);
	for (std::size_t i = 0; i + 1 < points.size(); ++i)
	{
		const glm::vec2 p0(points[i].x * width, points[i].y * height);
		const glm::vec2 p1(points[i + 1].x * width, points[i + 1].y * height);
		const glm::vec2 delta = p1 - p0;
		const float length = glm::length(delta);
		// Duplicate samples produce no direction; the join circles already
		// cover the pixel, so skipping is correct rather than merely safe.
		if (length < 1e-4f) continue;
		const glm::vec2 normal(-delta.y / length, delta.x / length);
		const float r0 = base * points[i].width;
		const float r1 = base * points[i + 1].width;
		const glm::vec2 a = p0 + normal * r0;
		const glm::vec2 b = p0 - normal * r0;
		const glm::vec2 c = p1 + normal * r1;
		const glm::vec2 d = p1 - normal * r1;
		ribbon.addVertex(glm::vec3(a, 0.0f));
		ribbon.addVertex(glm::vec3(b, 0.0f));
		ribbon.addVertex(glm::vec3(c, 0.0f));
		ribbon.addVertex(glm::vec3(b, 0.0f));
		ribbon.addVertex(glm::vec3(d, 0.0f));
		ribbon.addVertex(glm::vec3(c, 0.0f));
	}
	if (ribbon.getNumVertices() > 0)
	{
		ribbon.draw();
	}
	for (const JPPaintPoint &point : points)
	{
		ofDrawCircle(point.x * width, point.y * height, base * point.width);
	}
	ofPopStyle();
}

ofPath JPbox_paint::strokeAreaPath(const JPPaintStroke &stroke,
	float width, float height) const
{
	ofPath path;
	path.setFilled(true);
	// A Region is a traced flood fill, so a contour inside another contour is a
	// HOLE and the winding rule has to be ODD. A lasso or a shape is a single
	// outline the user drew: overlaps there are a UNION, which is the decision
	// rebuildAdvancedMappingMask documents.
	const bool region = stroke.tool == (int)JPPaintTool::Region;
	path.setPolyWindingMode(region ? OF_POLY_WINDING_ODD :
		OF_POLY_WINDING_NONZERO);
	auto addContour = [&path, width, height]
		(const std::vector<JPPaintPoint> &contour)
	{
		if (contour.size() < 3) return;
		path.moveTo(contour[0].x * width, contour[0].y * height);
		for (std::size_t i = 1; i < contour.size(); ++i)
			path.lineTo(contour[i].x * width, contour[i].y * height);
		path.close();
	};
	addContour(stroke.points);
	if (region)
	{
		for (const std::vector<JPPaintPoint> &contour : stroke.contours)
			addContour(contour);
	}
	return path;
}

bool JPbox_paint::beginStrokeClip(const JPPaintStroke &stroke,
	float width, float height)
{
	// COUNTING, not one bit per clip.
	//
	// A bit per clip capped the nesting at eight in an 8 bit stencil, and the
	// ninth clip was silently ignored - a selection that looked like it had
	// worked and had not. A counter uses the same eight bits to reach 255:
	// every clip increments the pixels it keeps, so a pixel kept by all of them
	// ends up holding exactly the clip count.
	//
	// An inverted clip keeps the EXTERIOR of its polygon. Rather than build a
	// polygon complement, it increments everywhere and then decrements inside
	// the polygon, which lands on the same place.
	std::size_t count = 0;
	for (const JPPaintClip &clip : stroke.clips)
		if (clip.points.size() >= 3) ++count;
	if (count == 0) return false;
	// The counter must not wrap: at 256 clips a fully excluded pixel would read
	// as fully included. Nothing reaches this - jp_paint::kMaxStrokeClips bounds
	// what a selection can build, and the loader clamps what a savefile can ask
	// for - so it is a backstop, not a behaviour.
	count = std::min<std::size_t>(count, 255);

	glEnable(GL_STENCIL_TEST);
	glStencilMask(0xff);
	glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDisable(GL_BLEND);
	glStencilFunc(GL_ALWAYS, 0, 0xff);

	std::size_t applied = 0;
	for (const JPPaintClip &clip : stroke.clips)
	{
		if (clip.points.size() < 3) continue;
		if (applied >= count) break;
		++applied;

		// One path per clip and never subpaths: the tessellator hands back
		// non-overlapping triangles, which is what lets a pixel be incremented
		// exactly once per clip.
		ofPath path;
		path.setFilled(true);
		path.moveTo(clip.points[0].x * width, clip.points[0].y * height);
		for (std::size_t p = 1; p < clip.points.size(); ++p)
			path.lineTo(clip.points[p].x * width, clip.points[p].y * height);
		path.close();

		if (clip.inverted)
		{
			glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
			ofDrawRectangle(0.0f, 0.0f, width, height);
			glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
		}
		else
		{
			glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
		}
		path.draw();
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glStencilMask(0x00);
	glStencilFunc(GL_EQUAL, (GLint)applied, 0xff);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	ofEnableAlphaBlending();
	return true;
}

void JPbox_paint::endStrokeClip()
{
	glStencilMask(0xff);
	glDisable(GL_STENCIL_TEST);
	ofEnableAlphaBlending();
}

void JPbox_paint::ensureUnpremultiplyShader()
{
	if (unpremultiplyTried) return;
	unpremultiplyTried = true;
	// Inline source rather than a file in bin/data: this is one pass of
	// arithmetic that must always be available, and a missing or edited asset
	// would silently change what the box outputs.
	unpremultiply.setupShaderFromSource(GL_VERTEX_SHADER, kUnpremultiplyVert);
	unpremultiply.setupShaderFromSource(GL_FRAGMENT_SHADER, kUnpremultiplyFrag);
	unpremultiply.bindDefaults();
	unpremultiplyReady = unpremultiply.linkProgram();
	if (!unpremultiplyReady)
	{
		ofLogWarning("JPbox_paint") << name <<
			": un-premultiply shader failed to link; translucent strokes will "
			"composite darker than they should";
	}
}

void JPbox_paint::compositeToOutput(ofFbo &source, float opacity)
{
	ensureUnpremultiplyShader();
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	fbo.begin();
	// A FULL repaint every time. The render schedule skips three frames in four
	// and never clears this framebuffer in between, so anything that relied on
	// the previous contents would smear.
	ofClear(0, 0, 0, 0);
	if (unpremultiplyReady && source.isAllocated())
	{
		// The shader writes the complete destination pixel.
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		unpremultiply.begin();
		unpremultiply.setUniformTexture("src", source.getTexture(), 1);
		unpremultiply.setUniform2f("resolution",
			(float)fbo.getWidth(), (float)fbo.getHeight());
		unpremultiply.setUniform4f("bgColor",
			doc.bgR, doc.bgG, doc.bgB, doc.bgA);
		unpremultiply.setUniform1f("opacity", opacity);
		ofSetColor(255, 255, 255, 255);
		ofDrawRectangle(0, 0, fbo.getWidth(), fbo.getHeight());
		unpremultiply.end();
	}
	else if (source.isAllocated())
	{
		// Fallback if the shader could not link: visibly imperfect for
		// translucent strokes, but the box still draws.
		ofClear(channel(doc.bgR), channel(doc.bgG), channel(doc.bgB),
			channel(doc.bgA));
		beginPremultipliedBlend();
		ofSetColor(255, 255, 255, channel(opacity));
		source.draw(0, 0, fbo.getWidth(), fbo.getHeight());
	}
	fbo.end();
	// Resolve while updateFBO's guard still has the scissor suspended: this
	// framebuffer is drawn by the node graph, by render windows and by the
	// editor, and any of them can have a clip in force.
	fbo.getTexture();
	endCustomBlend();
	ofPopStyle();
}

void JPbox_paint::warmThumb(int index)
{
	jp_gl::ScopedNoScissor noClip;
	if (doc.frames.empty()) return;
	const int cel = std::clamp(index, 0, (int)doc.frames.size() - 1);
	if ((int)thumbs.size() != (int)doc.frames.size())
	{
		thumbs.resize(doc.frames.size());
	}
	ThumbSlot &slot = thumbs[(std::size_t)cel];
	const JPPaintFrame &frame = doc.frames[(std::size_t)cel];
	const int width = 96;
	const int height = std::max(16, (int)std::lround(96.0f / canvasAspect()));
	if (slot.valid && slot.frameId == frame.id &&
		slot.revision == frame.revision && slot.fbo.isAllocated() &&
		(int)slot.fbo.getWidth() == width)
	{
		return;
	}
	if (!slot.fbo.isAllocated() || (int)slot.fbo.getWidth() != width ||
		(int)slot.fbo.getHeight() != height)
	{
		ofFbo::Settings settings;
		settings.width = width;
		settings.height = height;
		settings.internalformat = GL_RGBA8;
		settings.textureTarget = GL_TEXTURE_2D;
		settings.useDepth = false;
		settings.useStencil = false;
		settings.numSamples = 4;
		slot.fbo.allocate(settings);
	}
	rebuildRaster(slot.fbo, doc, cel);
	// Resolve here rather than in drawThumb, which runs inside the filmstrip
	// clip. See the note in ensureRaster.
	slot.fbo.getTexture();
	slot.frameId = frame.id;
	slot.revision = frame.revision;
	slot.valid = true;
}

void JPbox_paint::drawThumb(int index, const ofRectangle &bounds)
{
	if (index < 0 || index >= (int)thumbs.size()) return;
	ThumbSlot &slot = thumbs[(std::size_t)index];
	if (!slot.valid || !slot.fbo.isAllocated()) return;
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	beginPremultipliedBlend();
	ofSetColor(255, 255, 255, 255);
	slot.fbo.draw(bounds);
	endCustomBlend();
	ofPopStyle();
}

void JPbox_paint::warmCel(int index)
{
	if (doc.frames.empty()) return;
	ensureRaster(index);
}

void JPbox_paint::drawCel(int index, float drawX, float drawY,
	float drawWidth, float drawHeight, const ofColor &tint)
{
	if (doc.frames.empty()) return;
	// ensureRaster can bind a framebuffer, so this must not be called between
	// a begin()/end() pair - and callers inside a scissor must warmCel first.
	ofFbo &raster = ensureRaster(index);
	if (!raster.isAllocated()) return;
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	// The cel is premultiplied, so it composites with ONE rather than SRC_ALPHA.
	// A tint scales both terms, which keeps it premultiplied.
	beginPremultipliedBlend();
	ofSetColor(tint);
	raster.draw(drawX, drawY, drawWidth, drawHeight);
	endCustomBlend();
	ofPopStyle();
}

void JPbox_paint::drawCelPreview(int index, int layerIndex,
	const std::vector<JPPaintStroke> &overrideStrokes,
	float drawX, float drawY, float drawWidth, float drawHeight,
	const ofColor &tint)
{
	if (doc.frames.empty()) return;
	const int width = std::max(1, (int)fbo.getWidth());
	const int height = std::max(1, (int)fbo.getHeight());
	if (!selectionPreview.isAllocated() ||
		(int)selectionPreview.getWidth() != width ||
		(int)selectionPreview.getHeight() != height)
	{
		ofFbo::Settings settings;
		settings.width = width;
		settings.height = height;
		settings.internalformat = GL_RGBA8;
		settings.textureTarget = GL_TEXTURE_2D;
		settings.useDepth = false;
		settings.useStencil = false;
		settings.numSamples = 4;
		selectionPreview.allocate(settings);
	}

	{
		jp_gl::ScopedNoScissor noClip;
		const int cel = std::clamp(index, 0, (int)doc.frames.size() - 1);
		rebuildRaster(selectionPreview, doc, cel,
			layerIndex, &overrideStrokes);
		selectionPreview.getTexture();
	}

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	beginPremultipliedBlend();
	ofSetColor(tint);
	selectionPreview.draw(drawX, drawY, drawWidth, drawHeight);
	endCustomBlend();
	ofPopStyle();
}

void JPbox_paint::drawStrokePreview(const JPPaintStroke &stroke, float drawX,
	float drawY, float drawWidth, float drawHeight)
{
	if (stroke.points.empty()) return;
	ofPushMatrix();
	ofTranslate(drawX, drawY);
	// Keep the preview on the same soft-edge path as the actual brush.  Calling
	// renderStrokeGeometry just once made every preview look 100% hard even
	// though the canvas correctly used the nested feather passes in paintStroke.
	const ofColor tint = ofGetStyle().color;
	const bool soft = stroke.hardness < 0.999f &&
		(stroke.tool == (int)JPPaintTool::Brush ||
		 stroke.tool == (int)JPPaintTool::Eraser);
	if (!soft)
	{
		renderStrokeGeometry(stroke, drawWidth, drawHeight);
	}
	else
	{
		const int steps = 16;
		for (int i = 0; i < steps; ++i)
		{
			JPPaintStroke pass = stroke;
			const float u = (float)i / (float)(steps - 1);
			pass.size *= ofLerp(1.0f, std::max(0.025f, stroke.hardness), u);
			const float alpha = 1.0f / (float)(steps - i);
			ofSetColor(tint.r * alpha, tint.g * alpha, tint.b * alpha,
				tint.a * alpha);
			renderStrokeGeometry(pass, drawWidth, drawHeight);
		}
	}
	ofPopMatrix();
}

bool JPbox_paint::sampleColor(float u, float v, ofFloatColor &out)
{
	if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f) return false;
	jp_gl::ScopedNoScissor noClip;
	ofFbo &raster = ensureRaster(currentCel());
	if (!raster.isAllocated()) return false;
	const int w = std::max(1, (int)raster.getWidth());
	const int h = std::max(1, (int)raster.getHeight());

	// A whole-surface readback to sample one pixel is wasteful, but it happens
	// once per click and glReadPixels cannot read a multisampled framebuffer
	// directly - readToPixels resolves on the way out. Reuses the fill buffer
	// rather than allocating: both are transient and never overlap.
	raster.readToPixels(fillReadback);
	if ((int)fillReadback.getWidth() != w ||
		(int)fillReadback.getHeight() != h) return false;
	if (fillReadback.getNumChannels() < 4) return false;

	// Same row convention the bucket fill uses and has been verified against.
	const int x = std::clamp((int)std::lround(u * (float)(w - 1)), 0, w - 1);
	const int y = std::clamp((int)std::lround(v * (float)(h - 1)), 0, h - 1);
	const unsigned char *p =
		fillReadback.getData() + ((std::size_t)y * (std::size_t)w + x) * 4;
	const float alpha = (float)p[3] / 255.0f;
	if (alpha <= 0.002f)
	{
		// Nothing drawn here, so the honest answer is the canvas background -
		// which is what the user sees at that pixel.
		out.set(doc.bgR, doc.bgG, doc.bgB, doc.bgA);
		return true;
	}
	// The raster is premultiplied; a colour picker deals in straight alpha.
	out.set(ofClamp((float)p[0] / 255.0f / alpha, 0.0f, 1.0f),
		ofClamp((float)p[1] / 255.0f / alpha, 0.0f, 1.0f),
		ofClamp((float)p[2] / 255.0f / alpha, 0.0f, 1.0f), alpha);
	return true;
}

ofFbo *JPbox_paint::referenceFbo()
{
	if (fbohandlergroup.getSize() <= 0) return nullptr;
	if (!fbohandlergroup.getisPointerSet(0)) return nullptr;
	// getFboPointerReference, NOT getFboPointer: the latter returns a COPY of
	// the framebuffer. See the note on the declaration.
	ofFbo *source = fbohandlergroup.getFboPointerReference(0);
	// A link can be resolved before the producer has allocated anything - on
	// load the pointer is set in a second pass, and a device box allocates when
	// its device opens.
	if (source == nullptr || !source->isAllocated()) return nullptr;
	return source;
}

float JPbox_paint::canvasAspect() const
{
	const float w = std::max(1.0f, (float)doc.canvasWidth);
	const float h = std::max(1.0f, (float)doc.canvasHeight);
	return w / h;
}

int JPbox_paint::canvasPixelWidth() const
{
	return std::max(16, doc.canvasWidth);
}

int JPbox_paint::canvasPixelHeight() const
{
	return std::max(16, doc.canvasHeight);
}

void JPbox_paint::setCanvasSize(int width, int height)
{
	const int w = std::clamp(width, 16, 8192);
	const int h = std::clamp(height, 16, 8192);
	if (doc.canvasWidth == w && doc.canvasHeight == h) return;
	doc.canvasWidth = w;
	doc.canvasHeight = h;
	jp_paint::bumpAllFrames(doc);
	invalidateRasters();
}

void JPbox_paint::setCanvasBackground(float r, float g, float b, float a)
{
	doc.bgR = ofClamp(r, 0.0f, 1.0f);
	doc.bgG = ofClamp(g, 0.0f, 1.0f);
	doc.bgB = ofClamp(b, 0.0f, 1.0f);
	doc.bgA = ofClamp(a, 0.0f, 1.0f);
}

void JPbox_paint::exportCelRange(bool useRange, int &firstCel,
	int &lastCel) const
{
	if (useRange)
	{
		jp_paint::celRange(doc, playback.rangeIn, playback.rangeOut,
			firstCel, lastCel);
		return;
	}
	firstCel = 0;
	lastCel = doc.frames.empty() ? 0 : (int)doc.frames.size() - 1;
}

bool JPbox_paint::renderCelPixels(int celIndex, ofPixels &pixels, float scale)
{
	if (doc.frames.empty()) return false;
	jp_gl::ScopedNoScissor noClip;
	const float clampedScale = ofClamp(scale, 0.05f, 8.0f);
	if (std::abs(clampedScale - 1.0f) > 0.0005f)
	{
		// Rasterized at the export size rather than resampled from the cache:
		// strokes are vector, so a 2x export is 2x the geometry, not 2x the
		// pixels.
		const int w = std::clamp((int)std::lround(
			(float)std::max(16, doc.canvasWidth) * clampedScale), 16, 16384);
		const int h = std::clamp((int)std::lround(
			(float)std::max(16, doc.canvasHeight) * clampedScale), 16, 16384);
		if (!exportBuffer.isAllocated() ||
			(int)exportBuffer.getWidth() != w ||
			(int)exportBuffer.getHeight() != h)
		{
			ofFbo::Settings settings;
			settings.width = w;
			settings.height = h;
			settings.internalformat = GL_RGBA8;
			settings.textureTarget = GL_TEXTURE_2D;
			settings.useDepth = false;
			settings.useStencil = false;
			settings.numSamples = 4;
			exportBuffer.allocate(settings);
		}
		rebuildRaster(exportBuffer, doc,
			std::clamp(celIndex, 0, (int)doc.frames.size() - 1));
		// Resolve while the scissor is suspended - see the note in ensureRaster.
		exportBuffer.getTexture();
		exportBuffer.readToPixels(pixels);
	}
	else
	{
		ofFbo &raster = ensureRaster(celIndex);
		raster.readToPixels(pixels);
	}
	if (!pixels.isAllocated() || pixels.getNumChannels() < 4) return false;
	// NO vertical flip. openFrameworks flips the MATRIX when it renders into a
	// framebuffer (OF_FBOMODE_MATRIXFLIP), so the row a shape drawn at y=0 lands
	// in is texture row 0, and glGetTexImage hands that row back first. Every
	// export used to mirror here on the assumption that a readback is bottom up,
	// which turned every exported PNG and GIF frame upside down. It is the same
	// convention the bucket fill and the eyedropper have always relied on: v maps
	// straight to a row.

	// The cached cel is premultiplied. Export formats expect straight alpha, and
	// the document background belongs in the exported canvas just as it does in
	// the PAINT output shader.
	const float bgA = ofClamp(doc.bgA, 0.0f, 1.0f);
	for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
	{
		const float a = pixels[i + 3] / 255.0f;
		const float outA = a + bgA * (1.0f - a);
		for (int c = 0; c < 3; ++c)
		{
			const float bg = c == 0 ? doc.bgR : c == 1 ? doc.bgG : doc.bgB;
			const float premult = pixels[i + c] / 255.0f +
				bg * bgA * (1.0f - a);
			pixels[i + c] = (unsigned char)std::lround(ofClamp(
				outA > 0.0005f ? premult / outA : 0.0f, 0.0f, 1.0f) * 255.0f);
		}
		pixels[i + 3] = (unsigned char)std::lround(outA * 255.0f);
	}
	return true;
}

bool JPbox_paint::exportCurrentPng(const std::string &path, float scale)
{
	ofPixels pixels;
	return renderCelPixels(currentCel(), pixels, scale) &&
		ofSaveImage(pixels, withExtension(path, ".png"));
}

bool JPbox_paint::exportPngSequence(const std::string &directory,
	const std::string &prefix, float scale, bool useRange)
{
	if (directory.empty()) return false;
	std::error_code error;
	std::filesystem::create_directories(directory, error);
	if (error) return false;
	int firstCel = 0, lastCel = 0;
	exportCelRange(useRange, firstCel, lastCel);
	const int digits = std::max(4, (int)ofToString(doc.frames.size()).size());
	for (int i = firstCel; i <= lastCel; ++i)
	{
		ofPixels pixels;
		if (!renderCelPixels(i, pixels, scale)) return false;
		std::ostringstream filename;
		// Numbered by the cel's own index, so a trimmed range still says which
		// cels it came from.
		filename << prefix << "_" << std::setw(digits) << std::setfill('0')
			<< (i + 1) << ".png";
		if (!ofSaveImage(pixels,
			(std::filesystem::path(directory) / filename.str()).string())) return false;
	}
	return true;
}

bool JPbox_paint::exportSpriteSheet(const std::string &path, float scale,
	bool useRange, int columns)
{
	if (doc.frames.empty()) return false;
	int firstCel = 0, lastCel = 0;
	exportCelRange(useRange, firstCel, lastCel);
	const int count = lastCel - firstCel + 1;
	if (count <= 0) return false;
	// Roughly square by default: a 12 frame walk cycle in one strip is 12 times
	// wider than tall and unusable as a texture.
	const int cols = columns > 0 ? columns :
		std::max(1, (int)std::ceil(std::sqrt((double)count)));
	const int rows = (count + cols - 1) / cols;

	ofPixels sheet;
	int cellW = 0, cellH = 0;
	for (int i = 0; i < count; ++i)
	{
		ofPixels pixels;
		if (!renderCelPixels(firstCel + i, pixels, scale)) return false;
		if (i == 0)
		{
			cellW = (int)pixels.getWidth();
			cellH = (int)pixels.getHeight();
			sheet.allocate(cellW * cols, cellH * rows, OF_PIXELS_RGBA);
			sheet.setColor(ofColor(0, 0, 0, 0));
		}
		if ((int)pixels.getWidth() != cellW ||
			(int)pixels.getHeight() != cellH) return false;
		// pasteInto rather than a per-pixel copy: same result, one memcpy per
		// row instead of a function call per channel.
		pixels.pasteInto(sheet, (i % cols) * cellW, (i / cols) * cellH);
	}
	return ofSaveImage(sheet, withExtension(path, ".png"));
}

bool JPbox_paint::exportGif(const std::string &path, float scale,
	bool useRange)
{
	if (doc.frames.empty()) return false;
	int firstCel = 0, lastCel = 0;
	exportCelRange(useRange, firstCel, lastCel);
	const std::string output = withExtension(path, ".gif");
	FIMULTIBITMAP *gif = FreeImage_OpenMultiBitmap(FIF_GIF, output.c_str(),
		TRUE, FALSE, TRUE, GIF_DEFAULT);
	if (gif == nullptr) return false;
	bool ok = true;
	for (int frame = firstCel; frame <= lastCel && ok; ++frame)
	{
		ofPixels pixels;
		if (!renderCelPixels(frame, pixels, scale)) { ok = false; break; }
		const int w = (int)pixels.getWidth(), h = (int)pixels.getHeight();
		FIBITMAP *rgb = FreeImage_Allocate(w, h, 24);
		if (rgb == nullptr) { ok = false; break; }
		// GIF has one transparent palette entry and no alpha, so transparent
		// pixels have to be painted a colour that is then declared transparent.
		// That colour must not appear in the drawing: a hard coded magenta
		// punched holes in anything drawn in magenta. So the frame is scanned
		// first and the key is a candidate the frame does not use.
		//
		// The candidates are deliberately odd values - nobody picks 3,252,1 off
		// a colour wheel - and one pass marks which of them the frame contains.
		struct KeyCandidate { BYTE r, g, b; };
		static const KeyCandidate candidates[] = {
			{255, 0, 255}, {1, 254, 3}, {254, 1, 253}, {3, 252, 1},
			{250, 2, 251}, {2, 3, 250}, {253, 254, 2}, {5, 250, 253}};
		constexpr int candidateCount =
			(int)(sizeof(candidates) / sizeof(candidates[0]));
		bool candidateUsed[candidateCount] = {false};
		bool hasTransparency = false;
		const unsigned char *src = pixels.getData();
		const std::size_t stride = pixels.getNumChannels();
		for (int y = 0; y < h; ++y)
		{
			// FreeImage bitmaps are bottom up; `pixels` is top down.
			BYTE *dst = FreeImage_GetScanLine(rgb, h - 1 - y);
			for (int x = 0; x < w; ++x)
			{
				const unsigned char *p =
					src + ((std::size_t)y * (std::size_t)w + x) * stride;
				const bool transparent = p[3] < 128;
				hasTransparency = hasTransparency || transparent;
				if (transparent) continue;
				dst[x * 3 + FI_RGBA_RED] = p[0];
				dst[x * 3 + FI_RGBA_GREEN] = p[1];
				dst[x * 3 + FI_RGBA_BLUE] = p[2];
				for (int c = 0; c < candidateCount; ++c)
					if (p[0] == candidates[c].r && p[1] == candidates[c].g &&
						p[2] == candidates[c].b) candidateUsed[c] = true;
			}
		}
		int key = -1;
		for (int c = 0; c < candidateCount && key < 0; ++c)
			if (!candidateUsed[c]) key = c;
		if (key < 0)
		{
			// Every candidate is in the artwork. Vanishingly unlikely, and the
			// honest failure is a hole in the drawing that says why.
			key = 0;
			ofLogWarning("JPbox_paint") << name <<
				": no unused colour left for GIF transparency; pixels drawn in "
				"the key colour will come out transparent";
		}
		if (hasTransparency)
		{
			// Second pass, now that the key is known: only the transparent
			// pixels need it, and the opaque ones were written above.
			for (int y = 0; y < h; ++y)
			{
				BYTE *dst = FreeImage_GetScanLine(rgb, h - 1 - y);
				for (int x = 0; x < w; ++x)
				{
					const unsigned char *p =
						src + ((std::size_t)y * (std::size_t)w + x) * stride;
					if (p[3] >= 128) continue;
					dst[x * 3 + FI_RGBA_RED] = candidates[key].r;
					dst[x * 3 + FI_RGBA_GREEN] = candidates[key].g;
					dst[x * 3 + FI_RGBA_BLUE] = candidates[key].b;
				}
			}
		}
		RGBQUAD reserved{};
		reserved.rgbRed = candidates[key].r;
		reserved.rgbGreen = candidates[key].g;
		reserved.rgbBlue = candidates[key].b;
		FIBITMAP *indexed = FreeImage_ColorQuantizeEx(rgb, FIQ_WUQUANT,
			256, hasTransparency ? 1 : 0, hasTransparency ? &reserved : nullptr);
		FreeImage_Unload(rgb);
		if (indexed == nullptr) { ok = false; break; }
		if (hasTransparency)
		{
			const RGBQUAD *palette = FreeImage_GetPalette(indexed);
			int transparentIndex = 0;
			for (int i = 0; i < 256; ++i)
				if (palette[i].rgbRed == candidates[key].r &&
					palette[i].rgbGreen == candidates[key].g &&
					palette[i].rgbBlue == candidates[key].b)
				{
					transparentIndex = i;
					break;
				}
			BYTE alpha[256];
			std::fill(std::begin(alpha), std::end(alpha), (BYTE)255);
			alpha[transparentIndex] = 0;
			FreeImage_SetTransparencyTable(indexed, alpha, 256);
		}
		const DWORD frameTime = (DWORD)std::max(10.0f,
			1000.0f * jp_paint::holdOf(doc.frames[(std::size_t)frame]) /
			std::max(0.1f, doc.fps));
		setGifMetadata(indexed, "FrameTime", FIDT_LONG,
			&frameTime, sizeof(frameTime));
		const BYTE disposal = 2;
		setGifMetadata(indexed, "DisposalMethod", FIDT_BYTE,
			&disposal, sizeof(disposal));
		if (frame == firstCel)
		{
			const DWORD loop = 0;
			setGifMetadata(indexed, "Loop", FIDT_LONG, &loop, sizeof(loop));
		}
		FreeImage_AppendPage(gif, indexed);
		FreeImage_Unload(indexed);
	}
	ok = FreeImage_CloseMultiBitmap(gif, GIF_DEFAULT) && ok;
	if (!ok) std::filesystem::remove(output);
	return ok;
}

// ----------------------------------------------------------------- playback

bool JPbox_paint::scrubEnabled() const
{
	// Every JPParameterGroup getter is non-const, so a const query has to cast.
	// Same shape as JPboxgroup::getMappingPanelPreviewRect.
	return const_cast<JPParameterGroup &>(parameters).getBoolValue(PARAM_SCRUB);
}

float JPbox_paint::opacityParam() const
{
	return ofClamp(
		const_cast<JPParameterGroup &>(parameters).getFloatValue(PARAM_OPACITY),
		0.0f, 1.0f);
}

void JPbox_paint::advancePlayback()
{
	const double now = ofGetElapsedTimef();
	float dt = (float)(now - lastClockTime);
	lastClockTime = now;
	// A first frame, a window drag or a shader compile can hand us a huge
	// delta. Stepping the animation by all of it would look like a glitch, and
	// the user cannot tell the difference if we drop the stall entirely.
	if (dt < 0.0f || dt > 0.5f) dt = 0.0f;

	const int ticks = jp_paint::tickCount(doc);
	if (scrubEnabled())
	{
		// The parameter is the single master in this mode - that is the whole
		// point of the toggle, and it is what lets audio or MIDI flip cels.
		const float normalized = ofClamp(
			parameters.getFloatValue(PARAM_PLAYHEAD), 0.0f, 1.0f);
		playheadTicks = normalized * (float)ticks;
		playback.position = normalized;
		return;
	}

	jp_paint::advance(doc, playback, playheadTicks, dt);
	// Mirror the truth back into the parameter so switching scrub on does not
	// teleport the animation. Guarded so an idle box is not writing a parameter
	// sixty times a second.
	if (std::abs(parameters.getFloatValue(PARAM_PLAYHEAD) -
		playback.position) > 0.001f)
	{
		parameters.setFloatValue(playback.position, PARAM_PLAYHEAD);
		parameters.setFloatLerpValue(playback.position, PARAM_PLAYHEAD);
	}
}

int JPbox_paint::currentCel() const
{
	if (doc.frames.empty()) return 0;
	const int last = (int)doc.frames.size() - 1;
	// Stopped and not scrubbing means the user is drawing, so the cel they
	// selected in the filmstrip wins over wherever the playhead happens to sit.
	if (!playback.playing && !scrubEnabled())
	{
		return std::clamp(doc.currentFrame, 0, last);
	}
	return jp_paint::celAtPlayhead(doc, playheadTicks);
}

void JPbox_paint::setCurrentCel(int index)
{
	if (doc.frames.empty()) return;
	doc.currentFrame = std::clamp(index, 0, (int)doc.frames.size() - 1);
	// Selecting a cel also parks the playhead on it, so the inspector timeline
	// and the filmstrip never disagree about where we are.
	int tick = 0;
	for (int i = 0; i < doc.currentFrame; ++i)
	{
		tick += jp_paint::holdOf(doc.frames[(std::size_t)i]);
	}
	playheadTicks = (float)tick;
	const int ticks = jp_paint::tickCount(doc);
	playback.position = ticks > 0 ? playheadTicks / (float)ticks : 0.0f;
}

// -------------------------------------------------------- JPMediaInspectable

std::string JPbox_paint::mediaStatus() const
{
	const int cels = (int)doc.frames.size();
	return ofToString(cels) + (cels == 1 ? " cel @ " : " cels @ ") +
		ofToString(doc.fps, 0) + " fps";
}

double JPbox_paint::mediaDurationSeconds() const
{
	const float fps = std::max(0.1f, doc.fps);
	return (double)jp_paint::tickCount(doc) / (double)fps;
}

int JPbox_paint::mediaFrameCount() const
{
	return jp_paint::tickCount(doc);
}

float JPbox_paint::mediaSteppedPosition(float normalized, int frames) const
{
	const int ticks = jp_paint::tickCount(doc);
	if (ticks <= 0) return 0.0f;
	return ofClamp(normalized + (float)frames / (float)ticks, 0.0f, 1.0f);
}

void JPbox_paint::mediaSeek(float normalized)
{
	const int ticks = jp_paint::tickCount(doc);
	const float clamped = ofClamp(normalized, 0.0f, 1.0f);
	playheadTicks = clamped * (float)ticks;
	playback.position = clamped;
	// Keep the filmstrip selection following the transport, so stopping the
	// playback leaves the user editing the cel they were looking at.
	doc.currentFrame = jp_paint::celAtPlayhead(doc, playheadTicks);
}

void JPbox_paint::mediaStep(int frames)
{
	mediaSeek(mediaSteppedPosition(playback.position, frames));
}

void JPbox_paint::mediaRestart()
{
	mediaSeek(playback.reverse ? playback.rangeOut : playback.rangeIn);
}

// ---------------------------------------------------------------- mutations

void JPbox_paint::recordEdit(const JPPaintEdit &edit)
{
	history.push(edit);
}

void JPbox_paint::commitStroke(const JPPaintStroke &stroke)
{
	if (stroke.points.empty()) return;
	jp_paint::clampCurrentLayer(doc);
	if (doc.layers[(std::size_t)doc.currentLayer].locked) return;
	const int cel = std::clamp(doc.currentFrame, 0,
		(int)doc.frames.size() - 1);
	const std::vector<JPPaintStroke> *list =
		jp_paint::strokeListFor(doc, cel, doc.currentLayer);
	if (list == nullptr) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::AddStroke;
	edit.frameIndex = cel;
	edit.layerIndex = doc.currentLayer;
	edit.strokeIndex = (int)list->size();
	edit.stroke = stroke;
	// Symmetry is expanded HERE rather than in the editor, so every way a stroke
	// can be committed - brush, shape, and a materialised bucket fill - mirrors
	// the same way, and one undo takes the whole group.
	jp_paint::appendSymmetryStrokes(stroke, doc.symmetry, edit.extraStrokes);
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
	std::vector<JPPaintStroke> appended;
	appended.reserve(1 + edit.extraStrokes.size());
	appended.push_back(stroke);
	appended.insert(appended.end(), edit.extraStrokes.begin(),
		edit.extraStrokes.end());
	appendStrokesToRasterCache(cel, doc.currentLayer, appended);
}

void JPbox_paint::clearCurrentLayer()
{
	jp_paint::clampCurrentLayer(doc);
	clearCel(doc.currentFrame, doc.currentLayer);
}

void JPbox_paint::clearCel(int frameIndex, int layerIndex)
{
	if (layerIndex < 0 || layerIndex >= (int)doc.layers.size() ||
		doc.layers[(std::size_t)layerIndex].locked) return;
	const std::vector<JPPaintStroke> *list =
		jp_paint::strokeListFor(doc, frameIndex, layerIndex);
	if (list == nullptr || list->empty()) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::ClearLayer;
	edit.frameIndex = frameIndex;
	edit.layerIndex = layerIndex;
	// The payload has to be captured BEFORE the edit is applied - it is what
	// undo puts back.
	edit.layer.sharedStrokes = *list;
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
}

void JPbox_paint::replaceStrokes(int frameIndex, int layerIndex, const std::vector<JPPaintStroke> &newStrokes)
{
	if (frameIndex < 0 || frameIndex >= (int)doc.frames.size()) return;
	if (layerIndex < 0 || layerIndex >= (int)doc.layers.size()) return;
	if (doc.layers[(std::size_t)layerIndex].locked) return;
	const std::vector<JPPaintStroke> *list =
		jp_paint::strokeListFor(doc, frameIndex, layerIndex);
	if (list == nullptr) return;

	JPPaintEdit edit;
	edit.kind = JPPaintEdit::ReplaceStrokes;
	edit.frameIndex = frameIndex;
	edit.layerIndex = layerIndex;
	edit.previousLayer.sharedStrokes = *list;
	edit.layer.sharedStrokes = newStrokes;
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
}

void JPbox_paint::toggleLayerBackground(int index)
{
	if (index < 0 || index >= (int)doc.layers.size()) return;
	JPPaintLayerInfo props = doc.layers[(std::size_t)index];
	props.background = !props.background;
	if (props.background && props.sharedStrokes.empty())
	{
		// Adopt what is on the CURRENT cel. Without this, marking a layer you
		// have just drawn on as the backdrop makes the drawing vanish - the
		// per-cel strokes are still there, but a background layer does not read
		// them. Turning the flag back off restores them untouched.
		const int cel = std::clamp(doc.currentFrame, 0,
			(int)doc.frames.size() - 1);
		if (index < (int)doc.frames[(std::size_t)cel].layers.size())
		{
			props.sharedStrokes =
				doc.frames[(std::size_t)cel].layers[(std::size_t)index].strokes;
		}
	}
	setLayerProps(index, props);
}

int JPbox_paint::currentLayer() const
{
	if (doc.layers.empty()) return 0;
	return std::clamp(doc.currentLayer, 0, (int)doc.layers.size() - 1);
}

void JPbox_paint::setCurrentLayer(int index)
{
	if (doc.layers.empty()) return;
	doc.currentLayer = std::clamp(index, 0, (int)doc.layers.size() - 1);
}

void JPbox_paint::addLayer()
{
	jp_paint::clampCurrentLayer(doc);
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::AddLayer;
	// Above the current one, which is where a new layer is expected to land.
	edit.layerIndex = doc.currentLayer + 1;
	// Mint FIRST, then name from the id that was minted. Naming from nextLayerId
	// before the mint gave every layer the same number - two rows both called
	// "Layer 1" - because the counter had not advanced yet.
	edit.layer = jp_paint::makeLayer(doc, "");
	edit.layer.name = "Layer " + ofToString(edit.layer.id + 1);
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
	setCurrentLayer(edit.layerIndex);
}

void JPbox_paint::duplicateLayer(int index)
{
	if (index < 0 || index >= (int)doc.layers.size()) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::AddLayer;
	edit.layerIndex = index + 1;
	edit.layer = doc.layers[(std::size_t)index];
	edit.layer.id = doc.nextLayerId++;
	edit.layer.name = (edit.layer.name.empty() ? "Layer" : edit.layer.name) +
		" copy";
	for (const JPPaintFrame &frame : doc.frames)
		edit.layerCels.push_back(index < (int)frame.layers.size() ?
			frame.layers[(std::size_t)index] : JPPaintLayer());
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
	setCurrentLayer(edit.layerIndex);
}

bool JPbox_paint::mergeLayerDown(int index)
{
	if (index <= 0 || index >= (int)doc.layers.size()) return false;
	const JPPaintLayerInfo &upper = doc.layers[(std::size_t)index];
	const JPPaintLayerInfo &lower = doc.layers[(std::size_t)(index - 1)];
	// Vector strokes cannot exactly encode a flattened multiply/screen layer.
	// Keep the operation lossless by allowing only ordinary visible layers.
	if (upper.locked || lower.locked || upper.background || lower.background ||
		!upper.visible || !lower.visible || upper.blendMode != 0 ||
		lower.blendMode != 0 || upper.opacity < 0.999f ||
		lower.opacity < 0.999f) return false;

	JPPaintEdit edit;
	edit.kind = JPPaintEdit::MergeLayerDown;
	edit.layerIndex = index;
	edit.layer = upper;
	edit.previousLayer = lower;
	edit.mergedLayer = lower;
	edit.mergedLayer.name = lower.name + " + " + upper.name;
	edit.mergedLayer.opacity = 1.0f;
	for (const JPPaintFrame &frame : doc.frames)
	{
		const JPPaintLayer lowerCel = frame.layers[(std::size_t)(index - 1)];
		const JPPaintLayer upperCel = frame.layers[(std::size_t)index];
		edit.previousLayerCels.push_back(lowerCel);
		edit.layerCels.push_back(upperCel);
		JPPaintLayer merged;
		merged.strokes.reserve(lowerCel.strokes.size() + upperCel.strokes.size());
		merged.strokes.insert(merged.strokes.end(), lowerCel.strokes.begin(),
			lowerCel.strokes.end());
		merged.strokes.insert(merged.strokes.end(), upperCel.strokes.begin(),
			upperCel.strokes.end());
		edit.mergedLayerCels.push_back(std::move(merged));
	}
	if (!jp_paint::applyEdit(doc, edit)) return false;
	recordEdit(edit);
	return true;
}

void JPbox_paint::deleteLayer(int index)
{
	if (doc.layers.size() <= 1) return;
	if (index < 0 || index >= (int)doc.layers.size()) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::DeleteLayer;
	edit.layerIndex = index;
	edit.layer = doc.layers[(std::size_t)index];
	// Captured in cel order, before the delete: this is what makes undo restore
	// the drawing rather than an empty layer.
	for (const JPPaintFrame &frame : doc.frames)
	{
		edit.layerCels.push_back(index < (int)frame.layers.size() ?
			frame.layers[(std::size_t)index] : JPPaintLayer());
	}
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
}

void JPbox_paint::moveLayer(int from, int to)
{
	if (from == to) return;
	const int count = (int)doc.layers.size();
	if (from < 0 || from >= count || to < 0 || to >= count) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::MoveLayer;
	edit.fromIndex = from;
	edit.toIndex = to;
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
	setCurrentLayer(to);
}

void JPbox_paint::setLayerProps(int index, const JPPaintLayerInfo &props)
{
	if (index < 0 || index >= (int)doc.layers.size()) return;
	const JPPaintLayerInfo &existing = doc.layers[(std::size_t)index];
	if (existing.name == props.name && existing.visible == props.visible &&
		existing.locked == props.locked &&
		existing.blendMode == props.blendMode &&
		existing.background == props.background &&
		existing.labelColor == props.labelColor &&
		existing.sharedStrokes.size() == props.sharedStrokes.size() &&
		std::abs(existing.opacity - props.opacity) < 0.001f)
	{
		// Nothing changed. Recording it anyway would spend an undo step on an
		// opacity drag that landed back where it started.
		return;
	}
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::SetLayerProps;
	edit.layerIndex = index;
	edit.previousLayer = existing;
	edit.layer = props;
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
}

void JPbox_paint::previewLayerOpacity(int index, float opacity)
{
	if (index < 0 || index >= (int)doc.layers.size()) return;
	const float clamped = ofClamp(opacity, 0.0f, 1.0f);
	if (std::abs(doc.layers[(std::size_t)index].opacity - clamped) < 0.0001f)
		return;
	doc.layers[(std::size_t)index].opacity = clamped;
	jp_paint::bumpAllFrames(doc);
}

void JPbox_paint::commitLayerOpacity(int index, float previousOpacity)
{
	if (index < 0 || index >= (int)doc.layers.size()) return;
	const JPPaintLayerInfo current = doc.layers[(std::size_t)index];
	const float before = ofClamp(previousOpacity, 0.0f, 1.0f);
	if (std::abs(current.opacity - before) < 0.001f) return;
	// The preview already put the document in its final state. Record the exact
	// before/after pair without applying it a second time.
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::SetLayerProps;
	edit.layerIndex = index;
	edit.layer = current;
	edit.previousLayer = current;
	edit.previousLayer.opacity = before;
	recordEdit(edit);
}

void JPbox_paint::addCel(bool duplicateCurrent)
{
	const int cel = std::clamp(doc.currentFrame, 0,
		(int)doc.frames.size() - 1);
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::AddFrame;
	edit.frameIndex = cel + 1;
	edit.frame = jp_paint::makeFrame(doc);
	if (duplicateCurrent)
	{
		// Every layer of the cel, not just the active one - a duplicated cel that
		// dropped its other layers would be a trap.
		edit.frame.layers = doc.frames[(std::size_t)cel].layers;
		edit.frame.hold = doc.frames[(std::size_t)cel].hold;
	}
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
	setCurrentCel(edit.frameIndex);
}

void JPbox_paint::deleteCel(int index)
{
	if (doc.frames.size() <= 1) return;
	if (index < 0 || index >= (int)doc.frames.size()) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::DeleteFrame;
	edit.frameIndex = index;
	edit.frame = doc.frames[(std::size_t)index];
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
	setCurrentCel(doc.currentFrame);
}

void JPbox_paint::moveCel(int from, int to)
{
	if (from == to) return;
	if (from < 0 || from >= (int)doc.frames.size()) return;
	if (to < 0 || to >= (int)doc.frames.size()) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::MoveFrame;
	edit.fromIndex = from;
	edit.toIndex = to;
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
	setCurrentCel(to);
}

void JPbox_paint::setCelHold(int index, int hold)
{
	if (index < 0 || index >= (int)doc.frames.size()) return;
	const int clamped = std::max(1, hold);
	if (doc.frames[(std::size_t)index].hold == clamped) return;
	JPPaintEdit edit;
	edit.kind = JPPaintEdit::SetHold;
	edit.frameIndex = index;
	edit.previousValue = doc.frames[(std::size_t)index].hold;
	edit.intValue = clamped;
	if (!jp_paint::applyEdit(doc, edit)) return;
	recordEdit(edit);
}

bool JPbox_paint::undo()
{
	if (!history.undo(doc)) return false;
	setCurrentCel(doc.currentFrame);
	return true;
}

bool JPbox_paint::redo()
{
	if (!history.redo(doc)) return false;
	setCurrentCel(doc.currentFrame);
	return true;
}

// -------------------------------------------------------------- persistence

void JPbox_paint::writeStrokes(ofXml &parent,
	const std::vector<JPPaintStroke> &strokes) const
{
	for (const JPPaintStroke &stroke : strokes)
	{
		if (stroke.points.empty()) continue;
		auto strokeNode = parent.appendChild("stroke");
		strokeNode.appendChild("color").set(
			ofToString(stroke.r) + " " + ofToString(stroke.g) + " " +
			ofToString(stroke.b) + " " + ofToString(stroke.a));
		strokeNode.appendChild("size").set(stroke.size);
		if (stroke.hardness < 0.999f)
			strokeNode.appendChild("hardness").set(stroke.hardness);
		strokeNode.appendChild("erase").set(stroke.erase);
		strokeNode.appendChild("tool").set(stroke.tool);
		if (stroke.tool == (int)JPPaintTool::Fill ||
			stroke.tool == (int)JPPaintTool::Region)
		{
			// Only a bucket has one, so only a bucket writes one. A Region no
			// longer needs it to render, but keeping it says what the region was
			// traced from.
			strokeNode.appendChild("tolerance").set(stroke.tolerance);
		}
		// A region's remaining contours, holes included. Same packed encoding as
		// the point run, so a traced 4K edge costs bytes rather than kilobytes.
		for (const std::vector<JPPaintPoint> &contour : stroke.contours)
		{
			if (contour.size() < 3) continue;
			strokeNode.appendChild("contour").appendChild("pts").set(
				jp_paint::packPoints(contour));
		}
		for (const JPPaintClip &clip : stroke.clips)
		{
			if (clip.points.size() < 3) continue;
			auto clipNode = strokeNode.appendChild("clip");
			clipNode.appendChild("inverted").set(clip.inverted);
			clipNode.appendChild("pts").set(jp_paint::packPoints(clip.points));
		}
		// One packed text run rather than an element per point. A five minute
		// doodle is tens of thousands of points, and verbose XML would spend over
		// a hundred bytes on each of them.
		strokeNode.appendChild("pts").set(jp_paint::packPoints(stroke.points));
	}
}

void JPbox_paint::readStrokes(const ofXml &parent,
	std::vector<JPPaintStroke> &out) const
{
	auto readFloatChild = [](const ofXml &node, const char *key, float fallback) {
		auto child = node.getChild(key);
		return child ? child.getFloatValue() : fallback;
	};
	for (const ofXml &strokeNode : parent.getChildren("stroke"))
	{
		JPPaintStroke stroke;
		auto color = strokeNode.getChild("color");
		if (color)
		{
			const std::vector<std::string> parts =
				ofSplitString(color.getValue(), " ", true, true);
			if (parts.size() >= 4)
			{
				stroke.r = ofToFloat(parts[0]);
				stroke.g = ofToFloat(parts[1]);
				stroke.b = ofToFloat(parts[2]);
				stroke.a = ofToFloat(parts[3]);
			}
		}
		stroke.size = ofClamp(readFloatChild(strokeNode, "size", 0.012f),
			0.0001f, 1.0f);
		stroke.hardness = ofClamp(
			readFloatChild(strokeNode, "hardness", 1.0f), 0.0f, 1.0f);
		auto erase = strokeNode.getChild("erase");
		stroke.erase = erase ? erase.getBoolValue() : false;
		auto tool = strokeNode.getChild("tool");
		stroke.tool = tool ? tool.getIntValue() : 0;
		stroke.tolerance = ofClamp(
			readFloatChild(strokeNode, "tolerance", 0.12f), 0.0f, 1.0f);
		auto points = strokeNode.getChild("pts");
		// A stroke whose point run is corrupt is dropped, not loaded as garbage
		// geometry.
		if (!points) continue;
		if (!jp_paint::unpackPoints(points.getValue(), stroke.points))
		{
			ofLogWarning("JPbox_paint") << name <<
				": dropping a stroke with a malformed point run";
			continue;
		}
		if (stroke.points.empty()) continue;
		for (const ofXml &contourNode : strokeNode.getChildren("contour"))
		{
			auto contourPoints = contourNode.getChild("pts");
			if (!contourPoints) continue;
			std::vector<JPPaintPoint> contour;
			if (!jp_paint::unpackPoints(contourPoints.getValue(), contour) ||
				contour.size() < 3) continue;
			stroke.contours.push_back(std::move(contour));
		}
		for (const ofXml &clipNode : strokeNode.getChildren("clip"))
		{
			// Same ceiling a selection can build. A savefile written by a
			// future version with a higher cap loads its first
			// kMaxStrokeClips clips rather than being rejected.
			if (stroke.clips.size() >= jp_paint::kMaxStrokeClips) break;
			auto clipPoints = clipNode.getChild("pts");
			if (!clipPoints) continue;
			JPPaintClip clip;
			auto inverted = clipNode.getChild("inverted");
			clip.inverted = inverted ? inverted.getBoolValue() : false;
			if (!jp_paint::unpackPoints(clipPoints.getValue(), clip.points) ||
				clip.points.size() < 3) continue;
			stroke.clips.push_back(std::move(clip));
		}
		out.push_back(stroke);
	}
}

void JPbox_paint::saveCustomState(ofXml &boxNode) const
{
	auto root = boxNode.appendChild("paint");
	root.appendChild("fps").set(doc.fps);
	root.appendChild("onionBefore").set(doc.onionBefore);
	root.appendChild("onionAfter").set(doc.onionAfter);
	root.appendChild("onionOpacity").set(doc.onionOpacity);
	root.appendChild("currentFrame").set(doc.currentFrame);
	root.appendChild("currentLayer").set(doc.currentLayer);
	root.appendChild("background").set(
		ofToString(doc.bgR) + " " + ofToString(doc.bgG) + " " +
		 ofToString(doc.bgB) + " " + ofToString(doc.bgA));
	root.appendChild("canvasSize").set(ofToString(doc.canvasWidth) + " " +
		ofToString(doc.canvasHeight));
	root.appendChild("symmetry").set(doc.symmetry);

	// The layer stack, described once. <layer> under <layers> is the layer
	// itself; <layer> under <frame> is that cel's strokes for it. Different
	// parents, so the two can never be confused.
	auto layersNode = root.appendChild("layers");
	for (const JPPaintLayerInfo &info : doc.layers)
	{
		auto layerNode = layersNode.appendChild("layer");
		layerNode.appendChild("id").set(info.id);
		layerNode.appendChild("name").set(info.name);
		layerNode.appendChild("visible").set(info.visible);
		if (info.locked) layerNode.appendChild("locked").set(true);
		layerNode.appendChild("opacity").set(info.opacity);
		if (info.blendMode != 0)
			layerNode.appendChild("blendMode").set(info.blendMode);
		if (info.labelColor >= 0)
			layerNode.appendChild("labelColor").set(info.labelColor);
		layerNode.appendChild("background").set(info.background);
		// Written whether or not the flag is set, so toggling background off
		// after a reload still restores what was drawn on it.
		writeStrokes(layerNode, info.sharedStrokes);
	}

	for (const JPPaintFrame &frame : doc.frames)
	{
		auto frameNode = root.appendChild("frame");
		frameNode.appendChild("hold").set(frame.hold);
		frameNode.appendChild("id").set(frame.id);
		for (const JPPaintLayer &layer : frame.layers)
		{
			// Positional, parallel to <layers>. Always emitted, even when empty,
			// or the arity would have to be guessed on load.
			auto layerNode = frameNode.appendChild("layer");
			writeStrokes(layerNode, layer.strokes);
		}
	}

	// The transport half of the state, in the same <media> node every other
	// media box writes, so loop mode and rate mean the same thing everywhere.
	jp_media::save(boxNode, playback);
}

void JPbox_paint::loadCustomState(const ofXml &boxNode)
{
	jp_media::load(boxNode, playback);

	auto root = boxNode.getChild("paint");
	// A box saved before this node existed, or a hand-edited file: keep the
	// blank document setup() built rather than failing the load.
	if (!root) return;

	// Every field is individually optional so an older savefile keeps working.
	auto readFloat = [](const ofXml &parent, const char *key, float fallback) {
		auto node = parent.getChild(key);
		return node ? node.getFloatValue() : fallback;
	};
	auto readInt = [](const ofXml &parent, const char *key, int fallback) {
		auto node = parent.getChild(key);
		return node ? node.getIntValue() : fallback;
	};

	doc.fps = ofClamp(readFloat(root, "fps", 12.0f), 0.1f, 120.0f);
	// Capped at 3 each, not because more would be unreadable but because the
	// raster cache holds 8 cels: 3 + current + 3 fits with a slot to spare,
	// and anything beyond that would thrash the LRU every single frame.
	doc.onionBefore = (int)ofClamp((float)readInt(root, "onionBefore", 1), 0.0f, 3.0f);
	doc.onionAfter = (int)ofClamp((float)readInt(root, "onionAfter", 1), 0.0f, 3.0f);
	doc.onionOpacity = ofClamp(readFloat(root, "onionOpacity", 0.35f), 0.0f, 1.0f);

	auto background = root.getChild("background");
	if (background)
	{
		const std::vector<std::string> parts =
			ofSplitString(background.getValue(), " ", true, true);
		if (parts.size() >= 4)
		{
			doc.bgR = ofToFloat(parts[0]);
			doc.bgG = ofToFloat(parts[1]);
			doc.bgB = ofToFloat(parts[2]);
			doc.bgA = ofToFloat(parts[3]);
		}
	}
	auto canvasSize = root.getChild("canvasSize");
	if (canvasSize)
	{
		const std::vector<std::string> parts = ofSplitString(canvasSize.getValue(), " ", true, true);
		if (parts.size() >= 2)
		{
			doc.canvasWidth = std::clamp(ofToInt(parts[0]), 16, 8192);
			doc.canvasHeight = std::clamp(ofToInt(parts[1]), 16, 8192);
		}
	}
	auto symmetry = root.getChild("symmetry");
	// Absent in documents written before symmetry existed, which is off.
	doc.symmetry = symmetry ? std::clamp(symmetry.getIntValue(), 0, 3) : 0;

	// The layer stack. A file written before layers existed has no <layers>
	// block at all, which is exactly when a single default layer is right.
	std::vector<JPPaintLayerInfo> loadedLayers;
	int highestLayerId = 0;
	auto layersNode = root.getChild("layers");
	if (layersNode)
	{
		for (const ofXml &layerNode : layersNode.getChildren("layer"))
		{
			JPPaintLayerInfo info;
			info.id = readInt(layerNode, "id", (int)loadedLayers.size());
			auto nameNode = layerNode.getChild("name");
			info.name = nameNode ? nameNode.getValue() :
				("Layer " + ofToString((int)loadedLayers.size() + 1));
			auto visible = layerNode.getChild("visible");
			info.visible = visible ? visible.getBoolValue() : true;
			auto locked = layerNode.getChild("locked");
			info.locked = locked ? locked.getBoolValue() : false;
			info.opacity = ofClamp(readFloat(layerNode, "opacity", 1.0f),
				0.0f, 1.0f);
			info.blendMode = std::clamp(
				readInt(layerNode, "blendMode", 0), 0, 3);
			info.labelColor = std::clamp(readInt(layerNode, "labelColor", -1), -1, 7);
			auto background = layerNode.getChild("background");
			info.background = background ? background.getBoolValue() : false;
			readStrokes(layerNode, info.sharedStrokes);
			highestLayerId = std::max(highestLayerId, info.id);
			loadedLayers.push_back(info);
		}
	}
	if (!loadedLayers.empty())
	{
		doc.layers = loadedLayers;
	}
	doc.nextLayerId = highestLayerId + 1;

	std::vector<JPPaintFrame> loaded;
	int highestId = 0;
	for (const ofXml &frameNode : root.getChildren("frame"))
	{
		JPPaintFrame frame;
		frame.layers.clear();
		frame.hold = std::max(1, readInt(frameNode, "hold", 1));
		frame.id = readInt(frameNode, "id", (int)loaded.size());
		highestId = std::max(highestId, frame.id);
		for (const ofXml &layerNode : frameNode.getChildren("layer"))
		{
			JPPaintLayer layer;
			readStrokes(layerNode, layer.strokes);
			frame.layers.push_back(layer);
		}
		if (frame.layers.empty())
		{
			// A savefile from before layers: the strokes hang straight off
			// <frame>. Everything it holds becomes layer 0, which is what a
			// one-layer document is.
			JPPaintLayer layer;
			readStrokes(frameNode, layer.strokes);
			frame.layers.push_back(layer);
		}
		loaded.push_back(frame);
	}

	if (!loaded.empty())
	{
		doc.frames = loaded;
	}
	// Ids come from the file, so the next minted one has to clear all of them
	// or two cels could share a raster cache slot.
	doc.nextFrameId = highestId + 1;
	doc.currentFrame = readInt(root, "currentFrame", 0);
	doc.currentLayer = readInt(root, "currentLayer", 0);
	// Squares up any cel that arrived with a different layer count - a legacy
	// file mixed with a <layers> block, or a hand edit.
	jp_paint::syncLayerArity(doc);
	jp_paint::clampCurrentFrame(doc);
	jp_paint::clampCurrentLayer(doc);

	// Rasters cache pixels derived from strokes that just changed wholesale,
	// and the undo history describes a document that no longer exists.
	invalidateRasters();
	history.clear();
	setCurrentCel(doc.currentFrame);
}

void JPbox_paint::copyCustomStateFrom(const JPbox *source)
{
	const JPbox_paint *other = dynamic_cast<const JPbox_paint *>(source);
	if (other == nullptr) return;
	// By value. Derived GPU resources are NEVER copied, only invalidated - the
	// same discipline JPbox_shader::copyCustomStateFrom follows, and the reason
	// JPPaintDocument holds no framebuffers.
	doc = other->doc;
	playback = other->playback;
	playheadTicks = other->playheadTicks;
	invalidateRasters();
	// History is per-box: inheriting another box's undo stack would let a user
	// undo their way into a document this box never had.
	history.clear();
	liveStrokeActive = false;
	liveStroke = JPPaintStroke();
}
