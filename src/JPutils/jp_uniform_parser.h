#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Source inspection only: no OpenGL, file I/O, random values or application
// globals. The caller decides which declarations become controls.
namespace jp_uniform_parser
{
	enum class Type { Float, Bool, Sampler2D, Sampler2DRect, Unsupported };
	enum class Severity { Warning, Error };
	enum class Code {
		InvalidDeclaration, UnterminatedComment, DuplicateName,
		UnsupportedType, UnsupportedArray, UnsupportedInitializer,
		ConditionalDeclaration, ShaderCompileError, SourceReadError
	};
	struct Location
	{
		std::size_t line = 1;
		std::size_t column = 1;
	};
	struct Diagnostic
	{
		Severity severity;
		Code code;
		Location location;
		std::string name;
		std::string message;
	};
	struct Declaration
	{
		Type type = Type::Unsupported;
		std::string typeName;
		std::string name;
		Location location;
		std::optional<float> floatDefault;
		std::optional<bool> boolDefault;
		bool array = false;
		bool internal = false;
		// Comments inside the declaration and directly after its semicolon.
		std::string annotations;
	};
	struct Result
	{
		std::vector<Declaration> declarations;
		std::vector<Diagnostic> diagnostics;
		bool hasErrors() const;
	};
	// Recognizes global float/bool/sampler declarations, qualifiers and comma
	// lists. Includes/macros/conditional branches are NOT expanded. Unsupported
	// types, arrays and initializer expressions are returned with diagnostics.
	Result parse(const std::string &source);
}
