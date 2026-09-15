#pragma once
#include <memory>
#include <string>
#include <cstdint>
#include <utility>

namespace jp {
enum class UpdateState { Disabled, Idle, Checking, Available, Downloading, Ready, Cancelled, Error, Installing };
struct UpdateStatus {
    UpdateState state = UpdateState::Disabled;
    double progress = 0;
    std::string message;
    std::string version;
    std::string notesUrl;
    UpdateStatus(UpdateState s=UpdateState::Disabled,double p=0,std::string m=""): state(s),progress(p),message(std::move(m)) {}
};
class UpdateBackend {
public:
    virtual ~UpdateBackend() = default;
    virtual UpdateStatus status() = 0;
    virtual void check(const std::string& channel, bool manual) = 0;
    virtual void download() = 0;
    virtual void cancel() = 0;
    // Called only after explicit install action and successful document save.
    virtual bool install() = 0;
    virtual bool readyToExit() const {return true;}
};
class UpdateService {
public:
    explicit UpdateService(std::unique_ptr<UpdateBackend> backend);
    UpdateStatus status();
    void check(bool manual, std::int64_t now);
    void download();
    void cancel();
    bool install(bool explicitAction, bool saved);
    bool exitRequested() const;
    void skip();
    std::string skippedVersion;
    bool automatic = false;
    std::int64_t lastCheck = 0;
    std::string channel = "stable";
private:
    std::unique_ptr<UpdateBackend> backend;
    bool installAccepted=false,manualCheck=false;
};
std::unique_ptr<UpdateBackend> platformUpdateBackend();
}
