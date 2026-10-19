#include "../src/JPutils/jp_screen_capture.h"
#include <cassert>
#include "../src/JPutils/jp_capture_resize.h"
#include <iostream>
using namespace jp_screen;
struct Fake final : Backend {
    int &stops;
    explicit Fake(int &stops):stops(stops){}
    void stop() override { ++stops; }
};
Frame frame(int w,int h,unsigned char color){return {w,h,0,std::vector<unsigned char>(size_t(w)*h*4,color)};}
int main(){
    ResizeRecovery recovery;
    recovery.size(800,600,0);recovery.frame(0);
    assert(!recovery.reconnect(10000)); // Static windows are not stalled streams.
    recovery.size(1920,1080,10000);recovery.frame(10010);
    assert(!recovery.reconnect(11000));assert(recovery.reconnect(12010));
    assert(!recovery.reconnect(15000)); // One retry, no reconnect loop.
    recovery.size(800,600,16000);recovery.frame(18001);
    assert(!recovery.reconnect(25000)); // Healthy resize disarms recovery.
    recovery.size(1920,1080,26000);assert(recovery.reconnect(28000));
    int stops=0;
    std::vector<std::shared_ptr<Mailbox>> sessions;
    Capture capture([&](const std::string &){auto backend=std::make_unique<Fake>(stops);sessions.push_back(backend->mailbox);return backend;});
    assert(capture.update().state==State::Idle);
    capture.select();assert(sessions.size()==1);assert(capture.update().state==State::Selecting);
    capture.select();assert(sessions.size()==1);
    sessions[0]->state(State::Cancelled);assert(capture.update().state==State::Idle);
    capture.select();sessions[1]->publish(frame(2,3,40));auto first=capture.update();
    assert(first.state==State::Streaming && first.frame->width==2 && first.frame->height==3);
    sessions[1]->publish(frame(4,2,90));auto resized=capture.update();assert(resized.frame->width==4 && resized.frame->serial==2);
    capture.select();assert(capture.update().frame==resized.frame);
    sessions[2]->state(State::Cancelled);assert(capture.update().frame==resized.frame);
    capture.select();sessions[3]->publish(frame(3,4,100));assert(capture.update().frame->height==4);
    // Replaced and deleted sessions cannot deliver late responses or frames.
    sessions[1]->publish(frame(9,9,200));assert(!sessions[1]->read().frame);
    capture.select();capture.stop();sessions[4]->publish(frame(5,5,255));sessions[4]->state(State::Streaming);
    assert(!capture.update().frame && capture.update().state==State::Stopped);
    capture.select();sessions[5]->state(State::Error,"revoked");assert(capture.update().state==State::Error);
    capture.select();sessions[6]->state(State::Cancelled);assert(capture.update().state==State::Error);
    capture.select();sessions[7]->publish(frame(2,2,255));capture.update();
    sessions[7]->state(State::Error,"monitor removed");assert(!capture.update().frame);
    Mailbox mailbox;mailbox.publish(frame(2,2,12));auto serial=mailbox.read().frame->serial;
    Frame invalid=frame(3,3,10);invalid.rgba.pop_back();mailbox.publish(invalid);assert(mailbox.read().frame->serial==serial);
    for(int i=0;i<100;++i) { mailbox.publish(frame(2,2,i)); }
    assert(mailbox.read().frame->serial==101);
    assert(stops==8);
    // Cancel an unresponsive picker and retry without stopping an existing feed.
    capture.select();auto abandoned=sessions.back();capture.cancelSelection();
    abandoned->publish(frame(2,2,255));assert(!abandoned->read().frame);
    assert(capture.snapshot().state==State::Error);
    capture.select();sessions.back()->publish(frame(2,2,80));auto live=capture.update().frame;
    capture.select();capture.cancelSelection();assert(capture.snapshot().frame==live);
    capture.select();sessions.back()->state(State::Cancelled);assert(capture.update().frame==live);
    capture.stop();
    std::cout<<"screen_capture_tests: PASS\n";
}
