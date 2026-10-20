#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace jp_youtube {
// Only single-video YouTube URLs are accepted. Never pass arbitrary user input
// to the downloader or use a URL as a filename.
std::string videoId(const std::string &url);
std::string canonicalUrl(const std::string &url);
enum class State { Empty, Preparing, Ready, Cancelled, Error };
struct Snapshot {
    State state = State::Empty;
    std::string message;
    std::filesystem::path file;
    std::string streamUrl = {};
    bool hasAudio = true;
};
using Runner = std::function<int(const std::vector<std::string> &,
    const std::filesystem::path &, const std::atomic<bool> &)>;
// No shell. Owns/reaps the complete process group (including ffmpeg).
int runProcess(const std::vector<std::string> &args,
    const std::filesystem::path &log, const std::atomic<bool> &cancel);
class Download {
public:
    explicit Download(std::filesystem::path cache, Runner runner = runProcess);
    ~Download();
    void start(const std::string &url, bool force = false);
    // Resolve a playable stream; never download a complete video. A tiny HLS
    // master playlist may be retained until cancel() to join audio and video.
    void resolve(const std::string &url);
    void cancel();
    Snapshot snapshot() const;
    std::filesystem::path cached(const std::string &url) const;
private:
    std::filesystem::path root, retainedDirectory;
    void startWork(const std::string &url, bool force, bool streaming);
    Runner runner;
    std::atomic<bool> cancelled{false};
    std::thread worker;
    mutable std::mutex mutex;
    Snapshot result;
    void publish(Snapshot value);
};
}
