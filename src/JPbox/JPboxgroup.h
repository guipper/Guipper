#pragma once


#include "defines.h"
#include "ofMain.h"
#include "jp_box_shader.h"
#include "jp_box.h"
#include "jp_box_image.h"
#include "jp_box_video.h"
#include "jp_box_cam.h"

#ifdef SPOUT
#include "jp_box_spout.h"
#endif
#include "jp_box_preset.h"
#include "jp_box_framedifference.h"

#ifdef NDI
#include "jp_box_ndi.h"
#endif

#include "../JPgui/jp_slider.h"
#include "../JPgui/jp_bang.h"
#include "../JPgui/jp_toogle.h"
#include "../JPgui/jp_complexslider.h"
// Esta clase como que va a manejar todos los shaderboxs y esas cosas:
#include "../JPutils/jp_constants.h"
#include "../JPutils/TransitionSR.h"
class JPboxgroup
{

public:
	JPboxgroup();
	~JPboxgroup();
	string test;

	void setup(ofTrueTypeFont &_font, int &_activerender);
	void draw();
	void draw_activerender(); // Dibuja el render activo. Esta es la <que corre en el ofApp.cpp
	void draw_activerender(float _width, float _height);
	void drawNodeEditorBackground(float _width, float _height);

	void update();
	void setActiveOnlyBox(int _val);
	void update_paramswindow();
	void update_resized(int w, int h);		   // Lo que hace cuando pinta resize
	void update_mouseDragged(int mousebutton); // Lo que hace cuando arrastras en la pantalla.
	void update_mousePressed(int mouseButton); // Lo que hace cada vez que haces click(ponele).
	void update_mouseReleased(int mouseButton);
	bool update_cueMousePressed(int mouseButton);
	bool update_cueMouseDragged(int mouseButton);
	bool update_cueMouseReleased(int mouseButton);
	bool mouseScrolled(int x, int y, float scrollX, float scrollY);

	void updateTransition(int _i);

	void save(string _diroutput);
	void load2(string _dirinput);
	// Guarda los valores a un XML
	void load(string _dirinput);

	void addBox(string directory, float _x, float _y);

	void addBox(string dir);
	void triggerCodeOnActiveShader();
	void deleteSelectedShader();

	// ACA ESTA TODO LO QUE TENGA QUE VER CON EL INSPECTOR PANEL DIGAMOS :
	// ESTO TE DICE QUE PANEL ESTA ABIERTO. SI EL PANEL QUE ESTA ABIERTO ES -1 ENTONCES EL PANEL NO ESTA ABIERTO

	/*ofColor CmouseOver;
	ofColor Cfront;
	ofColor Cback;
	ofColor Cactive;
	ofColor textcolor;
	*/
	ofFbo *getActiverender();
	int getActiverenderNum();
	void reloadActiveshader();
	void listenToOsc(string _dir, float _val);
	void setDurationGalleryMs(float _ms);
	float getDurationGalleryMs() const;
	vector<string> getBoxNames() const;
	bool hasBoxName(string boxName) const;
	bool toggleBypassForBox(string boxName);
	bool togglePauseForBox(string boxName);
	bool setBypassForBox(string boxName, bool value);
	bool setPauseForBox(string boxName, bool value);
	bool selectOpenBoxByName(string boxName);
	bool selectOpenBoxByIndex(int index);
	bool setCueFromSelected();
	bool setCueByIndex(int index);
	bool toggleCueByIndex(int index);
	void clearCue();
	bool applyCue();
	bool hasCue() const;
	JPbox *getInspectorBox();
	JPbox *getCuePreviewBox();
	bool setCueBoxByIndex(int index);
	bool setCueBoxByName(string boxName);
	bool toggleCueBoxByIndex(int index);
	bool hasCueBox() const;
	bool promoteCueToActive();
	bool requestCueApply();
	bool beginCueDraftForActiveShader();
	void clearCueDraft();
	bool applyCueDraftToSource();
	void setCuePanelLayout(float x, float y, float w, float h);
	void getCuePanelLayout(float &x, float &y, float &w, float &h) const;
	int getMaxParameterCount() const;
	bool setOpenBoxParameterAtIndex(int parameterIndex, float value);
	bool setLastBoxOnOff(bool value);

	bool mouseOverGui();
	ofVec2f screenToCanvas(const ofVec2f &screen) const;
	ofVec2f canvasToScreen(const ofVec2f &canvas) const;
	ofVec2f screenDeltaToCanvas(const ofVec2f &screenDelta) const;

	// void setVideoGrabberPointer(ofVideoGrabber &_ofvideograb);
	void clear();
	ofTexture *getActiveTexture();
	/***************GETTERS **************************/
	int getBoxesSize();

	bool draw_SelectionRect = false;
	ofVec2f lastMouseClick;
	float viewportZoom = 1.0f;
	ofVec2f viewportPan = ofVec2f(0, 0);
	bool viewportPanning = false;

	vector<JPcontroller *> controllers; // ESTE ARRAY ES DINAMICO , QUIERE DECIR QUE DEPENDE DE CUANDO CAMBIEN LOS COSOS
										// ESTO ES SOLO PARA QUE LERPEE LOS VALORES HACIA ESTO.
	vector<JPbox *> boxes;				// TODOS LOS SHADERRENDERS QUE TIENE EL OBJETO.

	int openguinumber = -1;
	int controllerselected; // ME INDICA QUE VARIABLE ESTA AGARRADA
	bool activeSequence; //SECUENCIA ACTIVA
private:
	enum CueMode
	{
		CUE_NONE,
		CUE_NORMAL_PREVIEW,
		CUE_DRAFT_CHAIN
	};
	enum CueMonitorMode
	{
		CUE_MONITOR_FINAL_OUTPUT,
		CUE_MONITOR_SELECTED_BOX
	};

	struct CueState
	{
		CueMode mode = CUE_NONE;
		int sourceIndex = -1;
		int previewIndex = -1;
		int draftInspectorRealIndex = -1;
		vector<JPbox *> draftBoxes;
		vector<int> draftRealIndices;
		vector<int> dirtyDraftRealIndices;
		JPbox_shader *draftSourceBox = nullptr;
		JPbox *draftOutputBox = nullptr;
		int draftOutputRealIndex = -1;
	};

	vector<JPTooglelist *> botones_modo;
	vector<JPToogle *> botones_speed;
	vector<JPSlider *> sliders_speed;

	int *activerender;

	void draw_cursorrect();
	void setControllers();
	void setupShaderRendersFromDataFolder(); // Esta es para que levante todos
	int findBoxIndexByName(string boxName) const;
	JPbox *findBoxByName(string boxName) const;

	ofTrueTypeFont *font_p;

	void setinspectorsetactiveparams();
	void draw_paramswindow(); // Dibuja la ventanita del inspector.
	void drawCuePreview();
	void drawLiveOutput(float x, float y, float w, float h);
	JPbox_shader *getCueDraftSourceBox();
	JPbox *getCueDraftBoxForRealIndex(int index) const;
	JPbox *getEditableBoxForRealIndex(int index);
	bool beginCueDraftForBoxIndex(int index);
	bool buildCueDraftGraph(int sourceIndex);
	bool collectCueDraftPath(int currentIndex, int activeIndex, vector<int> &path, vector<bool> &visiting);
	JPbox *cloneBoxForCueDraft(int index);
	int findCueDraftCloneIndexForRealIndex(int index) const;
	bool isCueSourceIndex(int index) const;
	bool isCueDraftRealIndex(int index) const;
	bool isRealIndexDraftEditable(int index) const;
	bool isCueDraftDirty(int index) const;
	bool isCueDraftMode() const;
	bool isCueNormalPreviewMode() const;
	void markCueDraftDirty(int index);
	void processPendingCueApply();
	void requestCueRebuild();
	void processPendingCueRebuild();
	bool rebuildCueAfterGraphChange();
	void rewireCueDraftGraph();
	void updateCueDraftGraph();
	void updateRealBoxesForCueApply();
	void copyParametersByNameOrIndex(JPParameterGroup &destination, JPParameterGroup &source);
	void setupGalleryDurationSlider();
	void drawGalleryDurationSlider();
	void setupDefaultCuePanelLayout();
	void clampCuePanelLayout();
	bool mouseOverCueHeader() const;
	bool mouseOverCueResizeHandle() const;
	bool mouseOverCueCloseIcon() const;
	bool mouseOverCueFullscreenIcon() const;
	bool mouseOverCueApplyIcon() const;
	bool mouseOverCueMonitorModeIcon() const;
	void clearSelection();
	void updateBoxSelection();
	void zoomViewport(const ofVec2f &screenAnchor, float zoomFactor);
	void panViewport(const ofVec2f &screenDelta);
	bool boxIntersectsSelection(JPbox *box) const;
	bool isBoxSelected(int index) const;
	bool deleteBoxAtIndex(int index);
	bool deleteSelectedBoxes();

	float inspectorwindow_width;
	float inspectorwindow_height;
	float inspectorwindow_x;
	float inspectorwindow_y;
	float inspectorwindow_sepy; // Esta es para el espacio que hay entre distintos sliders.

	JPBang inspectorsetactive;			 // ESTE BANG ES PARA SETEAR QUE EL QUE ESTA ABIERTO EN EL INSPECTOR PONGA COMO ACTIVE EN EL RENDER DE SALIDA
	JPBang inspectorreload;				 // ESTE BANG ES PARA SETEAR QUE EL QUE ESTA ABIERTO EN EL INSPECTOR PONGA COMO ACTIVE EN EL RENDER DE SALIDA
	float inspectorwindow_setactivesize; // Para el size del setactive:

	void draw_conections();

	// ofFbo boxesdrawing;

	float offsetx;
	float offsety;

	// FOR RANDOMISIN THE VALUES :
	int randomcnt = 0; // si supera este numero 5 veces los parametros se randomizan.

	// COSAS DE AGARRE
	bool shaderboxagarrado;
	bool ouletagarrado;
	int cualestaagarrado = -1;
	int outlet_cualestaagarrado = -1;
	CueState cueState;
	float cuePanelX = 24.0f;
	float cuePanelY = 360.0f;
	float cuePanelW = 420.0f;
	float cuePanelH = 270.0f;
	bool cueFullscreenPreview = false;
	CueMonitorMode cueMonitorMode = CUE_MONITOR_FINAL_OUTPUT;
	bool cuePanelDragging = false;
	bool cuePanelResizing = false;
	bool cuePanelApplyArmed = false;
	bool pendingCueApply = false;
	bool pendingCueRebuild = false;
	ofFbo cueApplySnapshotFbo;
	ofVec2f cuePanelDragStartMouse;
	ofVec2f cuePanelDragStartPos;
	ofVec2f cuePanelResizeStartSize;
	ofVec2f selectionEnd;
	vector<int> selectedBoxIndices;

	// Vamos a ver si podemos emular un doble click.
	bool isDoubleClick;
	float lasttime_mouseclick;
	float duration_mouseclick;
	

	//PARA LO DEL SHADER 
	TransitionSR transition;

	//MODO SECUENCIA 

	float	lasttime_sequence;
	float durationGalleryMs;
	//Transition shader render : 
	JPParameter galleryDurationParam;
	JPSlider galleryDurationSlider;
};
