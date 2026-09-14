#pragma once
#include <filesystem>
#include <string>

namespace jp {
// All paths are absolute. No graphics/framework dependencies.
class AppPaths {
public:
    std::filesystem::path bundle, data, config, state, cache;
    static AppPaths discover(const std::filesystem::path& bundle);
    void initialize() const;
    // Explicit old data directory, imported conservatively without overwrites.
    void importLegacy(const std::filesystem::path& source) const;
    std::filesystem::path preference(const std::string& name) const;
    // Removes resource management before an explicit edit; future upgrades
    // must preserve even a personal copy identical to the original.
    void makePersonal(const std::filesystem::path& path) const;
    static AppPaths& current();
};
// Replace on the same filesystem, never truncate the destination in place.
// Reports failure via exception; a failed write leaves the old file intact.
void atomicWrite(const std::filesystem::path& path, const std::string& bytes);
void recordEvent(const std::string& code);
std::string readBytes(const std::filesystem::path& path);
}
