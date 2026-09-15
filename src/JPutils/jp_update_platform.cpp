#include "jp_update_service.h"
#if !defined(GUIPPER_UPDATE_TEST) && __has_include("jp_update_config.h")
#include "jp_update_config.h"
#endif
#include "jp_version.h"
#include "jp_app_paths.h"
#include <cstring>
#include <cstdlib>
#include <thread>

// Enable only in signed, packaged builds. Development checkouts intentionally
// have no update trust root and must not install arbitrary remote binaries.
#if defined(GUIPPER_APPIMAGE_UPDATES)
#include <filesystem>
#include <unistd.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <chrono>
extern char** environ;
namespace jp {
#ifdef GUIPPER_UPDATE_TEST
#define GUIPPER_APPIMAGE_SIGNING_FINGERPRINT "test-key"
#endif
static const volatile char updateBuildConfiguration[]="GUIPPER_LINUX_UPDATES_V1\n"
    GUIPPER_APPIMAGE_STABLE "\n" GUIPPER_APPIMAGE_BETA "\n" GUIPPER_APPIMAGE_SIGNING_FINGERPRINT;
class AppImageBackend final : public UpdateBackend {
    UpdateStatus current{UpdateState::Idle,0,""};
    std::string original,buffer,downloadFolder;
    bool keepDownload=false;
    pid_t worker=-1;
    int commands=-1,events=-1;
    bool exitReady=false;
    std::chrono::steady_clock::time_point started;
    void stop() {
        if (commands>=0) { ::close(commands); commands=-1; }
        if (events>=0) { ::close(events); events=-1; }
        if (worker>0) {
            ::kill(-worker,SIGTERM);
            const pid_t child=worker;
            const auto cleanup=keepDownload?std::string():downloadFolder;
            // Reaping never waits on the renderer. SDK work and GPG children
            // belong to the worker's process group, not Guipper's.
            std::thread([child,cleanup]{
                int result; while (::waitpid(child,&result,0)<0 && errno==EINTR) {}
                if (!cleanup.empty()) {std::error_code error;std::filesystem::remove_all(cleanup,error);}
            }).detach();
            worker=-1;
        }
        buffer.clear();
    }
    void fail(const std::string& message) { stop(); current={UpdateState::Error,0,message}; }
    bool send(const char* command) {
        // A socket avoids SIGPIPE if the worker exits between polling and send.
        if (commands<0 || ::send(commands,command,std::strlen(command),MSG_NOSIGNAL)<0) {
            fail("Update worker stopped; current version kept."); return false;
        }
        started=std::chrono::steady_clock::now();
        return true;
    }
    void launchInstaller(const std::string& next) {
        try {
            const auto& paths=AppPaths::current();
            const auto helper=paths.cache / "install-appimage.sh";
            const auto health=paths.cache / ("health-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            atomicWrite(helper,readBytes(paths.bundle.parent_path()/"install-appimage.sh"));
            const auto parent=std::to_string(::getpid());
            pid_t child;
            std::string script=helper.string(),healthPath=health.string();
            const char* argv[]={"sh",script.c_str(),original.c_str(),next.c_str(),parent.c_str(),healthPath.c_str(),nullptr};
            posix_spawnattr_t attrs;posix_spawnattr_init(&attrs);
            posix_spawnattr_setflags(&attrs,POSIX_SPAWN_SETPGROUP);posix_spawnattr_setpgroup(&attrs,0);
            const int error=posix_spawn(&child,"/bin/sh",nullptr,&attrs,const_cast<char**>(argv),environ);
            posix_spawnattr_destroy(&attrs);
            if (error) throw std::runtime_error("Could not launch installer; current version kept.");
            keepDownload=true;
            exitReady=true;
            stop();
        } catch (const std::exception& e) { fail(e.what()); }
    }
    void receive(const std::string& line) {
        const std::string prefix="GUIPPER_UPDATE ";
        if (line.compare(0,prefix.size(),prefix)!=0) { fail("Invalid update worker response."); return; }
        const auto value=line.substr(prefix.size());
        if (value=="IDLE" && current.state==UpdateState::Checking) { stop();current={UpdateState::Idle,0,""}; }
        else if (value.rfind("AVAILABLE ",0)==0 && current.state==UpdateState::Checking) {
            current={UpdateState::Available,0,""}; current.version=value.substr(10);
        }
        else if (value.rfind("NOTES https://github.com/",0)==0 && current.state==UpdateState::Available)
            current.notesUrl=value.substr(6);
        else if (value.rfind("PROGRESS ",0)==0 && current.state==UpdateState::Downloading) {
            try { current.progress=std::stod(value.substr(9)); } catch (...) { fail("Invalid download progress."); }
        }
        else if (value=="VERIFYING" && current.state==UpdateState::Downloading) current.message="Verifying signature...";
        else if (value=="READY" && current.state==UpdateState::Downloading) {current.state=UpdateState::Ready;current.progress=1;current.message="Verified update ready.";}
        else if (value.rfind("INSTALL ",0)==0 && current.state==UpdateState::Installing) launchInstaller(value.substr(8));
        else if (value.rfind("ERROR ",0)==0) fail(value.substr(6));
        else fail("Unexpected update worker response.");
    }
public:
    AppImageBackend() {
        (void)updateBuildConfiguration[0];
        const char* image=std::getenv("APPIMAGE");
        if (!image || !*image) {current={UpdateState::Disabled,0,"Run the signed AppImage to update."};return;}
        original=std::filesystem::absolute(image).string();
    }
    ~AppImageBackend() override {stop();}
    UpdateStatus status() override {
        if (events>=0) {
            char chunk[1024];
            const auto count=::read(events,chunk,sizeof(chunk));
            if (count>0) buffer.append(chunk,size_t(count));
            else if (count==0) { fail("Update worker stopped; current version kept."); return current; }
            if (buffer.size()>8192) {fail("Invalid update response size.");return current;}
            size_t end;
            while (events>=0 && (end=buffer.find('\n'))!=std::string::npos) {
                const auto line=buffer.substr(0,end);buffer.erase(0,end+1);receive(line);
            }
        }
        const auto elapsed=std::chrono::steady_clock::now()-started;
        if ((current.state==UpdateState::Checking || current.state==UpdateState::Installing) && elapsed>std::chrono::seconds(60))
            fail("Update operation timed out; current version kept.");
        if (current.state==UpdateState::Downloading && elapsed>std::chrono::hours(2))
            fail("Download timed out; current version kept.");
        return current;
    }
    void check(const std::string& channel,bool) override {
        if (current.state==UpdateState::Disabled || current.state==UpdateState::Checking ||
            current.state==UpdateState::Downloading || current.state==UpdateState::Ready ||
            current.state==UpdateState::Installing) return;
        stop();
        int input[2],output[2];
        if (::socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,input)<0) {fail("Could not start update worker.");return;}
        if (::pipe2(output,O_CLOEXEC)<0) {::close(input[0]);::close(input[1]);fail("Could not start update worker.");return;}
        const auto helper=(AppPaths::current().bundle.parent_path()/"guipper-update-worker").string();
        const char* feed=channel=="beta"?GUIPPER_APPIMAGE_BETA:GUIPPER_APPIMAGE_STABLE;
        std::string pattern=(std::filesystem::path(original).parent_path()/".guipper-update-XXXXXX").string();
        if (!::mkdtemp(pattern.data())) {
            ::close(input[0]);::close(input[1]);::close(output[0]);::close(output[1]);
            fail("Move the AppImage to a writable folder before updating.");return;
        }
        downloadFolder=pattern;keepDownload=false;
        const char* argv[]={helper.c_str(),original.c_str(),feed,downloadFolder.c_str(),nullptr};
        posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions,input[1],0);
        posix_spawn_file_actions_adddup2(&actions,output[1],3);
        posix_spawn_file_actions_addopen(&actions,1,"/dev/null",O_WRONLY,0);
        posix_spawn_file_actions_addopen(&actions,2,"/dev/null",O_WRONLY,0);
        posix_spawnattr_t attrs;posix_spawnattr_init(&attrs);
        posix_spawnattr_setflags(&attrs,POSIX_SPAWN_SETPGROUP);posix_spawnattr_setpgroup(&attrs,0);
        const int error=posix_spawn(&worker,helper.c_str(),&actions,&attrs,const_cast<char**>(argv),environ);
        posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attrs);
        ::close(input[1]);::close(output[1]);
        if (error) {std::filesystem::remove(downloadFolder);worker=-1;::close(input[0]);::close(output[0]);fail("Update worker unavailable in this package.");return;}
        commands=input[0];events=output[0];fcntl(events,F_SETFL,O_NONBLOCK);
        current={UpdateState::Checking,0,""};started=std::chrono::steady_clock::now();
    }
    void download() override {if (current.state==UpdateState::Available && send("DOWNLOAD\n")) current.state=UpdateState::Downloading;}
    void cancel() override {stop();if (current.state!=UpdateState::Disabled) current={UpdateState::Cancelled,0,""};}
    bool install() override {
        if (current.state!=UpdateState::Ready || !send("INSTALL\n")) return false;
        current.state=UpdateState::Installing;current.message="Verifying before installation...";
        return true;
    }
    bool readyToExit() const override {return exitReady;}
};
std::unique_ptr<UpdateBackend> platformUpdateBackend() {return std::make_unique<AppImageBackend>();}
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
