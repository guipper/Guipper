#include "jp_recovery.h"
#include "../JPbox/JPboxgroup.h"
#include <functional>
#include <map>

namespace jp {
namespace {
struct Capture {
    std::vector<ofXml> documents;
    std::vector<std::string> names;
    std::map<std::string,std::string> groupNames;
    std::string identity;
};
Capture captureGraph(JPboxgroup& group) {
    std::vector<ofXml> documents;
    std::vector<std::string> names;
    std::map<std::string, std::string> groupNames;
    documents.push_back(group.snapshotXml()); names.push_back("session.xml");
    std::function<void(JPbox*)> capture = [&](JPbox* box) {
        if (auto* preset = dynamic_cast<JPbox_preset*>(box)) {
            const auto key = preset->uid;
            if (groupNames.count(key)) return;
            const auto name = "group-" + std::to_string(documents.size()) + ".xml";
            groupNames[key] = name;
            documents.push_back(preset->snapshotXml()); names.push_back(name);
            for (auto* child : preset->boxes) capture(child);
        }
    };
    for (auto* box : group.boxes) capture(box);
    std::string identity;
    for (auto& doc : documents) { const auto bytes = doc.toString(); identity += std::to_string(bytes.size()) + ":" + bytes; }
    return {std::move(documents),std::move(names),std::move(groupNames),std::move(identity)};
}
}
void RecoveryService::markSaved(JPboxgroup& group, bool clearPending) {
    if (AppPaths::current().state.empty()) return;
    if (clearPending) finish(true);
    previous=captureGraph(group).identity;
}

RecoveryService::~RecoveryService() { if (writer.valid()) writer.wait(); }
void RecoveryService::finish(bool clean) {
    if (writer.valid()) {
        try { writer.get(); } catch (const std::exception& e) { ofLogError("recovery") << e.what(); }
    }
    if (clean) dismiss();
}
std::string RecoveryService::pending() const {
    const auto& paths = AppPaths::current();
    if (paths.state.empty()) return {};
    try {
        const auto marker = paths.state / "recovery/latest";
        if (!std::filesystem::exists(marker)) return {};
        const auto generation = readBytes(marker);
        if (generation.empty() || generation.find_first_not_of("0123456789") != std::string::npos) return {};
        const auto file = paths.state / "recovery" / generation / "session.xml";
        return std::filesystem::is_regular_file(file) ? file.string() : std::string();
    } catch (...) { return {}; }
}
void RecoveryService::dismiss() {
    if (AppPaths::current().state.empty()) return;
    std::error_code ignored;
    std::filesystem::remove(AppPaths::current().state / "recovery/latest", ignored);
}
void RecoveryService::tick(JPboxgroup& group, double now) {
    if (AppPaths::current().state.empty() || now < next) return;
    next = now + 120.0;
    if (writer.valid()) {
        if (writer.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try { writer.get(); }
        catch (const std::exception& error) {
            previous.clear(); // retry unchanged snapshot after write failure
            ofLogError("recovery") << error.what();
        }
    }
    auto captured=captureGraph(group);
    auto& documents=captured.documents;
    auto& names=captured.names;
    auto& groupNames=captured.groupNames;
    auto& identity=captured.identity;
    if (identity == previous) return;
    const auto generation = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    const auto root = AppPaths::current().state / "recovery";
    const auto destination = root / generation;
    std::vector<std::pair<std::string, std::string>> files;
    for (size_t i = 0; i < documents.size(); ++i) {
        for (auto node : documents[i].getChildren("box")) {
            auto dir = node.getChild("directory");
            auto found = groupNames.find(node.getChild("uid").getValue());
            if (found != groupNames.end()) dir.set((destination / found->second).string());
        }
        files.emplace_back(names[i], documents[i].toString());
    }
    previous = std::move(identity);
    writer = std::async(std::launch::async, [files=std::move(files), destination, root, generation]() {
        for (const auto& file : files) atomicWrite(destination / file.first, file.second);
        // Publish only after every nested group is complete. Incomplete folders
        // are harmless and cannot be offered as a recovery snapshot.
        atomicWrite(root / "latest", generation);
        recordEvent("recovery_snapshot_published");
    });
}
}
