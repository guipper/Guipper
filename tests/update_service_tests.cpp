#include "../src/JPutils/jp_update_service.h"
#include <cassert>
#include <iostream>
struct Fake final : jp::UpdateBackend {
    jp::UpdateStatus value{jp::UpdateState::Idle,0,""};
    int checks=0, downloads=0, installs=0;
    bool exitReady=true;
    bool readyToExit() const override {return exitReady;}
    jp::UpdateStatus status() override { return value; }
    void check(const std::string& channel,bool) override { assert(channel=="stable" || channel=="beta"); ++checks; }
    void download() override { ++downloads; value.state=jp::UpdateState::Downloading; }
    void cancel() override { value.state=jp::UpdateState::Cancelled; }
    bool install() override { ++installs; return true; }
};
int main() {
    auto fake=std::make_unique<Fake>(); auto* raw=fake.get();
    jp::UpdateService service(std::move(fake));
    service.check(false,100); assert(raw->checks==0);
    service.check(true,100); assert(raw->checks==1);
    service.automatic=true;
    service.check(false,101); assert(raw->checks==1);
    service.check(false,86500); assert(raw->checks==2);
    service.check(false,1); assert(raw->checks==2); // clock moved backwards
    service.download(); assert(raw->downloads==0);
    raw->value.state=jp::UpdateState::Available;
    service.download(); assert(raw->downloads==1);
    assert(!service.install(true,true));
    raw->value.state=jp::UpdateState::Ready;
    assert(!service.install(false,true)); assert(!service.install(true,false));
    assert(service.install(true,true) && raw->installs==1);
    assert(service.exitRequested());
    service.cancel();assert(service.status().state==jp::UpdateState::Ready); // installer already launched
    raw->exitReady=false;
    service.cancel(); assert(service.status().state==jp::UpdateState::Cancelled);
    raw->value.state=jp::UpdateState::Disabled;
    service.check(true,100000); assert(raw->checks==2);
    raw->value={jp::UpdateState::Available,0,""};raw->value.version="Guipper-0.2.0-linux-x64.AppImage";
    service.skip();assert(service.skippedVersion=="stable:Guipper-0.2.0-linux-x64.AppImage");
    service.check(false,200000);
    raw->value.state=jp::UpdateState::Available;assert(service.status().state==jp::UpdateState::Cancelled);
    service.check(true,200001);
    raw->value.state=jp::UpdateState::Available;assert(service.status().state==jp::UpdateState::Available);
    std::cout << "update policy tests passed\n";
}
