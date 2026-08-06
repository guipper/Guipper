#pragma once
#include "ofMain.h"
#include <filesystem>

#include "defines.h"
//#include "Shaderrender.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_dragobject.h"
#include "../JPgui/jp_toogle.h"

// Normalize a stored file path so sessions saved on Windows (which use '\'
// separators) load on Linux/macOS. Only the separator needs fixing; the
// "data/" prefix is handled by ofToDataPath. Idempotent for Unix-style paths.
inline std::string jp_normalizePath(std::string p) {
	for (char &c : p) { if (c == '\\') c = '/'; }
	return p;
}

// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox : public JPdragobject
{
public:
	JPbox();
	virtual ~JPbox();

	bool isactiverender;

	float inlet_size;
	float outlet_x;
	float outlet_y;
	float outlet_size;

	float triangleangle;

	JPParameterGroup parameters;
	string dir;
	JPFbohandlerGroup fbohandlergroup;
	// Shaderrender shaderrender; //HOLDS THE RENDER OF THE SHADER

	ofFbo fbo;
	void reloadShaderonly(); // ESTA FUNCION LA VOY A PONER PARA DEBUGGEAR A VER.
	virtual void reload();
	void setup(ofTrueTypeFont &_font);
	virtual void setup(string _directory, string _name);
	virtual void update();
	virtual void draw();
	virtual void updateFBO();
	void draw_outlet();
	virtual void clear();
	virtual void saveCustomState(ofXml &boxNode) const;
	virtual void loadCustomState(const ofXml &boxNode);
	virtual void copyCustomStateFrom(const JPbox *source);
	void setPos(float _x, float _y)
	{
		JPdragobject::setPos(_x, _y);
	}

	ofColor border;
	ofColor border_mouseover;
	ofColor border_grab;
	ofColor Cfront;
	bool useBackgroundOverride = false;
	ofColor backgroundOverride;
	ofColor backgroundBorderOverride;
	void setBackgroundOverride(const ofColor &_background, const ofColor &_border)
	{
		useBackgroundOverride = true;
		backgroundOverride = _background;
		backgroundBorderOverride = _border;
	}
	void clearBackgroundOverride()
	{
		useBackgroundOverride = false;
	}
	string name;

	bool activeFlag;
	bool outletActiveFlag;
	bool mouseOverOutlet();

	enum type
	{
		SHADERBOX,
		IMAGEBOX,
		VIDEOBOX,
		CAMBOX,
		SPOUTBOX,
		PRESETBOX,
		FRAMEDIFFERENCEBOX,
		NDIBOX,
		SEQUENCERBOX,
		KINECT2BOX,
		// POINTCLOUDBOX was removed; the ordinal stays reserved because saved
		// sessions store these as ints. Append only.
		POINTCLOUDBOX_REMOVED,
		POINTERCLOUDBOX
	};

	int getTipo();
	void setTipo(int _tipo);
	// File-modification timestamp used to auto-reload shaders when the source
	// file changes. Must match std::filesystem::last_write_time()'s return type:
	// on Linux/macOS file_time_type does not implicitly convert to time_t.
	std::filesystem::file_time_type datemodified;

	// Me perturba que el nombre de la variable sea tan verga.
	void setonoff(bool _val);
	bool getonoff();
	JPToogle onoff;
	void setBypass(bool _val);
	bool getBypass();
	bool tryPassThroughFBO();
	JPToogle bypass;

	//SHOWCODE
	bool showCode = false;
protected:
	int tipo; // Habra una manera menos cacuija de hacer esto? no se, pero ya me pudrio si, esta bien o mal me la chupa.

	uint64_t titleHoverStartMillis = 0;
	uint64_t bypassHoverStartMillis = 0;
	uint64_t onoffHoverStartMillis = 0;

	float padding_top;
	float padding_leftright;
	float padding_bottom;

	float fbowidth;
	float fboheight;
};
