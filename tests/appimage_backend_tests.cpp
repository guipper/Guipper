#include "../src/JPutils/jp_update_service.h"
#include "../src/JPutils/jp_app_paths.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <unistd.h>
using S=jp::UpdateState;
template<class Predicate> void waitUntil(Predicate ready) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while (!ready()) {
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
int main() {
    auto root=std::filesystem::temp_directory_path()/("guipper-worker-test-"+std::to_string(getpid()));
    auto& paths=jp::AppPaths::current();paths.bundle=root/"data";paths.cache=root/"cache";
    std::filesystem::create_directories(paths.bundle);
    const auto helper=root/"guipper-update-worker";
    auto script=[&](const std::string& body) {
        jp::atomicWrite(helper,"#!/bin/sh\n"+body);
        std::filesystem::permissions(helper,std::filesystem::perms::owner_all);
    };
    const auto original=root/"Guipper.AppImage";
    jp::atomicWrite(original,"original");setenv("APPIMAGE",original.c_str(),1);
    script("sleep 60\n");
    jp::UpdateService service(jp::platformUpdateBackend());
    service.check(true,1);assert(service.status().state==S::Checking);
    const auto start=std::chrono::steady_clock::now();
    service.cancel();assert(service.status().state==S::Cancelled);
    assert(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100));
    script("echo 'GUIPPER_UPDATE AVAILABLE Guipper-0.2.0-linux-x64.AppImage' >&3\nread command\necho 'GUIPPER_UPDATE PROGRESS 0.5' >&3\nsleep 60\n");
    service.check(true,2);waitUntil([&]{return service.status().state==S::Available;});
    assert(!service.status().version.empty());
    service.channel="beta";service.check(true,3);
    assert(service.status().state==S::Checking);
    waitUntil([&]{return service.status().state==S::Available;});
    service.download();waitUntil([&]{return service.status().progress==0.5;});
    service.cancel();assert(service.status().state==S::Cancelled);
    assert(jp::readBytes(original)=="original");
    script("echo 'GUIPPER_UPDATE AVAILABLE candidate.AppImage' >&3\nread command\necho 'GUIPPER_UPDATE ERROR Update signature rejected' >&3\n");
    service.check(true,3);waitUntil([&]{return service.status().state==S::Available;});
    service.download();waitUntil([&]{return service.status().state==S::Error;});
    assert(!service.install(true,true));
    script("echo 'GUIPPER_UPDATE AVAILABLE candidate.AppImage' >&3\nread command\necho 'GUIPPER_UPDATE READY' >&3\nread command\nsleep 60\n");
    service.check(true,4);waitUntil([&]{return service.status().state==S::Available;});service.download();
    waitUntil([&]{return service.status().state==S::Ready;});
    assert(!service.install(false,true));assert(!service.install(true,false));
    assert(service.install(true,true));assert(service.status().state==S::Installing);
    assert(!service.exitRequested());service.cancel();
    unsetenv("APPIMAGE");
    jp::UpdateService unpackaged(jp::platformUpdateBackend());
    unpackaged.cancel();assert(unpackaged.status().state==S::Disabled);
    std::cout << "AppImage process lifecycle tests passed\n";
    // Reapers may still remove their own isolated download directories.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::filesystem::remove_all(root);
}
