#include "jp_box_webrender.h"
#include "../JPutils/jp_text_input.h"
#include "../JPutils/jp_textwrap.h"
#include "../JPutils/jp_tooltip.h"
#include "../JPgui/jp_gl_state.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincodec.h>
#include <objbase.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

namespace
{
	struct DecodedWebFrame
	{
		int width = 0;
		int height = 0;
		uint64_t serial = 0;
		std::vector<uint8_t> rgba;
	};

	std::string trimWhitespace(const std::string &input)
	{
		size_t start = 0;
		while (start < input.size() &&
			   (std::isspace(static_cast<unsigned char>(input[start])) || input[start] == '"' || input[start] == '\''))
		{
			++start;
		}
		size_t end = input.size();
		while (end > start &&
			   (std::isspace(static_cast<unsigned char>(input[end - 1])) || input[end - 1] == '"' || input[end - 1] == '\''))
		{
			--end;
		}
		return input.substr(start, end - start);
	}

	std::string normalizeWebUrl(const std::string &rawInput)
	{
		std::string s = trimWhitespace(rawInput);
		if (s.empty()) return "";

		std::string lower = ofToLower(s);
		if (lower.rfind("http://", 0) == 0 ||
			lower.rfind("https://", 0) == 0 ||
			lower.rfind("file://", 0) == 0 ||
			lower.rfind("data:", 0) == 0 ||
			lower.rfind("about:", 0) == 0 ||
			lower.rfind("chrome://", 0) == 0 ||
			lower.rfind("edge://", 0) == 0)
		{
			return s;
		}

		try
		{
			std::filesystem::path p(s);
			if (p.is_absolute() && std::filesystem::exists(p))
			{
				std::string generic = p.generic_string();
				if (!generic.empty() && generic[0] != '/')
					return "file:///" + generic;
				return "file://" + generic;
			}
		}
		catch (...)
		{
		}

		if (lower.rfind("localhost", 0) == 0 ||
			lower.rfind("127.0.0.1", 0) == 0 ||
			lower.rfind("0.0.0.0", 0) == 0 ||
			lower.rfind("192.168.", 0) == 0 ||
			lower.rfind("10.", 0) == 0)
		{
			return "http://" + s;
		}

		return "https://" + s;
	}

	std::string jsonEscape(const std::string &s)
	{
		std::string out;
		out.reserve(s.size() + 16);
		for (unsigned char c : s)
		{
			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20)
				{
					char buf[8];
					snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
					out += buf;
				}
				else
				{
					out.push_back(static_cast<char>(c));
				}
				break;
			}
		}
		return out;
	}

	void base64Decode(const std::string &in, std::vector<uint8_t> &out)
	{
		static const uint8_t kDecTable[256] = {
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
			52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,
			64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
			15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
			64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
			41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
			64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
		};

		out.clear();
		out.reserve((in.size() * 3) / 4 + 4);
		uint32_t val = 0;
		int bits = -8;
		for (unsigned char c : in)
		{
			uint8_t d = kDecTable[c];
			if (d == 64) continue;
			val = (val << 6) | d;
			bits += 6;
			if (bits >= 0)
			{
				out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
				bits -= 8;
			}
		}
	}

#ifdef _WIN32
	std::filesystem::path findChromiumExecutable()
	{
		std::vector<std::filesystem::path> candidates = {
			R"(C:\Program Files\Google\Chrome\Application\chrome.exe)",
			R"(C:\Program Files (x86)\Google\Chrome\Application\chrome.exe)",
			R"(C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe)",
			R"(C:\Program Files\Microsoft\Edge\Application\msedge.exe)"
		};

		if (const char *localAppData = std::getenv("LOCALAPPDATA"))
		{
			std::filesystem::path local(localAppData);
			candidates.push_back(local / "Google" / "Chrome" / "Application" / "chrome.exe");
			candidates.push_back(local / "Microsoft" / "Edge" / "Application" / "msedge.exe");
		}

		for (const auto &candidate : candidates)
		{
			std::error_code ec;
			if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
			{
				return candidate;
			}
		}
		return {};
	}

	bool decodeJpegWithWic(IWICImagingFactory *factory,
						   const uint8_t *jpegData,
						   size_t jpegSize,
						   DecodedWebFrame &outFrame)
	{
		if (!factory || !jpegData || jpegSize == 0) return false;

		IWICStream *stream = nullptr;
		HRESULT hr = factory->CreateStream(&stream);
		if (FAILED(hr) || !stream) return false;

		hr = stream->InitializeFromMemory(const_cast<BYTE *>(jpegData), static_cast<DWORD>(jpegSize));
		if (FAILED(hr))
		{
			stream->Release();
			return false;
		}

		IWICBitmapDecoder *decoder = nullptr;
		hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
		if (FAILED(hr) || !decoder)
		{
			stream->Release();
			return false;
		}

		IWICBitmapFrameDecode *frame = nullptr;
		hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr) || !frame)
		{
			decoder->Release();
			stream->Release();
			return false;
		}

		UINT width = 0, height = 0;
		hr = frame->GetSize(&width, &height);
		if (FAILED(hr) || width == 0 || height == 0)
		{
			frame->Release();
			decoder->Release();
			stream->Release();
			return false;
		}

		IWICFormatConverter *converter = nullptr;
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr) || !converter)
		{
			frame->Release();
			decoder->Release();
			stream->Release();
			return false;
		}

		hr = converter->Initialize(
			frame,
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
		{
			converter->Release();
			frame->Release();
			decoder->Release();
			stream->Release();
			return false;
		}

		const UINT stride = width * 4;
		const UINT bufferSize = stride * height;
		outFrame.width = static_cast<int>(width);
		outFrame.height = static_cast<int>(height);
		outFrame.rgba.resize(bufferSize);

		hr = converter->CopyPixels(nullptr, stride, bufferSize, outFrame.rgba.data());

		converter->Release();
		frame->Release();
		decoder->Release();
		stream->Release();

		return SUCCEEDED(hr);
	}
#endif
}

struct JPbox_webrender::Impl
{
	struct Command
	{
		enum class Type
		{
			Navigate,
			Reload,
			Back,
			Forward,
			Resize,
			ScrollZoom,
			MouseMove,
			MouseClick,
			SetMuted
		};
		Type type = Type::Navigate;
		std::string text;
		int width = 1280;
		int height = 720;
		float a = 0.0f;
		float b = 0.0f;
		float c = 1.0f;
		bool flag = false;
	};

	std::atomic<bool> running{false};
	std::thread ioThread;
	std::thread decodeThread;

	mutable std::mutex stateMutex;
	std::string statusText = "Listo: pega un link y presiona Enter";
	std::string currentUrl;
	std::deque<Command> commandQueue;
	std::shared_ptr<const DecodedWebFrame> latestFrame;
	uint64_t nextFrameSerial = 0;

	std::mutex decodeMutex;
	std::condition_variable decodeCv;
	std::string pendingBase64Jpeg;
	bool hasPendingJpeg = false;

	int targetWidth = 1280;
	int targetHeight = 720;

#ifdef _WIN32
	HANDLE jobHandle = nullptr;
	HANDLE processHandle = nullptr;
#endif
	std::filesystem::path userDataDir;

	~Impl()
	{
		stop();
	}

	void setStatus(const std::string &s)
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		statusText = s;
	}

	std::string getStatus() const
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		return statusText;
	}

	void pushCommand(Command cmd)
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		// Coalesce high-frequency continuous commands so the queue stays tiny
		if (cmd.type == Command::Type::ScrollZoom ||
			cmd.type == Command::Type::MouseMove ||
			cmd.type == Command::Type::Resize)
		{
			for (auto &existing : commandQueue)
			{
				if (existing.type == cmd.type)
				{
					existing = std::move(cmd);
					return;
				}
			}
		}
		commandQueue.push_back(std::move(cmd));
	}

	std::shared_ptr<const DecodedWebFrame> getLatestFrame() const
	{
		std::lock_guard<std::mutex> lock(stateMutex);
		return latestFrame;
	}

	void submitEncodedFrame(std::string &&base64Data)
	{
		{
			std::lock_guard<std::mutex> lock(decodeMutex);
			pendingBase64Jpeg = std::move(base64Data);
			hasPendingJpeg = true;
		}
		decodeCv.notify_one();
	}

	void start(int initialWidth, int initialHeight, const std::string &initialUrl, bool initialMuted)
	{
		if (running.load()) return;
		targetWidth = std::clamp(initialWidth, 320, 1920);
		targetHeight = std::clamp(initialHeight, 240, 1080);
		running.store(true);

		if (!initialUrl.empty())
		{
			Command nav;
			nav.type = Command::Type::Navigate;
			nav.text = initialUrl;
			pushCommand(std::move(nav));
		}
		if (initialMuted)
		{
			Command m;
			m.type = Command::Type::SetMuted;
			m.flag = true;
			pushCommand(std::move(m));
		}

		decodeThread = std::thread([this] { decodeLoop(); });
		ioThread = std::thread([this] { ioLoop(); });
	}

	void stop()
	{
		if (!running.exchange(false)) return;

		decodeCv.notify_all();
		if (ioThread.joinable()) ioThread.join();
		if (decodeThread.joinable()) decodeThread.join();

		cleanupProcess();
	}

	void cleanupProcess()
	{
#ifdef _WIN32
		if (processHandle)
		{
			TerminateProcess(processHandle, 0);
			WaitForSingleObject(processHandle, 500);
			CloseHandle(processHandle);
			processHandle = nullptr;
		}
		if (jobHandle)
		{
			CloseHandle(jobHandle);
			jobHandle = nullptr;
		}
#endif
		if (!userDataDir.empty())
		{
			for (int attempt = 0; attempt < 5; ++attempt)
			{
				std::error_code ec;
				std::filesystem::remove_all(userDataDir, ec);
				if (!std::filesystem::exists(userDataDir, ec)) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(25));
			}
			userDataDir.clear();
		}
	}

	void decodeLoop()
	{
#ifdef _WIN32
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		IWICImagingFactory *wicFactory = nullptr;
		CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&wicFactory));
#endif
		std::vector<uint8_t> jpegBytes;

		while (running.load())
		{
			std::string base64Data;
			{
				std::unique_lock<std::mutex> lock(decodeMutex);
				decodeCv.wait(lock, [this] { return !running.load() || hasPendingJpeg; });
				if (!running.load()) break;
				base64Data = std::move(pendingBase64Jpeg);
				hasPendingJpeg = false;
			}

			if (base64Data.empty()) continue;
			base64Decode(base64Data, jpegBytes);
			if (jpegBytes.empty()) continue;

			auto decoded = std::make_shared<DecodedWebFrame>();
			bool ok = false;
#ifdef _WIN32
			if (wicFactory)
			{
				ok = decodeJpegWithWic(wicFactory, jpegBytes.data(), jpegBytes.size(), *decoded);
			}
#endif
			if (!ok)
			{
				ofBuffer buf(reinterpret_cast<const char *>(jpegBytes.data()), jpegBytes.size());
				ofPixels pix;
				if (ofLoadImage(pix, buf))
				{
					pix.setImageType(OF_IMAGE_COLOR_ALPHA);
					decoded->width = static_cast<int>(pix.getWidth());
					decoded->height = static_cast<int>(pix.getHeight());
					const size_t byteCount = pix.size();
					decoded->rgba.assign(pix.getData(), pix.getData() + byteCount);
					ok = (decoded->width > 0 && decoded->height > 0);
				}
			}

			if (ok && decoded->width > 0 && decoded->height > 0)
			{
				std::lock_guard<std::mutex> lock(stateMutex);
				decoded->serial = ++nextFrameSerial;
				latestFrame = std::move(decoded);
			}
		}

#ifdef _WIN32
		if (wicFactory) wicFactory->Release();
		CoUninitialize();
#endif
	}

#ifdef _WIN32
	static SOCKET connectLocalPort(int port)
	{
		SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET) return INVALID_SOCKET;

		BOOL noDelay = TRUE;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&noDelay), sizeof(noDelay));
		int rcvBuf = 4 * 1024 * 1024;
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&rcvBuf), sizeof(rcvBuf));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<u_short>(port));
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

		if (connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR)
		{
			closesocket(s);
			return INVALID_SOCKET;
		}
		return s;
	}

	static bool sendAll(SOCKET s, const uint8_t *data, size_t len)
	{
		size_t sentTotal = 0;
		while (sentTotal < len)
		{
			int n = send(s, reinterpret_cast<const char *>(data + sentTotal), static_cast<int>(len - sentTotal), 0);
			if (n <= 0) return false;
			sentTotal += static_cast<size_t>(n);
		}
		return true;
	}

	static bool sendWsText(SOCKET s, const std::string &payload)
	{
		std::vector<uint8_t> frame;
		const size_t len = payload.size();
		frame.reserve(len + 14);
		frame.push_back(0x81); // FIN + text frame

		if (len < 126)
		{
			frame.push_back(static_cast<uint8_t>(0x80 | len));
		}
		else if (len <= 65535)
		{
			frame.push_back(static_cast<uint8_t>(0x80 | 126));
			frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
			frame.push_back(static_cast<uint8_t>(len & 0xFF));
		}
		else
		{
			frame.push_back(static_cast<uint8_t>(0x80 | 127));
			uint64_t l64 = static_cast<uint64_t>(len);
			for (int i = 7; i >= 0; --i)
				frame.push_back(static_cast<uint8_t>((l64 >> (i * 8)) & 0xFF));
		}

		const uint8_t mask[4] = {0x37, 0xFA, 0x21, 0x3D};
		frame.push_back(mask[0]);
		frame.push_back(mask[1]);
		frame.push_back(mask[2]);
		frame.push_back(mask[3]);

		for (size_t i = 0; i < len; ++i)
		{
			frame.push_back(static_cast<uint8_t>(payload[i]) ^ mask[i & 3]);
		}

		return sendAll(s, frame.data(), frame.size());
	}

	static std::string findPageWebSocketPath(int port)
	{
		SOCKET s = connectLocalPort(port);
		if (s == INVALID_SOCKET) return {};

		DWORD timeoutMs = 1500;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));

		std::string req =
			"GET /json/list HTTP/1.1\r\n"
			"Host: 127.0.0.1:" + std::to_string(port) + "\r\n"
			"Connection: close\r\n\r\n";
		sendAll(s, reinterpret_cast<const uint8_t *>(req.data()), req.size());

		std::string response;
		char buf[4096];
		for (;;)
		{
			int n = recv(s, buf, sizeof(buf), 0);
			if (n <= 0) break;
			response.append(buf, static_cast<size_t>(n));
		}
		closesocket(s);

		const auto headerEnd = response.find("\r\n\r\n");
		if (headerEnd == std::string::npos) return {};
		const std::string body = response.substr(headerEnd + 4);

		try
		{
			auto j = ofJson::parse(body);
			if (j.is_array())
			{
				for (const auto &item : j)
				{
					if (item.value("type", std::string()) == "page")
					{
						std::string wsUrl = item.value("webSocketDebuggerUrl", std::string());
						const auto slashPos = wsUrl.find("/devtools/");
						if (slashPos != std::string::npos)
							return wsUrl.substr(slashPos);
					}
				}
			}
		}
		catch (...)
		{
		}
		return {};
	}
#endif

	void ioLoop()
	{
#ifdef _WIN32
		WSADATA wsaData{};
		WSAStartup(MAKEWORD(2, 2), &wsaData);

		const auto browserExe = findChromiumExecutable();
		if (browserExe.empty())
		{
			setStatus("Error: No se encontro Chrome ni Microsoft Edge instalado.");
			WSACleanup();
			return;
		}

		setStatus("Iniciando motor WebRender (GPU)...");

		std::error_code ec;
		userDataDir = std::filesystem::temp_directory_path(ec) / ("guipper_webrender_" + jp_boxuid::mint());
		std::filesystem::create_directories(userDataDir, ec);

		jobHandle = CreateJobObjectW(nullptr, nullptr);
		if (jobHandle)
		{
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
			jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			SetInformationJobObject(jobHandle, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
		}

		int currentW = targetWidth;
		int currentH = targetHeight;

		std::wstring cmdLine = L"\"" + browserExe.wstring() + L"\" "
			L"--headless=new "
			L"--use-angle=d3d11 "
			L"--enable-webgl "
			L"--ignore-gpu-blocklist "
			L"--enable-gpu-rasterization "
			L"--enable-zero-copy "
			L"--autoplay-policy=no-user-gesture-required "
			L"--hide-scrollbars "
			L"--disable-background-timer-throttling "
			L"--disable-backgrounding-occluded-windows "
			L"--disable-renderer-backgrounding "
			L"--disable-infobars "
			L"--disable-notifications "
			L"--no-first-run "
			L"--no-default-browser-check "
			L"--remote-debugging-port=0 "
			L"--remote-allow-origins=* "
			L"--window-size=" + std::to_wstring(currentW) + L"," + std::to_wstring(currentH) + L" "
			L"--user-data-dir=\"" + userDataDir.wstring() + L"\" "
			L"about:blank";

		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		PROCESS_INFORMATION pi{};

		std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
		mutableCmd.push_back(L'\0');

		BOOL created = CreateProcessW(
			browserExe.c_str(),
			mutableCmd.data(),
			nullptr,
			nullptr,
			FALSE,
			CREATE_NO_WINDOW | CREATE_SUSPENDED,
			nullptr,
			nullptr,
			&si,
			&pi);

		if (!created)
		{
			setStatus("Error: No se pudo iniciar el proceso del navegador.");
			cleanupProcess();
			WSACleanup();
			return;
		}

		processHandle = pi.hProcess;
		if (jobHandle)
		{
			AssignProcessToJobObject(jobHandle, processHandle);
		}
		ResumeThread(pi.hThread);
		CloseHandle(pi.hThread);

		// Wait for DevToolsActivePort file
		const auto portFile = userDataDir / "DevToolsActivePort";
		int debugPort = 0;
		for (int attempt = 0; attempt < 400 && running.load(); ++attempt)
		{
			if (std::filesystem::exists(portFile, ec))
			{
				std::ifstream in(portFile);
				std::string line;
				if (std::getline(in, line))
				{
					try
					{
						int p = std::stoi(line);
						if (p > 0 && p < 65536)
						{
							debugPort = p;
							break;
						}
					}
					catch (...)
					{
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}

		if (!running.load() || debugPort == 0)
		{
			if (running.load()) setStatus("Error: Tiempo agotado esperando puerto DevTools.");
			cleanupProcess();
			WSACleanup();
			return;
		}

		// Discover page target WebSocket path
		std::string wsPath;
		for (int attempt = 0; attempt < 200 && running.load(); ++attempt)
		{
			wsPath = findPageWebSocketPath(debugPort);
			if (!wsPath.empty()) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}

		if (!running.load() || wsPath.empty())
		{
			if (running.load()) setStatus("Error: No se pudo conectar al destino de pagina.");
			cleanupProcess();
			WSACleanup();
			return;
		}

		SOCKET ws = connectLocalPort(debugPort);
		if (ws == INVALID_SOCKET)
		{
			setStatus("Error: Fallo la conexion WebSocket local.");
			cleanupProcess();
			WSACleanup();
			return;
		}

		std::string upgradeReq =
			"GET " + wsPath + " HTTP/1.1\r\n"
			"Host: 127.0.0.1:" + std::to_string(debugPort) + "\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n";

		if (!sendAll(ws, reinterpret_cast<const uint8_t *>(upgradeReq.data()), upgradeReq.size()))
		{
			closesocket(ws);
			cleanupProcess();
			WSACleanup();
			return;
		}

		std::vector<uint8_t> rxBuffer;
		rxBuffer.reserve(256 * 1024);
		bool upgraded = false;
		while (running.load() && !upgraded)
		{
			uint8_t tmp[4096];
			int n = recv(ws, reinterpret_cast<char *>(tmp), sizeof(tmp), 0);
			if (n <= 0) break;
			rxBuffer.insert(rxBuffer.end(), tmp, tmp + n);
			for (size_t i = 0; i + 3 < rxBuffer.size(); ++i)
			{
				if (rxBuffer[i] == '\r' && rxBuffer[i + 1] == '\n' &&
					rxBuffer[i + 2] == '\r' && rxBuffer[i + 3] == '\n')
				{
					rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + static_cast<ptrdiff_t>(i + 4));
					upgraded = true;
					break;
				}
			}
		}

		if (!upgraded || !running.load())
		{
			closesocket(ws);
			cleanupProcess();
			WSACleanup();
			return;
		}

		int nextCdpId = 1;
		auto sendCdp = [&](const std::string &method, const std::string &paramsJson = "{}") -> int {
			int id = nextCdpId++;
			std::string msg = "{\"id\":" + std::to_string(id) +
							  ",\"method\":\"" + method +
							  "\",\"params\":" + paramsJson + "}";
			sendWsText(ws, msg);
			return id;
		};

		auto startScreencast = [&]() {
			std::string params = "{\"format\":\"jpeg\",\"quality\":85,\"maxWidth\":" +
								 std::to_string(currentW) + ",\"maxHeight\":" +
								 std::to_string(currentH) + ",\"everyNthFrame\":1}";
			sendCdp("Page.startScreencast", params);
		};

		sendCdp("Page.enable");
		sendCdp("Runtime.enable");
		// Use a standard desktop Chrome User-Agent so websites don't block HeadlessChrome
		sendCdp("Emulation.setUserAgentOverride",
				"{\"userAgent\":\"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36\"}");
		sendCdp("Emulation.setDeviceMetricsOverride",
				"{\"width\":" + std::to_string(currentW) +
				",\"height\":" + std::to_string(currentH) +
				",\"deviceScaleFactor\":1,\"mobile\":false}");
		// Hide scrollbars cleanly across navigated documents
		sendCdp("Page.addScriptToEvaluateOnNewDocument",
				"{\"source\":\"try{const s=document.createElement('style');s.textContent='::-webkit-scrollbar{display:none!important;}';document.documentElement.appendChild(s);}catch(e){}\"}");
		startScreencast();

		setStatus("Motor listo · Pega cualquier link y presiona Enter");

		bool screenshotInFlight = false;
		int pendingScreenshotId = -1;
		auto lastFrameTime = std::chrono::steady_clock::now();
		auto lastScreenshotTime = std::chrono::steady_clock::now();
		auto burstUntilTime = std::chrono::steady_clock::now();
		bool hasNavigatedUrl = false;
		std::string activeNavigatedUrl;
		std::string fragmentedMessage;

		auto requestBurst = [&](int durationMs = 3000) {
			burstUntilTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
		};

		auto handleWsMessage = [&](const std::string &msg) {
			if (msg.find("\"method\":\"Page.screencastFrame\"") != std::string::npos)
			{
				// 1. Extract sessionId and immediately ACK to keep compositor at 60fps
				const auto sidPos = msg.find("\"sessionId\":");
				if (sidPos != std::string::npos)
				{
					size_t p = sidPos + 12;
					while (p < msg.size() && (msg[p] == ' ' || msg[p] == '\t')) ++p;
					int sessionId = 0;
					while (p < msg.size() && msg[p] >= '0' && msg[p] <= '9')
					{
						sessionId = sessionId * 10 + (msg[p] - '0');
						++p;
					}
					if (sessionId > 0)
					{
						sendCdp("Page.screencastFrameAck", "{\"sessionId\":" + std::to_string(sessionId) + "}");
					}
				}

				// 2. Extract base64 JPEG payload
				const auto dataPos = msg.find("\"data\":\"");
				if (dataPos != std::string::npos)
				{
					const size_t start = dataPos + 8;
					const size_t end = msg.find('"', start);
					if (end != std::string::npos && end > start)
					{
						lastFrameTime = std::chrono::steady_clock::now();
						submitEncodedFrame(msg.substr(start, end - start));
					}
				}
				return;
			}

			// Handle Page.captureScreenshot response
			if (screenshotInFlight && msg.find("\"method\":") == std::string::npos)
			{
				const auto dataPos = msg.find("\"data\":\"");
				if (dataPos != std::string::npos)
				{
					screenshotInFlight = false;
					const size_t start = dataPos + 8;
					const size_t end = msg.find('"', start);
					if (end != std::string::npos && end > start)
					{
						lastFrameTime = std::chrono::steady_clock::now();
						submitEncodedFrame(msg.substr(start, end - start));
					}
					return;
				}
				if (pendingScreenshotId > 0 &&
					msg.find("\"id\":" + std::to_string(pendingScreenshotId)) != std::string::npos)
				{
					screenshotInFlight = false;
				}
			}

			if (msg.find("\"method\":\"Page.loadEventFired\"") != std::string::npos ||
				msg.find("\"method\":\"Page.frameStoppedLoading\"") != std::string::npos)
			{
				if (hasNavigatedUrl)
				{
					setStatus("En vivo (" + std::to_string(currentW) + "x" + std::to_string(currentH) + ") · " + activeNavigatedUrl);
					startScreencast();
					requestBurst(2500);
				}
			}
		};

		while (running.load())
		{
			// 1. Drain commands from main thread
			std::deque<Command> localCmds;
			{
				std::lock_guard<std::mutex> lock(stateMutex);
				localCmds.swap(commandQueue);
			}

			for (const auto &cmd : localCmds)
			{
				switch (cmd.type)
				{
				case Command::Type::Navigate:
				{
					activeNavigatedUrl = normalizeWebUrl(cmd.text);
					if (!activeNavigatedUrl.empty())
					{
						hasNavigatedUrl = true;
						setStatus("Cargando: " + activeNavigatedUrl);
						sendCdp("Page.navigate", "{\"url\":\"" + jsonEscape(activeNavigatedUrl) + "\"}");
						startScreencast();
						requestBurst(5000);
					}
					break;
				}
				case Command::Type::Reload:
					if (hasNavigatedUrl)
					{
						setStatus("Recargando: " + activeNavigatedUrl);
						sendCdp("Page.reload", "{\"ignoreCache\":false}");
						startScreencast();
						requestBurst(4000);
					}
					break;
				case Command::Type::Back:
					sendCdp("Runtime.evaluate", "{\"expression\":\"window.history.back();\"}");
					requestBurst(2500);
					break;
				case Command::Type::Forward:
					sendCdp("Runtime.evaluate", "{\"expression\":\"window.history.forward();\"}");
					requestBurst(2500);
					break;
				case Command::Type::Resize:
				{
					int newW = std::clamp(cmd.width, 320, 1920);
					int newH = std::clamp(cmd.height, 240, 1080);
					if (newW != currentW || newH != currentH)
					{
						currentW = newW;
						currentH = newH;
						sendCdp("Emulation.setDeviceMetricsOverride",
								"{\"width\":" + std::to_string(currentW) +
								",\"height\":" + std::to_string(currentH) +
								",\"deviceScaleFactor\":1,\"mobile\":false}");
						sendCdp("Page.stopScreencast");
						startScreencast();
						requestBurst(2000);
					}
					break;
				}
				case Command::Type::ScrollZoom:
				{
					std::ostringstream js;
					js << "try{"
					   << "const z=" << cmd.c << ";"
					   << "if(document.documentElement)document.documentElement.style.zoom=z;"
					   << "const maxX=Math.max(0,(document.documentElement.scrollWidth||document.body.scrollWidth||0)-window.innerWidth);"
					   << "const maxY=Math.max(0,(document.documentElement.scrollHeight||document.body.scrollHeight||0)-window.innerHeight);"
					   << "window.scrollTo(" << cmd.a << "*maxX," << cmd.b << "*maxY);"
					   << "}catch(e){}";
					sendCdp("Runtime.evaluate", "{\"expression\":\"" + jsonEscape(js.str()) + "\"}");
					requestBurst(1000);
					break;
				}
				case Command::Type::MouseMove:
				{
					int mx = static_cast<int>(std::clamp(cmd.a, 0.0f, 1.0f) * currentW);
					int my = static_cast<int>(std::clamp(cmd.b, 0.0f, 1.0f) * currentH);
					sendCdp("Input.dispatchMouseEvent",
							"{\"type\":\"mouseMoved\",\"x\":" + std::to_string(mx) +
							",\"y\":" + std::to_string(my) + "}");
					requestBurst(800);
					break;
				}
				case Command::Type::MouseClick:
				{
					int mx = static_cast<int>(std::clamp(cmd.a, 0.0f, 1.0f) * currentW);
					int my = static_cast<int>(std::clamp(cmd.b, 0.0f, 1.0f) * currentH);
					std::string base = ",\"x\":" + std::to_string(mx) +
									   ",\"y\":" + std::to_string(my) +
									   ",\"button\":\"left\",\"clickCount\":1}";
					sendCdp("Input.dispatchMouseEvent", "{\"type\":\"mouseMoved\"" + base);
					sendCdp("Input.dispatchMouseEvent", "{\"type\":\"mousePressed\"" + base);
					sendCdp("Input.dispatchMouseEvent", "{\"type\":\"mouseReleased\"" + base);
					requestBurst(2000);
					break;
				}
				case Command::Type::SetMuted:
				{
					std::string muteJs = cmd.flag
						? "try{document.querySelectorAll('video,audio').forEach(e=>e.muted=true);}catch(e){}"
						: "try{document.querySelectorAll('video,audio').forEach(e=>e.muted=false);}catch(e){}";
					sendCdp("Runtime.evaluate", "{\"expression\":\"" + jsonEscape(muteJs) + "\"}");
					break;
				}
				}
			}

			// 2. If page is static or in burst window and screencast hasn't sent a frame recently, pulse screenshot
			const auto now = std::chrono::steady_clock::now();
			if (hasNavigatedUrl && !screenshotInFlight)
			{
				const auto sinceFrameMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFrameTime).count();
				const auto sinceShotMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastScreenshotTime).count();
				const bool inBurst = now < burstUntilTime;
				if ((inBurst && sinceFrameMs > 180 && sinceShotMs > 180) ||
					(!inBurst && sinceFrameMs > 1200 && sinceShotMs > 1200))
				{
					screenshotInFlight = true;
					lastScreenshotTime = now;
					pendingScreenshotId = sendCdp("Page.captureScreenshot", "{\"format\":\"jpeg\",\"quality\":85}");
				}
			}
			else if (screenshotInFlight)
			{
				const auto sinceShotMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastScreenshotTime).count();
				if (sinceShotMs > 2000) screenshotInFlight = false;
			}

			// 3. Poll WebSocket for incoming frames (up to 8ms wait)
			fd_set readSet;
			FD_ZERO(&readSet);
			FD_SET(ws, &readSet);
			timeval tv{};
			tv.tv_sec = 0;
			tv.tv_usec = 8000;

			int sel = select(0, &readSet, nullptr, nullptr, &tv);
			if (sel > 0 && FD_ISSET(ws, &readSet))
			{
				uint8_t chunk[65536];
				int n = recv(ws, reinterpret_cast<char *>(chunk), sizeof(chunk), 0);
				if (n <= 0)
				{
					if (running.load()) setStatus("Conexion con el navegador cerrada.");
					break;
				}
				rxBuffer.insert(rxBuffer.end(), chunk, chunk + n);
			}

			// 4. Parse complete RFC 6455 WebSocket frames in rxBuffer
			while (rxBuffer.size() >= 2)
			{
				const uint8_t b0 = rxBuffer[0];
				const uint8_t b1 = rxBuffer[1];
				const bool fin = (b0 & 0x80) != 0;
				const uint8_t opcode = b0 & 0x0F;
				const bool masked = (b1 & 0x80) != 0;
				uint64_t payloadLen = b1 & 0x7F;
				size_t headerLen = 2;

				if (payloadLen == 126)
				{
					if (rxBuffer.size() < 4) break;
					payloadLen = (static_cast<uint64_t>(rxBuffer[2]) << 8) | rxBuffer[3];
					headerLen = 4;
				}
				else if (payloadLen == 127)
				{
					if (rxBuffer.size() < 10) break;
					payloadLen = 0;
					for (int i = 0; i < 8; ++i)
						payloadLen = (payloadLen << 8) | rxBuffer[2 + i];
					headerLen = 10;
				}

				if (masked) headerLen += 4;
				if (rxBuffer.size() < headerLen + payloadLen) break;

				const uint8_t *payloadPtr = rxBuffer.data() + headerLen;
				std::string framePayload(reinterpret_cast<const char *>(payloadPtr), static_cast<size_t>(payloadLen));
				if (masked)
				{
					const uint8_t *maskKey = rxBuffer.data() + (headerLen - 4);
					for (size_t i = 0; i < framePayload.size(); ++i)
						framePayload[i] ^= static_cast<char>(maskKey[i & 3]);
				}

				rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + static_cast<ptrdiff_t>(headerLen + payloadLen));

				if (opcode == 0x8) // Close
				{
					running.store(false);
					break;
				}
				else if (opcode == 0x9) // Ping -> reply Pong
				{
					uint8_t pong[6] = {0x8A, 0x80, 0x11, 0x22, 0x33, 0x44};
					sendAll(ws, pong, sizeof(pong));
				}
				else if (opcode == 0x1 || opcode == 0x0) // Text or Continuation
				{
					if (fin && fragmentedMessage.empty())
					{
						handleWsMessage(framePayload);
					}
					else
					{
						fragmentedMessage += framePayload;
						if (fin)
						{
							handleWsMessage(fragmentedMessage);
							fragmentedMessage.clear();
						}
					}
				}
			}
		}

		closesocket(ws);
		cleanupProcess();
		WSACleanup();
#else
		setStatus("WebRender requiere Windows Chromium/Edge en esta compilacion.");
#endif
	}
};

JPbox_webrender::JPbox_webrender()
	: impl(std::make_unique<Impl>())
{
}

JPbox_webrender::~JPbox_webrender()
{
	clear();
}

void JPbox_webrender::setup(string _directory, string _nombre)
{
	JPbox::setup(_directory, _nombre);
	dir = "webRender";
	tipo = WEBRENDERBOX;

	parameters.clear();
	parameters.setName(name);

	auto addSlider = [&](const char *pname, float value, float minVal, float maxVal) -> int {
		parameters.addFloatValue(value, pname);
		int idx = parameters.getSize() - 1;
		auto *p = parameters.getJParameter(idx);
		p->nativeMin = p->min = minVal;
		p->nativeMax = p->max = maxVal;
		p->defaultFloatValue = value;
		return idx;
	};

	scalex = addSlider("scalex", 0.5f, 0.0f, 1.0f);
	scaley = addSlider("scaley", 0.5f, 0.0f, 1.0f);
	offsetx = addSlider("offsetx", 0.5f, 0.0f, 1.0f);
	offsety = addSlider("offsety", 0.5f, 0.0f, 1.0f);
	parameters.addBoolValue(false, "strech");
	strech = parameters.getSize() - 1;
	scaleRatioIndex = addSlider("scale ratio", 1.0f, 0.1f, 4.0f);

	scrollYIndex = addSlider("scroll y", 0.0f, 0.0f, 1.0f);
	scrollXIndex = addSlider("scroll x", 0.0f, 0.0f, 1.0f);
	zoomIndex = addSlider("zoom", 1.0f, 0.25f, 3.0f);
	mouseXIndex = addSlider("mouse x", 0.5f, 0.0f, 1.0f);
	mouseYIndex = addSlider("mouse y", 0.5f, 0.0f, 1.0f);

	uploadedSerial = 0;
	hasRenderedWebFrame = false;
	drawPlaceholderToFbo();
}

void JPbox_webrender::ensureSessionRunning()
{
	if (!impl) impl = std::make_unique<Impl>();
	if (!impl->running.load())
	{
		const int w = std::max(320, jp_constants::renderWidth);
		const int h = std::max(240, jp_constants::renderHeight);
		impl->start(w, h, activeUrl, mutedState);
	}
}

void JPbox_webrender::navigate(const std::string &rawUrl)
{
	const std::string normalized = normalizeWebUrl(rawUrl);
	if (normalized.empty()) return;
	urlText = normalized;
	activeUrl = normalized;
	ensureSessionRunning();

	Impl::Command cmd;
	cmd.type = Impl::Command::Type::Navigate;
	cmd.text = activeUrl;
	impl->pushCommand(std::move(cmd));
}

void JPbox_webrender::reloadPage()
{
	if (!urlText.empty() && normalizeWebUrl(urlText) != activeUrl)
	{
		navigate(urlText);
		return;
	}
	if (activeUrl.empty()) return;
	ensureSessionRunning();
	Impl::Command cmd;
	cmd.type = Impl::Command::Type::Reload;
	impl->pushCommand(std::move(cmd));
}

void JPbox_webrender::goBack()
{
	if (!impl || !impl->running.load()) return;
	Impl::Command cmd;
	cmd.type = Impl::Command::Type::Back;
	impl->pushCommand(std::move(cmd));
}

void JPbox_webrender::goForward()
{
	if (!impl || !impl->running.load()) return;
	Impl::Command cmd;
	cmd.type = Impl::Command::Type::Forward;
	impl->pushCommand(std::move(cmd));
}

void JPbox_webrender::triggerMouseClick()
{
	ensureSessionRunning();
	Impl::Command cmd;
	cmd.type = Impl::Command::Type::MouseClick;
	cmd.a = mouseXIndex >= 0 ? parameters.getFloatValue(mouseXIndex) : 0.5f;
	cmd.b = mouseYIndex >= 0 ? parameters.getFloatValue(mouseYIndex) : 0.5f;
	impl->pushCommand(std::move(cmd));
}

void JPbox_webrender::setMuted(bool muted)
{
	mutedState = muted;
	if (impl && impl->running.load())
	{
		Impl::Command cmd;
		cmd.type = Impl::Command::Type::SetMuted;
		cmd.flag = mutedState;
		impl->pushCommand(std::move(cmd));
	}
}

std::string JPbox_webrender::statusLabel() const
{
	return impl ? impl->getStatus() : "Inactivo";
}

void JPbox_webrender::setCaptureActive(bool active)
{
	captureActive = active;
	if (!active)
	{
		urlFocused = false;
		if (impl) impl->stop();
	}
}

void JPbox_webrender::reload()
{
	reloadPage();
}

void JPbox_webrender::clear()
{
	urlFocused = false;
	if (impl) impl->stop();
	webTexture.clear();
	uploadedSerial = 0;
	hasRenderedWebFrame = false;
	JPbox::clear();
}

void JPbox_webrender::saveCustomState(ofXml &boxNode) const
{
	boxNode.removeChild("webRender");
	ofXml node = boxNode.appendChild("webRender");
	node.appendChild("url").set(activeUrl.empty() ? urlText : activeUrl);
	node.appendChild("muted").set(mutedState ? 1 : 0);
	node.appendChild("cursor").set(showCursorOverlay ? 1 : 0);
}

void JPbox_webrender::loadCustomState(const ofXml &boxNode)
{
	ofXml node = boxNode.getChild("webRender");
	if (!node) return;

	if (auto u = node.getChild("url"))
	{
		urlText = trimWhitespace(u.getValue());
		activeUrl = normalizeWebUrl(urlText);
	}
	if (auto m = node.getChild("muted"))
	{
		mutedState = (m.getIntValue() != 0);
	}
	if (auto c = node.getChild("cursor"))
	{
		showCursorOverlay = (c.getIntValue() != 0);
	}

	if (!activeUrl.empty() && captureActive && onoff.boolValue && impl && impl->running.load())
	{
		navigate(activeUrl);
	}
}

void JPbox_webrender::copyCustomStateFrom(const JPbox *source)
{
	const auto *other = dynamic_cast<const JPbox_webrender *>(source);
	if (!other) return;
	urlText = other->urlText;
	activeUrl = other->activeUrl;
	mutedState = other->mutedState;
	showCursorOverlay = other->showCursorOverlay;
}

void JPbox_webrender::update()
{
	if (!onoff.boolValue || !captureActive)
	{
		if (impl && impl->running.load()) impl->stop();
		return;
	}

	JPbox::update();
	ensureSessionRunning();

	const int rw = std::clamp(jp_constants::renderWidth, 320, 1920);
	const int rh = std::clamp(jp_constants::renderHeight, 240, 1080);
	if (impl && impl->running.load() && (rw != impl->targetWidth || rh != impl->targetHeight))
	{
		impl->targetWidth = rw;
		impl->targetHeight = rh;
		Impl::Command rcmd;
		rcmd.type = Impl::Command::Type::Resize;
		rcmd.width = rw;
		rcmd.height = rh;
		impl->pushCommand(std::move(rcmd));
	}

	// Forward continuous parameter changes (scroll, zoom, mouse) throttled to ~30 Hz
	const uint64_t nowMs = ofGetElapsedTimeMillis();
	if (nowMs - lastInputSendMs >= 33 && impl && impl->running.load())
	{
		const float sx = scrollXIndex >= 0 ? parameters.getFloatValue(scrollXIndex) : 0.0f;
		const float sy = scrollYIndex >= 0 ? parameters.getFloatValue(scrollYIndex) : 0.0f;
		const float zm = zoomIndex >= 0 ? parameters.getFloatValue(zoomIndex) : 1.0f;
		if (std::fabs(sx - lastSentScrollX) > 0.001f ||
			std::fabs(sy - lastSentScrollY) > 0.001f ||
			std::fabs(zm - lastSentZoom) > 0.002f)
		{
			lastSentScrollX = sx;
			lastSentScrollY = sy;
			lastSentZoom = zm;
			Impl::Command cmd;
			cmd.type = Impl::Command::Type::ScrollZoom;
			cmd.a = sx;
			cmd.b = sy;
			cmd.c = zm;
			impl->pushCommand(std::move(cmd));
			lastInputSendMs = nowMs;
		}

		const float mx = mouseXIndex >= 0 ? parameters.getFloatValue(mouseXIndex) : 0.5f;
		const float my = mouseYIndex >= 0 ? parameters.getFloatValue(mouseYIndex) : 0.5f;
		if (std::fabs(mx - lastSentMouseX) > 0.002f ||
			std::fabs(my - lastSentMouseY) > 0.002f)
		{
			lastSentMouseX = mx;
			lastSentMouseY = my;
			Impl::Command cmd;
			cmd.type = Impl::Command::Type::MouseMove;
			cmd.a = mx;
			cmd.b = my;
			impl->pushCommand(std::move(cmd));
			lastInputSendMs = nowMs;
		}
	}

	updateFBO();
}

void JPbox_webrender::drawPlaceholderToFbo()
{
	const int w = std::max(1, jp_constants::renderWidth);
	const int h = std::max(1, jp_constants::renderHeight);
	if (!fbo.isAllocated() || fbo.getWidth() != w || fbo.getHeight() != h)
	{
		fbo.allocate(w, h, GL_RGBA);
	}

	jp_gl::ScopedNoScissor noScissor;
	ofPushStyle();
	fbo.begin();
	ofClear(14, 18, 26, 255);

	// Subtle grid background
	ofSetColor(28, 38, 54, 180);
	const int step = std::max(32, w / 24);
	for (int gx = 0; gx < w; gx += step) ofDrawLine(gx, 0, gx, h);
	for (int gy = 0; gy < h; gy += step) ofDrawLine(0, gy, w, gy);

	// Center card
	const float cardW = std::min(w * 0.75f, 680.0f);
	const float cardH = 140.0f;
	const float cardX = (w - cardW) * 0.5f;
	const float cardY = (h - cardH) * 0.5f;
	ofFill();
	ofSetColor(18, 24, 34, 235);
	ofDrawRectRounded(cardX, cardY, cardW, cardH, 8.0f);
	ofNoFill();
	ofSetColor(0, 200, 255, 180);
	ofDrawRectRounded(cardX, cardY, cardW, cardH, 8.0f);
	ofFill();

	if (jp_constants::p_font.isLoaded())
	{
		ofSetColor(0, 215, 255);
		jp_constants::p_font.drawString("WEBRENDER LIVE", cardX + 24.0f, cardY + 42.0f);
		ofSetColor(215, 222, 232);
		const std::string sub = activeUrl.empty()
			? "Pega cualquier link en el inspector y presiona Enter"
			: statusLabel();
		jp_constants::p_font.drawString(sub, cardX + 24.0f, cardY + 80.0f);
		if (!activeUrl.empty())
		{
			ofSetColor(145, 160, 180);
			jp_constants::p_font.drawString(activeUrl.substr(0, 72), cardX + 24.0f, cardY + 112.0f);
		}
	}

	fbo.end();
	ofPopStyle();
}

void JPbox_webrender::updateFBO()
{
	const int w = std::max(1, jp_constants::renderWidth);
	const int h = std::max(1, jp_constants::renderHeight);
	if (!fbo.isAllocated() || fbo.getWidth() != w || fbo.getHeight() != h)
	{
		fbo.allocate(w, h, GL_RGBA);
	}

	if (impl)
	{
		if (auto frame = impl->getLatestFrame(); frame && frame->serial != uploadedSerial &&
			frame->width > 0 && frame->height > 0 &&
			frame->rgba.size() >= static_cast<size_t>(frame->width) * frame->height * 4)
		{
			webTexture.loadData(frame->rgba.data(), frame->width, frame->height, GL_RGBA);
			uploadedSerial = frame->serial;
			hasRenderedWebFrame = true;
		}
	}

	if (!hasRenderedWebFrame || !webTexture.isAllocated())
	{
		drawPlaceholderToFbo();
		return;
	}

	float drawW = static_cast<float>(fbo.getWidth());
	float drawH = static_cast<float>(fbo.getHeight());
	if (!parameters.getBoolValue(strech) && webTexture.getWidth() > 0 && webTexture.getHeight() > 0)
	{
		const float fit = std::min(drawW / webTexture.getWidth(), drawH / webTexture.getHeight());
		drawW = webTexture.getWidth() * fit;
		drawH = webTexture.getHeight() * fit;
	}
	const float ratioValue = scaleRatioIndex >= 0
		? std::max(0.01f, parameters.getFloatValue(scaleRatioIndex))
		: 1.0f;
	drawW = std::max(1.0f, drawW * parameters.getFloatValue(scalex) * 2.0f * ratioValue);
	drawH = std::max(1.0f, drawH * parameters.getFloatValue(scaley) * 2.0f * ratioValue);
	const float drawX = (fbo.getWidth() - drawW) * 0.5f +
						(parameters.getFloatValue(offsetx) - 0.5f) * 2.0f * fbo.getWidth();
	const float drawY = (fbo.getHeight() - drawH) * 0.5f +
						(parameters.getFloatValue(offsety) - 0.5f) * 2.0f * fbo.getHeight();

	jp_gl::ScopedNoScissor noScissor;
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	fbo.begin();
	ofClear(0, 0, 0, 255);
	ofSetColor(255, 255, 255, 255);
	webTexture.draw(drawX, drawY, drawW, drawH);

	if (showCursorOverlay)
	{
		const float mx = (mouseXIndex >= 0 ? parameters.getFloatValue(mouseXIndex) : 0.5f) * fbo.getWidth();
		const float my = (mouseYIndex >= 0 ? parameters.getFloatValue(mouseYIndex) : 0.5f) * fbo.getHeight();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofNoFill();
		ofSetLineWidth(2.0f);
		ofSetColor(0, 230, 255, 220);
		ofDrawCircle(mx, my, 8.0f);
		ofDrawLine(mx - 12.0f, my, mx + 12.0f, my);
		ofDrawLine(mx, my - 12.0f, mx, my + 12.0f);
		ofFill();
	}

	ofSetRectMode(OF_RECTMODE_CORNER);
	fbo.end();
	ofPopStyle();
}

void JPbox_webrender::draw()
{
	JPbox::draw();
	ofSetColor(255, 255, 255);
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
}

void JPbox_webrender::layoutInspector(float left, float top, float width)
{
	float y = top + 18.0f;
	urlFieldRect.set(left, y, width, 32.0f);
	y += 38.0f;

	const float gap = 6.0f;
	const float halfW = (width - gap) * 0.5f;
	loadButtonRect.set(left, y, halfW, 28.0f);
	reloadButtonRect.set(left + halfW + gap, y, halfW, 28.0f);
	y += 34.0f;

	const float thirdW = (width - gap * 2.0f) / 3.0f;
	backButtonRect.set(left, y, thirdW, 28.0f);
	forwardButtonRect.set(left + thirdW + gap, y, thirdW, 28.0f);
	clickButtonRect.set(left + (thirdW + gap) * 2.0f, y, thirdW, 28.0f);
	y += 34.0f;

	muteButtonRect.set(left, y, halfW, 28.0f);
	cursorButtonRect.set(left + halfW + gap, y, halfW, 28.0f);
	y += 34.0f;

	statusRect.set(left, y, width, 36.0f);
}

void JPbox_webrender::drawInspector()
{
	auto &font = jp_constants::inspector_media_font;
	ofPushStyle();

	ofSetColor(COL_TEXT_SECONDARY);
	font.drawString("Enlace Web (Enter para cargar)", urlFieldRect.x, urlFieldRect.y - 6.0f);

	ofFill();
	ofSetColor(COL_BG_INPUT);
	ofDrawRectRounded(urlFieldRect, 3.0f);
	ofNoFill();
	ofSetColor(urlFocused ? COL_ACCENT_CYAN : COL_BORDER_MUTED);
	ofDrawRectRounded(urlFieldRect, 3.0f);
	ofFill();

	auto field = jp_text_input::field(
		"webrender-url-" + uid,
		"webrender",
		urlFieldRect,
		font,
		urlText,
		urlFocused);
	field.limit = 2048;
	field.focus = [this] { urlFocused = true; };
	field.cancel = [this] { urlFocused = false; };
	field.commit = [this] {
		urlFocused = false;
		if (!urlText.empty()) navigate(urlText);
		return true;
	};
	jp_text_input::controller().add(std::move(field));

	if (urlText.empty() && !urlFocused)
	{
		ofSetColor(COL_TEXT_SECONDARY);
		font.drawString("https://... (cualquier sitio o WebGL)", urlFieldRect.x + 6.0f, urlFieldRect.y + 20.0f);
	}

	auto drawBtn = [&](const ofRectangle &rect, const std::string &label, bool highlighted) {
		const bool hover = rect.inside(ofGetMouseX(), ofGetMouseY());
		ofFill();
		ofSetColor(hover ? COL_BG_HOVER : COL_BG_INPUT);
		ofDrawRectRounded(rect, 3.0f);
		ofNoFill();
		ofSetColor(highlighted ? COL_ACCENT_CYAN : COL_BORDER_MUTED);
		ofDrawRectRounded(rect, 3.0f);
		ofFill();
		ofSetColor(COL_TEXT_PRIMARY);
		std::string fit = label;
		while (!fit.empty() && font.stringWidth(fit) > rect.width - 12.0f)
			fit.pop_back();
		font.drawString(fit, rect.x + 7.0f, rect.y + 19.0f);
	};

	const bool urlPending = !urlText.empty() && normalizeWebUrl(urlText) != activeUrl;
	drawBtn(loadButtonRect, "Cargar URL", urlPending || !activeUrl.empty());
	drawBtn(reloadButtonRect, "Recargar", false);
	drawBtn(backButtonRect, "< Atras", false);
	drawBtn(forwardButtonRect, "Adelante >", false);
	drawBtn(clickButtonRect, "Click cursor", false);
	drawBtn(muteButtonRect, mutedState ? "Audio: Mute" : "Audio: ON", !mutedState);
	drawBtn(cursorButtonRect, showCursorOverlay ? "Cursor: Visible" : "Cursor: Oculto", showCursorOverlay);

	ofSetColor(COL_TEXT_SECONDARY);
	const std::string status = statusLabel();
	const auto lines = jp_textwrap::wrap(
		[&font](const std::string &line) { return font.stringWidth(line); },
		status,
		statusRect.width);
	float textY = statusRect.y + 13.0f;
	for (const auto &line : lines)
	{
		if (textY > statusRect.getBottom()) break;
		font.drawString(line, statusRect.x, textY);
		textY += 16.0f;
	}
	jp_tooltip::draw(status, statusRect);

	ofPopStyle();
}

bool JPbox_webrender::clickInspector(float x, float y)
{
	if (loadButtonRect.inside(x, y))
	{
		urlFocused = false;
		if (!urlText.empty()) navigate(urlText);
		return true;
	}
	if (reloadButtonRect.inside(x, y))
	{
		urlFocused = false;
		reloadPage();
		return true;
	}
	if (backButtonRect.inside(x, y))
	{
		goBack();
		return true;
	}
	if (forwardButtonRect.inside(x, y))
	{
		goForward();
		return true;
	}
	if (clickButtonRect.inside(x, y))
	{
		triggerMouseClick();
		return true;
	}
	if (muteButtonRect.inside(x, y))
	{
		setMuted(!mutedState);
		return true;
	}
	if (cursorButtonRect.inside(x, y))
	{
		showCursorOverlay = !showCursorOverlay;
		return true;
	}
	return false;
}
