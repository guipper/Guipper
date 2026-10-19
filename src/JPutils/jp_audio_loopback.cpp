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

#ifdef __linux__
#include <pulse/pulseaudio.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>

namespace jp_audio_internal {
namespace {
struct PulseConnection {
    pa_mainloop *loop=pa_mainloop_new();
    pa_context *context=pa_context_new(pa_mainloop_get_api(loop),"Guipper audio");
    ~PulseConnection(){pa_context_disconnect(context);pa_context_unref(context);pa_mainloop_free(loop);}
    void iterate(){
        pa_mainloop_iterate(loop,0,nullptr);
        if(!PA_CONTEXT_IS_GOOD(pa_context_get_state(context)))throw std::runtime_error("Audio server disconnected");
    }
    template<class Predicate> void wait(Predicate done) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!done()){
            iterate();
            if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Audio server did not respond. Try Refresh.");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    void connect(){
        if(pa_context_connect(context,nullptr,PA_CONTEXT_NOAUTOSPAWN,nullptr)<0)throw std::runtime_error("Cannot connect to PipeWire / PulseAudio");
        wait([&]{return pa_context_get_state(context)==PA_CONTEXT_READY;});
    }
    void operation(pa_operation *op){
        if(!op)throw std::runtime_error(pa_strerror(pa_context_errno(context)));
        try{wait([&]{return pa_operation_get_state(op)!=PA_OPERATION_RUNNING;});}
        catch(...){pa_operation_cancel(op);pa_operation_unref(op);throw;}
        pa_operation_unref(op);
    }
};
struct SinkInfo {std::string name,monitor;uint32_t index=PA_INVALID_INDEX;};
void sinkInfo(pa_context*,const pa_sink_info *info,int,void *data){
    if(!info)return;
    auto &sink=*static_cast<SinkInfo*>(data);sink.name=info->name;sink.monitor=info->monitor_source_name;sink.index=info->index;
}
struct Recording {
    LoopbackCapture::Sink sink;
    bool failed=false,removed=false,serverChanged=false;
    uint32_t index=PA_INVALID_INDEX;
};
void readAudio(pa_stream *stream,size_t,void *data){
    auto &record=*static_cast<Recording*>(data);
    const void *samples=nullptr;size_t bytes=0;
    if(pa_stream_peek(stream,&samples,&bytes)<0){record.failed=true;return;}
    if(bytes){
        if(samples)record.sink(static_cast<const float*>(samples),bytes/(sizeof(float)*2),2);
        else {
            std::array<float,1024> silence{};
            for(size_t frames=bytes/(sizeof(float)*2);frames;){auto n=std::min(frames,size_t(512));record.sink(silence.data(),n,2);frames-=n;}
        }
        if(pa_stream_drop(stream)<0)record.failed=true;
    }
}
}
bool LoopbackCapture::supported(){return true;}
std::vector<LoopbackDevice> LoopbackCapture::devices(){
    PulseConnection pulse;pulse.connect();std::vector<LoopbackDevice> result;
    pulse.operation(pa_context_get_sink_info_list(pulse.context,[](pa_context*,const pa_sink_info *info,int,void *data){
        if(info)static_cast<std::vector<LoopbackDevice>*>(data)->push_back({info->name,info->description?info->description:info->name});
    },&result));
    return result;
}
std::string LoopbackCapture::error() const {std::lock_guard<std::mutex> lock(mutex);return failure;}
bool LoopbackCapture::start(const std::string &id,Sink sink){
    stop();quit=false;ready=false;
    {std::lock_guard<std::mutex> lock(mutex);failure.clear();}
    worker=std::thread(&LoopbackCapture::capture,this,id,std::move(sink));
    while(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    return live;
}
void LoopbackCapture::stop(){quit=true;if(worker.joinable())worker.join();live=false;}
void LoopbackCapture::capture(std::string id,Sink sink){
    try{
        PulseConnection pulse;pulse.connect();
        const bool followDefault=id.empty();
        auto defaultSink=[&]{
            std::string name;
            pulse.operation(pa_context_get_server_info(pulse.context,[](pa_context*,const pa_server_info *info,void *data){
                if(info && info->default_sink_name)*static_cast<std::string*>(data)=info->default_sink_name;
            },&name));return name;
        };
        if(followDefault)id=defaultSink();
        SinkInfo output;
        pulse.operation(pa_context_get_sink_info_by_name(pulse.context,id.c_str(),sinkInfo,&output));
        if(output.monitor.empty())throw std::runtime_error("Selected output is unavailable. Reconnect it or choose another output.");
        pa_sample_spec spec{PA_SAMPLE_FLOAT32NE,48000,2};rate=spec.rate;
        auto stream=pa_stream_new(pulse.context,"System output capture",&spec,nullptr);
        if(!stream)throw std::runtime_error("Cannot create output capture");
        struct StreamScope {pa_stream *stream;~StreamScope(){pa_stream_set_read_callback(stream,nullptr,nullptr);pa_stream_disconnect(stream);pa_stream_unref(stream);}} cleanup{stream};
        Recording recording{std::move(sink)};recording.index=output.index;
        struct SubscriptionScope {pa_context *context;~SubscriptionScope(){pa_context_set_subscribe_callback(context,nullptr,nullptr);}} subscription{pulse.context};
        pa_stream_set_read_callback(stream,readAudio,&recording);
        pa_context_set_subscribe_callback(pulse.context,[](pa_context*,pa_subscription_event_type_t event,uint32_t index,void *data){
            auto &r=*static_cast<Recording*>(data);
            const auto facility=event&PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
            if(facility==PA_SUBSCRIPTION_EVENT_SERVER)r.serverChanged=true;
            if(facility==PA_SUBSCRIPTION_EVENT_SINK && (event&PA_SUBSCRIPTION_EVENT_TYPE_MASK)==PA_SUBSCRIPTION_EVENT_REMOVE && index==r.index)r.removed=true;
        },&recording);
        pulse.operation(pa_context_subscribe(pulse.context,pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SERVER|PA_SUBSCRIPTION_MASK_SINK),nullptr,nullptr));
        pa_buffer_attr attr{uint32_t(-1),uint32_t(-1),uint32_t(-1),uint32_t(-1),480*2*sizeof(float)};
        if(pa_stream_connect_record(stream,output.monitor.c_str(),&attr,PA_STREAM_ADJUST_LATENCY)<0)throw std::runtime_error(pa_strerror(pa_context_errno(pulse.context)));
        pulse.wait([&]{auto state=pa_stream_get_state(stream);if(!PA_STREAM_IS_GOOD(state))throw std::runtime_error("Output capture failed");return state==PA_STREAM_READY;});
        live=true;ready=true;
        while(!quit){
            pulse.iterate();
            if(recording.failed || recording.removed || !PA_STREAM_IS_GOOD(pa_stream_get_state(stream)))throw std::runtime_error("Output disconnected. Waiting to reconnect.");
            if(recording.serverChanged){recording.serverChanged=false;if(followDefault && defaultSink()!=id)throw std::runtime_error("Default output changed. Reconnecting.");}
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        pa_context_set_subscribe_callback(pulse.context,nullptr,nullptr);
    }catch(const std::exception &error){std::lock_guard<std::mutex> lock(mutex);failure=error.what();}
    live=false;ready=true;
}
}
#endif
