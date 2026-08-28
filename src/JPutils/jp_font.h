#pragma once

#include "ofMain.h"

namespace jp_font
{
	inline bool loadLatin(ofTrueTypeFont &font, const std::string &path,
		int size)
	{
		ofTrueTypeFontSettings settings(path, size);
		settings.addRanges(ofAlphabet::Latin);
		return font.load(settings);
	}
}
