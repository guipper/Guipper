#include "jp_quick_image.h"
#include "jp_media.h"
#include <FreeImage.h>
#include <chrono>
#include <mutex>
#include <unordered_set>

namespace
{
	int gifMetadataInt(FIBITMAP *bitmap, const char *key, int fallback)
	{
		FITAG *tag = nullptr;
		if (!FreeImage_GetMetadata(FIMD_ANIMATION, bitmap, key, &tag) ||
			tag == nullptr) return fallback;
		const void *value = FreeImage_GetTagValue(tag);
		if (value == nullptr) return fallback;
		switch (FreeImage_GetTagType(tag))
		{
			case FIDT_BYTE: return *(const BYTE *)value;
			case FIDT_SHORT: return *(const WORD *)value;
			case FIDT_LONG: return (int)*(const DWORD *)value;
			default: return fallback;
		}
	}

	// FreeImage's one-time initialisation is not itself thread-safe, and both
	// decoders below run on workers. Both call this FIRST, from the main thread,
	// so the init has already happened by the time any worker touches FreeImage.
	// An empty path is enough: ofInitFreeImage() is the first line of OF's
	// loader, well before it cares that there is no file.
	void warmFreeImageOnMainThread()
	{
		static std::once_flag once;
		std::call_once(once, []()
		{
			ofPixels ignored;
			ofLoadImage(ignored, std::string());
		});
	}

	std::string canonicalKey(const std::string &input)
	{
		std::string path = ofToDataPath(input, true);
		try { path = std::filesystem::weakly_canonical(path).string(); }
		catch (...) {}
		try
		{
			path += ":" + std::to_string((long long)
				std::filesystem::last_write_time(path).time_since_epoch().count());
		}
		catch (...) {}
		return path;
	}

	bool sameMedia(const JPMediaState &a, const JPMediaState &b)
	{
		return a.fitMode == b.fitMode && a.loopMode == b.loopMode &&
			a.rangeIn == b.rangeIn &&
			a.rangeOut == b.rangeOut && a.rate == b.rate &&
			a.playing == b.playing && a.reverse == b.reverse &&
			a.userReverse == b.userReverse;
	}
}

bool operator==(const JPQuickImageLayerState &a,
	const JPQuickImageLayerState &b)
{
	return a.id == b.id && a.path == b.path && a.name == b.name &&
		a.visible == b.visible && a.opacity == b.opacity &&
		a.center == b.center && a.size == b.size &&
		a.rotationDegrees == b.rotationDegrees && a.sourceUid == b.sourceUid &&
		a.followChain == b.followChain && sameMedia(a.media, b.media);
}

bool operator==(const JPQuickImageStackState &a,
	const JPQuickImageStackState &b)
{
	return a.nextId == b.nextId && a.layers == b.layers;
}

namespace
{
	jp_quick_image::SourceResolver &sourceResolver()
	{
		static jp_quick_image::SourceResolver resolver;
		return resolver;
	}
}

void jp_quick_image::setSourceResolver(SourceResolver resolver)
{
	sourceResolver() = std::move(resolver);
}

const ofTexture *jp_quick_image::resolveSource(
	const JPQuickImageLayerState &layer)
{
	if (layer.sourceUid.empty() || !sourceResolver()) return nullptr;
	return sourceResolver()(layer);
}

std::shared_future<std::shared_ptr<const JPQuickGifData>>
jp_quick_image::requestGif(const std::string &input)
{
	using Result = std::shared_ptr<const JPQuickGifData>;
	warmFreeImageOnMainThread();
	static std::mutex mutex;
	static std::unordered_map<std::string, std::shared_future<Result>> cache;
	const std::string key = canonicalKey(input);
	std::lock_guard<std::mutex> lock(mutex);
	auto found = cache.find(key);
	if (found != cache.end()) return found->second;
	const std::string path = ofToDataPath(input, true);
	auto future = std::async(std::launch::async, [path]() -> Result
	{
		auto result = std::make_shared<JPQuickGifData>();
		FIMULTIBITMAP *multi = FreeImage_OpenMultiBitmap(FIF_GIF,
			path.c_str(), FALSE, TRUE, TRUE, GIF_LOAD256);
		if (multi == nullptr) return {};
		const int pages = FreeImage_GetPageCount(multi);
		ofPixels canvas, restore;
		int canvasW = 0, canvasH = 0;
		for (int i = 0; i < pages; ++i)
		{
			FIBITMAP *page = FreeImage_LockPage(multi, i);
			if (!page) continue;
			FIBITMAP *rgba = FreeImage_ConvertTo32Bits(page);
			const int pw = FreeImage_GetWidth(rgba);
			const int ph = FreeImage_GetHeight(rgba);
			const int left = gifMetadataInt(page, "FrameLeft", 0);
			const int top = gifMetadataInt(page, "FrameTop", 0);
			canvasW = std::max(canvasW, left + pw);
			canvasH = std::max(canvasH, top + ph);
			if (!canvas.isAllocated())
			{
				canvas.allocate(canvasW, canvasH, OF_PIXELS_RGBA);
				canvas.set(0);
			}
			else if (canvas.getWidth() < canvasW || canvas.getHeight() < canvasH)
			{
				ofPixels grown;
				grown.allocate(canvasW, canvasH, OF_PIXELS_RGBA);
				grown.set(0); canvas.pasteInto(grown, 0, 0); canvas.swap(grown);
			}
			const int disposal = gifMetadataInt(page, "DisposalMethod", 0);
			if (disposal == 3) restore = canvas;
			const BYTE *bits = FreeImage_GetBits(rgba);
			const int pitch = FreeImage_GetPitch(rgba);
			for (int y = 0; y < ph; ++y) for (int x = 0; x < pw; ++x)
			{
				const BYTE *src = bits + (ph - 1 - y) * pitch + x * 4;
				ofColor color(src[FI_RGBA_RED], src[FI_RGBA_GREEN],
					src[FI_RGBA_BLUE], src[FI_RGBA_ALPHA]);
				if (color.a > 0) canvas.setColor(left + x, top + y, color);
			}
			result->frames.push_back(canvas);
			const int ms = std::max(10, gifMetadataInt(page, "FrameTime", 100));
			result->duration += ms / 1000.0;
			result->ends.push_back(result->duration);
			if (disposal == 2)
				for (int y = 0; y < ph; ++y) for (int x = 0; x < pw; ++x)
					canvas.setColor(left + x, top + y, ofColor(0, 0));
			else if (disposal == 3 && restore.isAllocated()) canvas = restore;
			FreeImage_Unload(rgba);
			FreeImage_UnlockPage(multi, page, FALSE);
		}
		FreeImage_CloseMultiBitmap(multi, 0);
		return result->frames.empty() ? Result{} : result;
	}).share();
	cache[key] = future;
	return future;
}

std::shared_future<std::shared_ptr<const ofPixels>>
jp_quick_image::requestImage(const std::string &input)
{
	using Result = std::shared_ptr<const ofPixels>;
	warmFreeImageOnMainThread();
	// Deliberately NOT cached, unlike requestGif. The caller copies the pixels
	// into its own ofImage and drops the future, so a cache would be the only
	// thing still holding them - and for a 6000x4000 PNG that is ~96 MB kept
	// alive for the rest of the session to save a decode that happens once.
	const std::string path = ofToDataPath(input, true);
	return std::async(std::launch::async, [path]() -> Result
	{
		auto pixels = std::make_shared<ofPixels>();
		// ofLoadImage into ofPixels is pure FreeImage: no GL, so it is safe
		// here. The ofImage overload is NOT - it uploads a texture.
		if (!ofLoadImage(*pixels, path)) return {};
		return pixels;
	}).share();
}

JPQuickImageLayerState jp_quick_image::makeBoxLayer(
	JPQuickImageStackState &stack, const std::string &sourceUid,
	const std::string &name)
{
	JPQuickImageLayerState layer;
	layer.id = std::max<uint64_t>(1, stack.nextId++);
	layer.name = name;
	layer.sourceUid = sourceUid;
	layer.followChain = true;
	layer.size.set(1.0f, 1.0f);
	layer.media.loopMode = JPMediaLoopMode::Loop;
	layer.media.playing = true;
	layer.media.muted = true;
	return layer;
}

void jp_quick_image::normalize(JPQuickImageStackState &stack)
{
	std::unordered_set<uint64_t> ids;
	uint64_t next = std::max<uint64_t>(1, stack.nextId);
	for (auto &layer : stack.layers)
	{
		if (layer.id == 0 || ids.count(layer.id)) layer.id = next++;
		ids.insert(layer.id); next = std::max(next, layer.id + 1);
		layer.opacity = ofClamp(layer.opacity, 0.0f, 1.0f);
		layer.size.x = std::max(0.0001f, std::abs(layer.size.x));
		layer.size.y = std::max(0.0001f, std::abs(layer.size.y));
		jp_media::normalize(layer.media);
	}
	stack.nextId = next;
}

ofVec2f jp_quick_image::rotate(const ofVec2f &point, float degrees)
{
	const float radians = ofDegToRad(degrees);
	const float c = std::cos(radians), s = std::sin(radians);
	return ofVec2f(point.x * c - point.y * s,
		point.x * s + point.y * c);
}

std::array<ofVec2f, 4> jp_quick_image::corners(
	const JPQuickImageLayerState &layer)
{
	const ofVec2f half = layer.size * 0.5f;
	return {layer.center + rotate(ofVec2f(-half.x, -half.y), layer.rotationDegrees),
		layer.center + rotate(ofVec2f(half.x, -half.y), layer.rotationDegrees),
		layer.center + rotate(ofVec2f(half.x, half.y), layer.rotationDegrees),
		layer.center + rotate(ofVec2f(-half.x, half.y), layer.rotationDegrees)};
}

bool jp_quick_image::hitTest(const JPQuickImageLayerState &layer,
	const ofVec2f &uv)
{
	const ofVec2f local = rotate(uv - layer.center, -layer.rotationDegrees);
	return std::abs(local.x) <= layer.size.x * 0.5f &&
		std::abs(local.y) <= layer.size.y * 0.5f;
}

void jp_quick_image::saveStack(ofXml &parent,
	const JPQuickImageStackState &stack)
{
	auto root = parent.appendChild("quickImages");
	root.appendChild("nextId").set(ofToString(stack.nextId));
	for (const auto &layer : stack.layers)
	{
		auto node = root.appendChild("image");
		node.appendChild("id").set(ofToString(layer.id));
		node.appendChild("path").set(layer.path);
		node.appendChild("name").set(layer.name);
		node.appendChild("visible").set(layer.visible);
		node.appendChild("opacity").set(layer.opacity);
		node.appendChild("centerX").set(layer.center.x);
		node.appendChild("centerY").set(layer.center.y);
		node.appendChild("width").set(layer.size.x);
		node.appendChild("height").set(layer.size.y);
		node.appendChild("rotation").set(layer.rotationDegrees);
		if (!layer.sourceUid.empty())
		{
			node.appendChild("sourceUid").set(layer.sourceUid);
			node.appendChild("followChain").set(layer.followChain);
		}
		jp_media::save(node, layer.media);
	}
}

void jp_quick_image::loadStack(const ofXml &parent,
	JPQuickImageStackState &stack)
{
	auto parseId = [](const std::string &value, uint64_t fallback) {
		if (value.empty()) return fallback;
		try { return (uint64_t)std::stoull(value); }
		catch (...) { return fallback; }
	};
	stack = JPQuickImageStackState();
	auto root = parent.getChild("quickImages");
	if (!root) return;
	auto next = root.getChild("nextId");
	if (next) stack.nextId = std::max<uint64_t>(1,
		parseId(next.getValue(), 1));
	for (auto node : root.getChildren("image"))
	{
		JPQuickImageLayerState layer;
		auto id = node.getChild("id");
		if (id) layer.id = parseId(id.getValue(), 0);
		layer.path = node.getChild("path").getValue();
		layer.name = node.getChild("name").getValue();
		if (layer.name.empty()) layer.name = ofFilePath::getBaseName(layer.path);
		auto visible = node.getChild("visible");
		layer.visible = !visible || visible.getBoolValue();
		auto opacity = node.getChild("opacity");
		if (opacity) layer.opacity = opacity.getFloatValue();
		layer.center.set(node.getChild("centerX").getFloatValue(),
			node.getChild("centerY").getFloatValue());
		layer.size.set(node.getChild("width").getFloatValue(),
			node.getChild("height").getFloatValue());
		auto rotation = node.getChild("rotation");
		if (rotation) layer.rotationDegrees = rotation.getFloatValue();
		auto sourceUid = node.getChild("sourceUid");
		if (sourceUid) layer.sourceUid = sourceUid.getValue();
		auto followChain = node.getChild("followChain");
		layer.followChain = !followChain || followChain.getBoolValue();
		jp_media::load(node, layer.media);
		stack.layers.push_back(layer);
	}
	normalize(stack);
}

void JPQuickImageRenderer::draw(JPQuickImageStackState &stack,
	float width, float height, int scope)
{
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofEnableAlphaBlending();
	for (auto &layer : stack.layers)
	{
		if (!layer.visible || layer.opacity <= 0.0f) continue;
		const ofTexture *source = jp_quick_image::resolveSource(layer);
		if (source == nullptr) continue;
		ofPushMatrix();
		ofTranslate(layer.center.x * width, layer.center.y * height);
		ofRotateDeg(layer.rotationDegrees);
		ofSetColor(255, (int)std::round(layer.opacity * 255.0f));
		source->draw(-layer.size.x * width * 0.5f,
			-layer.size.y * height * 0.5f,
			layer.size.x * width, layer.size.y * height);
		ofPopMatrix();
	}
	ofPopStyle();
}
