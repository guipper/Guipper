#pragma once

#include "ofMain.h"
#include "jp_box.h"
#include "defines.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "jp_media.h"
#include "../JPutils/jp_media_stats.h"
#include <future>
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_image : public JPbox, public JPMediaInspectable
{
public:
	JPbox_image(); // constructor declared
	~JPbox_image();

	ofImage img;
	JPMediaState media;
	JPMediaState &mediaState() override { return media; }
	const JPMediaState &mediaState() const override { return media; }
	bool mediaPlayable() const override;
	bool mediaHasAudio() const override { return false; }
	bool mediaReady() const override;
	std::string mediaStatus() const override;
	double mediaDurationSeconds() const override;
	int mediaFrameCount() const override;
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
	void reload();
	void setup(string _dir, string _nombre);
	void update();
	void updateFBO();
	void draw();
	void clear();
	void setPos(float _x, float _y)
	{
		JPdragobject::setPos(_x, _y);
		// setfbohandler_nodepos();
	}

	// METODOS Y VARIABLES PROPIAS DE LA CLASE :
	// void setfbohandler_nodepos();
	// void update_NonglobalUniforms();
	// void update_globalUniforms();//GLOBAL UNIFORMS
	// JPParameterGroup getUniformsToJPParameterGroup(string _dir, string _name);
	// void setUniforms(JPParameterGroup & _parameters, JPFbohandlerGroup & _fbohandlergroup, string _dir, string _name);
	// ofFbo fbo;
	// ofShader shader;
private:
	struct GifData;
	std::shared_ptr<const GifData> gif;
	std::shared_future<std::shared_ptr<const GifData>> gifFuture;
	ofTexture gifTexture;
	int gifFrame = -1;
	double gifLastUpdate = 0.0;
	std::string loadStatus;
	bool lastLegacyStretch = true;
	// jp_media::isGif() lowercases a fresh std::string out of the path every
	// time it is asked. That used to happen twice per frame per image box, to
	// answer a question that can only change when the file does.
	bool isGifSource = false;
	// Signature of the last pass actually rendered, and a counter bumped by
	// anything that produces new pixels. See JPMediaRenderSignature.
	JPMediaRenderSignature lastRenderSignature;
	unsigned long long sourceGeneration = 1;
	void startGifLoad();
	void updateGif();
	// Forces the next frame to render regardless of the signature.
	void invalidateRender();
	float lasttime_autoreload;
	float duration_autoreload;
};
