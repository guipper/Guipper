#include "jp_app_paths.h"
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <mutex>
#include <ctime>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace jp {
namespace {
fs::path envPath(const char* name, const fs::path& fallback) {
    const char* value = std::getenv(name);
    return value && *value && fs::path(value).is_absolute() ? fs::path(value) : fallback;
}
bool inside(const fs::path& child, const fs::path& parent) {
    const auto relative=fs::absolute(child).lexically_normal().lexically_relative(fs::absolute(parent).lexically_normal());
    return !relative.empty() && *relative.begin()!="..";
}
bool regular(const fs::path& p) { return fs::is_regular_file(fs::symlink_status(p)); }
void copyMissing(const fs::path& from, const fs::path& to) {
    if (regular(from) && !fs::exists(to)) atomicWrite(to, readBytes(from));
}
bool preferenceName(const std::string& name) {
    return name == "settings.xml" || name == "midi_keymap.xml" ||
        name == "shader_favorites.xml" || name == "paint_palette.xml";
}
}
std::string readBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read " + path.string());
    std::string bytes((std::istreambuf_iterator<char>(in)), {});
    if (in.bad()) throw std::runtime_error("Read failed: " + path.string());
    return bytes;
}
void atomicWrite(const fs::path& path, const std::string& bytes) {
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
    static std::atomic<unsigned long> counter{0};
    fs::path temp = path;
    temp += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
        "-" + std::to_string(counter++);
    try {
#ifdef _WIN32
        HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create temporary file");
        std::size_t offset = 0;
        bool ok = true;
        while (offset < bytes.size()) {
            DWORD wrote = 0;
            DWORD amount = static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset, 1024*1024));
            if (!WriteFile(file, bytes.data()+offset, amount, &wrote, nullptr) || !wrote) { ok = false; break; }
            offset += wrote;
        }
        ok = FlushFileBuffers(file) && ok;
        CloseHandle(file);
        if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Atomic replacement failed: " + path.string());
#else
        int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) throw std::runtime_error("Cannot create temporary file: " + temp.string());
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            auto written = ::write(fd, bytes.data()+offset, bytes.size()-offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) { ::close(fd); throw std::runtime_error("Write failed: " + path.string()); }
            offset += static_cast<std::size_t>(written);
        }
        const int synced = ::fsync(fd);
        const int closed = ::close(fd);
        if (synced != 0 || closed != 0) throw std::runtime_error("Flush failed: " + path.string());
        fs::rename(temp, path);
        int directory = ::open(path.parent_path().empty() ? "." : path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
        if (directory >= 0) { ::fsync(directory); ::close(directory); }
#endif
    } catch (...) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        throw;
    }
}
void recordEvent(const std::string& code) {
    if (code.empty() || code.find_first_not_of("abcdefghijklmnopqrstuvwxyz_-0123456789") != std::string::npos) return;
    const auto& paths=AppPaths::current();
    if (paths.state.empty()) return;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    try {
        const auto file=paths.state / "logs/events.log";
        fs::create_directories(file.parent_path());
        if (fs::exists(file) && fs::file_size(file)>65536) {
            auto previous=readBytes(file);
            const auto begin=previous.find('\n',previous.size()-32768);
            atomicWrite(file,begin==std::string::npos?"":previous.substr(begin+1));
        }
        std::ofstream log(file,std::ios::app);
        log << std::time(nullptr) << " " << code << "\n";
    } catch (...) { /* Diagnostics must never prevent a save or startup. */ }
}
AppPaths AppPaths::discover(const fs::path& source) {
    AppPaths p;
    p.bundle = fs::absolute(source).lexically_normal();
    const auto override = envPath("GUIPPER_USER_ROOT", {});
    if (!override.empty()) {
        p.data = override / "data"; p.config = override / "config";
        p.state = override / "state"; p.cache = override / "cache";
    } else {
#ifdef _WIN32
        auto root = envPath("LOCALAPPDATA", {});
        if (root.empty()) throw std::runtime_error("LOCALAPPDATA is unavailable");
        root /= "Guipper";
        p.data = root / "data"; p.config = root / "config";
        p.state = root / "state"; p.cache = root / "cache";
#else
        auto home = envPath("HOME", {});
        if (home.empty()) throw std::runtime_error("HOME is unavailable");
        p.data = envPath("XDG_DATA_HOME", home / ".local/share") / "guipper";
        p.config = envPath("XDG_CONFIG_HOME", home / ".config") / "guipper";
        p.state = envPath("XDG_STATE_HOME", home / ".local/state") / "guipper";
        p.cache = envPath("XDG_CACHE_HOME", home / ".cache") / "guipper";
#endif
    }
    return p;
}
AppPaths& AppPaths::current() { static AppPaths paths; return paths; }
fs::path AppPaths::preference(const std::string& name) const {
    if (fs::path(name).filename() != fs::path(name)) throw std::runtime_error("Invalid preference name");
    return config / name;
}
void AppPaths::makePersonal(const fs::path& path) const {
    if (data.empty()) return;
    const auto relative = fs::absolute(path).lexically_normal().lexically_relative(data);
    if (relative.empty() || *relative.begin() == "..") return;
    fs::remove(cache / "resource-baseline" / relative);
}
void AppPaths::importLegacy(const fs::path& source) const {
    const auto root=fs::absolute(source).lexically_normal();
    if (inside(data,root) || inside(root,data) || !fs::is_directory(root) ||
        (!fs::is_directory(root / "shaders") && !regular(root / "settings.xml")))
        throw std::runtime_error("Choose the data folder of the previous installation");
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!regular(entry.path())) continue;
        const auto relative=entry.path().lexically_relative(root);
        const auto first=relative.begin()->string();
        if (first=="uishots" || first=="logs" || first=="recovery" ||
            entry.path().extension()==".log" || relative=="distribution.marker") continue;
        copyMissing(entry.path(), preferenceName(relative.generic_string()) ?
            preference(relative.string()) : data / relative);
    }
    recordEvent("legacy_import_completed");
}
void AppPaths::initialize() const {
    if (!fs::is_directory(bundle)) throw std::runtime_error("Missing application resources: " + bundle.string());
    for (const auto& root : {data,config,state,cache})
        if (inside(root,bundle) || inside(bundle,root))
            throw std::runtime_error("User and application directories must not overlap");
    for (const auto& root : {data, config, state / "recovery", state / "logs", cache}) fs::create_directories(root);
    const auto legacy=envPath("GUIPPER_LEGACY_DATA", {});
    if (!legacy.empty() && !regular(state / "legacy-explicit-import-v1")) {
        importLegacy(legacy);
        atomicWrite(state / "legacy-explicit-import-v1", legacy.string());
    }
    // A packaged build contains an explicit marker and only curated assets.
    // An older checkout may contain personal files: migrate them conservatively.
    const bool packaged = regular(bundle / "distribution.marker");
    const bool migrated = regular(state / "legacy-import-v1");
    for (const auto& entry : fs::recursive_directory_iterator(bundle)) {
        if (!regular(entry.path())) continue; // never follow external symlinks
        const auto relative = entry.path().lexically_relative(bundle);
        const auto first = relative.begin()->string();
        if (first == "uishots" || first == "logs" || first == "recovery" ||
            entry.path().extension() == ".log" || relative == "distribution.marker") continue;
        if (preferenceName(relative.generic_string())) {
            copyMissing(entry.path(), preference(relative.string()));
            continue;
        }
        const auto destination = data / relative;
        const auto baseline = cache / "resource-baseline" / relative;
        if (!packaged) {
            if (!migrated) copyMissing(entry.path(), destination);
            continue;
        }
        // Exact comparison, not timestamp or hash: only unchanged managed
        // resources can be refreshed. Losing the cache is conservative.
        if (!fs::exists(destination) || (regular(baseline) && regular(destination) &&
            readBytes(destination) == readBytes(baseline))) {
            const auto bytes = readBytes(entry.path());
            if (!regular(destination) || readBytes(destination) != bytes) atomicWrite(destination, bytes);
            atomicWrite(baseline, bytes);
        }
    }
    if (!packaged && !migrated) atomicWrite(state / "legacy-import-v1", bundle.string());
}
}
