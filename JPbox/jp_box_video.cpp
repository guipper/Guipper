#include "jp_box_video.h"

JPbox_video::JPbox_video(){}
JPbox_video::~JPbox_video(){}

void JPbox_video::setup(string _dir, string _nombre){

	JPbox::setup(_dir,_nombre);

	//parameters.coutData();
	name = _nombre;
	dir = _dir;
	//img.loadImage(_dir);
	movie.loadAsync(_dir);
	movie.setLoopState(OF_LOOP_NORMAL);
	movie.play();

	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addBoolValue(true, "strech");
	parameters.addFloatValue(0.25, "speed");
	parameters.addFloatValue(0.0, "position");
	parameters.addBoolValue(true, "play");
	tipo = VIDEOBOX;
}
void JPbox_video::update(){
	JPbox::update();
	updateFBO();
}
void JPbox_video::updateFBO()
{
	if (onoff.boolValue) {
		movie.update();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);

		//float pos = ofMap(parameters.getFloatValue(6), 0.0, 1.0, 0.0, movie.getTotalNumFrames());
		//movie.setPosition(pos);
		float position = ofMap(movie.getCurrentFrame(), 0.0, movie.getTotalNumFrames(), 0.0, 1.0);
		parameters.setFloatValue(position, 6);
		//cout << "POSITION " << position << endl;

		movie.setSpeed(ofMap(parameters.getFloatValue(5), 0.0, 1.0, .0, 4.0));
		float mscalex = ofMap(parameters.getFloatValue(0), 0.0, 1.0, 0.0, jp_constants::renderWidth);
		float mscaley = ofMap(parameters.getFloatValue(1), 0.0, 1.0, 0.0, jp_constants::renderHeight);
		float moffsetx = ofMap(parameters.getFloatValue(2), 0.0, 1.0,
			-jp_constants::renderWidth / 2 - mscalex / 2,
			jp_constants::renderWidth / 2 + mscalex / 2);

		float moffsety = ofMap(parameters.getFloatValue(3), 0.0, 1.0,
			-jp_constants::renderHeight / 2 - mscaley / 2,
			jp_constants::renderHeight / 2 + mscaley / 2);
		fbo.begin();
		ofClear(0, 255);
		if (!parameters.getBoolValue(4)) {
			movie.draw(jp_constants::renderWidth / 2 - mscalex / 2 + moffsetx,
				jp_constants::renderHeight / 2 - mscaley / 2 + moffsety,
				mscalex,
				mscaley);
		}
		else {
			movie.draw(0, 0, jp_constants::renderWidth, jp_constants::renderHeight);
		}
		fbo.end();
	}
	else {
		JPbox::updateFBO();
	}
}
void JPbox_video::draw(){
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
}
void JPbox_video::clear(){
	JPbox::clear();
	movie.close();
	movie.closeMovie();
	//movie.unbind();
	//movie.
	//movie.clear();
	cout << "CORRE CLEAR VIDEOBOX " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
