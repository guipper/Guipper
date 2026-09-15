#include "jp_update_service.h"
namespace jp {
UpdateService::UpdateService(std::unique_ptr<UpdateBackend> adapter): backend(std::move(adapter)) {}
UpdateStatus UpdateService::status() {
    auto result=backend->status();
    if (!manualCheck && result.state==UpdateState::Available && !skippedVersion.empty() &&
        skippedVersion==channel+":"+result.version) {backend->cancel();result=backend->status();}
    return result;
}
void UpdateService::skip() {
    const auto value=status();
    if (value.state==UpdateState::Available && !value.version.empty()) {
        skippedVersion=channel+":"+value.version;backend->cancel();
    }
}
bool UpdateService::exitRequested() const {return installAccepted && backend->readyToExit();}
void UpdateService::check(bool manual, std::int64_t now) {
    const auto state = status().state;
    if (state == UpdateState::Disabled || state == UpdateState::Checking ||
        state == UpdateState::Downloading || state == UpdateState::Ready || state == UpdateState::Installing) return;
    if (!manual && (!automatic || (lastCheck != 0 && now - lastCheck < 86400))) return;
    if (channel != "stable" && channel != "beta") channel = "stable";
    manualCheck=manual;
    lastCheck = now;
    backend->check(channel, manual);
}
void UpdateService::download() {
    if (status().state == UpdateState::Available) backend->download();
}
void UpdateService::cancel() {
    if (exitRequested()) return;
    installAccepted=false;backend->cancel();
}
bool UpdateService::install(bool explicitAction, bool saved) {
    installAccepted=explicitAction && saved && status().state == UpdateState::Ready && backend->install();
    return installAccepted;
}
}
