#pragma once

#include "defines.h"
#include "ofMain.h"
#include "jp_box.h"
#include "jp_box_cam.h"
#include "jp_box_camdepth.h"
#include "jp_box_kinect2.h"
#include "jp_box_pointercloud.h"
#include "jp_box_image.h"
#include "jp_box_shader.h"
#ifdef SPOUT
#include "jp_box_spout.h"
#endif
#ifdef NDI
#include "jp_box_ndi.h"
#endif
#include "jp_box_video.h"
#include "jp_box_framedifference.h"
#include "jp_box_paint.h"
//#include "ofxSpout2Receiver.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "../JPutils/TransitionSR.h"

#ifdef SPOUT
#include "../SpoutSDK/Spout.h" // Spout SDK
#endif
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_preset : public JPbox
{
public:
	struct ExposedTextureInput
	{
		string publicName;
		string targetBoxName;
		string targetSamplerName;
	};

	JPbox_preset(); // constructor declared
	~JPbox_preset();

	void setup(string _directory, string _name);

	// void setup(float _x, float _y, string _dirinput);
	// void setup(string _dir);

	vector<JPbox *> boxes; // ESTO SERIA UNA RELACION FRACTAL O QUE CARAJO ?
						   // string dir;
	// JPFbohandlerGroup fbohandlergroup;

	// METODOS HEREDADOS :
	// void reload();

	// void setup();
	void update();
	void updateFBO();
	void renderActiveRender();
	void draw();
	void clear();
	void addBox(JPbox &_box);

	int activeRender;

	// Exposed parameters: exposedParams[childBoxIndex][paramIndex] = true means exposed
	vector<vector<bool>> exposedParams;
	// For propagated exposes (from grandchildren): stores (grandchildBoxIndex, paramIndex) for each exposedParams entry
	// exposedParamOriginalIndices[childBoxIndex][paramIndex] = {grandchildIndex, paramIndex}
	// Used when an exposed param comes from a child's child (propagated one more level)
	vector<vector<pair<int,int>>> exposedParamOriginalIndices;
	vector<ExposedTextureInput> exposedTextureInputs;

	// Per-preset viewport zoom/pan - saved/loaded from XML
	float viewportZoom = 1.0f;
	ofVec2f viewportPan = ofVec2f(0, 0);

	// Local crossfade for switching between this preset's child renders.
	TransitionSR activeRenderTransition;
	bool activeRenderTransitionInitialized = false;
	bool activeRenderTransitionRunning = false;
	int lastCompositedActiveRender = -1;
	int activeRenderTransitionTarget = -1;

	void setExposedParam(int childIndex, int paramIndex, bool exposed);
	bool isParamExposed(int childIndex, int paramIndex) const;
	void clearExposedParams();
	void resizeExposedParams(int numChildren);
	bool exposeTextureInput(const string &targetBoxName,
		const string &targetSamplerName,
		string *publicName = nullptr);
	bool removeExposedTextureInput(const string &targetBoxName,
		const string &targetSamplerName);
	bool removeExposedTextureInputsForBox(const string &targetBoxName);
	bool isTextureInputExposed(const string &targetBoxName,
		const string &targetSamplerName) const;
	bool isExposedTextureInputTarget(const string &targetBoxName,
		const string &targetSamplerName) const;
	void renameExposedTextureInputTarget(const string &oldBoxName,
		const string &newBoxName);
	bool retargetExposedTextureInput(const string &publicName,
		const string &targetBoxName,
		const string &targetSamplerName);
	void setExposedTextureInputs(
		const vector<ExposedTextureInput> &inputs);
	void rebuildExposedTextureInputHandlers();
	void syncExposedTextureInputs();
	void pruneInvalidExposedTextureInputs();
	string getExposedTextureInputTargetLabel(
		const string &publicName) const;
	void save();

private:
	JPbox *findDirectChildByName(const string &childName) const;
	string makeUniqueExposedTextureInputName(
		const string &samplerName) const;
	void updateExposedTextureInputNodePositions();
};
