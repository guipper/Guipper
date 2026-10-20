#include "jp_youtube.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace jp_youtube {
namespace {
bool validId(const std::string &id) {
    return id.size() == 11 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
bool available(const std::filesystem::path &path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) &&
        std::filesystem::file_size(path, error) > 0 && !error;
}
std::string utf8Path(const std::filesystem::path &path) {
    const auto bytes = path.u8string(); return std::string(bytes.begin(), bytes.end());
}
std::string diagnostic(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::string line, last;
    while (std::getline(in, line)) {
        if (line.find("ERROR:") != std::string::npos) last = line;
    }
    if (last.size() > 300) last.resize(300);
    return last;
}
}
std::string videoId(const std::string &input) {
    if (input.size() > 2048) return {};
    const auto first = input.find_first_not_of(" \r\n\t");
    if (first == std::string::npos) return {};
    auto url = input.substr(first, input.find_last_not_of(" \r\n\t") - first + 1);
    if (url.find_first_of(" \r\n\t\\") != std::string::npos) return {};
    if (url.rfind("https://", 0) == 0) url.erase(0, 8);
    else if (url.rfind("http://", 0) == 0) url.erase(0, 7);
    const auto slash = url.find('/');
    if (slash == std::string::npos) return {};
    auto host = url.substr(0, slash);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) {return std::tolower(c);});
    auto path = url.substr(slash);
    std::string id;
    if (host == "youtu.be" || host == "www.youtu.be") id = path.substr(1, path.find_first_of("?#", 1) - 1);
    else if (host == "youtube.com" || host == "www.youtube.com" || host == "m.youtube.com" || host == "music.youtube.com") {
        if (path.rfind("/watch?", 0) == 0) {
            auto query = path.substr(7);
            query = query.substr(0, query.find('#'));
            for (size_t pos = 0; pos < query.size();) {
                const auto end = query.find('&', pos);
                const auto item = query.substr(pos, end - pos);
                if (item.rfind("v=", 0) == 0) { if (!id.empty()) return {}; id = item.substr(2); }
                if (end == std::string::npos) break;
                pos = end + 1;
            }
        } else for (const auto *prefix : {"/shorts/", "/embed/", "/live/"}) {
            if (path.rfind(prefix, 0) == 0) {
                id = path.substr(std::char_traits<char>::length(prefix));
                id = id.substr(0, id.find_first_of("?#")); break;
            }
        }
    }
    return validId(id) ? id : std::string();
}
std::string canonicalUrl(const std::string &url) {
    const auto id = videoId(url);
    return id.empty() ? "" : "https://www.youtube.com/watch?v=" + id;
}

int runProcess(const std::vector<std::string> &args, const std::filesystem::path &log,
    const std::atomic<bool> &cancel) {
    if (args.empty() || cancel) return -2;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(30);
#ifdef _WIN32
    auto wide = [](const std::string &s) {
        const int size = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
        std::wstring value(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), value.data(), size); return value;
    };
    std::wstring command;
    for (const auto &arg : args) {
        command += L"\"";
        unsigned slashes = 0;
        for (wchar_t c : wide(arg)) {
            if (c == L'\\') { ++slashes; continue; }
            command.append(slashes * (c == L'"' ? 2 : 1), L'\\'); slashes = 0;
            if (c == L'"') command += L'\\';
            command += c;
        }
        command.append(slashes * 2, L'\\'); command += L"\" ";
    }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input; startup.hStdOutput = startup.hStdError = output;
    PROCESS_INFORMATION process{};
    bool started = output != INVALID_HANDLE_VALUE && input != INVALID_HANDLE_VALUE && job &&
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) &&
        CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
    int code = -1;
    if (started) {
        if (AssignProcessToJobObject(job, process.hProcess)) {
            ResumeThread(process.hThread);
            while (WaitForSingleObject(process.hProcess, 20) == WAIT_TIMEOUT && !cancel && std::chrono::steady_clock::now() < deadline) {}
            if (cancel || std::chrono::steady_clock::now() >= deadline) {
                TerminateJobObject(job, 1); code = cancel ? -2 : -3;
            } else { DWORD exitCode = 1; GetExitCodeProcess(process.hProcess, &exitCode); code = int(exitCode); }
        } else TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    }
    if (job) CloseHandle(job);
    if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    return code;
#else
    std::vector<char *> argv;
    for (const auto &arg : args) argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);
    posix_spawn_file_actions_t files; posix_spawn_file_actions_init(&files);
    posix_spawn_file_actions_addopen(&files, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&files, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    posix_spawn_file_actions_adddup2(&files, 1, 2);
    posix_spawnattr_t attr; posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP); posix_spawnattr_setpgroup(&attr, 0);
    pid_t pid = -1;
    const int error = posix_spawnp(&pid, argv[0], &files, &attr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&files); posix_spawnattr_destroy(&attr);
    if (error) return -1;
    int status = 0;
    for (;;) {
        const auto result = waitpid(pid, &status, WNOHANG);
        if (result == pid) return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
        if (result < 0 && errno != EINTR) return 1;
        if (cancel || std::chrono::steady_clock::now() >= deadline) {
            kill(-pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
            return cancel ? -2 : -3;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
#endif
}
Download::Download(std::filesystem::path cache, Runner run) : root(std::move(cache)), runner(std::move(run)) {}
Download::~Download() { cancel(); }
Snapshot Download::snapshot() const { std::lock_guard<std::mutex> lock(mutex); return result; }
void Download::publish(Snapshot value) { std::lock_guard<std::mutex> lock(mutex); if (!cancelled) result = std::move(value); }
std::filesystem::path Download::cached(const std::string &url) const {
    const auto id = videoId(url);
    const auto file = root / (id + "-1080p-v1.mp4");
    return !id.empty() && available(file) ? file : std::filesystem::path();
}
void Download::cancel() {
    cancelled = true;
    if (worker.joinable()) worker.join();
    if (!retainedDirectory.empty()) {
        std::error_code error; std::filesystem::remove_all(retainedDirectory, error); retainedDirectory.clear();
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (result.state == State::Preparing || !result.streamUrl.empty() || result.file.extension() == ".m3u8")
        result = {State::Cancelled, "Conexion cancelada", {}};
}
void Download::start(const std::string &url, bool force) { startWork(url, force, false); }
void Download::resolve(const std::string &url) { startWork(url, false, true); }
void Download::startWork(const std::string &url, bool force, bool streaming) {
    cancel(); cancelled = false;
    const auto canonical = canonicalUrl(url);
    if (canonical.empty()) { publish({State::Error, "Pega un enlace de un video de YouTube", {}}); return; }
    const auto hit = cached(canonical);
    if (!streaming && !force && !hit.empty()) { publish({State::Ready, "Video en cache", hit}); return; }
    publish({State::Preparing, streaming ? "Conectando streaming..." : "Descargando en cache (hasta 1080p)...", {}});
    worker = std::thread([this, canonical, streaming] {
        std::filesystem::path temporary;
        try {
            std::filesystem::create_directories(root);
            // Atomic directory creation also protects simultaneous boxes/processes.
            for (unsigned i = 0;; ++i) {
                temporary = root / ("prepare-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(i));
                if (std::filesystem::create_directory(temporary)) break;
            }
            const auto output = temporary / "video.mp4", log = temporary / "download.log";
            std::string executable = "yt-dlp";
#ifdef _WIN32
            const auto privateTool = root.parent_path() / "tools" / "yt-dlp.exe";
#else
            const auto privateTool = root.parent_path() / "tools" / "yt-dlp";
#endif
            if (available(privateTool)) executable = utf8Path(privateTool);
            if (const char *override = std::getenv("GUIPPER_YTDLP")) if (*override) executable = override;
            const auto urlsFile = temporary / "stream.txt";
            std::vector<std::string> args{executable, "--ignore-config", "--no-playlist", "--no-progress", "--no-colors",
                "--socket-timeout", "15", "--retries", "2", "--fragment-retries", "2",
                "--match-filter", "!is_live & !is_upcoming", "--js-runtimes", "node"};
            if (streaming) {
                const std::vector<std::string> options{"--skip-download", "--format",
                    "bv[height<=1080][vcodec^=avc1][protocol^=m3u8]+ba[protocol^=m3u8]/b[height<=1080][ext=mp4]/bv[height<=1080][vcodec^=avc1][protocol=https]",
                    "--print-to-file", "%(acodec)s\n%(urls)s", utf8Path(urlsFile)};
                args.insert(args.end(), options.begin(), options.end());
            } else {
                const std::vector<std::string> options{"--no-part", "--max-filesize", "2G", "--format",
                    "bv*[height<=1080][vcodec^=avc1]+ba[ext=m4a]/b[height<=1080][ext=mp4]",
                    "--merge-output-format", "mp4", "--output", utf8Path(output)};
                args.insert(args.end(), options.begin(), options.end());
            }
            args.insert(args.end(), {"--", canonical});
            const int code = runner(args, log, cancelled);
            if (!cancelled) {
                if (code == 0 && streaming) {
                    if (!available(urlsFile) || std::filesystem::file_size(urlsFile) > 65536)
                        throw std::runtime_error("No se encontro un stream compatible. Prueba Descargar en cache.");
                    std::ifstream input(urlsFile); std::string codec, line; std::getline(input, codec);
                    if (!codec.empty() && codec.back() == '\r') codec.pop_back();
                    std::vector<std::string> urls;
                    while (std::getline(input, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        if (line.empty()) continue;
                        if (line.rfind("https://", 0) != 0 || line.find_first_of(" \t\r\n\"\\") != std::string::npos)
                            throw std::runtime_error("YouTube devolvio una direccion de streaming no valida.");
                        urls.push_back(line);
                    }
                    if (urls.size() == 1) publish({State::Ready, "Streaming disponible", {}, urls[0], codec != "none"});
                    else if (urls.size() == 2) {
                        const auto manifest = temporary / "stream.m3u8";
                        std::ofstream playlist(manifest);
                        playlist << "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio\",NAME=\"Audio\",DEFAULT=YES,AUTOSELECT=YES,URI=\"" << urls[1] << "\"\n"
                            << "#EXT-X-STREAM-INF:BANDWIDTH=6000000,AUDIO=\"audio\"\n" << urls[0] << "\n";
                        playlist.close();
                        if (!playlist) throw std::runtime_error("No se pudo abrir el stream.");
                        std::filesystem::remove(urlsFile); std::filesystem::remove(log);
                        retainedDirectory = temporary; temporary.clear();
                        publish({State::Ready, "Streaming disponible", manifest, {}, true});
                    } else throw std::runtime_error("No se encontro un stream compatible. Prueba Descargar en cache.");
                } else if (code == 0 && available(output)) {
                    if (std::filesystem::file_size(output) > 2ULL * 1024 * 1024 * 1024) throw std::runtime_error("El video supera el limite de cache de 2 GB.");
                    const auto destination = root / (videoId(canonical) + "-1080p-v1.mp4");
                    std::error_code error; std::filesystem::rename(output, destination, error);
                    // Windows cannot replace an open cached file. A concurrent successful
                    // download is safe to reuse; never remove another box's playing file.
                    if (error && !available(destination)) throw std::system_error(error);
                    publish({State::Ready, "Video listo", destination});
                } else {
                    auto message = code == -1 ? "No se pudo iniciar yt-dlp. Instala yt-dlp, ffmpeg y un runtime JavaScript." :
                        code == -3 ? "La preparacion tardo demasiado. Reintenta." : diagnostic(log);
                    if (message.find("403") != std::string::npos) message = "YouTube rechazo la conexion. Actualiza yt-dlp o prueba otro video.";
                    else if (message.find("Sign in") != std::string::npos || message.find("sign in") != std::string::npos) message = "Este video requiere iniciar sesion. Prueba un video publico.";
                    if (message.empty()) message = "No se pudo preparar el video. Comprueba el enlace; los directos no estan disponibles.";
                    publish({State::Error, message, {}});
                }
            }
        } catch (const std::exception &error) { publish({State::Error, std::string("No se pudo preparar: ") + error.what(), {}}); }
        if (!temporary.empty()) { std::error_code error; std::filesystem::remove_all(temporary, error); }
    });
}
}
