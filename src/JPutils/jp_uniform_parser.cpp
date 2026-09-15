#include "jp_uniform_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <locale>
#include <sstream>
#include <unordered_set>

namespace
{
	using namespace jp_uniform_parser;
	struct Token
	{
		std::string text;
		Location location;
		std::size_t begin, end;
		bool conditional;
	};
	struct Comment
	{
		std::string text;
		Location location;
		std::size_t begin;
	};
	bool identifierStart(char c)
	{
		return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
	}
	bool identifierPart(char c)
	{
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
	}
	bool identifier(const std::string &text)
	{
		return !text.empty() && identifierStart(text.front()) &&
			std::all_of(text.begin(), text.end(), identifierPart);
	}
	bool qualifier(const std::string &text)
	{
		return text == "lowp" || text == "mediump" || text == "highp" ||
			text == "precise" || text == "coherent" || text == "volatile" ||
			text == "restrict" || text == "readonly" || text == "writeonly";
	}
	Type typeFor(const std::string &text)
	{
		if (text == "float") return Type::Float;
		if (text == "bool") return Type::Bool;
		if (text == "sampler2D") return Type::Sampler2D;
		if (text == "sampler2DRect") return Type::Sampler2DRect;
		return Type::Unsupported;
	}
	void diagnose(Result &result, Severity severity, Code code,
		Location location, const std::string &name, const std::string &message)
	{
		result.diagnostics.push_back({severity, code, location, name, message});
	}
	std::vector<Token> tokenize(const std::string &source,
		std::vector<Comment> &comments, Result &result)
	{
		std::vector<Token> tokens;
		std::size_t pos = 0;
		Location location;
		bool lineStart = true;
		int conditionalDepth = 0;
		auto advance = [&]() {
			if (source[pos++] == '\n')
			{
				++location.line;
				location.column = 1;
				lineStart = true;
			}
			else ++location.column;
		};
		// UTF-8 BOM does not count as a column of GLSL.
		if (source.compare(0, 3, "\xef\xbb\xbf") == 0) pos = 3;
		while (pos < source.size())
		{
			if (std::isspace(static_cast<unsigned char>(source[pos])))
			{
				advance();
				continue;
			}
			const std::size_t begin = pos;
			const Location start = location;
			if (source.compare(pos, 2, "//") == 0)
			{
				while (pos < source.size() && source[pos] != '\n') advance();
				comments.push_back({source.substr(begin + 2, pos - begin - 2), start, begin});
				continue;
			}
			if (source.compare(pos, 2, "/*") == 0)
			{
				advance(); advance();
				while (pos < source.size() && source.compare(pos, 2, "*/") != 0) advance();
				comments.push_back({source.substr(begin + 2, pos - begin - 2), start, begin});
				if (pos == source.size())
					diagnose(result, Severity::Error, Code::UnterminatedComment,
						start, "", "Unterminated block comment.");
				else { advance(); advance(); }
				continue;
			}
			if (lineStart && source[pos] == '#')
			{
				// Consume continued macro lines so a uniform token inside a
				// macro body can never become an accidental control.
				do
				{
					while (pos < source.size() && source[pos] != '\n')
					{
						if (source.compare(pos, 2, "/*") == 0)
						{
							const Location commentStart = location;
							advance(); advance();
							while (pos < source.size() && source.compare(pos, 2, "*/") != 0) advance();
							if (pos == source.size())
								diagnose(result, Severity::Error, Code::UnterminatedComment,
									commentStart, "", "Unterminated block comment in directive.");
							else { advance(); advance(); }
						}
						else advance();
					}
					std::size_t tail = pos;
					if (tail > begin && source[tail - 1] == '\r') --tail;
					const bool continued = tail > begin && source[tail - 1] == '\\';
					if (pos < source.size()) advance();
					if (!continued) break;
				} while (pos < source.size());
				std::istringstream directive(source.substr(begin + 1, pos - begin - 1));
				std::string word;
				directive >> word;
				if (word == "if" || word == "ifdef" || word == "ifndef") ++conditionalDepth;
				else if (word == "endif" && conditionalDepth > 0) --conditionalDepth;
				continue;
			}
			lineStart = false;
			if (identifierStart(source[pos]))
			{
				advance();
				while (pos < source.size() && identifierPart(source[pos])) advance();
			}
			else if (std::isdigit(static_cast<unsigned char>(source[pos])) ||
				(source[pos] == '.' && pos + 1 < source.size() &&
				 std::isdigit(static_cast<unsigned char>(source[pos + 1]))))
			{
				advance();
				while (pos < source.size())
				{
					const char c = source[pos];
					if (identifierPart(c) || c == '.' ||
						((c == '+' || c == '-') && pos > begin &&
						 (source[pos - 1] == 'e' || source[pos - 1] == 'E')))
						advance();
					else break;
				}
			}
			else if (source[pos] == '"' || source[pos] == '\'')
			{
				const char quote = source[pos];
				advance();
				while (pos < source.size() && source[pos] != quote && source[pos] != '\n')
				{
					if (source[pos] == '\\' && pos + 1 < source.size()) advance();
					advance();
				}
				if (pos < source.size() && source[pos] == quote) advance();
			}
			else advance();
			tokens.push_back({source.substr(begin, pos - begin), start, begin, pos,
				conditionalDepth > 0});
		}
		return tokens;
	}
	std::optional<float> floatLiteral(std::string value)
	{
		if (!value.empty() && (value.back() == 'f' || value.back() == 'F')) value.pop_back();
		// Restrict to decimal literals rather than letting the C locale accept
		// inf/nan, hex or a valid numeric prefix followed by other text.
		if (value.empty() || value.find_first_not_of("0123456789.eE+-") != std::string::npos)
			return {};
		std::istringstream stream(value);
		stream.imbue(std::locale::classic());
		float number = 0.0f;
		stream >> std::noskipws >> number;
		if (!stream || !stream.eof() || !std::isfinite(number)) return {};
		return number;
	}
}

bool jp_uniform_parser::Result::hasErrors() const
{
	return std::any_of(diagnostics.begin(), diagnostics.end(),
		[](const Diagnostic &diagnostic) { return diagnostic.severity == Severity::Error; });
}

jp_uniform_parser::Result jp_uniform_parser::parse(const std::string &source)
{
	Result result;
	std::vector<Comment> comments;
	const auto tokens = tokenize(source, comments, result);
	std::unordered_set<std::string> names;
	int braces = 0, parentheses = 0;
	for (std::size_t i = 0; i < tokens.size();)
	{
		const Token &token = tokens[i];
		if (token.text != "uniform" || braces != 0 || parentheses != 0)
		{
			if (token.text == "{") ++braces;
			else if (token.text == "}") braces = std::max(0, braces - 1);
			else if (token.text == "(") ++parentheses;
			else if (token.text == ")") parentheses = std::max(0, parentheses - 1);
			++i;
			continue;
		}
		const std::size_t begin = i++;
		if (token.conditional)
			diagnose(result, Severity::Warning, Code::ConditionalDeclaration, token.location,
				"", "Conditional uniform inspected without evaluating the preprocessor.");
		while (i < tokens.size() && qualifier(tokens[i].text)) ++i;
		if (i == tokens.size() || !identifier(tokens[i].text) || tokens[i].text == "uniform")
		{
			diagnose(result, Severity::Error, Code::InvalidDeclaration, token.location,
				"", "Expected a uniform type.");
			continue;
		}
		const std::string typeName = tokens[i++].text;
		const Type type = typeFor(typeName);
		if (i < tokens.size() && tokens[i].text == "{")
		{
			diagnose(result, Severity::Warning, Code::UnsupportedType, token.location,
				typeName, "Uniform blocks are not exposed as controls.");
			int depth = 0;
			do
			{
				if (tokens[i].text == "{") ++depth;
				else if (tokens[i].text == "}") --depth;
				++i;
			} while (i < tokens.size() && depth > 0);
			while (i < tokens.size() && tokens[i].text != ";" &&
				tokens[i].text != "uniform") ++i;
			if (depth != 0 || i == tokens.size() || tokens[i].text != ";")
				diagnose(result, Severity::Error, Code::InvalidDeclaration, token.location,
					typeName, "Unterminated uniform block.");
			else ++i;
			continue;
		}
		std::vector<Declaration> declarations;
		bool terminated = false;
		std::size_t semicolon = i;
		while (i < tokens.size())
		{
			if (!identifier(tokens[i].text) || tokens[i].text == "uniform" ||
				qualifier(tokens[i].text))
			{
				diagnose(result, Severity::Error, Code::InvalidDeclaration, tokens[i].location,
					"", "Expected a uniform name after " + typeName + ".");
				break;
			}
			Declaration declaration;
			declaration.type = type;
			declaration.typeName = typeName;
			declaration.name = tokens[i].text;
			declaration.location = tokens[i++].location;
			if (i < tokens.size() && tokens[i].text == "[")
			{
				declaration.array = true;
				while (i < tokens.size() && tokens[i].text != "]" &&
					tokens[i].text != ";" && tokens[i].text != "uniform") ++i;
				if (i == tokens.size() || tokens[i].text != "]")
				{
					diagnose(result, Severity::Error, Code::InvalidDeclaration,
						declaration.location, declaration.name, "Unterminated uniform array.");
					break;
				}
				++i;
			}
			if (i < tokens.size() && tokens[i].text == "=")
			{
				++i;
				const std::size_t initializerBegin = i;
				std::string initializer;
				std::size_t initializerTokens = 0;
				int nested = 0;
				while (i < tokens.size())
				{
					const auto &part = tokens[i].text;
					if (part == ";" || part == "uniform" || part == "{" || part == "}" ||
						(part == "," && nested == 0)) break;
					if (part == "(") ++nested;
					if (part == ")") --nested;
					initializer += part;
					++initializerTokens;
					++i;
				}
				if (initializer.empty())
				{
					diagnose(result, Severity::Error, Code::InvalidDeclaration,
						declaration.location, declaration.name, "Expected an initializer after '='.");
				}
				else if (type == Type::Float && (initializerTokens == 1 ||
					(initializerTokens == 2 && (tokens[initializerBegin].text == "+" ||
					 tokens[initializerBegin].text == "-"))))
					declaration.floatDefault = floatLiteral(initializer);
				else if (type == Type::Bool && initializerTokens == 1)
				{
					if (initializer == "true") declaration.boolDefault = true;
					else if (initializer == "false") declaration.boolDefault = false;
				}
				if (!initializer.empty() && !declaration.floatDefault && !declaration.boolDefault)
					diagnose(result, Severity::Warning, Code::UnsupportedInitializer,
						declaration.location, declaration.name,
						"Initializer is not a supported scalar literal; the caller must use its fallback.");
			}
			declarations.push_back(declaration);
			if (i < tokens.size() && tokens[i].text == ";")
			{
				semicolon = i++;
				terminated = true;
				break;
			}
			if (i < tokens.size() && tokens[i].text == ",") { ++i; continue; }
			diagnose(result, Severity::Error, Code::InvalidDeclaration, declaration.location,
				declaration.name, "Expected ',' or ';' after uniform declaration.");
			break;
		}
		if (!terminated)
		{
			diagnose(result, Severity::Error, Code::InvalidDeclaration, token.location,
				"", "Uniform declaration is incomplete.");
			// Leave the next uniform available for recovery. Braces are handled
			// by the outer scanner, which skips uniform blocks and function bodies.
			while (i < tokens.size() && tokens[i].text != ";" && tokens[i].text != "uniform" &&
				tokens[i].text != "{" && tokens[i].text != "}") ++i;
			if (i < tokens.size() && tokens[i].text == ";") ++i;
			continue;
		}
		std::string annotations;
		auto commentIt = std::lower_bound(comments.begin(), comments.end(), tokens[begin].begin,
			[](const Comment &comment, std::size_t offset) { return comment.begin < offset; });
		for (; commentIt != comments.end(); ++commentIt)
		{
			const auto &comment = *commentIt;
			if (i < tokens.size() && comment.begin >= tokens[i].begin) break;
			const bool inside = comment.begin < tokens[semicolon].end;
			const bool trailing = comment.location.line == tokens[semicolon].location.line &&
				(i == tokens.size() || comment.begin < tokens[i].begin);
			if (inside || trailing) annotations += comment.text + "\n";
			else break;
		}
		for (auto &declaration : declarations)
		{
			declaration.annotations = annotations;
			declaration.internal = annotations.find("@internal") != std::string::npos;
			if (!names.insert(declaration.name).second)
			{
				diagnose(result, Severity::Error, Code::DuplicateName, declaration.location,
					declaration.name, "Duplicate uniform name.");
				continue;
			}
			if (declaration.array)
				diagnose(result, Severity::Warning, Code::UnsupportedArray, declaration.location,
					declaration.name, "Uniform arrays are not exposed as controls.");
			else if (type == Type::Unsupported)
				diagnose(result, Severity::Warning, Code::UnsupportedType, declaration.location,
					declaration.name, "Uniform type " + typeName + " is not exposed as a control.");
			result.declarations.push_back(std::move(declaration));
		}
	}
	return result;
}
