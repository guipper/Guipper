#include "jp_shader_globals.h"

#include "jp_audio.h"
#include "jp_constants.h"

void jp_shader_globals::apply(ofShader &shader, const JPShaderGlobalsCtx &ctx)
{
	if (ctx.feedback != nullptr)
	{
		shader.setUniformTexture("feedback", *ctx.feedback, 0);
	}
	shader.setUniform2f("resolution", ctx.width, ctx.height);

	const float mouseX = ctx.liveMouse ?
		ofMap(ofGetMouseX(), 0, ofGetWidth(), 0, 1) : 0.5f;
	const float mouseY = ctx.liveMouse ?
		ofMap(ofGetMouseY(), 0, ofGetHeight(), 0, 1) : 0.5f;
	const float pressedX = ctx.liveMouse ?
		ofMap(jp_constants::mousePressedPos.x, 0, ofGetWidth(), 0, 1) : 0.5f;
	const float pressedY = ctx.liveMouse ?
		ofMap(jp_constants::mousePressedPos.y, 0, ofGetHeight(), 0, 1) : 0.5f;
	shader.setUniform4f("mouse", mouseX, mouseY, pressedX, pressedY);

	shader.setUniform1i("globalframeNum", ctx.liveMouse ? ofGetFrameNum() : 0);
	shader.setUniform1i("boxframeNum", ctx.boxFrameNum);
	shader.setUniform2f("window_mouse",
		ctx.liveMouse ?
			ofMap(jp_constants::window_mousex, 0, jp_constants::window_width, 0, 1) :
			mouseX,
		ctx.liveMouse ?
			ofMap(jp_constants::window_mousey, 0, jp_constants::window_height, 0, 1) :
			mouseY);
	shader.setUniform1f("time", ofGetElapsedTimef());
	shader.setUniform1f("bpm", jp_constants::bpm);

	// Audio. Packed into vec4s deliberately: the .frag parser matches lines
	// containing "float", so a vec4 can never be mistaken for a slider.
	const jp_audio::AudioSnapshot audio = jp_audio::getSnapshot();
	shader.setUniform4f("audio_bands",
		audio.low, audio.mid, audio.high, audio.level);
	shader.setUniform4f("audio_hits",
		jp_audio::getValue(jp_audio::SRC_KICK),
		jp_audio::getValue(jp_audio::SRC_SNARE),
		jp_audio::getValue(jp_audio::SRC_LOWBASS),
		jp_audio::getValue(jp_audio::SRC_HIGHMID));
	const int div = jp_audio::getShaderDiv();
	shader.setUniform1f("audio_trigger",
		jp_audio::getValue(jp_audio::SRC_KICK_TRIGGER, div));
	shader.setUniform1f("audio_express",
		jp_audio::getValue(jp_audio::SRC_KICK_EXPRESS, div));
	shader.setUniform1f("audio_logic",
		jp_audio::getValue(jp_audio::SRC_KICK_LOGIC, div));
	shader.setUniform4f("audio_onsets",
		audio.kickTrigger, audio.snareTrigger,
		audio.kickLogic, audio.snareLogic);
	shader.setUniform4f("audio_rhythm",
		audio.beatPhase, audio.beatPulse,
		audio.detectedBpm, audio.tempoConfidence);
	shader.setUniform4f("audio_spectrum0", audio.spectrum[0], audio.spectrum[1], audio.spectrum[2], audio.spectrum[3]);
	shader.setUniform4f("audio_spectrum1", audio.spectrum[4], audio.spectrum[5], audio.spectrum[6], audio.spectrum[7]);
	shader.setUniform4f("audio_spectrum2", audio.spectrum[8], audio.spectrum[9], audio.spectrum[10], audio.spectrum[11]);
	shader.setUniform4f("audio_spectrum3", audio.spectrum[12], audio.spectrum[13], audio.spectrum[14], audio.spectrum[15]);
}

bool jp_shader_globals::isGlobalName(const std::string &name)
{
	return name == "time" || name == "resolution" || name == "bpm" ||
		name == "mouse" || name == "window_mouse" ||
		name == "globalframeNum" || name == "boxframeNum" ||
		name == "feedback" || isNewGlobalName(name);
}

bool jp_shader_globals::isNewGlobalName(const std::string &name)
{
	// audio_bands / audio_hits are vec4 and never reach the float parser.
	return name == "audio_trigger" || name == "audio_express" ||
		name == "audio_logic" || name == "audio_onsets" ||
		name == "audio_rhythm" || name == "audio_spectrum0" ||
		name == "audio_spectrum1" || name == "audio_spectrum2" ||
		name == "audio_spectrum3";
}
