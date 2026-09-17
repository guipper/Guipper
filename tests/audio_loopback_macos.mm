#include "../src/JPutils/jp_audio_loopback.h"
#include "../src/JPutils/jp_audio_device_id.h"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    using namespace jp_audio_internal;
    if (loopbackId("") != "coreaudio-loopback:default") return 1;
    std::cout << "Core Audio taps supported: " << LoopbackCapture::supported() << '\n';
    if (!LoopbackCapture::supported()) return 1;
    if (argc < 2 || std::string(argv[1]) != "--capture") return 0;
    try {
        if (!LoopbackCapture::supported()) return 1;
        for (const auto& device : LoopbackCapture::devices()) std::cout << device.name << " [" << device.id << "]\n";
        LoopbackCapture capture;
        std::atomic<size_t> frames{0};
        std::atomic<float> peak{0};
        const std::string id = argc > 2 ? argv[2] : "";
        if (!capture.start(id, [&](const float* data, size_t count, size_t channels) {
            frames += count;
            float maximum = peak;
            for (size_t i = 0; i < count * channels; ++i) maximum = std::max(maximum, std::abs(data[i]));
            peak = maximum;
        })) { std::cerr << capture.error() << '\n'; return 1; }
        std::cout << "Play audio on the selected output during this ten-second capture.\n";
        std::this_thread::sleep_for(std::chrono::seconds(10));
        const bool running = capture.running();
        capture.stop();
        std::cout << "Frames: " << frames << " peak: " << peak << '\n';
        if (!running || !frames || peak < 0.0001f) {
            std::cerr << "No captured signal, permission denied, or device changed: " << capture.error() << '\n';
            return 1;
        }
        if (capture.start("nonexistent-output-uid", [](const float*, size_t, size_t) {})) return 1;
        capture.stop();
        std::cout << "Core Audio loopback tests passed\n";
    } catch (const std::exception& ex) { std::cerr << ex.what() << '\n'; return 1; }
}
