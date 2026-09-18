#pragma once

#include "ofMain.h"
#include "jp_app_paths.h"

namespace jp_font
{
    // One family, with weight assigned by role instead of per-screen choices.
    inline constexpr const char *bodyFace = "font/Overpass-Regular.ttf";
    inline constexpr const char *emphasisFace = "font/Overpass-SemiBold.ttf";

	inline bool loadLatin(ofTrueTypeFont &font, const std::string &path,
		int size)
	{
		// UI assets follow the executable, including already migrated profiles.
        const auto &bundle = jp::AppPaths::current().bundle;
        const auto source = bundle.empty() ? std::filesystem::path(ofToDataPath(path, true)) : bundle / path;
        ofTrueTypeFontSettings settings(source, size);
		settings.addRanges(ofAlphabet::Latin);
		if (!font.load(settings)) return false;
		// OF defaults to nearest sampling at UI sizes (<=20 pt). Fractional
		// positions from centering/scrolling then produce uneven glyph edges.
		// OF exposes only a const atlas getter, even on this mutable font.
		auto &atlas = const_cast<ofTexture &>(font.getFontTexture());
		atlas.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
		return true;
	}
}
