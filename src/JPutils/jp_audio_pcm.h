#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
namespace jp_audio_internal {
inline bool pcmFormatSupported(unsigned bits, bool floating) {
    return floating ? bits == 32 || bits == 64 : bits == 8 || bits == 16 || bits == 24 || bits == 32;
}
inline float decodePcm(const unsigned char* p, unsigned bits, bool floating) {
    if (floating) {
        double value;
        if (bits == 32) { float f; std::memcpy(&f, p, 4); value = f; }
        else std::memcpy(&value, p, 8);
        return std::isfinite(value) ? static_cast<float>(value) : 0.0f;
    }
    if (bits == 8) return (int(*p) - 128) / 128.0f;
    uint32_t value = 0;
    for (unsigned i = 0; i < bits / 8; ++i) value |= uint32_t(p[i]) << (i * 8);
    const int64_t signedValue = value & (uint32_t(1) << (bits - 1)) ? int64_t(value) - (int64_t(1) << bits) : value;
    return static_cast<float>(signedValue / double(int64_t(1) << (bits - 1)));
}
}
