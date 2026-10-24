// Hardware smoke test for the same SDK capture used by Guipper.
// Build with MSVC /EHsc /std:c++17 and /I "%KINECTSDK20_DIR%inc".
#include "../src/JPutils/jp_kinect_sdk.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <algorithm>

int main(int argc, char **) {
    jp_kinect_sdk::Capture capture;
    if (!capture.open()) { std::cerr << capture.status << '\n'; return 1; }
    auto start = std::chrono::steady_clock::now();
    int frames = 0, colors = 0, gaps = 0;
    auto lastFrame = start;
    bool stalled = false;
    size_t validDepth = 0, validIR = 0, aligned = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(20)) {
        if (!capture.read()) {
            if (frames > 0 && !stalled && std::chrono::steady_clock::now() - lastFrame > std::chrono::seconds(1)) {
                ++gaps; stalled = true; std::cout << "gap: " << capture.status << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue;
        }
        lastFrame = std::chrono::steady_clock::now(); stalled = false;
        ++frames;
        validDepth = std::count_if(capture.depth.begin(), capture.depth.end(), [](UINT16 z) { return z >= 500 && z <= 4500; });
        validIR = std::count_if(capture.infrared.begin(), capture.infrared.end(), [](UINT16 z) { return z > 0; });
        if (capture.newColor) {
            ++colors;
            if (!aligned) {
                auto depth = capture.colorDepth();
                aligned = std::count_if(depth.begin(), depth.end(), [](float z) { return z > 0; });
            }
        }
        if (argc == 1 && frames >= 60 && colors > 0 && validDepth && validIR && aligned && capture.intrinsics.FocalLengthX > 0) break;
    }
    std::cout << capture.status << " depth_frames=" << frames << " color_frames=" << colors
        << " depth_pixels=" << validDepth << " ir_pixels=" << validIR << " aligned_pixels=" << aligned
        << " fx=" << capture.intrinsics.FocalLengthX << " fy=" << capture.intrinsics.FocalLengthY << " gaps=" << gaps << '\n';
    return frames >= 60 && colors && validDepth && validIR && aligned && capture.intrinsics.FocalLengthX > 0 && (argc == 1 || gaps == 0) ? 0 : 2;
}
