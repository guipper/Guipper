#pragma once
#include <atomic>
#include <string>
#include <thread>
namespace appimage::update {
// Controllable asynchronous SDK boundary; no network or installer execution.
class Updater {
public:
    enum State { INITIALIZED, RUNNING, STOPPING, SUCCESS, ERROR };
    enum ValidationState { VALIDATION_PASSED=0, VALIDATION_NOT_SIGNED=1001, VALIDATION_BAD_SIGNATURE=2003 };
    inline static std::atomic<bool> releaseCheck{false}, checkEntered{false};
    inline static std::atomic<int> constructions{0}, stops{0};
    inline static State downloadState=INITIALIZED;
    inline static ValidationState signature=VALIDATION_PASSED;
    explicit Updater(const std::string&,bool) { ++constructions; downloadState=INITIALIZED; }
    bool checkForChanges(bool& available) {
        checkEntered=true;
        while (!releaseCheck.load()) std::this_thread::yield();
        available=true; return true;
    }
    void setUpdateInformation(const std::string&) {}
    State state() { return downloadState; }
    bool start() { downloadState=RUNNING; return true; }
    bool stop() { ++stops; downloadState=STOPPING; return true; }
    bool isDone() { return downloadState==SUCCESS || downloadState==ERROR; }
    bool hasError() { return downloadState==ERROR; }
    bool progress(double& value) { value=0.5; return true; }
    ValidationState validateSignature() { return signature; }
    bool pathToNewFile(std::string&) { return false; }
    void copyPermissionsToNewFile() {}
};
}
