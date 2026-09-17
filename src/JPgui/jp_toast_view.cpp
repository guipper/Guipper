#include "jp_toast_view.h"
#include "JPutils/jp_constants.h"
#include "JPutils/jp_tooltip.h"
namespace jp {
// Keep the existing typeface at 12/14 of the modal text size.
static constexpr float textScale = 12.f / 14.f;
static std::string fitLine(ofTrueTypeFont& font, std::string text, float width) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    if (font.stringWidth(text) * textScale <= width) return text;
    while (!text.empty() && font.stringWidth(text + "...") * textScale > width) {
        size_t end = text.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
        text.erase(end);
    }
    return width >= font.stringWidth("...") * textScale ? text + "..." : "";
}
static void drawText(ofTrueTypeFont& font, const std::string& text, float x, float y) {
    ofPushMatrix(); ofTranslate(x, y); ofScale(textScale, textScale);
    font.drawString(text, 0, 0); ofPopMatrix();
}
static std::string actionLabel(const std::string& label, float width) {
    return width < 440 ? label.substr(0, label.find(" · ")) : label;
}
void ToastView::layout(const ToastManager& manager, ofTrueTypeFont& font, float width, float height) {
    cards.clear();
    const float w = std::max(1.f, std::min(480.f, width - 24.f));
    float bottom = height - 12;
    for (const auto& t : manager.items()) {
        Card card; card.id = t.id;
        const float h = 38;
        float progress = std::min(1.0, t.transition / .16);
        float offset = 8 * (t.closing ? progress : 1 - progress);
        card.bounds = {(width - w) / 2, bottom - h + offset, w, h};
        card.close = {card.bounds.getRight() - 34, card.bounds.y + 4, 30, 30};
        float right = card.close.x - 4;
        // Reserve a readable message area, with compact actions alongside it.
        const float actionLimit = std::max(0.f, (w - 132) / std::max<size_t>(1, t.actions.size()) - 4);
        for (auto it = t.actions.rbegin(); it != t.actions.rend(); ++it) {
            const float aw = std::min(actionLimit, font.stringWidth(actionLabel(it->label, w)) * textScale + 16);
            right -= aw;
            card.actions.insert(card.actions.begin(), {it->id, {right, card.bounds.y + 5, aw, 28}});
            right -= 4;
        }
        card.lines = {fitLine(font, t.message, std::max(0.f, right - card.bounds.x - 30))};
        cards.push_back(std::move(card)); bottom -= h + 8;
    }
}
std::string ToastView::hovered(float x, float y) const {
    for (const auto& c : cards) if (c.bounds.inside(x,y)) return c.id;
    return {};
}
std::string ToastView::hitAction(const Card& c, float x, float y) const {
    if (c.close.inside(x,y)) return "close";
    for (const auto& a : c.actions) if (a.second.inside(x,y)) return a.first;
    return {};
}
bool ToastView::press(float x, float y, int button, bool blocked) {
    for (const auto& c : cards) if (c.bounds.inside(x,y)) {
        presses[button] = {c.id, hitAction(c,x,y), !blocked && button == OF_MOUSE_BUTTON_LEFT}; return true;
    }
    if (captures()) { presses[button] = {{}, {}, false}; return true; }
    return false;
}
bool ToastView::release(float x, float y, int button, bool blocked, ToastManager& manager) {
    auto it = presses.find(button);
    if (it == presses.end()) return captures();
    const auto press = it->second; presses.erase(it);
    if (!blocked && press.enabled && !press.action.empty())
        for (const auto& c : cards) if (c.id == press.id && hitAction(c,x,y) == press.action)
            manager.activate(press.id, press.action);
    return true;
}
void ToastView::draw(const ToastManager& manager, ofTrueTypeFont& font, bool blocked, bool spanish) {
    const char* symbols[] = {"", "i", "!", "×"};
    const ofColor colors[] = {COL_ACCENT_GREEN, COL_ACCENT_CYAN, COL_ACCENT_GOLD, COL_ACCENT_RED};
    ofPushStyle(); ofSetRectMode(OF_RECTMODE_CORNER); ofEnableAlphaBlending(); ofSetLineWidth(1);
    for (const auto& c : cards) {
        const auto* t = manager.find(c.id); if (!t) continue;
        float p = std::min(1.0, t->transition / .16);
        float alpha = 255 * (t->closing ? 1 - p : p);
        auto color = [&](ofColor value){value.a = value.a * alpha / 255.f; ofSetColor(value);};
        auto accent = colors[static_cast<int>(t->state)];
        ofFill(); auto background = COL_BG_PANEL; background.a = 190;
        color(background); ofDrawRectRounded(c.bounds, 5);
        color(accent); ofDrawRectangle(c.bounds.x, c.bounds.y + 6, 3, c.bounds.height - 12);
        color(accent);
        if (t->state == ToastState::Success) {
            ofSetLineWidth(1.5);
            ofDrawLine(c.bounds.x + 11, c.bounds.y + 19, c.bounds.x + 14, c.bounds.y + 22);
            ofDrawLine(c.bounds.x + 14, c.bounds.y + 22, c.bounds.x + 21, c.bounds.y + 15);
            ofSetLineWidth(1);
        } else drawText(font, symbols[static_cast<int>(t->state)], c.bounds.x + 12, c.bounds.y + 24);
        if (!blocked) jp_tooltip::drawFor(t->message, c.bounds,
            c.bounds.inside(ofGetMouseX(),ofGetMouseY()) && hitAction(c,ofGetMouseX(),ofGetMouseY()).empty(), "toast-message-" + c.id);
        color(COL_TEXT_PRIMARY);
        drawText(font, c.lines.front(), c.bounds.x + 28, c.bounds.y + 24);
        color(blocked ? COL_TEXT_DIM : COL_TEXT_PRIMARY);
        auto center = c.close.getCenter();
        ofSetLineWidth(1.5); ofDrawLine(center.x-4,center.y-4,center.x+4,center.y+4); ofDrawLine(center.x+4,center.y-4,center.x-4,center.y+4); ofSetLineWidth(1);
        if (!blocked) jp_tooltip::drawFor(t->dismissTooltip.empty() ? (spanish ? "Cerrar" : "Close") : t->dismissTooltip,
            c.close, c.close.inside(ofGetMouseX(),ofGetMouseY()), "toast-close-" + c.id);
        for (size_t i=0; i<c.actions.size(); ++i) {
            const auto& r = c.actions[i].second;
            color(!blocked && r.inside(ofGetMouseX(),ofGetMouseY()) ? COL_BG_HOVER : COL_BG_BUTTON); ofDrawRectRounded(r,3);
            color(blocked ? COL_TEXT_DIM : COL_TEXT_PRIMARY);
            if (!blocked) jp_tooltip::drawFor(t->actions[i].label, r,
                r.inside(ofGetMouseX(),ofGetMouseY()), "toast-action-" + c.id + c.actions[i].first);
            drawText(font, fitLine(font, actionLabel(t->actions[i].label, c.bounds.width), r.width - 16), r.x+8, r.y+19);
        }
    }
    ofPopStyle();
}
}
