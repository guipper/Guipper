#include "../src/JPutils/jp_app_paths.h"
#include <cassert>
#include <iostream>
namespace fs = std::filesystem;
int main() {
    const auto root = fs::temp_directory_path() / ("guipper-paths-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    jp::AppPaths p;
    p.bundle=root/"bundle"; p.data=root/"data"; p.config=root/"config"; p.state=root/"state"; p.cache=root/"cache";
    jp::atomicWrite(p.bundle/"shaders/example.frag", "official v1");
    jp::atomicWrite(p.bundle/"settings.xml", "legacy preferences");
    jp::atomicWrite(p.bundle/"savefiles/user.xml", "user project");
    p.initialize();
    assert(jp::readBytes(p.preference("settings.xml")) == "legacy preferences");
    assert(jp::readBytes(p.data/"savefiles/user.xml") == "user project");
    jp::atomicWrite(p.data/"savefiles/user.xml", "edited project");
    p.initialize();
    assert(jp::readBytes(p.data/"savefiles/user.xml") == "edited project");
    assert(jp::readBytes(p.bundle/"savefiles/user.xml") == "user project");
    jp::atomicWrite(p.bundle/"distribution.marker", "1");
    jp::atomicWrite(p.bundle/"shaders/new.frag", "v1");
    p.initialize();
    jp::atomicWrite(p.bundle/"shaders/new.frag", "v2");
    p.initialize();
    assert(jp::readBytes(p.data/"shaders/new.frag") == "v2");
    jp::atomicWrite(p.data/"shaders/new.frag", "personal");
    jp::atomicWrite(p.bundle/"shaders/new.frag", "v3");
    p.initialize();
    assert(jp::readBytes(p.data/"shaders/new.frag") == "personal");
    jp::atomicWrite(p.bundle/"shaders/explicit.frag", "same");
    p.initialize();
    p.makePersonal(p.data/"shaders/explicit.frag");
    jp::atomicWrite(p.bundle/"shaders/explicit.frag", "changed");
    p.initialize();
    assert(jp::readBytes(p.data/"shaders/explicit.frag") == "same");
    fs::remove_all(p.cache);
    p.initialize();
    assert(jp::readBytes(p.data/"shaders/new.frag") == "personal");
    bool failed=false;
    fs::create_directory(root/"directory-target");
    try { jp::atomicWrite(root/"directory-target", "bad"); } catch (...) { failed=true; }
    assert(failed && fs::is_directory(root/"directory-target"));
    for (const auto& item : fs::directory_iterator(root)) assert(item.path().string().find(".tmp-")==std::string::npos);
    auto overlapping=p; overlapping.data=p.bundle/"profile";
    bool overlapRejected=false;
    try { overlapping.initialize(); } catch (...) { overlapRejected=true; }
    assert(overlapRejected);
    auto second=root/"old-location";
    jp::atomicWrite(second/"settings.xml","other settings");
    jp::atomicWrite(second/"shaders/custom.frag","custom");
    p.importLegacy(second);
    assert(jp::readBytes(p.data/"shaders/custom.frag")=="custom");
    assert(jp::readBytes(p.preference("settings.xml"))=="legacy preferences");
    fs::remove_all(root);
    std::cout << "app paths and atomic storage tests passed\n";
}
