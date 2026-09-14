#include "jp_update_service.h"
#if __has_include("jp_update_config.h")
#include "jp_update_config.h"
#endif
#include "jp_version.h"
#include "jp_app_paths.h"
#include <future>
#include <cstdlib>

// Enable only in signed, packaged builds. Development checkouts intentionally
// have no update trust root and must not install arbitrary remote binaries.
#if defined(GUIPPER_APPIMAGE_UPDATES)
#include <appimage/update.h>
#include <filesystem>
#include <unistd.h>
namespace jp {
class AppImageBackend final : public UpdateBackend {
    std::unique_ptr<appimage::update::Updater> updater;
    std::future<bool> checking;
    UpdateStatus current{UpdateState::Idle,0,""};
    std::string original;
    bool cancelRequested=false;
public:
    AppImageBackend() {
        const char* image = std::getenv("APPIMAGE");
        if (!image || !*image) { current={UpdateState::Disabled,0,"Run the signed AppImage to update."}; return; }
        original=image;
        updater=std::make_unique<appimage::update::Updater>(original, false);
    }
    ~AppImageBackend() override {
        if (checking.valid()) checking.wait();
        if (updater && updater->state()==appimage::update::Updater::RUNNING) updater->stop();
    }
    UpdateStatus status() override {
        if (checking.valid() && checking.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {
                const bool available=checking.get();
                current={cancelRequested?UpdateState::Cancelled:
                    (available?UpdateState::Available:UpdateState::Idle),0,""};
            } catch (const std::exception& e) {
                current=cancelRequested?UpdateStatus{UpdateState::Cancelled,0,""}:
                    UpdateStatus{UpdateState::Error,0,e.what()};
            }
            cancelRequested=false;
        }
        if (current.state==UpdateState::Downloading) {
            updater->progress(current.progress);
            if (updater->isDone()) {
                if (cancelRequested) {
                    current={UpdateState::Cancelled,0,""};
                    cancelRequested=false;
                }
                else if (updater->hasError()) current={UpdateState::Error,0,"Download failed; current version kept."};
                else if (updater->validateSignature()!=appimage::update::Updater::VALIDATION_PASSED)
                    current={UpdateState::Error,0,"Update signature rejected; current version kept."};
                else current={UpdateState::Ready,1,"Verified update ready."};
            }
        }
        return current;
    }
    void check(const std::string& channel, bool) override {
        // STOPPING is still active: replacing its owner can block the UI or
        // race the SDK worker. Keep reporting Downloading until isDone().
        if (current.state==UpdateState::Disabled || checking.valid() ||
            current.state==UpdateState::Downloading || current.state==UpdateState::Ready) return;
        cancelRequested=false;
        updater=std::make_unique<appimage::update::Updater>(original,false);
        updater->setUpdateInformation(channel=="beta" ? GUIPPER_APPIMAGE_BETA : GUIPPER_APPIMAGE_STABLE);
        current={UpdateState::Checking,0,""};
        checking=std::async(std::launch::async,[this]{
            bool available=false;
            if (!updater->checkForChanges(available)) throw std::runtime_error("Could not check updates.");
            return available;
        });
    }
    void download() override {
        if (current.state!=UpdateState::Available || cancelRequested) return;
        current=updater->start()?UpdateStatus{UpdateState::Downloading,0,""}:
            UpdateStatus{UpdateState::Error,0,"Could not start download."};
    }
    void cancel() override {
        if (checking.valid()) {
            // The SDK exposes no check cancellation. Discard its eventual
            // result, keeping Checking until the worker releases the updater.
            cancelRequested=true;
            current.message="Cancelling check...";
            return;
        }
        if (current.state==UpdateState::Downloading) {
            if (!cancelRequested) {
                cancelRequested=true;
                if (updater->state()==appimage::update::Updater::RUNNING) updater->stop();
            }
            current.message="Stopping download...";
            return;
        }
        if (current.state!=UpdateState::Disabled) current={UpdateState::Cancelled,0,""};
    }
    bool install() override {
        if (current.state!=UpdateState::Ready || cancelRequested) return false;
        std::string next;
        if (!updater->pathToNewFile(next) || updater->validateSignature()!=appimage::update::Updater::VALIDATION_PASSED)
            return false;
        updater->copyPermissionsToNewFile();
        const auto& paths = AppPaths::current();
        const auto helper = paths.cache / "install-appimage.sh";
        const auto health = paths.cache / ("health-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        atomicWrite(helper, readBytes(paths.bundle.parent_path() / "install-appimage.sh"));
        const auto parent = std::to_string(::getpid());
        const pid_t child = ::fork();
        if (child < 0) return false;
        if (child == 0) {
            ::setsid();
            ::execl("/bin/sh", "sh", helper.c_str(), original.c_str(), next.c_str(),
                parent.c_str(), health.c_str(), static_cast<char*>(nullptr));
            ::_exit(127);
        }
        return true;
    }
};
std::unique_ptr<UpdateBackend> platformUpdateBackend() { return std::make_unique<AppImageBackend>(); }
}
#elif defined(_WIN32) && defined(GUIPPER_WINSPARKLE)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <winsparkle.h>
#include <mutex>
namespace jp {
class WinSparkleBackend final : public UpdateBackend {
    std::mutex mutex;
    UpdateStatus current{UpdateState::Idle,0,""};
    std::wstring installer;
    bool initialized=false;
    static WinSparkleBackend* instance;
    static void setState(UpdateState state) {
        if (!instance) return;
        std::lock_guard<std::mutex> lock(instance->mutex);
        instance->current.state=state;
    }
    static int stageInstaller(const wchar_t* payload) {
        if (!instance) return -1;
        try {
            const auto target=AppPaths::current().cache / "Guipper-update.exe";
            atomicWrite(target,readBytes(std::filesystem::path(payload)));
            std::lock_guard<std::mutex> lock(instance->mutex);
            instance->installer=target.wstring();
            instance->current={UpdateState::Ready,1,"Verified update ready."};
            return 1; // handled: never let WinSparkle launch it itself
        } catch (...) { setState(UpdateState::Error); return -1; }
    }
public:
    WinSparkleBackend() {
        instance=this;
        win_sparkle_set_app_details(L"Guipper",L"Guipper", GUIPPER_WIDE_VERSION);
        if (!win_sparkle_set_eddsa_public_key(GUIPPER_WINSPARKLE_PUBLIC_KEY)) {
            current={UpdateState::Disabled,0,"Invalid update public key."}; return;
        }
        win_sparkle_set_automatic_check_for_updates(0); // shared policy owns consent/time
        win_sparkle_set_appcast_url(GUIPPER_WINSPARKLE_STABLE);
        win_sparkle_set_did_find_update_callback([]{setState(UpdateState::Available);});
        win_sparkle_set_did_not_find_update_callback([]{setState(UpdateState::Idle);});
        win_sparkle_set_error_callback([]{setState(UpdateState::Error);});
        win_sparkle_set_update_cancelled_callback([]{setState(UpdateState::Cancelled);});
        win_sparkle_set_can_shutdown_callback([]{return 1;});
        win_sparkle_set_shutdown_request_callback([]{}); // main thread controls shutdown
        win_sparkle_set_user_run_installer_callback(stageInstaller);
        win_sparkle_init(); initialized=true;
    }
    ~WinSparkleBackend() override { if (initialized) win_sparkle_cleanup(); instance=nullptr; }
    UpdateStatus status() override { std::lock_guard<std::mutex> lock(mutex); return current; }
    void check(const std::string& channel,bool manual) override {
        // WinSparkle configuration is immutable after init: channel changes
        // reinitialize it, after the policy has ruled out an active operation.
        if (initialized) win_sparkle_cleanup();
        win_sparkle_set_appcast_url(channel=="beta"?GUIPPER_WINSPARKLE_BETA:GUIPPER_WINSPARKLE_STABLE);
        win_sparkle_init(); initialized=true; setState(UpdateState::Checking);
        if (manual) win_sparkle_check_update_with_ui();
        else win_sparkle_check_update_without_ui();
    }
    void download() override { win_sparkle_check_update_with_ui(); }
    void cancel() override {
        if (initialized) { win_sparkle_cleanup(); initialized=false; }
        setState(UpdateState::Cancelled);
    }
    bool install() override {
        std::wstring target;
        { std::lock_guard<std::mutex> lock(mutex); target=installer; }
        if (target.empty()) return false;
        SHELLEXECUTEINFOW execution{};
        execution.cbSize=sizeof(execution); execution.fMask=SEE_MASK_NOCLOSEPROCESS;
        execution.lpVerb=L"open"; execution.lpFile=target.c_str(); execution.nShow=SW_SHOWNORMAL;
        if (!ShellExecuteExW(&execution)) return false;
        if (execution.hProcess) CloseHandle(execution.hProcess);
        return true;
    }
};
WinSparkleBackend* WinSparkleBackend::instance=nullptr;
std::unique_ptr<UpdateBackend> platformUpdateBackend() { return std::make_unique<WinSparkleBackend>(); }
}

#else
namespace jp {
class UnconfiguredBackend final : public UpdateBackend {
public:
    UpdateStatus status() override { return {UpdateState::Disabled,0,"Updates are not configured for this build."}; }
    void check(const std::string&, bool) override {}
    void download() override {}
    void cancel() override {}
    bool install() override { return false; }
};
std::unique_ptr<UpdateBackend> platformUpdateBackend() { return std::make_unique<UnconfiguredBackend>(); }
}
#endif
