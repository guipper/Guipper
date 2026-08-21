#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "jp_media.h"
#include "jp_paint_doc.h"
#include "defines.h"

#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"

// A hand-drawn canvas with Procreate style cel animation.
//
// Every other source box in the program renders something that came from
// outside - a shader file, an image, a camera, an NDI stream. This one renders
// what the user drew, and it is the only box whose contents live entirely in
// the savefile.
//
// The document (jp_paint_doc.h) is the source of truth and is deliberately
// vector: strokes, not pixels. That is what makes undo cheap, keeps a drawing
// resolution independent when the render size changes under it, and lets a
// whole animation fit in the session XML instead of a folder of PNGs. Pixels
// exist only as a CACHE, rebuilt from strokes whenever a cel is edited.
//
// The one texture inlet is a tracing reference. It is drawn in the editor and
// deliberately NOT composited into the output - same as Procreate's reference
// layer, and it keeps the box a clean source rather than a paint-over effect.
class JPbox_paint : public JPbox, public JPMediaInspectable
{
public:
	JPbox_paint();
	~JPbox_paint();

	// Parameter slots. APPEND ONLY - JPbox_preset::setup still loads group
	// children positionally, so reordering these silently shifts every value in
	// every saved group.
	//
	// Deliberately NOT here: play, rate and position. The media inspector
	// already draws real transport UI for those and writes JPMediaState
	// directly; duplicating them as parameters would give two masters for one
	// value and no way to say which won.
	enum ParamSlot
	{
		PARAM_OPACITY = 0,
		PARAM_PLAYHEAD,
		PARAM_SCRUB
	};

	// METODOS HEREDADOS :
	void reload();
	void setup(string _dir, string _name);
	void update();
	void updateFBO();
	void draw();
	void clear();
	void setfbohandler_nodepos();
	void saveCustomState(ofXml &boxNode) const;
	void loadCustomState(const ofXml &boxNode);
	void copyCustomStateFrom(const JPbox *source);
	void setPos(float _x, float _y)
	{
		JPdragobject::setPos(_x, _y);
	}

	// JPMediaInspectable. Implementing this is what gives the box a full
	// transport in the inspector - restart, prev, play, next, direction, loop
	// mode, speed, a scrubbable timeline and numeric IN/OUT fields - with no
	// new UI code, because JPboxgroup finds it by dynamic_cast alone.
	JPMediaState &mediaState() { return playback; }
	const JPMediaState &mediaState() const { return playback; }
	// Always: a canvas can always have a cel added, and the IN/OUT range is
	// meaningful even on a single cel. Reporting false would collapse the whole
	// transport card, which is the only playback UI the box has until the
	// editor panel is open.
	bool mediaPlayable() const { return true; }
	bool mediaHasAudio() const { return false; }
	// A canvas IS the render size, so there is nothing to fit it to.
	bool mediaHasFit() const { return false; }
	bool mediaReady() const { return true; }
	std::string mediaStatus() const;
	double mediaDurationSeconds() const;
	int mediaFrameCount() const;
	float mediaSteppedPosition(float normalized, int frames) const;
	void mediaSeek(float normalized);
	void mediaStep(int frames);
	void mediaRestart();

	// ---------------------------------------------------------- the document
	//
	// The editor panel in JPboxgroup_paint.cpp drives all of this. Every
	// mutator records an undoable edit and bumps the touched cel's revision,
	// which is the single mechanism that invalidates the raster cache - so undo
	// and the cache cannot disagree about what is stale.
	JPPaintDocument &document() { return doc; }
	const JPPaintDocument &document() const { return doc; }

	int currentCel() const;
	void setCurrentCel(int index);

	void commitStroke(const JPPaintStroke &stroke);
	// Floods the ACTIVE LAYER once from a normalized seed, traces the region the
	// flood covered and commits it as ordinary geometry. This is what a bucket
	// click does now: a Region stroke can be selected, moved and transformed,
	// costs no readback when the cel is rebuilt, and does not change under the
	// user when something below it is edited. False when the seed is off canvas,
	// the layer is locked or the flood covered nothing.
	bool materializeFill(float u, float v, float tolerance,
		const ofFloatColor &color);
	void clearCurrentLayer();
	// Clears one cel of the grid. Same edit as clearCurrentLayer with the indices
	// passed in rather than read from the document - what the timeline's
	// right-click needs.
	void clearCel(int frameIndex, int layerIndex);
	void replaceStrokes(int frameIndex, int layerIndex, const std::vector<JPPaintStroke> &newStrokes);
	int currentLayer() const;
	void setCurrentLayer(int index);
	void addLayer();
	void duplicateLayer(int index);
	bool mergeLayerDown(int index);
	void deleteLayer(int index);
	void moveLayer(int from, int to);
	// Name / visible / opacity / background, as one undoable step.
	void setLayerProps(int index, const JPPaintLayerInfo &props);
	void previewLayerOpacity(int index, float opacity);
	void commitLayerOpacity(int index, float previousOpacity);
	// Flips the background flag, adopting the current cel's strokes the first
	// time it goes on. Marking a layer as the backdrop AFTER drawing it is the
	// normal order of operations.
	void toggleLayerBackground(int index);
	void addCel(bool duplicateCurrent);
	void deleteCel(int index);
	void moveCel(int from, int to);
	void setCelHold(int index, int hold);

	bool undo();
	bool redo();
	bool canUndo() const { return history.canUndo(); }
	bool canRedo() const { return history.canRedo(); }

	// The stroke being drawn right now. It is composited on top at render time
	// rather than committed, so dragging the mouse never re-rasterizes the cel.
	JPPaintStroke liveStroke;
	bool liveStrokeActive = false;

	// Draws a cel into whatever framebuffer is currently bound, at the given
	// size. The editor calls this for onion ghosts; updateFBO calls it for the
	// output. Public because the panel lives in another translation unit.
	void drawCel(int index, float drawX, float drawY, float drawWidth,
		float drawHeight, const ofColor &tint);
	void drawCelPreview(int index, int layerIndex,
		const std::vector<JPPaintStroke> &overrideStrokes,
		float drawX, float drawY, float drawWidth, float drawHeight,
		const ofColor &tint);
	// Builds a cel's raster if it is not cached. Callers that draw inside a
	// scissor or a bound framebuffer MUST warm every cel they need first: a
	// rebuild binds its own framebuffer, and an active scissor box would clip
	// the rebuild to the panel's rectangle.
	void warmCel(int index);
	// Draws a cel at filmstrip size. Backed by its own small cache, NOT the
	// render-resolution one: the filmstrip shows a dozen cels at once, and
	// rebuilding full size rasters for all of them every frame would cost more
	// than the rest of the editor put together.
	void warmThumb(int index);
	void drawThumb(int index, const ofRectangle &bounds);
	// Reads back the pixel the canvas is showing at this normalized point and
	// hands it over as straight alpha. False when the point is off canvas.
	//
	// Samples the CURRENT cel's raster, which is what the editor draws, so the
	// eyedropper picks the colour the user can actually see - not the box's
	// output, which has the opacity parameter folded in.
	bool sampleColor(float u, float v, ofFloatColor &out);

	// The reference inlet's framebuffer, or nullptr when nothing usable is wired
	// in.
	//
	// Returns the PRODUCER'S framebuffer by pointer. It must never go through
	// JPFbohandlerGroup::getFboPointer, which returns an ofFbo BY VALUE: taking
	// the address of anything inside that copy leaves a dangling pointer the
	// moment the statement ends, and the copy's destructor releases GL handles
	// the producing box is still using.
	ofFbo *referenceFbo();
	// Renders a stroke's geometry into the bound framebuffer, mapped into the
	// given rect. Draws with the CURRENT ofColor rather than the stroke's own,
	// because the editor has to preview an eraser - which has no colour to
	// show - as something visible.
	void drawStrokePreview(const JPPaintStroke &stroke, float drawX, float drawY,
		float drawWidth, float drawHeight);

	float canvasAspect() const;
	int canvasPixelWidth() const;
	int canvasPixelHeight() const;
	// Keeps the graph-facing FBO at the project resolution while rebuilding the
	// internal stroke cache at this PAINT's native resolution.
	void setCanvasSize(int width, int height);
	void setCanvasBackground(float r, float g, float b, float a);
	// scale multiplies the canvas resolution the frames are rasterized at, so a
	// document can be exported bigger or smaller than it is drawn without
	// changing the document. useRange limits the frames to the transport's
	// IN/OUT selection, which is the range the box plays.
	bool exportCurrentPng(const std::string &path, float scale = 1.0f);
	bool exportPngSequence(const std::string &directory,
		const std::string &prefix = "paint_frame", float scale = 1.0f,
		bool useRange = false);
	bool exportGif(const std::string &path, float scale = 1.0f,
		bool useRange = false);
	// Every frame in one image, laid out left to right and top to bottom.
	// columns of 0 picks a roughly square sheet.
	bool exportSpriteSheet(const std::string &path, float scale = 1.0f,
		bool useRange = false, int columns = 0);

private:
	JPPaintDocument doc;
	JPMediaState playback;
	float playheadTicks = 0.0f;
	double lastClockTime = 0.0;
	JPPaintUndoRing history;

	// A cel's pixels, keyed by the cel's stable id plus its edit counter.
	// Keying on the INDEX would hand a cached raster to the wrong cel the first
	// time somebody reordered the filmstrip.
	struct RasterSlot
	{
		ofFbo fbo;
		int frameId = -1;
		unsigned long long revision = 0;
		unsigned long long lastUsed = 0;
		bool valid = false;
	};
	// One framebuffer per cel is not affordable: at 1080p that is 8.3MB each,
	// so a hundred cel animation would be 830MB of VRAM for pixels that are all
	// reproducible from a few kilobytes of strokes. Eight covers the current
	// cel plus its onion neighbours with room to spare, and a miss costs one
	// stroke replay.
	static const int kRasterSlots = 8;
	std::vector<RasterSlot> rasters;
	unsigned long long rasterClock = 0;
	// Scratch for compositing a translucent stroke in one pass. See paintStroke.
	ofFbo scratch;
	// One layer, rendered alone before it is composited at its opacity. Distinct
	// from `scratch`, which is per STROKE and is used while this is bound.
	ofFbo layerScratch;
	// Premultiplied cel plus live stroke, when there is a live stroke to merge.
	// Allocated lazily, because most of the time there is not one.
	ofFbo composite;
	// Selection transforms render here without touching the document cache.
	ofFbo selectionPreview;
	// Cels are stored PREMULTIPLIED (see beginPremultipliedBlend). The output
	// FBO must not be: every other box in the graph produces straight alpha and
	// downstream shaders sample .rgb directly. This converts in one pass, and
	// composites the canvas background while it is there.
	ofShader unpremultiply;
	bool unpremultiplyReady = false;
	bool unpremultiplyTried = false;
	void ensureUnpremultiplyShader();
	void compositeToOutput(ofFbo &source, float opacity);
	// scale of 1 reads the cached raster; anything else rasterizes the cel into
	// `exportBuffer` at the scaled size, because the cache is the canvas size
	// and stretching pixels is not the same as drawing them bigger.
	bool renderCelPixels(int celIndex, ofPixels &pixels, float scale = 1.0f);
	ofFbo exportBuffer;
	// The cel range an export covers: the whole document, or the transport's
	// IN/OUT selection.
	void exportCelRange(bool useRange, int &firstCel, int &lastCel) const;

	// One per cel, grown as the document grows. At 96px wide a two hundred cel
	// animation is a few megabytes, so this needs no eviction policy at all -
	// which is the whole reason it is separate from the LRU above.
	struct ThumbSlot
	{
		ofFbo fbo;
		int frameId = -1;
		unsigned long long revision = 0;
		bool valid = false;
	};
	std::vector<ThumbSlot> thumbs;

	ofFbo &ensureRaster(int celIndex);
	// Draws a just-appended stroke group straight onto the cached raster, so a
	// commit does not cost a replay of everything already on the cel. Takes the
	// whole group at once because one edit bumps the cel's revision once: a
	// second call would no longer recognise the slot as one revision behind.
	void appendStrokesToRasterCache(int celIndex, int layerIndex,
		const std::vector<JPPaintStroke> &strokes);
	void rebuildRaster(ofFbo &target, const JPPaintDocument &document,
		int celIndex, int overrideLayerIndex = -1,
		const std::vector<JPPaintStroke> *overrideStrokes = nullptr);
	void paintStrokeList(ofFbo &target,
		const std::vector<JPPaintStroke> &strokes);
	void ensureLayerScratch(float width, float height);
	void writeStrokes(ofXml &parent,
		const std::vector<JPPaintStroke> &strokes) const;
	void readStrokes(const ofXml &parent,
		std::vector<JPPaintStroke> &out) const;
	void ensureScratch(float width, float height);
	void invalidateRasters();
	void paintStroke(ofFbo &target, const JPPaintStroke &stroke);
	// The stroke's outline as a path, with the winding rule its tool needs: a
	// traced region's contours nest, a lasso's overlaps unite.
	ofPath strokeAreaPath(const JPPaintStroke &stroke,
		float width, float height) const;
	// A bucket fill: reads the target back, floods from the stored seed, and
	// composites the colour through the resulting mask. Reused buffers rather
	// than locals so a rebuild does not churn a full-resolution allocation per
	// fill.
	void paintFillStroke(ofFbo &target, const JPPaintStroke &stroke);
	ofPixels fillReadback;
	std::vector<std::uint8_t> fillMask;
	ofPixels fillPixels;
	ofTexture fillTexture;
	void renderStrokeGeometry(const JPPaintStroke &stroke,
		float width, float height);
	bool beginStrokeClip(const JPPaintStroke &stroke, float width, float height);
	void endStrokeClip();

	void advancePlayback();
	void recordEdit(const JPPaintEdit &edit);
	float opacityParam() const;
	bool scrubEnabled() const;
};
