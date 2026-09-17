#pragma once
#include <cmath>
#include <cstddef>

namespace jp_audio_internal {
struct AudioPlane { const float* data; std::size_t channels; };

// Supports both interleaved buffers and one buffer per channel; null data is silence.
inline void interleaveAudioPlanes(const AudioPlane* planes, std::size_t planeCount,
    std::size_t channels, std::size_t offset, std::size_t frames, float* output) {
    std::size_t firstChannel = 0;
    for (std::size_t b = 0; b < planeCount; ++b) {
        const auto& plane = planes[b];
        for (std::size_t f = 0; f < frames; ++f)
            for (std::size_t c = 0; c < plane.channels; ++c) {
                const float value = plane.data ? plane.data[(offset + f) * plane.channels + c] : 0;
                output[f * channels + firstChannel + c] = std::isfinite(value) ? value : 0;
            }
        firstChannel += plane.channels;
    }
}
}
