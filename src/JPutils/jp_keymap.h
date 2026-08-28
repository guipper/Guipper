#pragma once

#include <cstddef>
#include <string>
#include <vector>

// The physical keys behind a help row, and the QWERTY board they are drawn on.
//
// This is DATA, not a reading of the help text. The `keys` column of a help row
// is prose for a human - it holds "Ctrl/Cmd+Z" but also "Bucket", "Live
// outputs", "/setactiverender" and "uniform vec4 audio_bands;". Forty-five per
// cent of the rows carry no key at all, and the "/" in the ones that do means
// five different things depending on the row: an alternative (b / e), a
// platform alias (Ctrl/Cmd+G), a modifier distributed over a list
// (Ctrl/Cmd+C / X / V), a button paired with a key (CUE / z), or an ordinary
// word (Arrows / Shift).
//
// Deriving the map from that string would be the same mistake jp_help_content.h
// was refactored to remove - its header says outright that nothing is ever
// inferred from the wording - and it would fail silently and visually, on the
// screen a beginner is reading. So a row states its keys or it stays off the
// board.
//
// Free of openFrameworks so tests/ can compile it on its own.
namespace jp_keymap
{
	// One physical key on the board. `id` is what an annotation names it by and
	// is matched case-insensitively; `cap` is what is printed on it.
	struct Key
	{
		const char *id = "";
		const char *cap = "";
		// Width in units of one letter key, so a Shift or a Space can be wide
		// without the row needing its own layout code.
		float units = 1.0f;
	};

	// A physical-key row. Rows are drawn top to bottom, keys left to right.
	inline const std::vector<std::vector<Key>> &board()
	{
		static const std::vector<std::vector<Key>> rows = {
			{{"1","1"},{"2","2"},{"3","3"},{"4","4"},{"5","5"},{"6","6"},
			 {"7","7"},{"8","8"},{"9","9"},{"0","0"},{"-","-"},{"=","="},
			 {"backspace","BACKSP",2.0f}},
			{{"tab","TAB",1.5f},{"q","Q"},{"w","W"},{"e","E"},{"r","R"},
			 {"t","T"},{"y","Y"},{"u","U"},{"i","I"},{"o","O"},{"p","P"},
			 {"[","["},{"]","]"}},
			{{"caps","CAPS",1.8f},{"a","A"},{"s","S"},{"d","D"},{"f","F"},
			 {"g","G"},{"h","H"},{"j","J"},{"k","K"},{"l","L"},{";",";"},
			 {"enter","ENTER",2.2f}},
			{{"shift","SHIFT",2.4f},{"z","Z"},{"x","X"},{"c","C"},{"v","V"},
			 {"b","B"},{"n","N"},{"m","M"},{",",","},{".","."},{"/","/"},
			 {"up","UP"}},
			{{"ctrl","CTRL",1.6f},{"alt","ALT",1.4f},
			 {"space","SPACE",5.0f},{"cmd","CMD",1.4f},{"esc","ESC",1.3f},
			 {"del","DEL",1.3f},{"home","HOM",1.3f},{"end","END",1.3f},
			 // Two letters, because a 1-unit cap is not wide enough for "LEFT"
			 // at this size and an unlabelled cap is worse than an abbreviated
			 // one. UP sits on the row above, where it already fits.
			 {"left","LT"},{"down","DN"},{"right","RT"}},
		};
		return rows;
	}

	// True when the board has a key with this id.
	inline bool isKnownKey(const std::string &id)
	{
		for (const std::vector<Key> &row : board())
			for (const Key &key : row)
				if (id == key.id) return true;
		return false;
	}

	// Splits an annotation into the ids it names.
	//
	// The grammar is deliberately tiny, because the annotation is written for
	// this parser rather than for a reader: ids separated by spaces, commas or
	// plus signs, lower case. "ctrl+shift+g" lights CTRL, SHIFT and G; "b e"
	// lights both letters. Anything that is not a known key is DROPPED, and the
	// test that walks the table turns such a typo into a failure instead of a
	// key that quietly never lights up.
	inline std::vector<std::string> parse(const std::string &annotation)
	{
		std::vector<std::string> out;
		std::string token;
		auto flush = [&]()
		{
			if (token.empty()) return;
			if (isKnownKey(token))
			{
				bool already = false;
				for (const std::string &s : out) if (s == token) already = true;
				if (!already) out.push_back(token);
			}
			token.clear();
		};
		for (char c : annotation)
		{
			if (c == '+' || c == ' ' || c == ',') { flush(); continue; }
			token += (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
		}
		flush();
		return out;
	}

	// Every id an annotation names that the board does NOT have. Empty for a
	// well-formed annotation; the table test asserts exactly this.
	inline std::vector<std::string> unknownIds(const std::string &annotation)
	{
		std::vector<std::string> bad;
		std::string token;
		auto flush = [&]()
		{
			if (!token.empty() && !isKnownKey(token)) bad.push_back(token);
			token.clear();
		};
		for (char c : annotation)
		{
			if (c == '+' || c == ' ' || c == ',') { flush(); continue; }
			token += (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
		}
		flush();
		return bad;
	}
}
