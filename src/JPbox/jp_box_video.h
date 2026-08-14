#pragma once

#include "defines.h"
#include "ofMain.h"
#include "jp_box.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "jp_media.h"
#include "../JPutils/jp_media_stats.h"
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_video : public JPbox, public JPMediaInspectable
{
public:
	JPbox_video(); // constructor declared
	~JPbox_video();

	ofVideoPlayer movie;
	JPMediaState media;
	JPMediaState &mediaState() override { return media; }
	const JPMediaState &mediaState() const override { return media; }
	bool mediaPlayable() const override { return true; }
	bool mediaHasAudio() const override { return true; }
	bool mediaReady() const override;
	std::string mediaStatus() const override;
	double mediaDurationSeconds() const override { return sourceDuration; }
	int mediaFrameCount() const override { return sourceFrames; }
	float mediaSteppedPosition(float normalized, int frames) const override;
	void mediaSeek(float normalized) override;
	void mediaStep(int frames) override;
	void mediaRestart() override;
	void saveCustomState(ofXml &boxNode) const override;
	void loadCustomState(const ofXml &boxNode) override;
	void copyCustomStateFrom(const JPbox *source) override;
	// string dir;
	// JPFbohandlerGroup fbohandlergroup;

	// METODOS HEREDADOS :
	// void reload();
	void setup(ofTrueTypeFont &_font,
			   string _dir,
			   string _nombre);
	void setup(string _dir, string _nombre);
	void update();
	void updateFBO();
	void draw();
	void clear();

	float auxpos; //Guardamos la posicion anterior del parametro.
	float lastLegacySpeed = 0.25f;
	float lastLegacyPosition = 0.0f;
	bool lastLegacyPlay = true;
	bool lastLegacyStretch = true;
	bool mediaSeekPending = false;
	double sourceDuration = 0.0;
	int sourceFrames = 0;
	uint64_t nextMetadataQueryMs = 0;
	// Signature of the last pass rendered, and a counter bumped whenever the
	// decoder reports a new frame. A 30fps clip in a 60fps app only produces
	// new pixels every second frame; the rest would recomposite an identical
	// image at full render resolution.
	JPMediaRenderSignature lastRenderSignature;
	unsigned long long sourceGeneration = 1;
	// ofGstUtils::setVolume is an unguarded g_object_set on the pipeline, so
	// this tracks the last value to avoid poking GStreamer sixty times a second
	// with a number that has not moved. (setSpeed needs no such guard - it
	// already early-returns on an unchanged value.)
	float lastAppliedVolume = -1.0f;
	void invalidateRender();

	// Seek throttling. Both setSpeed and setPosition issue a FLUSH|ACCURATE
	// gst_element_seek, and issuing several in quick succession corrupts the
	// pipeline's segment - GStreamer aborts with
	// "gst_segment_do_seek: assertion 'segment->format == format' failed" and
	// then deadlocks hard enough to survive SIGTERM.
	//
	// Dragging the speed slider used to request a new rate on every single
	// frame, which is sixty of those seeks a second.
	float lastAppliedSpeed = 0.0f;
	float pendingSpeed = 0.0f;
	uint64_t pendingSpeedSinceMs = 0;
	bool seekedThisFrame = false;
	// Applies a rate change once it has stopped moving, so a drag produces one
	// seek at the end instead of one per frame. `immediate` is for a direction
	// flip, where the sign has to reach the backend before the paired position
	// seek or that seek goes the wrong way.
	void requestSpeed(float target, bool immediate);


	/*void setPos(float _x, float _y) {
		JPdragobject::setPos(_x, _y);
		//setfbohandler_nodepos();
	}*/
};
