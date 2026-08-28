#pragma once

#include <sstream>
#include <string>
#include <vector>

// Greedy word wrap, shared by every panel that lays out prose.
//
// It takes a MEASURER rather than an ofTrueTypeFont on purpose. The wrapping is
// pure arithmetic over string widths, and binding it to a font meant it could
// only run inside a live GL context with a loaded typeface - so the one piece of
// real logic in the help layout had no way to be tested. A template rather than
// a std::function so the call still inlines.
namespace jp_textwrap
{
	// `measure(s)` returns the rendered width of s. Never returns an empty
	// vector: a blank string wraps to one blank line, so callers can always
	// count lines to get a height.
	//
	// A single word wider than maxWidth is emitted on its own line rather than
	// being broken mid-word - breaking it would split a shortcut like
	// "Ctrl/Cmd+Shift+G" across two rows.
	template <typename Measure>
	inline std::vector<std::string> wrap(const Measure &measure,
		const std::string &text, float maxWidth)
	{
		std::vector<std::string> out;
		std::string line, word;
		std::istringstream words(text);
		while (words >> word)
		{
			const std::string candidate =
				line.empty() ? word : line + " " + word;
			if (!line.empty() && measure(candidate) > maxWidth)
			{
				out.push_back(line);
				line = word;
			}
			else
			{
				line = candidate;
			}
		}
		if (!line.empty()) out.push_back(line);
		if (out.empty()) out.push_back("");
		return out;
	}
}
