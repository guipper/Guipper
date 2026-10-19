#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace jp_screen {
enum class State { Idle, Selecting, Streaming, Stopped, Cancelled, Error, Unavailable };
struct Frame {
    int width = 0, height = 0;
    uint64_t serial = 0;
    std::vector<unsigned char> rgba;
};
struct Snapshot {
    State state = State::Selecting;
    std::string message;
    std::shared_ptr<const Frame> frame;
};
// Backend callbacks own only this mailbox. Closing it makes late callbacks inert.
class Mailbox {
    mutable std::mutex mutex;
    Snapshot value;
    bool closed = false;
    uint64_t serial = 0;
public:
    Snapshot read() const { std::lock_guard<std::mutex> lock(mutex); return value; }
    void state(State s, std::string message = {}) {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed) return;
        value.state = s; value.message = std::move(message);
        if (s != State::Streaming && s != State::Selecting) value.frame.reset();
    }
    void publish(Frame frame) {
        if (frame.width <= 0 || frame.height <= 0 ||
            frame.rgba.size() != size_t(frame.width) * size_t(frame.height) * 4) return;
        std::lock_guard<std::mutex> lock(mutex);
        if (closed || (value.state != State::Selecting && value.state != State::Streaming)) return;
        frame.serial = ++serial;
        value.frame = std::make_shared<Frame>(std::move(frame));
        value.state = State::Streaming; value.message.clear();
    }
    void close() { std::lock_guard<std::mutex> lock(mutex); closed = true; value.frame.reset(); }
};
struct Monitor { std::string id, label; };
class Backend {
public:
    std::shared_ptr<Mailbox> mailbox = std::make_shared<Mailbox>();
    virtual ~Backend() = default;
    virtual void stop() = 0;
};
std::unique_ptr<Backend> makeBackend(const std::string &monitor);
std::unique_ptr<Backend> makeWindowBackend(const std::string &window);
std::vector<Monitor> windows();
std::vector<Monitor> monitors(); // Windows: an explicit in-app choice; other platforms use a portal/picker.
using Factory = std::function<std::unique_ptr<Backend>(const std::string &)>;
class Capture {
    Factory factory;
    std::unique_ptr<Backend> active, pending;
    Snapshot current{State::Idle, {}, {}};
    Snapshot previous{State::Idle, {}, {}};
    static void release(std::unique_ptr<Backend> &backend) {
        if (backend) { backend->mailbox->close(); backend->stop(); backend.reset(); }
    }
public:
    explicit Capture(Factory factory = makeBackend) : factory(std::move(factory)) {}
    ~Capture() { stop(); }
    Capture(const Capture &) = delete;
    Capture &operator=(const Capture &) = delete;
    void select(const std::string &monitor = {}) {
        if (pending) return;
        previous = current;
        pending = factory(monitor);
        current.state = State::Selecting; current.message.clear();
    }
    Snapshot update() {
        if (active) {
            auto a = active->mailbox->read();
            current.frame = a.frame;
            if (a.state == State::Error || a.state == State::Stopped || a.state == State::Unavailable) {
                current = a; previous = a; release(active);
            } else if (!pending) current = a;
        }
        if (pending) {
            auto p = pending->mailbox->read();
            if (p.state == State::Streaming && p.frame) {
                release(active); active = std::move(pending); current = p; previous = {State::Idle, {}, {}};
            } else if (p.state == State::Cancelled) {
                release(pending);
                current = active ? active->mailbox->read() : previous;
            } else if (p.state == State::Error || p.state == State::Unavailable || p.state == State::Stopped) {
                release(pending);
                current = active ? active->mailbox->read() : p;
                current.message = p.message;
            } else current.state = State::Selecting;
        }
        return current;
    }
    void cancelSelection() {
        if(!pending)return;
        release(pending);
        current=active ? active->mailbox->read() : previous;
    }
    void stop() {
        release(pending); release(active);
        current = {State::Stopped, {}, {}}; previous = {State::Idle, {}, {}};
    }
    Snapshot snapshot() const { return current; }
};
}
