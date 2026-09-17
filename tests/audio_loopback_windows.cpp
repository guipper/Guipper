#include "../src/JPutils/jp_audio_loopback.h"
#include <Windows.h>
#include <mmsystem.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>
#pragma comment(lib, "winmm.lib")

int main() {
    using namespace jp_audio_internal;
    try {
        const auto devices = LoopbackCapture::devices();
        for (const auto& device : devices) std::cout << device.name << "\n";
        LoopbackCapture capture;
        std::atomic<unsigned long long> frames{0};
        std::atomic<float> peak{0};
        auto sink = [&](const float* data, size_t count, size_t channels) {
            frames += count;
            float p = peak.load();
            for (size_t i = 0; i < count * channels; ++i) p = (std::max)(p, std::abs(data[i]));
            peak = p;
        };
        if (!capture.start("", sink)) { std::cerr << capture.error(); return 1; }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (frames == 0) { std::cerr << "No idle frames"; return 1; }
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = 2;
        format.nSamplesPerSec = 48000; format.wBitsPerSample = 16;
        format.nBlockAlign = 4; format.nAvgBytesPerSec = 192000;
        HWAVEOUT output = nullptr;
        if (waveOutOpen(&output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return 1;
        std::vector<short> tone(48000 * 2);
        for (size_t i = 0; i < 48000; ++i) tone[i * 2] = tone[i * 2 + 1] = short(3000 * std::sin(i * 440.0 * 6.28318530718 / 48000));
        WAVEHDR header{}; header.lpData = reinterpret_cast<char*>(tone.data()); header.dwBufferLength = DWORD(tone.size() * sizeof(short));
        waveOutPrepareHeader(output, &header, sizeof(header));
        waveOutWrite(output, &header, sizeof(header));
        std::this_thread::sleep_for(std::chrono::milliseconds(1400));
        waveOutReset(output); waveOutUnprepareHeader(output, &header, sizeof(header)); waveOutClose(output);
        capture.stop();
        std::cout << "Captured frames=" << frames << " peak=" << peak << "\n";
        if (peak < 0.001f) return 1;
        if (!devices.empty()) {
            if (!capture.start(devices.front().id, sink)) { std::cerr << capture.error(); return 1; }
            capture.stop();
        }
        if (capture.start("nonexistent-endpoint", sink)) return 1;
        capture.stop();
        std::cout << "loopback tests passed\n";
    } catch (const std::exception& ex) { std::cerr << ex.what(); return 1; }
}
