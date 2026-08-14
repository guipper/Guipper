#pragma once

#include "ofMain.h"

class ofApp;

// Reproducible screenshots of the inspector panel.
//
// The panel's spacing has only ever been checked by looking at it, and looking
// at it is not reproducible: window size, cursor position, ofRandom parameter
// seeding (jp_box_shader.cpp seeds every uniform with ofRandom(1)) and a live
// shader animating behind the panel all change between two runs, so a 3px
// regression is indistinguishable from a different machine. Everything in the
// .cpp exists to make two runs byte-identical.
//
//   GUIPPER_UISHOT=<tag> ./Guipper
//
// walks a fixed list of inspector states and writes, per state,
//   bin/data/uishots/<tag>/<state>.png   - a fixed-rect grab of the panel
//   bin/data/uishots/<tag>/<state>.txt   - the geometry that PNG was painted from
// then exits. Compare two tags with:
//   compare -metric AE before/<s>.png after/<s>.png /tmp/d.png
//   diff -u before/<s>.txt after/<s>.txt
//
// The .txt is the point: a pixel count says something moved, the sidecar says
// WHICH rect moved and by how much.
namespace jp_uishot
{
	// True when GUIPPER_UISHOT is set. Cheap; safe to call before setup().
	bool active();

	// Last line of ofApp::setup(), AFTER loadSettings() - that auto-loads the
	// default composition, which the harness has to clobber.
	void setup(ofApp &app);

	// Test-only stale state injection. Called immediately before the production
	// window-state reset, after all offscreen updates have finished.
	void poisonWindowStateForTest();

	// First line of ofApp::update(), so state is armed before boxes.update()
	// runs rebuildControllersIfLayoutStale().
	void update(ofApp &app);

	// LAST line of ofApp::draw(). grabScreen is glReadPixels on the back
	// buffer: correct at end of draw, empty after the swap.
	void draw(ofApp &app);
}
