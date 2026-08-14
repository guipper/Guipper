#pragma once

#include "ofMain.h"
#include "jp_media_state.h"
#include <algorithm>
#include <cctype>

class JPMediaInspectable
{
public:
	virtual ~JPMediaInspectable() = default;
	virtual JPMediaState &mediaState() = 0;
	virtual const JPMediaState &mediaState() const = 0;
	virtual bool mediaPlayable() const = 0;
	virtual bool mediaHasAudio() const = 0;
	virtual bool mediaReady() const = 0;
	virtual std::string mediaStatus() const = 0;
	virtual double mediaDurationSeconds() const = 0;
	virtual int mediaFrameCount() const = 0;
	virtual float mediaSteppedPosition(float normalized, int frames) const = 0;
	virtual void mediaSeek(float normalized) = 0;
	virtual void mediaStep(int frames) = 0;
	virtual void mediaRestart() = 0;
};

namespace jp_media
{
	inline std::string extension(const std::string &path)
	{
		std::string ext = ofFilePath::getFileExt(path);
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
		return ext;
	}
	inline bool isGif(const std::string &path) { return extension(path) == "gif"; }
	inline bool isImage(const std::string &path)
	{
		const std::string ext = extension(path);
		return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif";
	}
	inline bool isVideo(const std::string &path)
	{
		const std::string ext = extension(path);
		return ext == "mov" || ext == "mkv" || ext == "mp4" || ext == "flv" ||
			ext == "vob" || ext == "avi";
	}
	inline std::string stem(const std::string &path)
	{
		return ofFilePath::getBaseName(path);
	}
	inline void save(ofXml &box, const JPMediaState &s)
	{
		auto media = box.appendChild("media");
		media.appendChild("transformversion").set(2);
		media.appendChild("fitmode").set((int)s.fitMode);
		media.appendChild("loopmode").set((int)s.loopMode);
		media.appendChild("position").set(s.position);
		media.appendChild("rangein").set(s.rangeIn);
		media.appendChild("rangeout").set(s.rangeOut);
		media.appendChild("rate").set(s.rate);
		media.appendChild("playing").set(s.playing);
		media.appendChild("reverse").set(s.reverse);
		media.appendChild("muted").set(s.muted);
		media.appendChild("volume").set(s.volume);
	}
	inline int transformVersion(const ofXml &box)
	{
		auto media = box.getChild("media");
		if (!media) return 1;
		auto version = media.getChild("transformversion");
		return version ? version.getIntValue() : 1;
	}
	inline bool load(const ofXml &box, JPMediaState &s)
	{
		auto media = box.getChild("media");
		if (!media) return false;
		auto setFloat = [&](const char *key, float &v) { auto n = media.getChild(key); if (n) v = n.getFloatValue(); };
		auto fit = media.getChild("fitmode"); if (fit) s.fitMode = (JPMediaFitMode)ofClamp(fit.getIntValue(), 0, 4);
		auto loop = media.getChild("loopmode"); if (loop) s.loopMode = (JPMediaLoopMode)ofClamp(loop.getIntValue(), 0, 2);
		setFloat("position", s.position); setFloat("rangein", s.rangeIn);
		setFloat("rangeout", s.rangeOut); setFloat("rate", s.rate); setFloat("volume", s.volume);
		auto playing = media.getChild("playing"); if (playing) s.playing = playing.getBoolValue();
		auto reverse = media.getChild("reverse"); if (reverse) s.reverse = reverse.getBoolValue();
		auto muted = media.getChild("muted"); if (muted) s.muted = muted.getBoolValue();
		normalize(s);
		return true;
	}
	inline ofRectangle fittedRect(float sourceW, float sourceH,
		float targetW, float targetH, JPMediaFitMode mode)
	{
		if (sourceW <= 0 || sourceH <= 0) return {};
		if (mode == JPMediaFitMode::Stretch || mode == JPMediaFitMode::Custom)
			return {0, 0, targetW, targetH};
		if (mode == JPMediaFitMode::Original)
			return {(targetW-sourceW)*0.5f, (targetH-sourceH)*0.5f, sourceW, sourceH};
		const float scale = mode == JPMediaFitMode::Fill ?
			std::max(targetW/sourceW, targetH/sourceH) :
			std::min(targetW/sourceW, targetH/sourceH);
		const float w = sourceW*scale, h = sourceH*scale;
		return {(targetW-w)*0.5f, (targetH-h)*0.5f, w, h};
	}
	inline ofRectangle transformedRect(float sourceW, float sourceH,
		float targetW, float targetH, JPMediaFitMode mode,
		float scaleX, float scaleY, float offsetX, float offsetY,
		float scaleRatio, float neutralScaleX, float customBaseScaleX)
	{
		neutralScaleX = std::max(0.0001f, neutralScaleX);
		const float ratio = ofClamp(scaleRatio, 0.1f, 4.0f);
		ofRectangle rect;
		if (mode == JPMediaFitMode::Custom)
		{
			const float customScaleX=customBaseScaleX*scaleX/neutralScaleX;
			rect.set((targetW-targetW*customScaleX)*0.5f,
				(targetH-targetH*scaleY)*0.5f,
				targetW*customScaleX, targetH*scaleY);
		}
		else
		{
			rect = fittedRect(sourceW, sourceH, targetW, targetH, mode);
			rect.scaleFromCenter(scaleX/neutralScaleX,
				scaleY/0.5f);
		}
		rect.scaleFromCenter(ratio, ratio);
		// At 0.5 the media is centered. At the endpoints it moves completely
		// beyond the corresponding canvas edge, matching the legacy controls.
		rect.translate((offsetX-0.5f)*(targetW+rect.width),
			(offsetY-0.5f)*(targetH+rect.height));
		return rect;
	}
}
