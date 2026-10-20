#pragma once
#include <array>
#include <string>
#include <vector>
namespace jp_text {
struct Style {
    float size=96, leading=1.2f, tracking=0, width=.8f, x=.5f, y=.5f, margin=24;
    float outline=0, shadow=0, shadowX=4, shadowY=4, shadowBlur=4;
    std::array<float,4> color{1,1,1,1}, background{0,0,0,0}, edge{0,0,0,1};
    int align=1, anchor=1, weight=400; bool italic=false;
};
struct Raster {int width=0,height=0; std::vector<unsigned char> rgba; bool clipped=false; std::string error;};
// CPU-only renderer. Pixel storage is straight alpha, top-down RGBA.
Raster rasterize(const std::string &text, const std::string &family, const Style &style, int width, int height);
std::vector<std::string> families();
bool registerFont(const std::string &path, std::string &family);
}
