#include "jp_shadereditor_sync.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <set>
#include <sstream>

#include "../JPutils/jp_shader_globals.h"
#include "../JPutils/jp_uniform_parser.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace jp_shadereditor
{
	namespace
	{
		constexpr const char *kManifestName = ".jse_sync.json";
		// Header que declara los globals de Guipper. Va en un include a
		// proposito: el parser de uniforms lee el .frag crudo, no los includes,
		// asi que `time` no se convierte en un slider que pisa el reloj.
		constexpr const char *kHeaderName = "_jse_common.frag";
		// Subir esto reimporta todo: el manifiesto guarda con que version se
		// tradujo cada shader.
		constexpr int kTranslatorVersion = 2;
		constexpr int kMaxImportsPerFrame = 3;

		std::string trim(const std::string &text)
		{
			const auto first = text.find_first_not_of(" \t\r\n");
			if (first == std::string::npos) return {};
			const auto last = text.find_last_not_of(" \t\r\n");
			return text.substr(first, last - first + 1);
		}

		bool startsWith(const std::string &text, const std::string &prefix)
		{
			return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
		}

		std::string lowerCase(std::string text)
		{
			for (char &c : text) c = (char)std::tolower((unsigned char)c);
			return text;
		}

		std::string withoutComments(const std::string &line)
		{
			const auto comment = line.find("//");
			return comment == std::string::npos ? line : line.substr(0, comment);
		}

		// Uniforms que Guipper completa solo. Declararlos otra vez rompe la
		// compilación, así que se sacan del original.
		bool isGlobalUniform(const std::string &name)
		{
			static const std::set<std::string> names = {
				"resolution", "time", "mouse", "window_mouse", "bpm", "feedback",
				"globalframeNum", "boxframeNum", "iTime", "iResolution", "iMouse",
				"iFrame", "iFrameRate", "iTimeDelta", "iDate", "iSampleRate",
				"iChannelTime", "iChannelResolution",
				"audio_trigger", "audio_express", "audio_logic", "audio_onsets",
				"audio_rhythm", "audio_spectrum0", "audio_spectrum1",
				"audio_spectrum2", "audio_spectrum3"};
			return names.count(name) > 0;
		}

		struct Declared
		{
			std::string type;
			bool hasInitializer = false;
		};

		// Reescribe una sentencia `uniform ...;` dejando afuera los globals y
		// aplicando los valores por defecto que publica jpshadereditor.
		std::string rewriteUniform(const std::string &statement,
			const std::map<std::string, std::string> &defaults,
			std::map<std::string, Declared> &declared,
			std::map<std::string, std::string> &droppedTypes)
		{
			std::string body = trim(statement);
			if (!body.empty() && body.back() == ';') body.pop_back();
			const std::string lowered = lowerCase(body);
			if (!startsWith(lowered, "uniform")) return statement;

			std::istringstream stream(body);
			std::string keyword;
			stream >> keyword;
			std::string qualifier;
			std::string type;
			stream >> qualifier;
			if (qualifier == "highp" || qualifier == "mediump" || qualifier == "lowp")
				stream >> type;
			else
				type = qualifier;
			if (type.empty()) return statement;

			std::string rest;
			std::getline(stream, rest);
			// Un bloque uniform no se toca: no se puede partir sin romperlo.
			if (rest.find('{') != std::string::npos) return statement;

			struct Piece
			{
				std::string name;
				std::string text;
			};
			std::vector<Piece> pieces;
			std::string current;
			int depth = 0;
			for (size_t i = 0; i < rest.size(); ++i)
			{
				const char c = rest[i];
				if (c == '(' || c == '[') ++depth;
				if (c == ')' || c == ']') --depth;
				if (c == ';') break;
				if (c == ',' && depth == 0)
				{
					pieces.push_back({trim(current), trim(current)});
					current.clear();
					continue;
				}
				current += c;
			}
			if (!trim(current).empty()) pieces.push_back({trim(current), trim(current)});
			if (pieces.empty()) return statement;

			std::string rebuilt;
			for (auto &piece : pieces)
			{
				std::string text = piece.text;
				const auto comment = text.find("//");
				std::string trailingComment;
				if (comment != std::string::npos)
				{
					trailingComment = text.substr(comment);
					text = trim(text.substr(0, comment));
				}
				const auto equals = text.find('=');
				const std::string name = trim(text.substr(0, equals));
				if (name.empty()) continue;
				if (isGlobalUniform(name))
				{
					droppedTypes[name] = type;
					continue;
				}

				bool hasInitializer = equals != std::string::npos;
				const auto wanted = defaults.find(name);
				if (!hasInitializer && wanted != defaults.end())
				{
					if (type == "bool")
					{
						const bool truthy = wanted->second != "0" && wanted->second != "0.0" &&
							wanted->second != "false";
						text += truthy ? " = true" : " = false";
						hasInitializer = true;
					}
					else if (type == "float")
					{
						text += " = " + wanted->second;
						hasInitializer = true;
					}
				}
				declared[name] = {type, hasInitializer};
				if (!rebuilt.empty()) rebuilt += ", ";
				rebuilt += text;
				if (!trailingComment.empty()) rebuilt += " " + trailingComment;
			}
			if (rebuilt.empty()) return {};
			return "uniform " + (qualifier == "highp" || qualifier == "mediump" || qualifier == "lowp" ? qualifier + " " : std::string()) +
				type + " " + rebuilt + ";";
		}

		std::string numberLiteral(const ofJson &value)
		{
			if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
			if (value.is_number())
			{
				std::ostringstream text;
				text.precision(9);
				text << value.get<double>();
				std::string literal = text.str();
				if (literal.find('.') == std::string::npos && literal.find('e') == std::string::npos &&
					literal.find("inf") == std::string::npos && literal.find("nan") == std::string::npos)
					literal += ".0";
				return literal;
			}
			if (value.is_string())
			{
				const std::string text = trim(value.get<std::string>());
				if (!text.empty() && (std::isdigit((unsigned char)text[0]) || text[0] == '-' || text[0] == '.'))
					return text;
			}
			return {};
		}

		std::string urlJoin(const std::string &root, const std::string &path)
		{
			if (root.empty()) return path;
			if (root.back() == '/' && !path.empty() && path.front() == '/') return root + path.substr(1);
			if (root.back() != '/' && (path.empty() || path.front() != '/')) return root + "/" + path;
			return root + path;
		}

		std::string percentEncode(const std::string &text)
		{
			static const char *hex = "0123456789ABCDEF";
			std::string out;
			for (unsigned char c : text)
			{
				const bool safe = std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
				if (safe) out += (char)c;
				else
				{
					out += '%';
					out += hex[c >> 4];
					out += hex[c & 0x0f];
				}
			}
			return out;
		}

#ifdef _WIN32
		std::wstring widen(const std::string &text)
		{
			if (text.empty()) return {};
			const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
			std::wstring wide(size, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), wide.data(), size);
			return wide;
		}

		// Cliente WinHTTP con sesión y conexión reutilizadas: sin eso, cada
		// detalle paga un handshake TLS nuevo y el sync se vuelve eterno.
		class HttpClient
		{
		public:
			explicit HttpClient(const std::string &root)
			{
				std::string rest = root;
				if (startsWith(rest, "https://")) { secure = true; rest = rest.substr(8); }
				else if (startsWith(rest, "http://")) { secure = false; rest = rest.substr(7); }
				const auto slash = rest.find('/');
				std::string hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
				base = slash == std::string::npos ? "" : rest.substr(slash);
				const auto colon = hostPort.find(':');
				if (colon != std::string::npos)
				{
					port = std::atoi(hostPort.substr(colon + 1).c_str());
					hostPort = hostPort.substr(0, colon);
				}
				host = hostPort;
				if (host.empty()) return;
				session = WinHttpOpen(L"GuipperJPSync/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
					WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
				if (!session) return;
				WinHttpSetTimeouts(session, 10000, 20000, 20000, 30000);
				connection = WinHttpConnect(session, widen(host).c_str(), (INTERNET_PORT)port, 0);
			}

			~HttpClient()
			{
				if (connection) WinHttpCloseHandle(connection);
				if (session) WinHttpCloseHandle(session);
			}

			bool valid() const { return connection != nullptr; }

			std::string get(const std::string &path, std::string &error)
			{
				if (!connection) { error = "sin conexion"; return {}; }
				HINTERNET request = WinHttpOpenRequest(connection, L"GET", widen(urlJoin(base, path)).c_str(),
					nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
					secure ? WINHTTP_FLAG_SECURE : 0);
				if (!request) { error = "no se pudo abrir el request"; return {}; }
				std::string body;
				bool ok = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					WINHTTP_NO_REQUEST_DATA, 0, 0, 0) != FALSE;
				if (ok) ok = WinHttpReceiveResponse(request, nullptr) != FALSE;
				DWORD status = 0;
				if (ok)
				{
					DWORD size = sizeof(status);
					WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
						WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
					if (status != 200) { error = "HTTP " + ofToString((int)status); ok = false; }
				}
				else error = "sin respuesta";
				if (ok)
				{
					for (;;)
					{
						DWORD available = 0;
						if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
						std::string chunk(available, '\0');
						DWORD read = 0;
						if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
						chunk.resize(read);
						body += chunk;
					}
					if (body.empty()) { error = "respuesta vacia"; ok = false; }
				}
				WinHttpCloseHandle(request);
				if (!ok) return {};
				return body;
			}

		private:
			HINTERNET session = nullptr;
			HINTERNET connection = nullptr;
			std::string host;
			std::string base;
			int port = 443;
			bool secure = true;
		};
#else
		class HttpClient
		{
		public:
			explicit HttpClient(const std::string &root) : root(root) {}
			bool valid() const { return !root.empty(); }
			std::string get(const std::string &path, std::string &error)
			{
				ofHttpResponse response = ofLoadURL(urlJoin(root, path));
				if (response.status != 200)
				{
					error = "HTTP " + ofToString(response.status);
					return {};
				}
				return response.data;
			}

		private:
			std::string root;
		};
#endif

		void replaceAll(std::string &text, const std::string &from, const std::string &to)
		{
			if (from.empty()) return;
			size_t at = 0;
			while ((at = text.find(from, at)) != std::string::npos)
			{
				text.replace(at, from.size(), to);
				at += to.size();
			}
		}

		// Cambia un identificador completo: `varying` si, `varyingThing` no.
		void replaceToken(std::string &text, const std::string &from, const std::string &to)
		{
			size_t at = 0;
			while ((at = text.find(from, at)) != std::string::npos)
			{
				const size_t end = at + from.size();
				const bool beforeOk = at == 0 || !(std::isalnum((unsigned char)text[at - 1]) || text[at - 1] == '_');
				const bool afterOk = end >= text.size() || !(std::isalnum((unsigned char)text[end]) || text[end] == '_');
				if (beforeOk && afterOk)
				{
					text.replace(at, from.size(), to);
					at += to.size();
				}
				else
				{
					at = end;
				}
			}
		}

		bool mentions(const std::string &text, const std::string &name)
		{
			size_t at = 0;
			while ((at = text.find(name, at)) != std::string::npos)
			{
				const size_t end = at + name.size();
				const bool beforeOk = at == 0 || !(std::isalnum((unsigned char)text[at - 1]) || text[at - 1] == '_');
				const bool afterOk = end >= text.size() || !(std::isalnum((unsigned char)text[end]) || text[end] == '_');
				if (beforeOk && afterOk) return true;
				at = end;
			}
			return false;
		}

		bool declaresFunction(const std::string &text, const std::string &name)
		{
			size_t at = 0;
			while ((at = text.find(name, at)) != std::string::npos)
			{
				const size_t end = at + name.size();
				const bool beforeOk = at == 0 || !(std::isalnum((unsigned char)text[at - 1]) || text[at - 1] == '_');
				const bool afterOk = end >= text.size() || !(std::isalnum((unsigned char)text[end]) || text[end] == '_');
				at = end;
				if (!beforeOk || !afterOk) continue;
				size_t open = end;
				while (open < text.size() && (text[open] == ' ' || text[open] == '\t')) ++open;
				if (open >= text.size() || text[open] != '(') continue;
				const size_t lineStart = text.rfind('\n', at);
				const std::string prefix = text.substr(lineStart == std::string::npos ? 0 : lineStart + 1,
					at - (lineStart == std::string::npos ? 0 : lineStart + 1));
				if (prefix.find("void") != std::string::npos) return true;
			}
			return false;
		}

		bool definesMacro(const std::string &text, const std::string &macro)
		{
			size_t at = 0;
			while ((at = text.find("#define", at)) != std::string::npos)
			{
				size_t cursor = at + 7;
				while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t')) ++cursor;
				if (text.compare(cursor, macro.size(), macro) == 0)
				{
					const size_t end = cursor + macro.size();
					if (end >= text.size() || !(std::isalnum((unsigned char)text[end]) || text[end] == '_'))
						return true;
				}
				at += 7;
			}
			return false;
		}

		// Reconocer si el shader ya declara el nombre, para no declararlo dos
		// veces. Los comentarios no cuentan: media biblioteca documenta el
		// header del motor con lineas `// uniform vec4 lidarPointers[30];`.
		bool declaresName(const std::string &text, const std::string &name)
		{
			std::istringstream lines(text);
			std::string line;
			while (std::getline(lines, line))
			{
				const std::string code = withoutComments(line);
				if (code.find("uniform") == std::string::npos) continue;
				size_t at = 0;
				while ((at = code.find(name, at)) != std::string::npos)
				{
					const size_t end = at + name.size();
					const bool beforeOk = at == 0 ||
						!(std::isalnum((unsigned char)code[at - 1]) || code[at - 1] == '_');
					const bool afterOk = end >= code.size() ||
						!(std::isalnum((unsigned char)code[end]) || code[end] == '_');
					if (beforeOk && afterOk) return true;
					at = end;
				}
			}
			return false;
		}

		// ofBuffer no toma std::string en esta version de oF: el archivo se
		// escribe a mano para no arrastrar la division de lineas de Windows.
		bool writeTextFile(const std::string &path, const std::string &text)
		{
			std::ofstream stream(of::filesystem::path(path), std::ios::binary | std::ios::trunc);
			if (!stream) return false;
			stream.write(text.data(), (std::streamsize)text.size());
			return stream.good();
		}

		// Globals de Guipper en un include: el parser de uniforms del nodo lee
		// el .frag crudo y no expande includes, asi que estos nombres no se
		// convierten en controles. Si vivieran en el .frag, `time` seria un
		// slider y el shader quedaria congelado en ese valor.
		bool writeGlobalHeader(const std::string &directory)
		{
			static const char *kHeader =
				"#version 330\n"
				"// Globals de Guipper para los shaders importados de jpshadereditor.\n"
				"uniform sampler2D feedback;\n"
				"uniform float time;\n"
				"uniform vec4 mouse;\n"
				"uniform vec2 resolution;\n"
				"uniform vec2 window_mouse;\n"
				"uniform float bpm;\n"
				"out vec4 fragColor;\n";
			const std::string path = ofFilePath::join(directory, kHeaderName);
			std::error_code error;
			if (std::filesystem::is_regular_file(path, error))
			{
				const std::string current = ofBufferFromFile(path).getText();
				if (current == kHeader) return true;
			}
			return writeTextFile(path, kHeader);
		}

		bool programLinked(ofShader &shader)
		{
			if (!shader.isLoaded() || !shader.getProgram()) return false;
			GLint linked = GL_FALSE;
			glGetProgramiv(shader.getProgram(), GL_LINK_STATUS, &linked);
			return linked == GL_TRUE;
		}

		ofShaderSettings settingsFor(const std::string &file)
		{
			ofShaderSettings settings;
			settings.shaderFiles[GL_VERTEX_SHADER] = ofToDataPath("shaders/default.vert", true);
			settings.sourceDirectoryPath = ofFilePath::getEnclosingDirectory(ofToDataPath(file, true));
			return settings;
		}
	}

	std::string importDirectory()
	{
		return ofToDataPath("shaders/jpshadereditor", true);
	}

	std::string fileNameFor(const std::string &name)
	{
		std::string out;
		for (unsigned char c : name)
		{
			if (std::isalnum(c) || c == '-' || c == '_') out += (char)c;
			else if (c == ' ' || c == '.') out += '_';
			// Se descartan acentos y simbolos raros: el nombre visible queda en
			// la cabecera del .frag, el archivo solo necesita ser estable.
			else if (c >= 0x80) out += '_';
		}
		out = trim(out);
		while (!out.empty() && out.front() == '_') out.erase(out.begin());
		if (out.empty()) out = "shader";
		if (out.size() > 96) out.resize(96);
		return out;
	}

	std::string translate(const std::string &name, const ofJson &detail, std::string &error)
	{
		error.clear();
		const ofJson passes = detail.contains("passesData") && detail["passesData"].is_object()
			? detail["passesData"] : ofJson::object();
		const int passCounter = detail.value("passCounter", 0);
		if (passCounter > 0 || passes.size() > 1)
		{
			error = "multipass";
			return {};
		}

		std::string code;
		if (detail.contains("contenido") && detail["contenido"].is_string())
			code = detail["contenido"].get<std::string>();
		if (code.empty() && passes.contains("main") && passes["main"].is_object() &&
			passes["main"].contains("code") && passes["main"]["code"].is_string())
			code = passes["main"]["code"].get<std::string>();
		if (code.empty())
		{
			error = "sin codigo";
			return {};
		}
		if (code.size() > 512 * 1024)
		{
			error = "codigo demasiado grande";
			return {};
		}
		if (code.size() >= 3 && (unsigned char)code[0] == 0xEF && (unsigned char)code[1] == 0xBB &&
			(unsigned char)code[2] == 0xBF)
			code = code.substr(3);

		// Valores por defecto publicados por el editor, para que los sliders
		// arranquen donde el autor los dejo y no en un valor al azar.
		std::map<std::string, std::string> defaults;
		if (detail.contains("uniforms") && detail["uniforms"].is_object())
			for (const auto &item : detail["uniforms"].items())
			{
				const std::string literal = numberLiteral(item.value());
				if (!literal.empty()) defaults[item.key()] = literal;
			}
		if (detail.contains("uniformFormats") && detail["uniformFormats"].is_object())
			for (const auto &item : detail["uniformFormats"].items())
			{
				if (!item.value().is_string() || item.value().get<std::string>() != "switch") continue;
				auto found = defaults.find(item.key());
				if (found == defaults.end()) continue;
				found->second = (found->second == "0" || found->second == "0.0" || found->second == "false")
					? "false" : "true";
			}

		// Un shader que ya trae common.frag se respeta: sus globals y helpers
		// vienen de ahi y el preambulo propio los duplicaria.
		const bool usesCommonFrag = code.find("common.frag") != std::string::npos &&
			code.find("#pragma include") != std::string::npos;

		std::string normalized;
		normalized.reserve(code.size());
		for (char c : code)
		{
			if (c == '\r') continue;
			normalized += c;
		}

		std::map<std::string, Declared> declared;
		std::map<std::string, std::string> droppedTypes;
		std::string body;
		body.reserve(normalized.size());
		std::istringstream lines(normalized);
		std::string line;
		std::string pendingUniform;
		while (std::getline(lines, line))
		{
			const std::string trimmed = trim(line);
			const std::string bare = trim(withoutComments(trimmed));

			// El preambulo pone #version; un segundo #version es error. Los
			// precision/extension son de GLES y no aplican a 330. El pragma se
			// reescribe arriba de todo cuando corresponde.
			if (startsWith(trimmed, "#version") || startsWith(trimmed, "#extension") ||
				startsWith(bare, "precision ") || startsWith(trimmed, "#pragma include"))
				continue;
			if (startsWith(bare, "out ") && bare.find("fragColor") != std::string::npos)
				continue;

			if (!pendingUniform.empty())
			{
				pendingUniform += "\n" + line;
				if (trimmed.find(';') != std::string::npos)
				{
					body += rewriteUniform(pendingUniform, defaults, declared, droppedTypes);
					body += "\n";
					pendingUniform.clear();
				}
				continue;
			}
			if (startsWith(bare, "uniform "))
			{
				if (trimmed.find(';') != std::string::npos)
				{
					body += rewriteUniform(trimmed, defaults, declared, droppedTypes);
					body += "\n";
				}
				else
				{
					pendingUniform = line;
				}
				continue;
			}
			body += line;
			body += "\n";
		}
		if (!pendingUniform.empty())
		{
			body += rewriteUniform(pendingUniform, defaults, declared, droppedTypes);
			body += "\n";
		}

		// JSE escribe Shadertoy; Guipper compila GLSL 330 con `fragColor`,
		// `texture()` y `in` en lugar de `varying`.
		replaceAll(body, "texture2DRect", "texture");
		replaceAll(body, "texture2D", "texture");
		replaceAll(body, "textureCube", "texture");
		replaceAll(body, "gl_FragColor", "fragColor");
		replaceAll(body, "gl_FragData[0]", "fragColor");
		replaceToken(body, "varying", "in");

		// Si el original declaraba `mouse` como vec2 (estilo Guipper), las
		// asignaciones `vec2 m = mouse;` explotarian contra el vec4 global.
		const bool mouseIsVec2 = droppedTypes["mouse"] == "vec2";
		if (mouseIsVec2) replaceToken(body, "mouse", "mouse.xy");

		const bool hasMain = declaresFunction(body, "main");
		const bool hasMainImage = declaresFunction(body, "mainImage");
		if (!hasMain && !hasMainImage)
		{
			error = "sin main()";
			return {};
		}

		std::string out;
		if (usesCommonFrag)
			out += "#pragma include \"../common.frag\"\n";
		else
			out += "#pragma include \"" + std::string(kHeaderName) + "\"\n";
		out += "// jpshadereditor · " + name + "\n";
		if (detail.contains("autor") && detail["autor"].is_string())
			out += "// autor: " + detail["autor"].get<std::string>() + "\n";
		out += std::string("// fuente: ") + kPublicUrl + name + "\n";

		if (!usesCommonFrag)
		{
			// Los globals viven en _jse_common.frag (ver kHeaderName): aca solo
			// quedan macros y lo que si es parametro del shader.
			if (!definesMacro(body, "iTime")) out += "#define iTime time\n";
			if (!definesMacro(body, "iFrame")) out += "#define iFrame floor(time*60.)\n";
			if (!definesMacro(body, "iResolution")) out += "#define iResolution vec3(resolution, 1.0)\n";
			if (!definesMacro(body, "iMouse"))
				out += mouseIsVec2 ? "#define iMouse vec4(mouse.xy, 0.0, 0.0)\n" : "#define iMouse mouse\n";
			// `fragCoord` es un parametro en mainImage(..., vec2 fragCoord): si se
			// define como macro, la propia declaracion de la funcion deja de
			// compilar. Solo se define cuando no hay parametro con ese nombre.
			if (!mentions(body, "vec2 fragCoord") && !definesMacro(body, "fragCoord"))
				out += "#define fragCoord gl_FragCoord.xy\n";
			// Lo que el motor de jpshadereditor inyecta antes de compilar y que
			// Guipper no tiene: sin esto, media biblioteca no compila.
			if (mentions(body, "fxrand") && !definesMacro(body, "fxrand") &&
				!declaresName(body, "fxrand"))
				out += "#define fxrand fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453)\n";
			if (mentions(body, "JP_LIDAR_POINTERS_MAX") && !definesMacro(body, "JP_LIDAR_POINTERS_MAX"))
				out += "#define JP_LIDAR_POINTERS_MAX 32\n";
			if (mentions(body, "JP_POINTERS_MAX") && !definesMacro(body, "JP_POINTERS_MAX"))
				out += "#define JP_POINTERS_MAX 32\n";
			if (mentions(body, "lidarPointers") && !declaresName(body, "lidarPointers"))
				out += "uniform vec4 lidarPointers[32];\n";
			if (mentions(body, "lidarPointersCount") && !declaresName(body, "lidarPointersCount"))
				out += "uniform float lidarPointersCount;\n";
			if (mentions(body, "pointers") && !declaresName(body, "pointers"))
				out += "uniform vec4 pointers[32];\n";
			if (mentions(body, "pointersCount") && !declaresName(body, "pointersCount"))
				out += "uniform float pointersCount;\n";
		}

		// Entradas de imagen: solo las que el shader realmente usa y no declara.
		for (int channel = 0; channel < 4; ++channel)
		{
			const std::string channelName = "iChannel" + ofToString(channel);
			if (body.find(channelName) == std::string::npos) continue;
			if (declared.count(channelName) > 0) continue;
			out += "uniform sampler2D " + channelName + ";\n";
		}
		out += body;

		if (!hasMain)
			out += "\nvoid main(){\n\tvec4 jp_jse_color = vec4(0.0, 0.0, 0.0, 1.0);\n"
				   "\tmainImage(jp_jse_color, gl_FragCoord.xy);\n\tfragColor = jp_jse_color;\n}\n";
		return out;
	}

	Sync::~Sync()
	{
		cancel();
	}

	void Sync::start(const std::string &root, const std::string &dir, bool force)
	{
		cancel();
		{
			std::lock_guard<std::mutex> lock(mutex);
			remote.clear();
			outcomeList.clear();
			queue.clear();
			manifest.clear();
			importedNames.clear();
			importedFiles.clear();
			usedFiles.clear();
			totalCount = processedCount = importedCount = failedCount = skippedCount = 0;
			multipassCount = 0;
			failure.clear();
			rescanPending = false;
			finalized = false;
			state = Status::Catalog;
			apiRoot = root;
			directory = dir;
		}
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		if (error)
		{
			std::lock_guard<std::mutex> lock(mutex);
			failure = "no se pudo crear " + directory;
			state = Status::Failed;
			return;
		}
		ofJson stored = ofJson::object();
		try
		{
			const auto file = std::filesystem::path(directory) / kManifestName;
			if (std::filesystem::is_regular_file(file))
				stored = ofJson::parse(ofBufferFromFile(file).getText());
		}
		catch (const std::exception &error)
		{
			ofLogWarning("jpshadereditor") << "manifest ilegible: " << error.what();
		}
		// Un manifiesto de otra version del traductor se reimporta entero: es la
		// unica forma de que una mejora en la traduccion llegue a los ya bajados.
		if (stored.value("version", 0) != kTranslatorVersion) force = true;
		writeGlobalHeader(directory);
		std::map<std::string, std::string> known;
		std::map<std::string, std::string> knownFiles;
		if (stored.contains("shaders") && stored["shaders"].is_object())
			for (const auto &item : stored["shaders"].items())
			{
				if (!item.value().is_object()) continue;
				manifest[item.key()] = item.value().value("updated", std::string());
				known[item.key()] = item.value().value("updated", std::string());
				knownFiles[item.key()] = item.value().value("file", std::string());
			}
		cancelFlag = false;
		workerDone = false;
		worker = std::thread(&Sync::workerLoop, this, root, known, knownFiles, force);
	}

	void Sync::cancel()
	{
		cancelFlag = true;
		if (worker.joinable()) worker.join();
		std::lock_guard<std::mutex> lock(mutex);
		queue.clear();
		if (state == Status::Catalog || state == Status::Importing) state = Status::Idle;
	}

	void Sync::workerLoop(std::string root, std::map<std::string, std::string> known,
		std::map<std::string, std::string> knownFiles, bool force)
	{
		HttpClient client(root);
		std::string error;
		if (!client.valid())
		{
			std::lock_guard<std::mutex> lock(mutex);
			failure = "no se pudo inicializar la conexion";
			state = Status::Failed;
			workerDone = true;
			return;
		}
		const std::string body = client.get("/shaders", error);
		if (body.empty())
		{
			std::lock_guard<std::mutex> lock(mutex);
			failure = error.empty() ? "catalogo vacio" : error;
			state = Status::Failed;
			workerDone = true;
			return;
		}
		ofJson index;
		try
		{
			index = ofJson::parse(body);
		}
		catch (const std::exception &exception)
		{
			std::lock_guard<std::mutex> lock(mutex);
			failure = std::string("json del catalogo: ") + exception.what();
			state = Status::Failed;
			workerDone = true;
			return;
		}
		if (!index.is_array())
		{
			std::lock_guard<std::mutex> lock(mutex);
			failure = "el catalogo no es una lista";
			state = Status::Failed;
			workerDone = true;
			return;
		}

		std::vector<RemoteShader> entries;
		for (const auto &item : index)
		{
			RemoteShader entry;
			entry.name = item.value("nombre", std::string());
			if (entry.name.empty()) continue;
			entry.author = item.value("autor", std::string());
			entry.updated = item.value("fechaActualizacion", std::string());
			entry.thumbnail = item.value("imagePath", std::string());
			entry.passes = item.value("passCounter", 0);
			entry.compiles = item.contains("compiles") && item["compiles"].is_boolean()
				? item["compiles"].get<bool>() : true;
			entries.push_back(entry);
		}
		std::sort(entries.begin(), entries.end(),
			[](const RemoteShader &a, const RemoteShader &b) { return lowerCase(a.name) < lowerCase(b.name); });

		int upToDate = 0;
		int candidates = 0;
		for (const auto &entry : entries)
		{
			if (entry.passes > 0) { ++upToDate; continue; } // multipass: etapa siguiente
			++candidates;
			const auto found = known.find(entry.name);
			const auto fileIt = knownFiles.find(entry.name);
			const std::string leaf = fileIt != knownFiles.end() && !fileIt->second.empty()
				? fileIt->second : fileNameFor(entry.name) + ".frag";
			const std::string file = ofFilePath::join(directory, leaf);
			if (!force && found != known.end() && found->second == entry.updated &&
				std::filesystem::is_regular_file(ofToDataPath(file, false)))
				++upToDate;
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			remote = entries;
			totalCount = candidates;
			skippedCount = upToDate;
			processedCount = skippedCount;
			state = Status::Importing;
		}

		for (const auto &entry : entries)
		{
			if (cancelFlag) break;
			if (entry.passes > 0) continue;
			const auto found = known.find(entry.name);
			const auto fileIt = knownFiles.find(entry.name);
			const std::string leaf = fileIt != knownFiles.end() && !fileIt->second.empty()
				? fileIt->second : fileNameFor(entry.name) + ".frag";
			const std::string file = ofFilePath::join(directory, leaf);
			if (!force && found != known.end() && found->second == entry.updated &&
				std::filesystem::is_regular_file(ofToDataPath(file, false)))
				continue;
			Fetch fetch;
			fetch.name = entry.name;
			std::string fetchError;
			fetch.body = client.get("/shaders/" + percentEncode(entry.name), fetchError);
			fetch.error = fetch.body.empty() ? (fetchError.empty() ? "sin detalle" : fetchError) : std::string();
			std::lock_guard<std::mutex> lock(mutex);
			queue.push_back(std::move(fetch));
		}
		workerDone = true;
	}

	void Sync::update()
	{
		if (state == Status::Idle || state == Status::Finished || state == Status::Failed) return;
		std::deque<Fetch> work;
		{
			std::lock_guard<std::mutex> lock(mutex);
			for (int i = 0; i < kMaxImportsPerFrame && !queue.empty(); ++i)
			{
				work.push_back(std::move(queue.front()));
				queue.pop_front();
			}
		}
		for (const auto &fetch : work) importFetch(fetch);
		const bool done = workerDone.load();
		if (done && work.empty())
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (queue.empty() && !finalized) finish();
		}
	}

	void Sync::importFetch(const Fetch &fetch)
	{
		ImportResult result;
		result.name = fetch.name;
		std::string failureText = fetch.error;
		if (failureText.empty())
		{
			ofJson detail;
			try
			{
				detail = ofJson::parse(fetch.body);
			}
			catch (const std::exception &error)
			{
				failureText = std::string("json: ") + error.what();
			}
			if (failureText.empty())
			{
				std::string translateError;
				const std::string source = translate(fetch.name, detail, translateError);
				result.multipass = translateError == "multipass";
				if (source.empty())
				{
					failureText = translateError.empty() ? "traduccion vacia" : translateError;
				}
				else
				{
					// Dos shaders distintos pueden sanear al mismo archivo:
					// el segundo entra con sufijo en vez de pisar al primero.
					const std::string base = fileNameFor(fetch.name);
					std::string leaf = base + ".frag";
					for (int suffix = 2; suffix < 100 && usedFiles.count(lowerCase(leaf)) > 0; ++suffix)
						leaf = base + "-" + ofToString(suffix) + ".frag";
					usedFiles.insert(lowerCase(leaf));
					const std::string file = ofFilePath::join(directory, leaf);
					const std::string absolute = ofToDataPath(file, true);
					ofShaderSettings settings = settingsFor(absolute);
					settings.shaderSources[GL_FRAGMENT_SHADER] = source;
					ofShader candidate;
					bool compiled = false;
					if (writeTextFile(absolute, source))
					{
						compiled = candidate.setup(settings) && programLinked(candidate);
						if (compiled)
						{
							for (const auto &declaration : jp_uniform_parser::parse(source).declarations)
								if ((declaration.type == jp_uniform_parser::Type::Float ||
									declaration.type == jp_uniform_parser::Type::Bool) &&
									!jp_shader_globals::isGlobalName(declaration.name))
									++result.parameters;
							smokeRender(candidate);
						}
					}
					else
					{
						failureText = "no se pudo escribir " + file;
					}
					if (!failureText.empty() || !compiled)
					{
						if (failureText.empty()) failureText = "no compila";
						// Deja el archivo en disco para diagnosticar, sin que
						// cuente como importado en el manifiesto.
						if (!std::getenv("GUIPPER_JSE_KEEP_FAILED"))
						{
							std::error_code ignored;
							std::filesystem::remove(absolute, ignored);
						}
					}
					else
					{
						result.file = file;
						result.updated = true;
					}
				}
			}
		}

		std::lock_guard<std::mutex> lock(mutex);
		++processedCount;
		if (result.file.empty())
		{
			// Multipass es una etapa pendiente, no un shader roto: no se mezclan
			// en el mismo numero que los que no compilan.
			if (result.multipass) ++multipassCount;
			else ++failedCount;
			result.error = failureText;
			ofLogWarning("jpshadereditor") << "import fallo " << fetch.name << ": " << failureText;
		}
		else
		{
			++importedCount;
			importedNames.insert(fetch.name);
			importedFiles[fetch.name] = ofFilePath::getFileName(result.file);
			ofLogNotice("jpshadereditor") << "importado " << fetch.name << " -> " << result.file;
		}
		outcomeList.push_back(result);
	}

	void Sync::finish()
	{
		finalized = true;
		state = Status::Finished;
		// Un solo rescan al final: la lista se rearma una vez y no en cada
		// importacion, que con 600 shaders costaba un barrido de disco por frame.
		rescanPending = importedCount > 0;
		// Rehacer el manifiesto desde el catalogo: nombre -> fecha publicada.
		ofJson shaders = ofJson::object();
		for (const auto &entry : remote)
		{
			if (entry.passes > 0) continue;
			if (importedNames.count(entry.name) == 0) continue;
			ofJson item = ofJson::object();
			item["updated"] = entry.updated;
			item["file"] = importedFiles[entry.name];
			shaders[entry.name] = item;
		}
		ofJson payload;
		payload["version"] = kTranslatorVersion;
		payload["api"] = apiRoot;
		payload["generated"] = ofGetTimestampString("%Y-%m-%d %H:%M:%S");
		payload["shaders"] = shaders;
		writeTextFile(ofFilePath::join(directory, kManifestName), payload.dump(1));
		ofLogNotice("jpshadereditor") << "sync listo: " << importedCount << " importados, "
									  << failedCount << " fallidos, " << skippedCount << " al dia";
	}

	bool Sync::smokeRender(ofShader &shader) const
	{
		if (!smoke.isAllocated())
			smoke.allocate(64, 64, GL_RGBA);
		if (!smoke.isAllocated()) return false;
		ofPushStyle();
		smoke.begin();
		ofClear(0, 0, 0, 255);
		shader.begin();
		JPShaderGlobalsCtx context;
		context.width = 64;
		context.height = 64;
		context.liveMouse = false;
		jp_shader_globals::apply(shader, context);
		ofSetColor(255);
		ofDrawRectangle(0, 0, 64, 64);
		shader.end();
		smoke.end();
		ofPopStyle();
		const bool clean = glGetError() == GL_NO_ERROR;
		if (!clean) ofLogWarning("jpshadereditor") << "el dibujo de prueba dejo un error GL";
		return clean;
	}

	bool Sync::takeRescan()
	{
		std::lock_guard<std::mutex> lock(mutex);
		const bool pending = rescanPending;
		rescanPending = false;
		return pending;
	}

	Sync::Status Sync::status() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return state;
	}

	bool Sync::running() const
	{
		const Status current = status();
		return current == Status::Catalog || current == Status::Importing;
	}

	int Sync::total() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return totalCount;
	}

	int Sync::processed() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return processedCount;
	}

	int Sync::imported() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return importedCount;
	}

	int Sync::failed() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return failedCount;
	}

	int Sync::multipass() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return multipassCount;
	}

	int Sync::skipped() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return skippedCount;
	}

	std::string Sync::lastError() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return failure;
	}

	std::string Sync::statusLine(bool spanish) const
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (state == Status::Failed) return failure.empty() ? (spanish ? "error de sync" : "sync failed") : failure;
		if (state == Status::Catalog) return spanish ? "leyendo catalogo" : "reading catalog";
		if (state == Status::Idle) return {};
		std::string line = (spanish ? "IMPORTANDO " : "IMPORTING ") +
			ofToString(processedCount) + "/" + ofToString(totalCount);
		line += "   " + ofToString(importedCount) + (spanish ? " ok" : " ok");
		if (failedCount > 0) line += " · " + ofToString(failedCount) + (spanish ? " fallo" : " failed");
		if (multipassCount > 0) line += " · " + ofToString(multipassCount) + " multipass";
		if (skippedCount > 0) line += " · " + ofToString(skippedCount) + (spanish ? " al dia" : " up to date");
		return line;
	}

	std::vector<ImportResult> Sync::results() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return outcomeList;
	}

	std::vector<RemoteShader> Sync::catalog() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		return remote;
	}

	int Sync::removeImported(const std::string &dir)
	{
		ofJson stored = ofJson::object();
		const auto file = std::filesystem::path(dir) / kManifestName;
		try
		{
			if (std::filesystem::is_regular_file(file))
				stored = ofJson::parse(ofBufferFromFile(file).getText());
		}
		catch (const std::exception &)
		{
			return 0;
		}
		int removed = 0;
		if (stored.contains("shaders") && stored["shaders"].is_object())
			for (const auto &item : stored["shaders"].items())
			{
				if (!item.value().is_object()) continue;
				const std::string name = item.value().value("file", std::string());
				if (name.empty() || name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
					continue;
				std::error_code error;
				if (std::filesystem::remove(std::filesystem::path(dir) / name, error)) ++removed;
			}
		std::error_code ignored;
		std::filesystem::remove(std::filesystem::path(dir) / kHeaderName, ignored);
		std::filesystem::remove(file, ignored);
		std::lock_guard<std::mutex> lock(mutex);
		manifest.clear();
		return removed;
	}
}
