#pragma once

#include <algorithm>
#include <cmath>
#include <string>

enum class JPMediaFitMode { Custom = 0, Fit, Fill, Stretch, Original };
enum class JPMediaLoopMode { Once = 0, Loop, PingPong };

struct JPMediaState
{
	// Original is the default: a newly dropped image or video shows at its own
	// pixel size, centred, rather than being scaled to the canvas. Files that
	// already carry a <media> node keep whatever fit they were saved with, and
	// pre-media compositions still fall back to the legacy `strech` bool, so
	// this only affects newly created boxes.
	JPMediaFitMode fitMode = JPMediaFitMode::Original;
	JPMediaLoopMode loopMode = JPMediaLoopMode::Loop;
	float position = 0.0f;
	float rangeIn = 0.0f;
	float rangeOut = 1.0f;
	float rate = 1.0f;
	float volume = 1.0f;
	bool playing = true;
	bool reverse = false;
	// The direction the USER chose, as distinct from the direction playback is
	// currently running in.
	//
	// PingPong OWNS `reverse` - applyBoundary flips it at every boundary - so
	// after a bounce or two `reverse` no longer records intent, it records where
	// the bounce happened to leave it. Leaving PingPong therefore has to restore
	// an intent that `reverse` cannot supply: without this, switching from
	// PingPong to Loop mid-bounce loops backwards forever, and switching to Once
	// snaps to the in-point and stops dead.
	bool userReverse = false;
	bool muted = true;
};

// Everything that can change the pixels a media box renders into its FBO.
//
// Unlike a shader - whose output moves every frame because time, audio and
// feedback feed it - an image or a video frame is STABLE between source
// changes. A static PNG composited with an unchanged transform produces a
// bit-identical result every frame, so re-running that full render-resolution
// pass sixty times a second buys nothing. A 30fps video in a 60fps app is in
// the same position for every second frame.
//
// Comparing this signature against the last rendered one is what makes skipping
// safe: identical inputs, identical output. It is deliberately a value type
// with no engine dependency so media_core_tests can exercise it directly.
struct JPMediaRenderSignature
{
	// Nothing has been rendered yet, so the first pass always runs. Also the
	// reset used when a box is cloned or its source is swapped out.
	bool valid = false;
	int fitMode = 0;
	float scaleX = 0.0f;
	float scaleY = 0.0f;
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	float scaleRatio = 0.0f;
	// Target dimensions are part of the signature so a render-resolution change
	// repaints instead of leaving a stale FBO at the old size.
	float targetW = 0.0f;
	float targetH = 0.0f;
	float sourceW = 0.0f;
	float sourceH = 0.0f;
	// Bumped by whatever produces new pixels: an image finishing its load, a GIF
	// advancing a frame, a video decoder reporting a new frame.
	unsigned long long sourceGeneration = 0;

	bool matches(const JPMediaRenderSignature &other) const
	{
		// An invalid signature never matches, including against another invalid
		// one - "neither of us has rendered" is not evidence the FBO is current.
		if (!valid || !other.valid) return false;
		return fitMode == other.fitMode &&
			sourceGeneration == other.sourceGeneration &&
			sameValue(scaleX, other.scaleX) &&
			sameValue(scaleY, other.scaleY) &&
			sameValue(offsetX, other.offsetX) &&
			sameValue(offsetY, other.offsetY) &&
			sameValue(scaleRatio, other.scaleRatio) &&
			sameValue(targetW, other.targetW) &&
			sameValue(targetH, other.targetH) &&
			sameValue(sourceW, other.sourceW) &&
			sameValue(sourceH, other.sourceH);
	}

private:
	// Exact comparison would be defensible here - these values are copied, not
	// recomputed - but parameters arrive through lerping and audio shaping, so
	// a value can wobble in the last bits while the on-screen result is
	// identical. The threshold is far below one pixel at any sane resolution.
	static bool sameValue(float a, float b)
	{
		return std::fabs(a - b) <= 1.0e-6f;
	}
};

namespace jp_media
{
	// The uniform-zoom parameter, by any of its spellings.
	//
	// The hardcoded boxes name it "scale ratio"; a shader cannot, because a
	// GLSL identifier has no spaces, so shaders spell it "scaleratio". One
	// predicate so the inspector ordering, the MIDI slot ordering and the
	// shader uniform parser cannot disagree about what counts.
	inline bool isScaleRatioParameter(const std::string &name)
	{
		return name == "scale ratio" || name == "scaleratio" ||
			name == "scale_ratio";
	}

	// Display rank for the transform parameters, so they read the same way in
	// every box: scale ratio, scalex, scaley, offsetx, offsety, then everything
	// else in whatever order the box declares.
	//
	// Ordering happens HERE, at display time, and never by reordering the
	// parameter array. Saved <param> blocks load positionally, so moving a
	// uniform in a .frag or an addFloatValue in a box would shift every
	// composition already using it. transform.frag declares scaley before
	// scalex; this is what makes the panel show scalex first anyway.
	//
	// kUnranked sorts after everything, and the same values drive both the
	// inspector rows and the MIDI bind slots - those two disagreeing is the bug
	// this exists to prevent.
	constexpr int kUnrankedTransformParameter = 1000;

	inline int transformParameterRank(const std::string &name)
	{
		if (isScaleRatioParameter(name)) return 0;
		if (name == "scalex") return 1;
		if (name == "scaley") return 2;
		if (name == "offsetx") return 3;
		if (name == "offsety") return 4;
		return kUnrankedTransformParameter;
	}

	inline bool isRankedTransformParameter(const std::string &name)
	{
		return transformParameterRank(name) != kUnrankedTransformParameter;
	}

	// The transform the source boxes have always used: camera, NDI and Spout.
	//
	// It is NOT the media-box transform in jp_media.h. There, scale is relative
	// to a fitted rect; here scale maps directly onto a fraction of the canvas,
	// which is what those three boxes' saved compositions were authored against
	// and must keep meaning.
	//
	// This lived inline and identical in all three, so the uniform-zoom
	// parameter would have had to be added three times. Kept here rather than
	// in jp_media.h because this header has no engine dependency, which is what
	// lets media_core_tests compile it directly.
	struct JPMediaRect
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
	};

	inline JPMediaRect legacyTransformRect(float scaleX, float scaleY,
		float offsetX, float offsetY, float ratio,
		float targetW, float targetH)
	{
		// Zoom about the centre FIRST, then derive the offsets from the size
		// that produces. Same order as transformedRect: doing it the other way
		// would stop a zoomed source travelling fully off canvas, because the
		// offset range is a function of the drawn size.
		const float zoom = std::clamp(ratio, 0.1f, 4.0f);
		const float w = scaleX * targetW * zoom;
		const float h = scaleY * targetH * zoom;
		// At 0.5 the source is centred; at the extremes it clears the canvas
		// edge completely, which is what the original ofMap range expressed.
		const float dx = (offsetX * 2.0f - 1.0f) * (targetW * 0.5f + w * 0.5f);
		const float dy = (offsetY * 2.0f - 1.0f) * (targetH * 0.5f + h * 0.5f);
		JPMediaRect rect;
		rect.width = w;
		rect.height = h;
		rect.x = targetW * 0.5f - w * 0.5f + dx;
		rect.y = targetH * 0.5f - h * 0.5f + dy;
		return rect;
	}

	inline void normalize(JPMediaState &s)
	{
		s.rangeIn = std::clamp(s.rangeIn, 0.0f, 1.0f);
		s.rangeOut = std::clamp(s.rangeOut, s.rangeIn, 1.0f);
		s.position = std::clamp(s.position, s.rangeIn, s.rangeOut);
		s.rate = std::clamp(s.rate, 0.25f, 4.0f);
		s.volume = std::clamp(s.volume, 0.0f, 1.0f);
	}

	inline void captureRangeIn(JPMediaState &s)
	{
		s.rangeIn = std::clamp(s.position, 0.0f, 1.0f);
		if (s.rangeIn > s.rangeOut) s.rangeOut = s.rangeIn;
		normalize(s);
	}

	inline void captureRangeOut(JPMediaState &s)
	{
		s.rangeOut = std::clamp(s.position, 0.0f, 1.0f);
		if (s.rangeOut < s.rangeIn) s.rangeIn = s.rangeOut;
		normalize(s);
	}

	// The ONLY places that should write `reverse` from a user action. Both keep
	// `userReverse` in step, which is what makes leaving PingPong recoverable.
	inline void toggleDirection(JPMediaState &s)
	{
		s.reverse = !s.reverse;
		s.userReverse = s.reverse;
	}

	inline void setLoopMode(JPMediaState &s, JPMediaLoopMode mode)
	{
		s.loopMode = mode;
		// Restores the user's direction, discarding whatever PingPong's last
		// bounce left behind.
		s.reverse = s.userReverse;
	}

	inline void cycleLoopMode(JPMediaState &s)
	{
		setLoopMode(s, (JPMediaLoopMode)(((int)s.loopMode + 1) % 3));
	}

	inline bool applyBoundary(JPMediaState &s, float &position)
	{
		const bool hit = s.reverse ? position <= s.rangeIn : position >= s.rangeOut;
		if (!hit) return false;
		if (s.loopMode == JPMediaLoopMode::Once)
		{
			position = s.reverse ? s.rangeIn : s.rangeOut;
			s.playing = false;
		}
		else if (s.loopMode == JPMediaLoopMode::Loop)
			position = s.reverse ? s.rangeOut : s.rangeIn;
		else
		{
			// Reflect the overshoot into the range. Seeking to the exact endpoint
			// on every boundary frame can leave some video backends parked there
			// for several updates before their negative rate takes effect.
			const bool wasReverse = s.reverse;
			const float span = s.rangeOut-s.rangeIn;
			if (span <= 0.000001f)
			{
				position = s.rangeIn;
			}
			else
			{
				const float overshoot = std::max(0.0f, wasReverse ?
					s.rangeIn-position : position-s.rangeOut);
				const long long segments = (long long)std::floor(overshoot/span);
				const float remainder = std::fmod(overshoot, span);
				const bool even = (segments%2)==0;
				if (!wasReverse)
				{
					position = even ? s.rangeOut-remainder : s.rangeIn+remainder;
					s.reverse = even;
				}
				else
				{
					position = even ? s.rangeIn+remainder : s.rangeOut-remainder;
					s.reverse = !even;
				}
			}
		}
		return true;
	}
}
