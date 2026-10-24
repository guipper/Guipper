#pragma once

// Windows backend: keep the Microsoft driver used by Kinect SDK applications.
#include <Kinect.h>
#include <wrl/client.h>
#include <vector>
#include <cmath>

namespace jp_kinect_sdk {
using Microsoft::WRL::ComPtr;

class Capture {
    HMODULE runtime = nullptr;
    ComPtr<IKinectSensor> sensor;
    ComPtr<IMultiSourceFrameReader> reader;
    ComPtr<IColorFrameReader> colorReader;
    ComPtr<ICoordinateMapper> mapper;
public:
    const char *status = "CONNECTING";
    CameraIntrinsics intrinsics{};
    std::vector<UINT16> depth, infrared;
    std::vector<unsigned char> color;
    INT64 depthTime = 0, colorTime = 0;
    bool newColor = false;

    Capture() = default;
    Capture(const Capture &) = delete;
    Capture &operator=(const Capture &) = delete;
    ~Capture() {
        colorReader.Reset(); reader.Reset(); mapper.Reset();
        if (sensor) sensor->Close();
        sensor.Reset();
        if (runtime) FreeLibrary(runtime);
    }

    bool open() {
        runtime = LoadLibraryExW(L"Kinect20.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!runtime) { status = "INSTALL KINECT V2 RUNTIME"; return false; }
        using GetSensor = HRESULT (WINAPI *)(IKinectSensor **);
        auto getSensor = reinterpret_cast<GetSensor>(GetProcAddress(runtime, "GetDefaultKinectSensor"));
        if (!getSensor || FAILED(getSensor(sensor.GetAddressOf())) || !sensor) {
            status = "KINECT NOT FOUND"; return false;
        }
        if (FAILED(sensor->Open())) { status = "KINECT OPEN FAILED"; return false; }
        ComPtr<IColorFrameSource> colorSource;
        if (FAILED(sensor->get_CoordinateMapper(mapper.GetAddressOf())) ||
            FAILED(sensor->OpenMultiSourceFrameReader(FrameSourceTypes_Depth | FrameSourceTypes_Infrared, reader.GetAddressOf())) ||
            FAILED(sensor->get_ColorFrameSource(colorSource.GetAddressOf())) ||
            FAILED(colorSource->OpenReader(colorReader.GetAddressOf()))) {
            status = "KINECT STREAM START FAILED"; return false;
        }
        depth.resize(512 * 424); infrared.resize(512 * 424);
        color.resize(1920 * 1080 * 4);
        return true;
    }

    bool read() {
        newColor = false;
        ComPtr<IMultiSourceFrame> frame;
        if (FAILED(reader->AcquireLatestFrame(frame.GetAddressOf()))) {
            BOOLEAN available = FALSE;
            sensor->get_IsAvailable(&available);
            status = available ? "WAITING FOR KINECT FRAMES" : "KINECT NOT AVAILABLE / CHECK USB AND POWER";
            return false;
        }
        ComPtr<IDepthFrameReference> depthRef;
        ComPtr<IInfraredFrameReference> irRef;
        ComPtr<IDepthFrame> depthFrame;
        ComPtr<IInfraredFrame> irFrame;
        if (FAILED(frame->get_DepthFrameReference(depthRef.GetAddressOf())) ||
            FAILED(frame->get_InfraredFrameReference(irRef.GetAddressOf())) ||
            FAILED(depthRef->AcquireFrame(depthFrame.GetAddressOf())) ||
            FAILED(irRef->AcquireFrame(irFrame.GetAddressOf())) ||
            FAILED(depthFrame->CopyFrameDataToArray(UINT(depth.size()), depth.data())) ||
            FAILED(irFrame->CopyFrameDataToArray(UINT(infrared.size()), infrared.data()))) return false;
        depthFrame->get_RelativeTime(&depthTime);
        mapper->GetDepthCameraIntrinsics(&intrinsics);
        ComPtr<IColorFrame> rgbFrame;
        if (SUCCEEDED(colorReader->AcquireLatestFrame(rgbFrame.GetAddressOf())) &&
            SUCCEEDED(rgbFrame->CopyConvertedFrameDataToArray(UINT(color.size()), color.data(), ColorImageFormat_Rgba))) {
            rgbFrame->get_RelativeTime(&colorTime);
            newColor = true;
        }
        status = "STREAMING";
        return true;
    }

    std::vector<float> colorDepth() {
        // SDK timestamps are in 100 ns units. Never attach stale depth to RGB.
        if (!newColor || std::abs(colorTime - depthTime) > 500000) return {};
        std::vector<DepthSpacePoint> points(1920 * 1080);
        if (FAILED(mapper->MapColorFrameToDepthSpace(UINT(depth.size()), depth.data(), UINT(points.size()), points.data()))) return {};
        std::vector<float> result(points.size(), 0.0f);
        for (size_t i = 0; i < points.size(); ++i) {
            const auto &p = points[i];
            if (!std::isfinite(p.X) || !std::isfinite(p.Y) || p.X < 0 || p.Y < 0 || p.X > 511 || p.Y > 423) continue;
            result[i] = float(depth[size_t(std::lround(p.Y)) * 512 + size_t(std::lround(p.X))]);
        }
        return result;
    }
};
}
