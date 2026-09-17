#include "jp_audio_loopback.h"
#ifdef _WIN32
#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <memory>
#include "jp_audio_pcm.h"
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

namespace jp_audio_internal {
bool LoopbackCapture::supported() { return true; }
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr) {
    if (FAILED(hr)) throw std::runtime_error("WASAPI error " + std::to_string(static_cast<unsigned long>(hr)));
}
struct ComScope {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComScope() { if (result != RPC_E_CHANGED_MODE) check(result); }
    ~ComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
};
std::string utf8(const wchar_t* s) {
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, &out[0], n, nullptr, nullptr);
    out.pop_back(); return out;
}
std::wstring wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
    out.pop_back(); return out;
}
std::string endpointId(IMMDevice* device) {
    LPWSTR raw = nullptr; check(device->GetId(&raw));
    auto id = utf8(raw); CoTaskMemFree(raw); return id;
}
ComPtr<IMMDeviceEnumerator> enumerator() {
    ComPtr<IMMDeviceEnumerator> result;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&result)));
    return result;
}
}
std::vector<LoopbackDevice> LoopbackCapture::devices() {
    ComScope com;
    auto e = enumerator();
    ComPtr<IMMDeviceCollection> list;
    check(e->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &list));
    UINT count = 0; check(list->GetCount(&count));
    std::vector<LoopbackDevice> result;
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device; check(list->Item(i, &device));
        ComPtr<IPropertyStore> props; check(device->OpenPropertyStore(STGM_READ, &props));
        PROPVARIANT name; PropVariantInit(&name);
        const HRESULT hr = props->GetValue(PKEY_Device_FriendlyName, &name);
        const auto id = endpointId(device.Get());
        result.push_back({id, SUCCEEDED(hr) && name.vt == VT_LPWSTR ? utf8(name.pwszVal) : id});
        PropVariantClear(&name);
    }
    return result;
}
std::string LoopbackCapture::error() const { std::lock_guard<std::mutex> lock(mutex); return failure; }
bool LoopbackCapture::start(const std::string& id, Sink sink) {
    stop(); quit = false; ready = false;
    { std::lock_guard<std::mutex> lock(mutex); failure.clear(); }
    worker = std::thread(&LoopbackCapture::capture, this, id, std::move(sink));
    while (!ready.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return live.load();
}
void LoopbackCapture::stop() {
    quit = true;
    if (worker.joinable()) worker.join();
    live = false;
}
void LoopbackCapture::capture(std::string id, Sink sink) {
    try {
        ComScope com;
        auto e = enumerator();
        ComPtr<IMMDevice> device;
        if (id.empty()) check(e->GetDefaultAudioEndpoint(eRender, eMultimedia, &device));
        else check(e->GetDevice(wide(id).c_str(), &device));
        const auto openedId = endpointId(device.Get());
        ComPtr<IAudioClient> client;
        check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client.GetAddressOf())));
        WAVEFORMATEX* raw = nullptr; check(client->GetMixFormat(&raw));
        std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> format(raw, CoTaskMemFree);
        bool floating = raw->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
        bool pcm = raw->wFormatTag == WAVE_FORMAT_PCM;
        if (raw->wFormatTag == WAVE_FORMAT_EXTENSIBLE && raw->cbSize >= 22) {
            auto ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(raw);
            floating = ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            pcm = ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
        }
        if ((!floating && !pcm) || !pcmFormatSupported(raw->wBitsPerSample, floating) ||
            raw->nChannels == 0 || raw->nSamplesPerSec == 0 ||
            raw->nBlockAlign != raw->nChannels * (raw->wBitsPerSample / 8))
            throw std::runtime_error("Unsupported loopback format");
        rate = raw->nSamplesPerSec;
        check(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 1000000, 0, raw, nullptr));
        ComPtr<IAudioCaptureClient> capture;
        check(client->GetService(IID_PPV_ARGS(&capture)));
        UINT capacity = 0; check(client->GetBufferSize(&capacity));
        std::vector<float> samples(static_cast<size_t>(capacity) * raw->nChannels);
        check(client->Start()); live = true; ready = true;
        auto last = std::chrono::steady_clock::now();
        auto nextCheck = last;
        while (!quit.load()) {
            UINT packet = 0; check(capture->GetNextPacketSize(&packet));
            while (packet && !quit.load()) {
                BYTE* data = nullptr; UINT frames = 0; DWORD flags = 0;
                check(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr));
                if (frames > capacity) { capture->ReleaseBuffer(frames); throw std::runtime_error("Loopback buffer overflow"); }
                const size_t count = static_cast<size_t>(frames) * raw->nChannels;
                for (size_t i = 0; i < count; ++i)
                    samples[i] = flags & AUDCLNT_BUFFERFLAGS_SILENT ? 0.0f : decodePcm(data + i * (raw->wBitsPerSample / 8), raw->wBitsPerSample, floating);
                check(capture->ReleaseBuffer(frames));
                sink(samples.data(), frames, raw->nChannels);
                last = std::chrono::steady_clock::now();
                check(capture->GetNextPacketSize(&packet));
            }
            const auto now = std::chrono::steady_clock::now();
            // Idle render endpoints may deliver no packets. Advance the DSP with silence.
            const auto elapsed = std::chrono::duration<double>(now - last).count();
            if (elapsed >= 0.02) {
                const size_t frames = (std::min)(static_cast<size_t>(capacity), static_cast<size_t>(elapsed * rate));
                std::fill(samples.begin(), samples.end(), 0.0f);
                sink(samples.data(), frames, raw->nChannels); last = now;
            }
            if (now >= nextCheck) {
                DWORD state = 0; check(device->GetState(&state));
                if (!(state & DEVICE_STATE_ACTIVE)) throw std::runtime_error("Output disconnected");
                if (id.empty()) {
                    ComPtr<IMMDevice> current; check(e->GetDefaultAudioEndpoint(eRender, eMultimedia, &current));
                    if (endpointId(current.Get()) != openedId) throw std::runtime_error("Default output changed");
                }
                nextCheck = now + std::chrono::milliseconds(500);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        client->Stop();
    } catch (const std::exception& ex) {
        std::lock_guard<std::mutex> lock(mutex); failure = ex.what();
    }
    live = false; ready = true;
}
}
#endif
