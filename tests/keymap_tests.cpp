#include "../src/JPutils/jp_keymap.h"
#include "../src/JPutils/jp_help_content.h"

#include <cstdint>
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

	std::string joined(const std::vector<std::string> &v)
	{
		std::string out;
		for (const std::string &s : v) out += (out.empty() ? "" : "|") + s;
		return out;
	}

	void testChord()
	{
		expect(joined(jp_keymap::parse("ctrl+shift+g")) == "ctrl|shift|g",
			"a two-modifier chord did not yield its three keys: " +
				joined(jp_keymap::parse("ctrl+shift+g")));
	}

	void testCaseAndSeparators()
	{
		expect(joined(jp_keymap::parse("Ctrl+S")) == "ctrl|s",
			"an annotation in mixed case was not folded down");
		expect(joined(jp_keymap::parse("b e")) == "b|e",
			"space-separated keys were not both returned");
		expect(joined(jp_keymap::parse("b, e")) == "b|e",
			"comma-separated keys were not both returned");
	}

	void testSingleKey()
	{
		expect(joined(jp_keymap::parse("z")) == "z", "a lone key was lost");
		expect(joined(jp_keymap::parse("space")) == "space",
			"a named key was lost");
	}

	// The default for the 78 rows that hold a widget name, an OSC path or a
	// GLSL declaration rather than a key.
	void testEmptyIsNotOnTheBoard()
	{
		expect(jp_keymap::parse("").empty(),
			"an empty annotation produced keys");
		expect(jp_keymap::parse("   ").empty(),
			"a whitespace annotation produced keys");
	}

	// Anything unrecognised is dropped rather than drawn as an invented key -
	// and unknownIds is what makes that silence visible to the table test.
	void testJunkIsDroppedButReported()
	{
		expect(jp_keymap::parse("bucket").empty(),
			"a widget name was turned into keys");
		expect(joined(jp_keymap::unknownIds("bucket")) == "bucket",
			"an unknown id was not reported");
		expect(joined(jp_keymap::parse("ctrl+nosuchkey")) == "ctrl",
			"a chord with one bad token lost its good token too");
		expect(joined(jp_keymap::unknownIds("ctrl+nosuchkey")) == "nosuchkey",
			"the bad token of a chord was not reported");
		expect(jp_keymap::unknownIds("ctrl+shift+g").empty(),
			"a well-formed chord reported a bogus unknown");
	}

	void testDuplicatesCollapse()
	{
		expect(joined(jp_keymap::parse("g+g")) == "g",
			"the same key twice was returned twice");
	}

	// The board is what the parser validates against, so a malformed board
	// would silently accept nothing at all.
	// The check that cannot be written by hand, and the reason unknownIds
	// exists: every annotation in the real table must name keys the board can
	// actually draw. A typo would simply never light up - invisible on screen,
	// and on the beginner-facing panel of all places. Here it is a failure.
	void testEveryAnnotationIsDrawable()
	{
		int annotated = 0;
		for (const jp_help::Line &line : jp_help::table())
		{
			const std::string phys = line.phys;
			if (phys.empty()) continue;
			++annotated;
			const std::vector<std::string> bad = jp_keymap::unknownIds(phys);
			expect(bad.empty(),
				std::string("row \"") + line.keys +
					"\" names a key the board cannot draw: " + joined(bad));
			expect(!jp_keymap::parse(phys).empty(),
				std::string("row \"") + line.keys +
					"\" is annotated but lights nothing");
		}
		// Guards the guard: if the annotations were ever dropped wholesale, the
		// loop above would pass by doing nothing.
		expect(annotated > 50,
			"far fewer annotated rows than expected: " +
				std::to_string(annotated));
	}

	// A row that carries no key must not be annotated - that is what keeps the
	// GLSL, OSC and widget-name rows off the board.
	void testNonKeyRowsAreNotAnnotated()
	{
		for (const jp_help::Line &line : jp_help::table())
		{
			const std::string keys = line.keys;
			if (keys.find("uniform ") == std::string::npos &&
				keys.find('/') != 0)
			{
				continue;
			}
			expect(std::string(line.phys).empty(),
				std::string("a row that is not a key was annotated: ") +
					line.keys);
		}
	}

	void testBoardIsSane()
	{
		std::size_t count = 0;
		for (const auto &row : jp_keymap::board()) count += row.size();
		expect(count > 40, "the board has suspiciously few keys");
		expect(jp_keymap::isKnownKey("ctrl") && jp_keymap::isKnownKey("z") &&
			jp_keymap::isKnownKey("space") && jp_keymap::isKnownKey("del"),
			"the board is missing keys the help table needs");
		expect(!jp_keymap::isKnownKey(""), "the empty id was accepted as a key");

		// Ids must be unique, or a highlight would land on two caps at once.
		for (const auto &rowA : jp_keymap::board())
			for (const jp_keymap::Key &a : rowA)
			{
				int seen = 0;
				for (const auto &rowB : jp_keymap::board())
					for (const jp_keymap::Key &b : rowB)
						if (std::string(a.id) == b.id) seen++;
				expect(seen == 1,
					std::string("duplicate key id on the board: ") + a.id);
			}
	}

	const jp_help::Line *findEntry(const std::string &keys)
	{
		for (const jp_help::Line &line : jp_help::table())
			if (line.kind == jp_help::Kind::Entry && keys == line.keys)
				return &line;
		return nullptr;
	}

	void testHelpCorpusIsBilingual()
	{
		int headings = 0;
		for (const jp_help::Line &line : jp_help::table())
		{
			if (line.kind == jp_help::Kind::Gap) continue;
			expect(std::string(line.en).size() > 0,
				"a HELP row has no English text");
			expect(std::string(line.es).size() > 0,
				"a HELP row has no Spanish text");
			if (line.kind == jp_help::Kind::Heading) ++headings;
		}
		expect(headings == 15,
			"HELP section count changed: " + std::to_string(headings));

		const auto &table = jp_help::table();
		expect(std::string(table.front().en) == "WHAT GUIPPER IS" &&
			std::string(table.front().es) == "QUÉ ES GUIPPER",
			"the bilingual introduction is no longer first");

		const jp_help::Line *osc = findEntry("/openguinumber/<parameterIndex>");
		expect(osc != nullptr &&
			std::string(osc->en).find("standard float") != std::string::npos &&
			std::string(osc->es).find("índices inválidos") != std::string::npos,
			"the strict indexed OSC contract is missing in EN or ES");

		const jp_help::Line *audio = findEntry("Kick trigger / Snare trigger");
		expect(audio != nullptr &&
			std::string(audio->en).find("detected onsets") != std::string::npos &&
			std::string(audio->es).find("detecciones") != std::string::npos,
			"the Audio trigger description is missing in EN or ES");

		bool hasQuickStartAccent = false;
		bool hasCompositionAccent = false;
		for (const jp_help::Line &line : table)
		{
			const std::string es = line.es;
			hasQuickStartAccent |= es == "INICIO RÁPIDO";
			hasCompositionAccent |= es.find("COMPOSICIÓN") != std::string::npos;
		}
		expect(hasQuickStartAccent && hasCompositionAccent,
			"required Spanish accents disappeared from the corpus");

		const jp_help::Line *editor = findEntry("Ctrl/Cmd+C / X / V / A");
		const jp_help::Line *settings = findEntry("Live outputs");
		const jp_help::Line *midi = findEntry("Learn");
		const jp_help::Line *nodes = findEntry("Ctrl/Cmd + click");
		expect(editor && editor->scope == jp_help::Scope::Editor &&
			settings && settings->scope == jp_help::Scope::Settings &&
			midi && midi->scope == jp_help::Scope::Midi &&
			nodes && nodes->scope == jp_help::Scope::Nodes,
			"a screen-specific HELP row lost its scope");
	}

	std::uint64_t hashBytes(std::uint64_t hash, const std::string &text)
	{
		for (unsigned char byte : text)
		{
			hash ^= byte;
			hash *= UINT64_C(1099511628211);
		}
		// Preserve field boundaries, including empty fields.
		hash ^= 0xff;
		hash *= UINT64_C(1099511628211);
		return hash;
	}

	void testPaintRowsStayUntouched()
	{
		std::uint64_t hash = UINT64_C(1469598103934665603);
		int rows = 0;
		bool inPaint = false;
		for (const jp_help::Line &line : jp_help::table())
		{
			if (line.kind == jp_help::Kind::Heading &&
				std::string(line.en) == "PAINT CANVAS EDITOR")
			{
				inPaint = true;
			}
			else if (inPaint && line.kind == jp_help::Kind::Heading)
			{
				break;
			}
			if (!inPaint) continue;
			++rows;
			if (line.kind != jp_help::Kind::Heading &&
				line.kind != jp_help::Kind::Gap)
			{
				expect(line.scope == jp_help::Scope::Paint,
					"a Paint HELP row lost its Paint scope");
			}
			hash = hashBytes(hash, std::to_string(static_cast<int>(line.kind)));
			hash = hashBytes(hash, std::to_string(static_cast<int>(line.scope)));
			hash = hashBytes(hash, line.keys);
			hash = hashBytes(hash, line.en);
			hash = hashBytes(hash, line.es);
			hash = hashBytes(hash, line.phys);
		}

		expect(rows == 47, "Paint HELP row count changed: " +
			std::to_string(rows));
		const std::uint64_t approvedPaintHash =
			UINT64_C(5768092553978544981);
		expect(hash == approvedPaintHash,
			"Paint HELP rows changed; expected " +
			std::to_string(approvedPaintHash) + " but got " +
			std::to_string(hash));
	}
}

int main()
{
	testChord();
	testCaseAndSeparators();
	testSingleKey();
	testEmptyIsNotOnTheBoard();
	testJunkIsDroppedButReported();
	testDuplicatesCollapse();
	testEveryAnnotationIsDrawable();
	testNonKeyRowsAreNotAnnotated();
	testBoardIsSane();
	testHelpCorpusIsBilingual();
	testPaintRowsStayUntouched();
	if (failures != 0)
	{
		std::cerr << failures << " keymap test(s) failed\n";
		return 1;
	}
	std::cout << "keymap tests passed\n";
	return 0;
}
