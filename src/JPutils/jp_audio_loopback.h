#pragma once
#if defined(_WIN32) || defined(__APPLE__)
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <vector>

namespace jp_audio_internal {
struct LoopbackDevice { std::string id, name; };
class LoopbackCapture {
public:
    using Sink = std::function<void(const float*, size_t, size_t)>;
    ~LoopbackCapture() { stop(); }
    static std::vector<LoopbackDevice> devices();
    static bool supported();
    bool start(const std::string& id, Sink sink);
    void stop();
    bool running() const { return live.load(); }
    unsigned sampleRate() const { return rate; }
    std::string error() const;
private:
    void capture(std::string id, Sink sink);
    std::thread worker;
    std::atomic<bool> quit{false}, live{false}, ready{false};
    unsigned rate = 48000;
    std::string failure;
    mutable std::mutex mutex;
};
}
#endif
