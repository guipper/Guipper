#pragma once

#include "ofMain.h"
#include "jp_media_state.h"
#include <array>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

struct JPQuickGifData
{
	std::vector<ofPixels> frames;
	std::vector<double> ends;
	double duration = 0.0;
};

struct JPQuickImageLayerState
{
	uint64_t id = 0;
	std::string path;
	std::string name;
	bool visible = true;
	float opacity = 1.0f;
	ofVec2f center = ofVec2f(0.5f, 0.5f);
	ofVec2f size = ofVec2f(0.5f, 0.5f);
	float rotationDegrees = 0.0f;
	// Uid of the graph box that feeds this layer. The box is the SOURCE, not a
	// copy: patch a shader after it and the layer keeps its own placement and
	// simply samples whatever that box currently renders.
	//
	// `path` is the FALLBACK, and it is empty for a layer built from a box that
	// has no file of its own - a shader, a camera. Deleting the source box of a
	// file-backed layer degrades to the still image; for a box-backed layer the
	// layer simply stops drawing.
	std::string sourceUid;
	// Follow the source box DOWNSTREAM to the last box of its chain, which is
	// what makes "patch an effect after it and the layer shows the effect"
	// work. Turned off, the layer stays pinned to the box it names - the only
	// way to be unambiguous when a chain forks.
	bool followChain = true;
	JPMediaState media;
};

struct JPQuickImageStackState
{
	uint64_t nextId = 1;
	std::vector<JPQuickImageLayerState> layers;
};

bool operator==(const JPQuickImageLayerState &a,
	const JPQuickImageLayerState &b);
bool operator==(const JPQuickImageStackState &a,
	const JPQuickImageStackState &b);

namespace jp_quick_image
{
	std::shared_future<std::shared_ptr<const JPQuickGifData>>
		requestGif(const std::string &path);
	// A layer fed by a graph box rather than a file. Full frame, because that
	// is what the box already renders; the user scales it down from there.
	JPQuickImageLayerState makeBoxLayer(JPQuickImageStackState &stack,
		const std::string &sourceUid, const std::string &name);
	void normalize(JPQuickImageStackState &stack);
	ofVec2f rotate(const ofVec2f &point, float degrees);
	std::array<ofVec2f, 4> corners(const JPQuickImageLayerState &layer);
	bool hitTest(const JPQuickImageLayerState &layer, const ofVec2f &uv);
	void saveStack(ofXml &parent, const JPQuickImageStackState &stack);
	void loadStack(const ofXml &parent, JPQuickImageStackState &stack);

	// Resolving a source needs the box tree, which lives in JPboxgroup; the
	// renderers that need the answer live in JPbox_shader and have no way back
	// to it. One installed hook beats threading a group pointer through both.
	//
	// Takes the whole layer, not just the uid: whether to follow the chain is a
	// property of the layer, so two layers naming the same box can legitimately
	// resolve to different textures.
	using SourceResolver =
		std::function<const ofTexture *(const JPQuickImageLayerState &layer)>;
	void setSourceResolver(SourceResolver resolver);
	const ofTexture *resolveSource(const JPQuickImageLayerState &layer);
}

// Draws a stack. Every layer is fed by a box in the graph - resolved through
// the hook above - so this owns no images, no decoding and no playback: that
// all belongs to the box, which has its own transport and its own inspector.
class JPQuickImageRenderer
{
public:
	void draw(JPQuickImageStackState &stack, float width, float height,
		int scope = 0);
	void clear() {}
};
