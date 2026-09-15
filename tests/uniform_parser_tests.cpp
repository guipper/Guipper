#include "../src/JPutils/jp_uniform_parser.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>

using namespace jp_uniform_parser;

namespace
{
	void require(bool condition, const char *message)
	{
		if (!condition) { std::cerr << message << '\n'; std::exit(1); }
	}
	bool has(const Result &r, Code code)
	{
		return std::any_of(r.diagnostics.begin(), r.diagnostics.end(),
			[code](const Diagnostic &d) { return d.code == code; });
	}
}

int main()
{
	const auto basic = parse("uniform float amount=0.7;\nuniform bool enabled;\n"
		"uniform sampler2D image;\nuniform sampler2DRect rect;");
	require(!basic.hasErrors() && basic.declarations.size() == 4, "basic declarations");
	require(basic.declarations[0].floatDefault &&
		std::abs(*basic.declarations[0].floatDefault - 0.7f) < 1e-6f, "float default");
	require(!basic.declarations[1].boolDefault && basic.declarations[1].type == Type::Bool,
		"absent boolean default must stay absent");
	require(basic.declarations[2].name == "image" &&
		basic.declarations[3].type == Type::Sampler2DRect, "sampler names");

	const auto formatting = parse("\xef\xbb\xbf \tuniform highp\nfloat\n gain = - 2.5e-2F,\n"
		"other=+1.; // @color r warm\nlayout(location=2) uniform bool enabled=true, hidden=false;");
	require(!formatting.hasErrors() && formatting.declarations.size() == 4, "formatting and lists");
	require(formatting.declarations[0].location.line == 3 &&
		formatting.declarations[0].location.column == 2, "source location");
	require(formatting.declarations[0].floatDefault &&
		std::abs(*formatting.declarations[0].floatDefault + 0.025f) < 1e-6f, "signed exponent");
	require(formatting.declarations[1].floatDefault == 1.0f, "signed integer/dot");
	require(formatting.declarations[2].boolDefault == true &&
		formatting.declarations[3].boolDefault == false, "boolean defaults");
	require(formatting.declarations[0].annotations.find("@color r warm") != std::string::npos,
		"multiline trailing annotation");

	const auto comments = parse("// uniform float ignored;\n/* uniform bool absent; */\n"
		"uniform /* type */ float visible; // @color b\n"
		"uniform sampler2D mask; /* @internal */\n"
		"uniform float a; uniform float b; // @color g\n"
		"void main() { uniform float local; }\n"
		"#define DECLARE \\\n uniform float macro;\n");
	require(!comments.hasErrors() && comments.declarations.size() == 4, "comments/macros/local scope");
	require(comments.declarations[1].internal, "internal annotation");
	require(comments.declarations[2].annotations.empty() &&
		!comments.declarations[3].annotations.empty(), "annotation belongs to adjacent declaration");
	require(parse("/* unterminated").hasErrors(), "unterminated comment");
	const auto macroComment = parse("#define VALUE /*\nuniform float commented;\n*/ 1\nuniform float shown;");
	require(!macroComment.hasErrors() && macroComment.declarations.size() == 1 &&
		macroComment.declarations[0].name == "shown", "multiline comment inside directive");
	require(parse("#define VALUE /* unterminated").hasErrors(), "unterminated directive comment");

	const auto unsupported = parse("uniform float values[8]; uniform vec3 color;\n"
		"uniform float a=sin(0.5), b=1.0+2.0, c=1 2;\n"
		"uniform Scene { float value; } scene; uniform float after;");
	require(!unsupported.hasErrors() && unsupported.declarations.size() == 6,
		"unsupported syntax should not hide later declarations");
	require(has(unsupported, Code::UnsupportedArray) && has(unsupported, Code::UnsupportedType) &&
		has(unsupported, Code::UnsupportedInitializer), "unsupported diagnostics");
	require(!unsupported.declarations[2].floatDefault &&
		!unsupported.declarations[3].floatDefault && !unsupported.declarations[4].floatDefault,
		"expressions and adjacent literals must not be parsed by prefix");
	for (const char *value : {"1e999", "nan", "inf", "0x1", "1.0oops"})
	{
		const auto r = parse(std::string("uniform float value=") + value + ";");
		require(r.declarations.size() == 1 && !r.declarations[0].floatDefault &&
			has(r, Code::UnsupportedInitializer), "invalid/out-of-range literal");
	}
	for (const char *source : {"uniform", "uniform float", "uniform float;",
		"uniform float a", "uniform float a=", "uniform float a=;",
		"uniform float a[;", "uniform Block { float a;", "uniform float x; uniform float x;"})
		require(parse(source).hasErrors(), "malformed input must have an error");
	const auto recovery = parse("uniform float = 2;\nuniform float ok=0.5;");
	require(recovery.hasErrors() && recovery.declarations.size() == 1 &&
		recovery.declarations[0].name == "ok", "recovery after invalid declaration");
	const auto missingSemi = parse("uniform float bad\nuniform bool good;");
	require(missingSemi.hasErrors() && missingSemi.declarations.size() == 1 &&
		missingSemi.declarations[0].name == "good", "recovery at next uniform");
	require(has(parse("#if ENABLED\nuniform float conditional;\n#endif"), Code::ConditionalDeclaration),
		"preprocessor limitation must be explicit");
	require(parse("").declarations.empty() && !parse("").hasErrors(), "empty input is safe");
	require(parse("uniformity float fake;").declarations.empty(), "identifier boundary");

	// Deterministic stress inputs exercise recovery and token boundaries under
	// ASan/UBSan too, including strings the shader compiler would reject.
	std::mt19937 generator(412);
	const std::string alphabet = "uniform float bool name0123=;/*#\n\t{}[](),+-.'\"\\";
	for (int i = 0; i < 6000; ++i)
	{
		std::string source = i % 2 == 0 ? "uniform float " : "";
		for (unsigned j = generator() % 160; j > 0; --j)
			source += alphabet[generator() % alphabet.size()];
		const auto result = parse(source);
		for (const auto &d : result.declarations)
			require(!d.name.empty() && d.location.line > 0 && d.location.column > 0, "fuzz invariant");
	}
	std::cout << "uniform parser tests passed\n";
}
