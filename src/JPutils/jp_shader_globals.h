#pragma once

#include "ofMain.h"
#include <string>

// The uniforms every shader gets for free.
//
// This block used to be copy-pasted in FOUR places - JPbox_shader,
// JPbox_sequencer, JPbox_framedifference and the IMPORT preview in ofApp - so
// adding one uniform meant editing four functions and silently getting three
// of them wrong. It lives here once now.
struct JPShaderGlobalsCtx
{
	float width = 0.0f;
	float height = 0.0f;
	int boxFrameNum = 0;
	// The IMPORT preview freezes the mouse at 0.5 and the frame counter at 0,
	// so a thumbnail does not depend on where the pointer happens to be.
	bool liveMouse = true;
	// Bound as "feedback" when set. The preview has no feedback buffer.
	const ofTexture *feedback = nullptr;
};

namespace jp_shader_globals
{
	void apply(ofShader &shader, const JPShaderGlobalsCtx &ctx);

	// Names the app fills in itself. Used by the IMPORT parser so a global does
	// not show up as a randomisable slider.
	//
	// NOT usable as a filter in JPbox_shader's .frag parser: that parser turns
	// every `uniform float` into a JPParameter, and saved compositions load
	// their <param> blocks POSITIONALLY. Dropping a parameter there would shift
	// every later index and scramble existing saves. See isNewGlobalName().
	bool isGlobalName(const std::string &name);

	// Only the uniforms introduced WITH this helper, which therefore appear in
	// no existing save file and can be skipped by the .frag parser safely.
	bool isNewGlobalName(const std::string &name);
}
