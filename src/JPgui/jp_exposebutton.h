#pragma once
#include "defines.h"
#include "ofMain.h"
#include "jp_toogle.h"

class JPExposeButton : public JPToogle
{
public:
	void setup(float _x, float _y, float _size);
	void draw();

private:
	void drawEyeIcon(float cx, float cy, float size);
};
