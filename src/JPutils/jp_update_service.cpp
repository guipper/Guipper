#include "jp_update_service.h"
namespace jp {
UpdateService::UpdateService(std::unique_ptr<UpdateBackend> adapter): backend(std::move(adapter)) {}
UpdateStatus UpdateService::status() { return backend->status(); }
void UpdateService::check(bool manual, std::int64_t now) {
    const auto state = status().state;
    if (state == UpdateState::Disabled || state == UpdateState::Checking ||
        state == UpdateState::Downloading || state == UpdateState::Ready) return;
    if (!manual && (!automatic || (lastCheck != 0 && now - lastCheck < 86400))) return;
    if (channel != "stable" && channel != "beta") channel = "stable";
    lastCheck = now;
    backend->check(channel, manual);
}
void UpdateService::download() {
    if (status().state == UpdateState::Available) backend->download();
}
void UpdateService::cancel() { backend->cancel(); }
bool UpdateService::install(bool explicitAction, bool saved) {
    return explicitAction && saved && status().state == UpdateState::Ready && backend->install();
}
}
