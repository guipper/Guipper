#include "../src/JPutils/jp_update_service.h"
#include <appimage/update.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
using U=appimage::update::Updater;
using S=jp::UpdateState;
template<class Predicate> void waitUntil(Predicate ready) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while (!ready()) {
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
int main() {
    setenv("APPIMAGE","/tmp/simulated-guipper.AppImage",1);
    jp::UpdateService service(jp::platformUpdateBackend());
    service.check(true,1);
    waitUntil([]{return U::checkEntered.load();});
    const int before=U::constructions;
    service.cancel();
    assert(service.status().state==S::Checking);
    service.check(true,2);
    assert(U::constructions==before);
    U::releaseCheck=true;
    waitUntil([&]{return service.status().state==S::Cancelled;});
    service.check(true,3);
    waitUntil([&]{return service.status().state==S::Available;});
    service.download();
    assert(service.status().state==S::Downloading);
    service.cancel(); service.cancel();
    assert(U::stops==1);
    assert(service.status().state==S::Downloading);
    const int downloading=U::constructions;
    service.check(true,4);
    assert(U::constructions==downloading);
    assert(!service.install(true,true));
    U::downloadState=U::SUCCESS; // completion racing cancellation must not offer install
    assert(service.status().state==S::Cancelled);
    for (auto signature: {U::VALIDATION_NOT_SIGNED,U::VALIDATION_BAD_SIGNATURE,U::VALIDATION_PASSED}) {
        service.check(true,5);
        waitUntil([&]{return service.status().state==S::Available;});
        service.download();
        U::signature=signature; U::downloadState=U::SUCCESS;
        assert(service.status().state==(signature==U::VALIDATION_PASSED?S::Ready:S::Error));
    }
    service.cancel();
    assert(service.status().state==S::Cancelled);
    unsetenv("APPIMAGE");
    jp::UpdateService unpackaged(jp::platformUpdateBackend());
    unpackaged.cancel();
    assert(unpackaged.status().state==S::Disabled);
    std::cout << "AppImage adapter lifecycle tests passed\n";
}
