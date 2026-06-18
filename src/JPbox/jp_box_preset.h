#pragma once

#include "defines.h"
#include "ofMain.h"
#include "jp_box.h"
#include "jp_box_cam.h"
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
//#include "ofxSpout2Receiver.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"

#ifdef SPOUT
#include "../SpoutSDK/Spout.h" // Spout SDK
#endif
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_preset : public JPbox
{
public:
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

	// Per-preset viewport zoom/pan - saved/loaded from XML
	float viewportZoom = 1.0f;
	ofVec2f viewportPan = ofVec2f(0, 0);

	void setExposedParam(int childIndex, int paramIndex, bool exposed);
	bool isParamExposed(int childIndex, int paramIndex) const;
	void clearExposedParams();
	void resizeExposedParams(int numChildren);
	void save();
};
