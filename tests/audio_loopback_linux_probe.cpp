#include "../src/JPutils/jp_audio_loopback.h"
#include <atomic>
#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
int main(int argc,char **argv){
 using namespace jp_audio_internal;
 for(const auto &d:LoopbackCapture::devices())std::cout<<d.id<<" | "<<d.name<<std::endl;
 if(argc<2)return 0;
 LoopbackCapture capture;std::atomic<unsigned long> count{0};std::atomic<float> peak{0};
 if(!capture.start(argv[1],[&](const float *data,size_t frames,size_t channels){count+=frames;for(size_t i=0;i<frames*channels;++i)if(std::abs(data[i])>peak)peak=std::abs(data[i]);})){std::cerr<<capture.error();return 1;}
 for(int i=0;i<100 && capture.running();++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));
 const bool disconnected=!capture.running() && !capture.error().empty();
 capture.stop();std::cout<<"frames="<<count<<" peak="<<peak<<std::endl;
 if(argc>2)return disconnected?0:3;
 return count>0 && peak>0.01f?0:2;
}
