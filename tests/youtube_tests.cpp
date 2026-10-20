#include "../src/JPutils/jp_youtube.h"
#include <cassert>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace jp_youtube;
void wait(Download &job) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (job.snapshot().state == State::Preparing && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(job.snapshot().state != State::Preparing);
}
int main() {
    const std::string id = "BaW_jenozKc", url = "https://www.youtube.com/watch?v=" + id;
    for (const auto &link : {url, " https://youtu.be/" + id + "?si=abc \n",
        "https://m.youtube.com/shorts/" + id, "https://www.youtube.com/watch?list=ignored&v=" + id + "&t=12",
        "youtube.com/embed/" + id, "https://music.youtube.com/watch?v=" + id}) assert(videoId(link) == id);
    for (const auto *bad : {"", "https://youtube.com.evil/watch?v=BaW_jenozKc", "https://youtube.com@evil/watch?v=BaW_jenozKc",
        "https://youtube.com/playlist?list=foo", "--exec=touch", "https://youtu.be/../../file", "file:///tmp/video",
        "https://youtu.be/BaW_jenozKc/extra", "https://youtube.com/watch?v=BaW_jenozKc&v=BaW_jenozKc",
        "https://youtube.com/watch?v=$(command)", "https://youtu.be/BaW_jenozKc\n--exec=bad"}) assert(videoId(bad).empty());
    assert(canonicalUrl("https://youtu.be/" + id + "?si=abc") == url);
    const auto root = std::filesystem::temp_directory_path() / ("guipper-youtube-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int calls = 0;
    Runner success = [&](const std::vector<std::string> &args, const auto &log, const auto &) {
        ++calls; assert(args.back() == url); assert(args[args.size() - 2] == "--");
        assert(std::find(args.begin(), args.end(), "--ignore-config") != args.end());
        assert(std::find(args.begin(), args.end(), "--no-playlist") != args.end());
        std::ofstream(log.parent_path() / "video.mp4") << "fake media"; return 0;
    };
    {
        Download job(root, success);
        job.start("invalid"); assert(job.snapshot().state == State::Error && calls == 0);
        job.start(url); wait(job); assert(job.snapshot().state == State::Ready && calls == 1);
        assert(!job.cached(url).empty()); job.start(url); assert(calls == 1);
        job.start(url, true); wait(job); assert(calls == 2);
    }
    {
        Download job(root, [](const auto &, const auto &log, const auto &) {
            std::ofstream(log) << "ERROR: Video unavailable\n"; return 1;
        });
        job.start(url, true); wait(job); assert(job.snapshot().state == State::Error);
        assert(job.snapshot().message == "ERROR: Video unavailable");
        // A failed refresh never destroys a previously valid cached video.
        assert(!job.cached(url).empty());
    }
    {
        std::atomic<bool> entered{false};
        Download job(root, [&](const auto &, const auto &log, const auto &cancel) {
            entered = true;
            while (!cancel) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            // A late success after cancellation must never be published.
            std::ofstream(log.parent_path() / "video.mp4") << "late"; return 0;
        });
        job.start(url, true);
        while (!entered) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        job.cancel(); assert(job.snapshot().state == State::Cancelled);
        job.start("invalid"); assert(job.snapshot().state == State::Error);
    }
    {
        Download stream(root, [&](const std::vector<std::string> &args, const auto &log, const auto &) {
            assert(std::find(args.begin(), args.end(), "--skip-download") != args.end());
            assert(std::find(args.begin(), args.end(), "--output") == args.end());
            std::ofstream(log.parent_path() / "stream.txt") << "mp4a.40.2\nhttps://example.com/video.m3u8?token=temporary\nhttps://example.com/audio.m3u8\n";
            std::ofstream(log) << "metadata only"; return 0;
        });
        stream.resolve(url); wait(stream);
        const auto ready = stream.snapshot();
        assert(ready.state == State::Ready && ready.hasAudio && ready.file.extension() == ".m3u8");
        std::ifstream playlist(ready.file); std::string content((std::istreambuf_iterator<char>(playlist)), {});
        assert(content.find("#EXT-X-MEDIA:TYPE=AUDIO") != std::string::npos);
        assert(content.find("https://example.com/video.m3u8") != std::string::npos);
        // Existing media cache must not bypass resolution or be overwritten.
        std::ifstream cached(stream.cached(url)); std::string bytes; std::getline(cached, bytes); assert(bytes == "fake media");
        stream.cancel(); assert(!std::filesystem::exists(ready.file));
        assert(stream.snapshot().state == State::Cancelled);
    }
    {
        Download stream(root, [](const auto &, const auto &log, const auto &) {
            std::ofstream(log.parent_path() / "stream.txt") << "none\nhttps://example.com/video.mp4\n"; return 0;
        });
        stream.resolve(url); wait(stream);
        assert(stream.snapshot().state == State::Ready && !stream.snapshot().hasAudio);
        assert(stream.snapshot().file.empty() && stream.snapshot().streamUrl == "https://example.com/video.mp4");
        stream.cancel(); assert(stream.snapshot().streamUrl.empty());
    }
    {
        Download stream(root, [](const auto &, const auto &log, const auto &) {
            std::ofstream(log.parent_path() / "stream.txt") << "none\nfile:///etc/passwd\n"; return 0;
        });
        stream.resolve(url); wait(stream); assert(stream.snapshot().state == State::Error);
    }
    for (const auto &file : std::filesystem::directory_iterator(root)) assert(!file.is_directory());
#ifndef _WIN32
    {
        std::atomic<bool> cancel{false};
        assert(runProcess({"/usr/bin/printf", "%s", "literal $(not-a-command)"}, root / "process.log", cancel) == 0);
        std::ifstream in(root / "process.log"); std::string text; std::getline(in, text);
        assert(text == "literal $(not-a-command)");
        int code = 0;
        std::thread process([&] { code = runProcess({"/bin/sleep", "30"}, root / "cancel.log", cancel); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); cancel = true; process.join(); assert(code == -2);
        cancel = false;
        assert(runProcess({"guipper-nonexistent-executable"}, root / "missing.log", cancel) == -1);
    }
#endif
    std::filesystem::remove_all(root);
    std::cout << "PASS: YouTube URL validation, cache, stream resolution, HLS/audio, retry, cancellation, late results and process lifecycle\n";
}
