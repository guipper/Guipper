#include "../src/JPutils/jp_textwrap.h"

#include <iostream>
#include <string>

namespace
{
	int failures = 0;

	void expect(bool condition, const std::string &message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}

	// One unit per character. Real fonts are proportional, but the wrapping is
	// pure arithmetic over whatever the measurer says, so a fixed width makes
	// every expectation below exact instead of approximate.
	auto fixed = [](const std::string &s) { return (float)s.size(); };

	std::string joined(const std::vector<std::string> &lines)
	{
		std::string out;
		for (const std::string &l : lines)
			out += (out.empty() ? "" : "|") + l;
		return out;
	}

	void testFitsOnOneLine()
	{
		const auto out = jp_textwrap::wrap(fixed, "abc def", 100.0f);
		expect(out.size() == 1 && out[0] == "abc def",
			"text that fits was split anyway: " + joined(out));
	}

	void testBreaksBetweenWords()
	{
		// "abc def" is 7 wide; at 7 it fits, at 6 it must break.
		expect(joined(jp_textwrap::wrap(fixed, "abc def", 7.0f)) == "abc def",
			"a line exactly at the limit was broken");
		expect(joined(jp_textwrap::wrap(fixed, "abc def", 6.0f)) == "abc|def",
			"a line over the limit did not break between words");
	}

	// A shortcut like "Ctrl/Cmd+Shift+G" is one word and must survive intact,
	// even in a column too narrow for it. Splitting it would invent a keystroke.
	void testLongWordIsNeverSplit()
	{
		const auto out = jp_textwrap::wrap(fixed, "Ctrl/Cmd+Shift+G", 4.0f);
		expect(out.size() == 1 && out[0] == "Ctrl/Cmd+Shift+G",
			"a single word wider than the column was broken mid-word: " +
				joined(out));
	}

	void testLongWordKeepsItsOwnLine()
	{
		const auto out = jp_textwrap::wrap(fixed, "ab enormouslylongword cd",
			5.0f);
		expect(joined(out) == "ab|enormouslylongword|cd",
			"an over-wide word did not get a line to itself: " + joined(out));
	}

	// Callers turn line COUNT into a row height, so an empty string still has to
	// produce one line or the row collapses.
	void testEmptyYieldsOneBlankLine()
	{
		const auto out = jp_textwrap::wrap(fixed, "", 50.0f);
		expect(out.size() == 1 && out[0].empty(),
			"empty text did not yield exactly one blank line: " + joined(out));
		const auto spaces = jp_textwrap::wrap(fixed, "   ", 50.0f);
		expect(spaces.size() == 1 && spaces[0].empty(),
			"whitespace-only text did not yield one blank line: " +
				joined(spaces));
	}

	// A degenerate width must not loop forever or drop text.
	void testZeroWidthStillTerminates()
	{
		const auto out = jp_textwrap::wrap(fixed, "a b c", 0.0f);
		expect(joined(out) == "a|b|c",
			"a zero width lost or duplicated words: " + joined(out));
	}

	void testCollapsesRuns()
	{
		const auto out = jp_textwrap::wrap(fixed, "a    b", 100.0f);
		expect(joined(out) == "a b",
			"runs of whitespace were not collapsed: " + joined(out));
	}
}

int main()
{
	testFitsOnOneLine();
	testBreaksBetweenWords();
	testLongWordIsNeverSplit();
	testLongWordKeepsItsOwnLine();
	testEmptyYieldsOneBlankLine();
	testZeroWidthStillTerminates();
	testCollapsesRuns();
	if (failures != 0)
	{
		std::cerr << failures << " text wrap test(s) failed\n";
		return 1;
	}
	std::cout << "text wrap tests passed\n";
	return 0;
}
