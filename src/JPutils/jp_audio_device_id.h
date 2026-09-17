#pragma once
#include <string>
namespace jp_audio_internal {
inline bool isLoopbackId(const std::string& id) {
    return id.compare(0, 16, "wasapi-loopback:") == 0 || id.compare(0, 19, "coreaudio-loopback:") == 0;
}
inline std::string loopbackId(const std::string& endpoint) {
#ifdef __APPLE__
    const std::string prefix = "coreaudio-loopback:";
#else
    const std::string prefix = "wasapi-loopback:";
#endif
    return prefix + (endpoint.empty() ? "default" : endpoint);
}
inline std::string loopbackEndpoint(const std::string& id) {
    const auto endpoint = id.substr(id.find(':') + 1);
    return endpoint == "default" ? "" : endpoint;
}
}
