#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace jp {
enum class ToastState { Success, Info, Warning, Error };
struct ToastAction { std::string id, label; };
struct Toast {
    std::string id;
    ToastState state = ToastState::Info;
    std::string message;
    // Negative selects the severity default; zero means persistent.
    double duration = -1;
    std::vector<ToastAction> actions;
    std::string dismissAction, dismissTooltip;
    double elapsed = 0, transition = 0;
    bool closing = false;
};
struct ToastEvent { std::string toast, action; };
class ToastManager {
    std::vector<Toast> items_;
    std::vector<ToastEvent> events_;
public:
    const std::vector<Toast>& items() const { return items_; }
    const Toast* find(const std::string& id) const {
        for (const auto& t : items_) if (t.id == id) return &t;
        return nullptr;
    }
    bool publish(Toast toast) {
        if (toast.duration < 0) toast.duration = 4 + 2 * static_cast<int>(toast.state);
        toast.actions.resize(std::min<size_t>(2, toast.actions.size()));
        toast.elapsed = toast.transition = 0; toast.closing = false;
        for (auto& t : items_) if (t.id == toast.id) {
            // Refreshing an active operation should not flash its entrance again.
            if (!t.closing) toast.transition = t.transition;
            t = std::move(toast); return true;
        }
        if (items_.size() == 3) {
            auto old = std::find_if(items_.begin(), items_.end(), [](const Toast& t){ return t.duration > 0; });
            if (old == items_.end()) return false;
            items_.erase(old);
        }
        items_.push_back(std::move(toast)); return true;
    }
    void close(const std::string& id) {
        for (auto& t : items_) if (t.id == id && !t.closing) { t.closing = true; t.transition = 0; }
    }
    void activate(const std::string& id, const std::string& action) {
        const auto* t = find(id);
        if (!t || t->closing) return;
        if (action == "close") {
            if (t->dismissAction.empty()) close(id);
            else events_.push_back({id, t->dismissAction});
        } else for (const auto& a : t->actions) if (a.id == action) { events_.push_back({id, action}); break; }
    }
    std::vector<ToastEvent> takeEvents() { auto result = std::move(events_); events_.clear(); return result; }
    void update(double dt, const std::string& hovered = {}, bool blocked = false) {
        dt = std::max(0.0, dt);
        for (auto& t : items_) {
            t.transition += dt;
            if (!blocked && t.id != hovered && !t.closing && t.duration > 0) {
                t.elapsed += dt;
                if (t.elapsed >= t.duration) { t.closing = true; t.transition = 0; }
            }
        }
        items_.erase(std::remove_if(items_.begin(), items_.end(), [](const Toast& t){return t.closing && t.transition >= .16;}), items_.end());
    }
};
}
