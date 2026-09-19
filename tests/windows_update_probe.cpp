// Uses the real WinSparkle DLL and the production backend; no OpenGL required.
// Compile with GUIPPER_UPDATE_TEST and an injected public-only update config.
#include "../src/JPutils/jp_update_service.h"
#include "../src/JPutils/jp_app_paths.h"
#include <windows.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int main(int argc,char**argv) {
    if(argc!=3)return 2;
    jp::AppPaths::current().cache=std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(jp::AppPaths::current().cache);
    auto backend=jp::platformUpdateBackend();
    backend->check("beta",false);
    bool downloading=false;int last=-1;
    const auto start=std::chrono::steady_clock::now();
    while(std::chrono::steady_clock::now()-start<std::chrono::seconds(60)) {
        MSG message;
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        const auto state=backend->status().state;
        if(int(state)!=last){last=int(state);std::cout<<"STATE "<<last<<std::endl;}
        if(state==jp::UpdateState::Available && !downloading) {
            if(std::string(argv[2])=="check")return 0;
            downloading=true;backend->download();
        }
        if(state==jp::UpdateState::Ready){std::cout<<"VERIFIED_READY"<<std::endl;return 0;}
        if(state==jp::UpdateState::Error){std::cout<<"REJECTED"<<std::endl;return 3;}
        if(state==jp::UpdateState::Disabled)return 4;
        if(std::string(argv[2])=="cancel" && std::chrono::steady_clock::now()-start>std::chrono::milliseconds(250)) {
            backend->cancel();return backend->status().state==jp::UpdateState::Cancelled?0:5;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    backend->cancel();return 6;
}
