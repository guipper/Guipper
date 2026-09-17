#include "../jp_audio_loopback.h"
#ifdef __APPLE__
#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#include <AvailabilityMacros.h>
#include "../jp_audio_queue.h"
#include "../jp_audio_planes.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140200 && __has_include(<CoreAudio/AudioHardwareTapping.h>)
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#define GUIPPER_CORE_AUDIO_TAPS 1
#else
#define GUIPPER_CORE_AUDIO_TAPS 0
#endif

namespace jp_audio_internal {
namespace {
void check(OSStatus status, const char* operation) {
    if (status != noErr) throw std::runtime_error(std::string(operation) +
        " (Core Audio " + std::to_string(status) + ")");
}
AudioObjectPropertyAddress address(AudioObjectPropertySelector selector,
    AudioObjectPropertyScope scope = kAudioObjectPropertyScopeGlobal) {
    return {selector, scope, kAudioObjectPropertyElementMain};
}
template<class T> T property(AudioObjectID object, AudioObjectPropertySelector selector) {
    T result{}; UInt32 size = sizeof(result); auto a = address(selector);
    check(AudioObjectGetPropertyData(object, &a, 0, nullptr, &size, &result), "Read audio property");
    return result;
}
std::string stringProperty(AudioObjectID object, AudioObjectPropertySelector selector) {
    CFStringRef value = property<CFStringRef>(object, selector);
    if (!value) return {};
    std::vector<char> bytes(CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1);
    const bool valid = CFStringGetCString(value, bytes.data(), bytes.size(), kCFStringEncodingUTF8);
    CFRelease(value);
    return valid ? std::string(bytes.data()) : std::string();
}
std::vector<AudioObjectID> outputs() {
    auto a = address(kAudioHardwarePropertyDevices); UInt32 size = 0;
    check(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &size), "List devices");
    std::vector<AudioObjectID> all(size / sizeof(AudioObjectID));
    check(AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, all.data()), "Read devices");
    std::vector<AudioObjectID> result;
    for (auto device : all) {
        auto streams = address(kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput);
        UInt32 bytes = 0;
        if (AudioObjectGetPropertyDataSize(device, &streams, 0, nullptr, &bytes) == noErr && bytes &&
            property<UInt32>(device, kAudioDevicePropertyDeviceIsAlive)) result.push_back(device);
    }
    return result;
}

#if GUIPPER_CORE_AUDIO_TAPS
struct TapSession {
    AudioObjectID tap = kAudioObjectUnknown, aggregate = kAudioObjectUnknown;
    AudioDeviceIOProcID io = nullptr;
    UInt32 channels = 0;
    SpscAudioQueue<1024, 64> queue;
    std::atomic<bool> invalidBuffer{false};
    ~TapSession() {
        if (@available(macOS 14.2, *)) {
            if (io) {
                AudioDeviceStop(aggregate, io);
                AudioDeviceDestroyIOProcID(aggregate, io);
            }
            if (aggregate) AudioHardwareDestroyAggregateDevice(aggregate);
            if (tap) AudioHardwareDestroyProcessTap(tap);
        }
    }
    static OSStatus receive(AudioObjectID, const AudioTimeStamp*, const AudioBufferList* input,
        const AudioTimeStamp*, AudioBufferList*, const AudioTimeStamp*, void* user) {
        auto& self = *static_cast<TapSession*>(user);
        if (!input || !input->mNumberBuffers) return noErr;
        if (input->mNumberBuffers > 32) { self.invalidBuffer = true; return noErr; }
        std::array<AudioPlane, 32> planes{};
        UInt32 channels = 0;
        size_t frames = SIZE_MAX;
        for (UInt32 b = 0; b < input->mNumberBuffers; ++b) {
            const auto& buffer = input->mBuffers[b];
            planes[b] = {static_cast<const float*>(buffer.mData), buffer.mNumberChannels};
            if (!buffer.mNumberChannels) continue;
            channels += buffer.mNumberChannels;
            frames = std::min(frames, size_t(buffer.mDataByteSize / (sizeof(float) * buffer.mNumberChannels)));
        }
        if (channels != self.channels || frames == SIZE_MAX) { self.invalidBuffer = true; return noErr; }
        std::array<float, 1024> samples{};
        for (size_t offset = 0; offset < frames;) {
            const size_t count = std::min(frames - offset, samples.size() / channels);
            interleaveAudioPlanes(planes.data(), input->mNumberBuffers, channels, offset, count, samples.data());
            self.queue.push(samples.data(), count * channels);
            offset += count;
        }
        return noErr;
    }
};
#endif
}

bool LoopbackCapture::supported() {
#if GUIPPER_CORE_AUDIO_TAPS
    if (@available(macOS 14.2, *)) return true;
#endif
    return false;
}
std::vector<LoopbackDevice> LoopbackCapture::devices() {
    std::vector<LoopbackDevice> result;
    if (!supported()) return result;
    for (auto device : outputs()) result.push_back({stringProperty(device, kAudioDevicePropertyDeviceUID),
        stringProperty(device, kAudioObjectPropertyName)});
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
    @autoreleasepool {
        try {
            if (!supported()) throw std::runtime_error("Output capture requires macOS 14.2+ and a build with the macOS 14.2+ SDK");
#if GUIPPER_CORE_AUDIO_TAPS
            if (@available(macOS 14.2, *)) {
                AudioObjectID device = kAudioObjectUnknown;
                if (id.empty()) device = property<AudioObjectID>(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultOutputDevice);
                else for (auto candidate : outputs())
                    if (stringProperty(candidate, kAudioDevicePropertyDeviceUID) == id) { device = candidate; break; }
                if (!device) throw std::runtime_error("Selected output is unavailable");
                const auto uid = stringProperty(device, kAudioDevicePropertyDeviceUID);
                auto session = std::make_unique<TapSession>();
                CATapDescription* description = [[CATapDescription alloc] initExcludingProcesses:@[]
                    andDeviceUID:[NSString stringWithUTF8String:uid.c_str()] withStream:0];
                description.name = @"Guipper audio analyser";
                [description setPrivate:YES];
                description.muteBehavior = CATapUnmuted;
                const OSStatus tapStatus = AudioHardwareCreateProcessTap(description, &session->tap);
#if !__has_feature(objc_arc)
                [description release];
#endif
                check(tapStatus, "Create output tap; allow Guipper system audio recording in Privacy & Security");
                const auto format = property<AudioStreamBasicDescription>(session->tap, kAudioTapPropertyFormat);
                if (format.mFormatID != kAudioFormatLinearPCM || !(format.mFormatFlags & kAudioFormatFlagIsFloat) ||
                    (format.mFormatFlags & kAudioFormatFlagIsBigEndian) || format.mBitsPerChannel != 32 ||
                    format.mChannelsPerFrame == 0 || format.mChannelsPerFrame > 32 || format.mSampleRate <= 0)
                    throw std::runtime_error("Unsupported output tap format");
                session->channels = format.mChannelsPerFrame; rate = unsigned(format.mSampleRate);
                const auto tapUID = stringProperty(session->tap, kAudioTapPropertyUID);
                NSDictionary* settings = @{
                    @kAudioAggregateDeviceNameKey: @"Guipper private capture",
                    @kAudioAggregateDeviceUIDKey: [[NSUUID UUID] UUIDString],
                    @kAudioAggregateDeviceIsPrivateKey: @YES,
                    @kAudioAggregateDeviceTapAutoStartKey: @YES,
                    @kAudioAggregateDeviceTapListKey: @[@{
                        @kAudioSubTapUIDKey: [NSString stringWithUTF8String:tapUID.c_str()],
                        @kAudioSubTapDriftCompensationKey: @YES}]
                };
                check(AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)settings, &session->aggregate), "Create capture device");
                check(AudioDeviceCreateIOProcID(session->aggregate, TapSession::receive, session.get(), &session->io), "Create capture callback");
                check(AudioDeviceStart(session->aggregate, session->io), "Start output capture; allow Guipper system audio recording in Privacy & Security");
                live = true; ready = true;
                auto last = std::chrono::steady_clock::now(), nextCheck = last;
                SpscAudioQueue<1024, 64>::Block block;
                std::array<float, 1024> silence{};
                while (!quit.load()) {
                    // Only this worker calls the sink, including synthetic silence.
                    while (session->queue.pop(block)) {
                        sink(block.samples.data(), block.count / session->channels, session->channels);
                        last = std::chrono::steady_clock::now();
                    }
                    auto now = std::chrono::steady_clock::now();
                    if (session->invalidBuffer.load()) throw std::runtime_error("Output channel layout changed");
                    if (std::chrono::duration<double>(now - last).count() >= 0.02) {
                        size_t remaining = size_t(std::min(0.1, std::chrono::duration<double>(now - last).count()) * rate);
                        while (remaining) {
                            const auto frames = std::min(remaining, silence.size() / session->channels);
                            sink(silence.data(), frames, session->channels); remaining -= frames;
                        }
                        last = now;
                    }
                    if (now >= nextCheck) {
                        if (!property<UInt32>(device, kAudioDevicePropertyDeviceIsAlive)) throw std::runtime_error("Output disconnected");
                        if (id.empty() && property<AudioObjectID>(kAudioObjectSystemObject, kAudioHardwarePropertyDefaultOutputDevice) != device)
                            throw std::runtime_error("Default output changed");
                        const auto current = property<AudioStreamBasicDescription>(session->tap, kAudioTapPropertyFormat);
                        if (current.mSampleRate != format.mSampleRate || current.mChannelsPerFrame != format.mChannelsPerFrame ||
                            current.mFormatFlags != format.mFormatFlags || current.mBitsPerChannel != format.mBitsPerChannel)
                            throw std::runtime_error("Output format changed");
                        nextCheck = now + std::chrono::milliseconds(500);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
#endif
        } catch (const std::exception& ex) {
            std::lock_guard<std::mutex> lock(mutex); failure = ex.what();
        }
    }
    live = false; ready = true;
}
}
#endif
