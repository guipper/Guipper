#pragma once
#include "ofMain.h"
#include "JPutils/jp_toast.h"
#include <map>
namespace jp {
class ToastView {
    struct Card {
        std::string id;
        ofRectangle bounds, close;
        std::vector<std::string> lines;
        std::vector<std::pair<std::string, ofRectangle>> actions;
    };
    struct Press { std::string id, action; bool enabled; };
    std::vector<Card> cards;
    std::map<int, Press> presses;
    std::string hitAction(const Card&, float x, float y) const;
public:
    void layout(const ToastManager&, ofTrueTypeFont&, float width, float height);
    void draw(const ToastManager&, ofTrueTypeFont&, bool blocked, bool spanish);
    std::string hovered(float x, float y) const;
    bool captures() const { return !presses.empty(); }
    bool press(float x, float y, int button, bool blocked);
    bool release(float x, float y, int button, bool blocked, ToastManager&);
};
}
