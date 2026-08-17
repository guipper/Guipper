#pragma once
#include "ofMain.h"
#include <filesystem>

#include "defines.h"
//#include "Shaderrender.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_dragobject.h"
#include "../JPutils/jp_pointer.h"
#include "../JPgui/jp_toogle.h"

// Normalize a stored file path so sessions saved on Windows (which use '\'
// separators) load on Linux/macOS. Only the separator needs fixing; the
// "data/" prefix is handled by ofToDataPath. Idempotent for Unix-style paths.
inline std::string jp_normalizePath(std::string p) {
	for (char &c : p) { if (c == '\\') c = '/'; }
	return p;
}

// Stable box identity, minted in JPbox's constructor so it is never empty.
//
// Minting by default is the safe default: a creation path nobody remembered to
// think about produces a FRESH identity - a binding breaks, visibly - rather
// than a DUPLICATE one, which silently points a live output at the wrong box.
// Preserving identity is therefore always explicit and greppable
// (`dst->uid = src->uid`), and there are only two such places: the cue draft
// clone and its preset-tree mirror.
//
// Salted per process, so a uid minted now cannot collide with one already
// stored in a file written by an earlier session.
namespace jp_boxuid
{
	std::string mint();
}

// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox;

// Render scheduling, shared by the top level and by groups.
//
// It lives here rather than in JPboxgroup because JPbox_preset needs the exact
// same rule for its children: a group used to update every child at full rate
// every frame, so putting a heavy shader inside a group silently opted it out
// of all throttling. Two copies of a dependency walk would drift - this file
// already carries the scars of one rule that got duplicated three times.
namespace jp_renderschedule
{
	// One frame in four for anything off the dependency path.
	constexpr int kPreviewInterval = 4;

	// Marks every box in `roots`, plus everything feeding their inlets
	// (recursively), to render this frame. The rest fall to kPreviewInterval,
	// STAGGERED by index so the cost spreads across frames instead of spiking
	// on every fourth one.
	//
	// Dependencies are matched by FBO POINTER, never by name: duplicate display
	// names are legal and name matching would select the wrong producer.
	//
	// Out-of-range or negative root indices are ignored, so callers can pass a
	// "nothing selected" sentinel without checking first.
	void apply(const std::vector<JPbox *> &boxes,
			   const std::vector<int> &roots,
			   uint64_t frame,
			   bool forceFullRate);
}

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
	// The graph scheduler can reduce the refresh rate of off-screen shader
	// thumbnails without pausing parameters or their animation clocks. Source
	// boxes ignore this hint unless their implementation explicitly opts in.
	void setRenderThisFrame(bool enabled) { renderThisFrame = enabled; }
	bool shouldRenderThisFrame() const { return renderThisFrame; }
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
	// The rect mouseOverOutlet tests. Shared with the GUIPPER_HITBOX overlay so
	// the outline and the hit test can never drift apart.
	ofRectangle outletBounds() const;
	// Outlines the box's selectable area, its texture OUT and every texture IN,
	// when GUIPPER_HITBOX is set. No-op otherwise.
	void drawHitboxDebug();

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
		POINTERCLOUDBOX,
		CAMDEPTHBOX
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

	// Identity that survives a rename. `name` cannot serve: renaming is a bare
	// assignment that never consults makeUniqueBoxName, so two boxes can share
	// one, and anything holding a name silently rebinds or breaks. Live-output
	// source selection holds this instead.
	//
	// Empty only between construction and the first mint/load. Unique across a
	// whole composition, nested group children included - JPboxgroup owns that
	// guarantee, see repairBoxUids.
	string uid;
	// Whether this box may be picked as a live-output source. Opt-in: the
	// picker used to list every box in the graph, which was noise, and it
	// could not reach group children at all.
	void setOutputCandidate(bool _val);
	bool getOutputCandidate() const;
	bool outputCandidate = false;

	//SHOWCODE
	bool showCode = false;
protected:
	int tipo; // Habra una manera menos cacuija de hacer esto? no se, pero ya me pudrio si, esta bien o mal me la chupa.
	bool renderThisFrame = true;

	uint64_t titleHoverStartMillis = 0;
	uint64_t bypassHoverStartMillis = 0;
	uint64_t onoffHoverStartMillis = 0;

	float padding_top;
	float padding_leftright;
	float padding_bottom;

	float fbowidth;
	float fboheight;
};
