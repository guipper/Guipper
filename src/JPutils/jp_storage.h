#pragma once
#include "ofMain.h"
#include "jp_app_paths.h"
namespace jp {
inline std::string preferencePath(const std::string& name) {
    const auto& paths = AppPaths::current();
    return paths.config.empty() ? ofToDataPath(name, true) : paths.preference(name).string();
}
inline bool saveXml(const ofXml& xml, const std::string& path) {
    try {
        const auto target = ofToDataPath(path, true);
        if (xml.getChild("guipper_format") && std::filesystem::is_regular_file(target)) {
            ofXml previous;
            if (previous.load(target) && !previous.getChild("guipper_format")) {
                const auto backup = target + ".pre-v1.bak";
                if (!std::filesystem::exists(backup)) atomicWrite(backup, readBytes(target));
            }
        }
        atomicWrite(target, xml.toString()); return true;
    }
    catch (const std::exception& error) { ofLogError("storage") << error.what(); return false; }
}
}
