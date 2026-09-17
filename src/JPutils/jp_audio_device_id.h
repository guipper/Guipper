#pragma once
#include <string>
namespace jp_audio_internal {
inline bool isLoopbackId(const std::string& id) { return id.compare(0, 16, "wasapi-loopback:") == 0; }
inline std::string loopbackId(const std::string& endpoint) { return "wasapi-loopback:" + (endpoint.empty() ? "default" : endpoint); }
inline std::string loopbackEndpoint(const std::string& id) {
    const auto endpoint = id.substr(16);
    return endpoint == "default" ? "" : endpoint;
}
}
