#pragma once
#include "ofMain.h"
#include "../JPutils/jp_hand_tracking.h"
#include <functional>
namespace jp_hand_ui {
// A binding target supplies a lifetime guard, so deleting or replacing a graph
// cannot leave callbacks holding a live-looking pointer into a former inspector.
struct Target {
 std::function<bool()> valid,active;
 std::function<void(bool)> activate;
 std::function<void(const std::function<void()> &)> edit;
 jp_hand::Binding *binding=nullptr; jp_hand::Filter *filter=nullptr;
 std::function<float()> low,high,value;
 std::string name;
};
void open(Target target={});bool visible();bool contains(float x,float y);
void draw();void settingsButton(const ofRectangle &bounds);void beginFrame();
bool press(int x,int y,int button);bool drag(int x,int y);bool release();
bool key(int key);bool editing();
}
