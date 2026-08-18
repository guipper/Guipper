#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "defines.h"
#include "ofMain.h"
#include "jp_box_shader.h"
#include "jp_box.h"
#include "jp_box_image.h"
#include "jp_box_video.h"
#include "jp_box_cam.h"
#include "jp_box_camdepth.h"
#include "jp_box_kinect2.h"
#include "jp_box_pointercloud.h"

class JPShaderEditor; // forward declaration

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
#include "jp_media.h"
#include "../JPgui/jp_exposebutton.h"
// Esta clase como que va a manejar todos los shaderboxs y esas cosas:
#include "../JPutils/jp_constants.h"
#include "../JPutils/TransitionSR.h"
class JPboxgroup
{

public:
	struct InspectorLayoutMetrics
	{
		float panelWidth = 450.0f;
		float outerInset = 10.0f;
		float contentPadding = 14.0f;
		float columnGap = 8.0f;
		float rowGap = 10.0f;
		float minControlHeight = 24.0f;
		float headerHeight = 56.0f;
		float titleFontSize = 18.0f;
		float bodyFontSize = 12.0f;
		float secondaryFontSize = 11.0f;
	};
	// How many parameter slots the MIDI keymap exposes per box. Bindings are
	// stored by index, so this has to stay fixed: growing it with the largest
	// box on screen silently remapped saved bindings.
	static constexpr int maxBindableParameters = 16;

	JPboxgroup();
	~JPboxgroup();
	string test;

	void setup(ofTrueTypeFont &_font, int &_activerender);
	void draw();
	void draw_activerender(); // Dibuja el render activo. Esta es la <que corre en el ofApp.cpp
	void draw_activerender(float _width, float _height);
	// normCrop is the normalized sub-rectangle of the source to show, and
	// bezelCanvasPx is inset from it on all four sides. Defaults are the whole
	// frame with no inset, i.e. the pre-wall behaviour. outEffectiveNorm
	// receives the rect actually sampled after the inset and clamping - the
	// mapping overlay needs that, not the raw request.
	bool drawLiveOutputSource(bool followMainActive,
		const string &sourceBoxUid, float _width, float _height,
		const ofRectangle &normCrop = ofRectangle(0.0f, 0.0f, 1.0f, 1.0f),
		float bezelCanvasPx = 0.0f,
		ofRectangle *outEffectiveNorm = nullptr);
	// Real pixel size of the texture a live output samples. Used to convert a
	// normalized crop into pixels; nothing reallocates render FBOs at runtime,
	// so these can differ from jp_constants::renderWidth/Height.
	ofVec2f getMasterCanvasSize() const;
	void drawMappingOverlay(float _x, float _y, float _width, float _height);
	void drawMappingOverlayForSource(bool followMainActive,
		const string &sourceBoxUid, float _x, float _y,
		float _width, float _height);
	void drawNodeEditorBackground(float _width, float _height);
	bool isMappingEditActive() const;
	bool toggleMappingEdit();
	bool toggleMappingGuides();
	bool toggleMappingGrid();
	void endMappingEdit();

	void update();
	void setActiveOnlyBox(int _val);
	void update_paramswindow();
	void update_resized(int w, int h);		   // Lo que hace cuando pinta resize
	void update_mouseDragged(int mousebutton); // Lo que hace cuando arrastras en la pantalla.
	void update_mousePressed(int mouseButton); // Lo que hace cada vez que haces click(ponele).
	void update_mouseReleased(int mouseButton);
	bool update_mappingMousePressed(int mouseButton);
	bool update_mappingMouseDragged(int mouseButton);
	bool update_mappingMouseReleased(int mouseButton);
	bool update_cueMousePressed(int mouseButton);
	bool update_cueMouseDragged(int mouseButton);
	bool update_cueMouseReleased(int mouseButton);
	bool mouseScrolled(int x, int y, float scrollX, float scrollY);

	void updateTransition(int _i);
	bool requestSetActiveRender(int index, bool activeOnly = false);
	int getCurrentViewBoxCount() const;
	int getCurrentViewSelectedIndex() const;
	int getCurrentViewActiveRenderIndex() const;
	bool selectOpenBoxForCurrentView(int index);
	bool requestSetActiveRenderForCurrentView(int index, bool activeOnly = false);

	void save(string _diroutput);
	void load2(string _dirinput);
	// Guarda los valores a un XML
	void load(string _dirinput);

	void addBox(string directory, float _x, float _y);

	void addBox(string dir);
	void triggerCodeOnActiveShader();
	void deleteSelectedShader();
	void keyPressed(int key); // Inline tab renaming and the media IN/OUT fields
	// True while a text surface owned by the boxgroup is accepting keystrokes.
	// ofApp must consult this before acting on any key of its own: the media
	// time fields take BACKSPACE and digits, which would otherwise fall through
	// to the global shortcuts and delete the selected box mid-edit.
	bool wantsKeyCapture() const;
	bool handleInspectorRangeShortcut(int key);
	bool handleMediaInspectorClick();
	void commitTabRename();
	void cancelTabRename();

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
	// The MAIN crossfade. Duration is milliseconds for a full 0..1 fade; type
	// selects which shader blends the two frames. Both are global settings, so
	// the boxgroup only forwards them to its TransitionSR.
	void setTransitionDurationMs(float _ms);
	float getTransitionDurationMs() const;
	void setTransitionType(int _type);
	int getTransitionType() const;
	vector<string> getBoxNames() const;
	bool hasBoxName(string boxName) const;

	// A box the user has marked "TO OUTPUT", addressed by uid rather than name
	// so a rename cannot break a live output bound to it.
	struct OutputCandidate
	{
		string uid;
		// Path-qualified for display, e.g. "gusanos / mirrorquad".
		string label;
	};
	// Marked boxes anywhere in the composition, group children included.
	vector<OutputCandidate> getOutputCandidates() const;
	// These walk the whole tree, unlike their name-based counterparts which are
	// top-level-only and have callers that rely on that.
	JPbox *findBoxByUid(const string &boxUid) const;
	bool hasBoxUid(const string &boxUid) const;
	ofVec2f getBoxFboSizeByUid(const string &boxUid) const;
	// Re-mint any uid that is empty or duplicated, shallowest box wins. Call
	// after load and after paste, where a shared group file can arrive carrying
	// identities that already exist in this composition.
	void repairBoxUids();
	// Top-level lookup by name. Public only so the live-output config can heal a
	// legacy name-based binding into a uid once, on first resolve.
	JPbox *findTopLevelBoxByName(const string &boxName) const;
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
	// The box index to cue for the graph currently on screen: the selected box
	// (groupInspectorIndex in group view, openguinumber in main), falling back
	// to that graph's active render when nothing is selected.
	int getCueEntryIndexForCurrentView() const;
	bool hasCueBox() const;
	bool promoteCueToActive();
	bool requestCueApply();
	bool beginCueDraftForActiveShader();
	void clearCueDraft();
	bool applyCueDraftToSource();
	void setCuePanelLayout(float x, float y, float w, float h);
	void getCuePanelLayout(float &x, float &y, float &w, float &h) const;
	// Surface-stack integration. The inspector, the cue panel and the mapping
	// panel are surfaces like any other, so each needs a rect to block clicks
	// with and the inspector needs a way to be dismissed from outside.
	bool isMappingShaderBox(JPbox *box) const;
	void closeInspector();
	ofRectangle getInspectorBounds() const;
	ofRectangle getInspectorHeaderBounds() const { return inspectorHeaderBounds; }
	ofRectangle getInspectorBodyViewport() const { return inspectorBodyViewport; }
	const InspectorLayoutMetrics &getInspectorLayoutMetrics() const
		{ return inspectorLayout; }
	float getInspectorContentHeight() const { return inspectorContentHeight; }
	float getInspectorScrollY() const { return inspectorScrollY; }
	float getInspectorMaxScrollY() const { return inspectorMaxScrollY; }
	void setInspectorScrollNormalized(float normalized);
	// Set by ofApp so the canvas also yields to surfaces this class does not
	// own - the MIDI panel, the shader editor, the save modal, dropdowns.
	void setExternalGuiHitTest(std::function<bool(float, float)> fn);
	void setExternalTextCaptureTest(std::function<bool()> fn);

	// --- Multi-select, the way every design program does it ----------------
	//
	// SHIFT while dragging a marquee ADDS to the selection instead of replacing
	// it. CTRL clicking a box toggles that one box and leaves the rest alone.
	// Named predicates rather than inline ofGetKeyPressed calls because the main
	// view and the group view both ask, and a mismatch between them would be
	// invisible until someone used whichever one was wrong.
	void setControllers();
	void clearSelection();
	bool selectionAddModifier() const;      // shift
	bool selectionToggleModifier() const;   // ctrl
	void toggleBoxSelection(int index);
	bool isBoxSelected(int index) const;
	const vector<int> &getSelectedBoxIndices() const { return selectedBoxIndices; }

	// Union of what a shift-drag kept and what the marquee is touching now.
	// Order-stable and DUPLICATE-FREE: the multi-drag walks this list and moves
	// each entry, so a box listed twice travels at double speed and slides out
	// of the group. Pure, so that invariant can be tested directly.
	static void mergeSelection(vector<int> &target, const vector<int> &base,
							   const vector<int> &marqueeHits);

	// The suppression half of the space-pan rule, without reading the keyboard.
	// Split out so it can be tested: whether space is physically down is trivial,
	// but "is a text field eating the keyboard right now" is where this goes
	// wrong, and panning the canvas while someone types a filename is the bug.
	bool spacePanAllowed() const;
	ofRectangle getCuePanelBounds();
	ofRectangle getMappingPanelBounds() const;
	void setMappingPanelLayout(float x, float y, float w, float h);
	void getMappingPanelLayout(float &x, float &y, float &w, float &h) const;
	// CUE target helpers (main boxes or preset boxes depending on context)
	vector<JPbox *>& getCueTargetBoxes();
	int getCueTargetBoxSize() const;
	JPbox *getCueTargetBoxAt(int index) const;
	int &getCueTargetActiveRender();
	// True when the active cue belongs to the graph currently on screen
	// (main graph in main view, or the active preset in group view).
	bool cueTargetsCurrentView() const;
	// Real index (within the current graph) of the selected/inspected box.
	int cueSelectedIndex() const;
	// Name lookup scoped to the cue's target graph (preset-aware findBoxIndexByName).
	int findCueTargetBoxIndexByName(const string &boxName) const;
	int getMaxParameterCount() const;
	int getOpenParameterCount() const;
	JPParameter *getOpenParameterAtIndex(int parameterIndex) const;
	// Maps a bind slot (what the MIDI panel calls "parameter N", counting from
	// the top of the inspector) onto an index into the box's parameter array.
	//
	// These are not the same thing for media boxes. The inspector deliberately
	// lays "scale ratio" out first, ahead of scalex, but the parameter array
	// keeps it last so saved compositions stay readable in order. Binding
	// straight to the array index therefore pointed knob 1 at the SECOND row on
	// screen. Slots now follow what you see.
	//
	// Order is: visible rows top to bottom, then any parameter the inspector
	// hides behind the transport card (speed/position/play/strech). Appending
	// rather than dropping them keeps those bindable instead of silently
	// removing four slots.
	vector<int> getBindableParameterOrder(JPbox *box) const;
	int resolveBindableParameterIndex(JPbox *box, int slot) const;
	bool setOpenBoxParameterAtIndex(int parameterIndex, float value);
	// Drive one named box directly. Used as the fallback when no inspector is
	// open, so a parameter bind still moves the box it was made against.
	bool setBoxParameterAtIndex(string boxName, int parameterIndex, float value);
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

	// Tab system - contextual navigation
	// activeTab: 0 = MAIN, 1+ = direct child presets of current context
	// activeGroupPath: empty = MAIN, [i] = preset at boxes[i], [i,j] = nested preset
	int activeTab = 0;
	vector<int> activeGroupPath;
	float tabBarOffsetY = 40; // Offset for screen-level tabs above the boxgroup tabs (clearance below the top screen-tab bar)
	void drawTabs();
	int getTabAtScreenPos(int screenX, int screenY) const;
	bool handleTabClick();

	// Inline tab rename state
	bool tabRenaming = false;
	int tabRenameTabIndex = -1; // Which tab (0-based across all tabs) is being renamed
	string tabRenameBuffer;
	int tabRenameCursor = 0;

	// Get indices of direct child presets in the current context (MAIN or active preset)
	vector<int> getDirectChildPresetIndices() const;

	// Navigate into a child preset tab
	bool navigateToChildPreset(int childIndex);

	// Navigate back to a breadcrumb level (0 = MAIN, 1 = first level, etc.)
	bool navigateToBreadcrumbLevel(int level);

	JPbox_preset *getActivePreset() const;
	bool isGroupViewActive() const { return !activeGroupPath.empty(); }
	void ensureTabStateSize();

	// Max recursion depth for preset traversal (safety against infinite loops)
	static const int MAX_PRESET_DEPTH = 128;
	int groupPreviewBoxIndex = -1; // sub-box index for double-click preview in group view
	int groupInspectorIndex = -1; // sub-box index for inspector in group view (separate from openguinumber)

	// Boxes added INTO a group while a cue was active. They live in the preset's
	// boxes immediately, but are staged: removed on cancel, kept on apply. (Boxes
	// added to the MAIN graph use cueState.cueAddedRealIndices instead.)
	vector<std::pair<JPbox_preset *, JPbox *>> cueAddedGroupBoxes;

	// Per-tab zoom/pan state (index 0 = main, 1+ = preset tabs)
	vector<float> tabZooms;
	vector<ofVec2f> tabPans;

	vector<JPcontroller *> controllers; // ESTE ARRAY ES DINAMICO , QUIERE DECIR QUE DEPENDE DE CUANDO CAMBIEN LOS COSOS
										// ESTO ES SOLO PARA QUE LERPEE LOS VALORES HACIA ESTO.
	JPComplexSlider *audioShapingDragSlider = nullptr;
	int audioShapingDragControl = JPComplexSlider::AUDIO_SHAPING_NONE;
	JPComplexSlider *rangeDragSlider = nullptr;
	int rangeDragHandle = 0;
	vector<JPExposeButton *> exposeButtons; // Expose buttons for group view inspector
	// One per colour triple on the inspected box: a swatch showing what its
	// three 0..1 channels add up to. Built in setControllers beside the rows
	// and painted in the same clipped loop, like the lock and range buttons.
	//
	// Holds POINTERS to the three parameters, not indices: setControllers
	// reorders rows under usesCanonicalOrder, so controllers[i] does not
	// correspond to parameter i and an index would drift.
	struct InspectorColorSwatch
	{
		ofRectangle bounds;
		JPParameter *r = nullptr;
		JPParameter *g = nullptr;
		JPParameter *b = nullptr;
	};
	vector<InspectorColorSwatch> inspectorColorSwatches;

	vector<ofRectangle> parameterLockButtons;
	vector<ofRectangle> parameterRangeButtons;
	vector<JPbox *> boxes;				// TODOS LOS SHADERRENDERS QUE TIENE EL OBJETO.

	int openguinumber = -1;
	int controllerselected; // ME INDICA QUE VARIABLE ESTA AGARRADA
	bool activeSequence; //SECUENCIA ACTIVA
	struct ProfileEntry
	{
		string name;
		float averageMs = 0.0f;
		float peakMs = 0.0f;
	};
	struct ProfileSnapshot
	{
		float parametersMs = 0.0f;
		float groupViewMs = 0.0f;
		float mainGraphMs = 0.0f;
		float cueDraftMs = 0.0f;
		float transitionMs = 0.0f;
		vector<ProfileEntry> boxes;
	};
	void setProfilingEnabled(bool enabled) { profilingEnabled = enabled; }
	const ProfileSnapshot &getProfileSnapshot() const { return profileSnapshot; }
	void setRequiredRenderSources(const vector<string> &names)
	{
		requiredRenderSources = names;
	}
	void groupSelectedBoxes();

	// Clipboard - copy/paste across main and group views
	string clipboardXml;
	void copySelectedBoxes();
	void pasteBoxes();

	// Shader editor pointer (set by ofApp)
	JPShaderEditor* shaderEditor = nullptr;
private:
	bool profilingEnabled = false;
	ProfileSnapshot profileSnapshot;
	void recordProfileValue(float &average, float &peak, float sampleMs);
	vector<string> requiredRenderSources;
	void scheduleTopLevelRenders();
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
	enum CueDirtyFlag
	{
		CUE_DIRTY_NONE = 0,
		CUE_DIRTY_PARAMS = 1 << 0,
		CUE_DIRTY_BYPASS_PAUSE = 1 << 1,
		CUE_DIRTY_LINKS = 1 << 2,
		CUE_DIRTY_ADDED = 1 << 3,
		CUE_DIRTY_DELETED = 1 << 4,
		CUE_DIRTY_STAGED_ACTIVE = 1 << 5,
		CUE_DIRTY_PRESET_ACTIVE = 1 << 6
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
		vector<unsigned int> draftDirtyFlags;
		vector<int> cueAddedRealIndices;
		vector<JPParameterGroup> draftBaselineParameters;
		vector<bool> draftBaselineOnOff;
		vector<bool> draftBaselineBypass;
		JPbox *draftSourceBox = nullptr;
		JPbox *draftOutputBox = nullptr;
		int draftOutputRealIndex = -1;
		int stagedActiveRenderIndex = -1;
		JPbox_preset *targetPreset = nullptr; // null = main boxes, non-null = preset boxes in group view
	};

	vector<JPTooglelist *> botones_modo;
	vector<JPToogle *> botones_speed;
	vector<JPSlider *> sliders_speed;

	int *activerender;

	void draw_cursorrect();
	vector<JPParameter *> getInspectorActionParameters() const;
	void rebuildControllersIfLayoutStale();
	void setupShaderRendersFromDataFolder(); // Esta es para que levante todos
	int findBoxIndexByName(string boxName) const;
	JPbox *findBoxByName(string boxName) const;

	ofTrueTypeFont *font_p;

	void setinspectorsetactiveparams();
	void draw_paramswindow(); // Dibuja la ventanita del inspector.
	struct InspectorInputRow
	{
		int linkIndex = -1;
		ofRectangle bounds;
		ofRectangle upButton;
		ofRectangle exposeButton;
		ofRectangle unlinkButton;
	};
	float layoutInspectorInputRows(JPbox *box, float startY);
	void drawInspectorInputRows(JPbox *box);
	bool handleInspectorInputClick(JPbox *box);
	bool handleInspectorAutomationClick();
	bool handleInspectorLockClick();
	bool handleInspectorRangeClick();
	bool moveInspectorInputUp(JPbox *box, int linkIndex);
	bool unlinkInspectorInput(JPbox *box, int linkIndex);
	class JPbox_preset *getInspectorInputOwnerPreset() const;
	bool isInspectorTextureInputExposed(
		JPbox *box, int linkIndex) const;
	bool toggleInspectorTextureInputExposure(
		JPbox *box, int linkIndex);
	void drawCuePreview();
	// srcNorm is the normalized part of each texture to sample. It defaults to
	// the whole frame: this is also the main render screen, the node editor
	// background and the cue preview, and only the live output path crops.
	void drawLiveOutput(float x, float y, float w, float h,
		const ofRectangle &srcNorm = ofRectangle(0.0f, 0.0f, 1.0f, 1.0f));
	JPbox *getCueDraftSourceBox();
	JPbox *getCueDraftBoxForRealIndex(int index) const;
	// Draft counterpart of a node currently visible in MAIN or a nested group.
	JPbox *getCueDraftBoxForCurrentViewIndex(int index) const;
	JPbox *getEditableBoxForRealIndex(int index);
	bool beginCueDraftForBoxIndex(int index);
	bool buildCueDraftGraph(int sourceIndex);
	bool collectCueDraftPath(int currentIndex, int activeIndex, vector<int> &path, vector<bool> &visiting);
	JPbox *cloneBoxForCueDraft(int index);
	JPbox *createBoxForDirectory(const string &directory, string &name) const;
	string makeNameFromDirectory(const string &directory) const;
	string makeUniqueBoxName(const string &baseName) const;
	string makeUniqueBoxName(const string &baseName, const vector<JPbox *> &checkBoxes) const;
	vector<JPbox *> *getCurrentViewBoxes();
	int *getCurrentViewActiveRenderPointer();
	string makeNextGroupName(const vector<JPbox *> &siblings) const;
	int findCueDraftCloneIndexForRealIndex(int index) const;
	bool isCueSourceIndex(int index) const;
	bool isCueDraftRealIndex(int index) const;
	bool isCueAddedRealIndex(int index) const;
	bool isCueDeletedRealIndex(int index) const;
	bool isRealIndexDraftEditable(int index) const;
	bool isCueDraftDirty(int index) const;
	unsigned int getCueDraftDirtyFlags(int index) const;
	bool isCueDraftMode() const;
	bool isCueNormalPreviewMode() const;
	bool setCueStagedActiveRenderIndex(int index);
	void markCueDraftDirty(int index, unsigned int flags = CUE_DIRTY_PARAMS);
	void removeCueDraftDirty(int index, unsigned int flags = 0);
	void addCueAddedRealIndex(int index);
	bool revertCueDraftBox(int index);
	void removeCueAddedBoxesFromRealGraph();
	// Remove group-internal boxes added during a cue (cancel path). Deletes each
	// from its preset and fixes activeRender/selection.
	void removeCueAddedGroupBoxes();
	bool commitCueDraftLink(int targetRealIndex, int linkIndex, int sourceRealIndex);
	void copyCueDraftLinksToReal(int realIndex);
	vector<int> getCueDirtyIndices(unsigned int mask = 0) const;
	string getCueDirtySummary() const;
	void copyEditableBoxState(JPbox *destination, JPbox *source);
	struct PresetLinkAssignment
	{
		vector<string> presetPath;
		string boxName;
		string samplerName;
		string sourceName;
		bool connected = false;
	};
	void copyBoxLinksByName(JPbox *destination, JPbox *source,
		const vector<JPbox *> &destinationSiblings);
	void snapshotPresetLinks(class JPbox_preset *preset,
		vector<PresetLinkAssignment> &assignments,
		const vector<string> &presetPath = {}) const;
	void restorePresetLinks(class JPbox_preset *preset,
		const vector<PresetLinkAssignment> &assignments);
	// Recursively copy a preset's internal sub-box param/onoff/bypass/active state
	// (used to seed a preset draft clone from the live box, and to write staged
	// preset edits back on apply). Only mirrors editable state, not structure.
	void copyPresetInternalState(class JPbox_preset *destination, class JPbox_preset *source);
	bool synchronizeCuePresetStructure(
		class JPbox_preset *destination,
		class JPbox_preset *source);
	void snapshotPresetActiveRenders(class JPbox_preset *source, vector<int> &values) const;
	void restorePresetActiveRenders(class JPbox_preset *destination, const vector<int> &values, int &valueIndex) const;
	class JPbox_preset *getDraftPresetForCurrentView() const;
	void setPresetActiveOnlyBox(class JPbox_preset *preset, int index);
	// The draft-tree box the inspector currently edits (navigates the global cue
	// draft by activeGroupPath + the selected sub-box). Null when not cueing.
	JPbox *getDraftBoxForCurrentInspector();
	// Render a dirty preset draft: re-render its shader sub-boxes (so staged edits
	// preview) but mirror the LIVE output for source boxes (camera/video/spout/ndi/
	// image) instead of re-opening their devices, recursing into nested presets.
	void renderPresetDraftMirroringLive(class JPbox_preset *draftPreset, class JPbox_preset *livePreset);
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
	void updateBoxSelection();

	void zoomViewport(const ofVec2f &screenAnchor, float zoomFactor);
	void panViewport(const ofVec2f &screenDelta);
	bool boxIntersectsSelection(JPbox *box) const;
	bool deleteBoxAtIndex(int index);
	bool deleteSelectedBoxes();

	std::function<bool(float, float)> externalGuiHitTest;

	// Set by ofApp: "a text field owns the keyboard right now". The canvas needs
	// it because SPACE is a pan gesture here but a printable character in a
	// field, and JPboxgroup only knows about its own two fields
	// (wantsKeyCapture) - the options, live-output, split, save-modal and shader
	// search fields all live in ofApp.
	std::function<bool()> externalTextCaptureTest;

	// True when space+drag should pan instead of doing whatever that button
	// normally does. Read only when a drag is ARMED, never mid-drag - see the
	// comment at the arm site.
	bool isSpacePanHeld() const;

	float inspectorwindow_width;
	float inspectorwindow_height;
	float inspectorwindow_x;
	float inspectorwindow_y;
	float inspectorwindow_sepy; // Esta es para el espacio que hay entre distintos sliders.
	InspectorLayoutMetrics inspectorLayout;
	float inspectorScrollY = 0.0f;
	float inspectorMaxScrollY = 0.0f;
	float inspectorContentHeight = 0.0f;
	ofRectangle inspectorHeaderBounds;
	ofRectangle inspectorBodyViewport;
	ofRectangle inspectorScrollbarTrack;
	ofRectangle inspectorScrollbarThumb;
	bool inspectorScrollbarDragging = false;
	float inspectorScrollbarDragOffset = 0.0f;
	JPbox *inspectorScrollOwner = nullptr;
	bool inspectorRelayoutForClamp = false;
	ofRectangle inspectorInputsHeaderBounds;
	bool inspectorInputsExpanded = true;
	vector<InspectorInputRow> inspectorInputRows;
	struct MediaInspectorLayout
	{
		ofRectangle card, fit, restart, previous, play, next, direction,
			loop, speed, timeline, inButton, inField, outButton, outField,
			mute, volume;
		void clear() { *this = MediaInspectorLayout(); }
	} mediaInspector;
	bool mediaInspectorPlayableBuilt = false;
	int mediaTimeFieldFocus = 0;
	string mediaTimeFieldBuffer;
	bool mediaTimeFieldReplaceOnType = false;
	bool mediaTimelineDragging = false;
	int mediaRangeDragging = 0;
	void layoutMediaInspector(JPMediaInspectable *media, float &cursorY);
	void drawMediaInspector(JPMediaInspectable *media);
	struct InspectorParameterGroupHeader
	{
		int layerIndex = -1;
		ofRectangle bounds;
	};
	vector<InspectorParameterGroupHeader>
		advancedMappingParameterHeaders;
	bool inspectorBodyContains(float x, float y) const;
	bool handleInspectorScrollbarPressed(float x, float y);

	JPBang inspectorsetactive;			 // ESTE BANG ES PARA SETEAR QUE EL QUE ESTA ABIERTO EN EL INSPECTOR PONGA COMO ACTIVE EN EL RENDER DE SALIDA
	JPBang inspectorreload;				 // ESTE BANG ES PARA SETEAR QUE EL QUE ESTA ABIERTO EN EL INSPECTOR PONGA COMO ACTIVE EN EL RENDER DE SALIDA
	JPBang inspectorrandom;              // Randomiza todos los parametros del shader en el inspector
	JPBang inspectordefault;
	JPBang inspectorsavedefault;
	JPBang editbutton;                   // Boton EDIT para abrir el shader en el editor de codigo
	JPBang mappingbutton;                // Enters the corner-pin mapping editor
	JPBang camerarefreshbutton;          // Re-enumerates camera capture devices
	JPBang tooutputbutton;               // Marks the box selectable as a live output source
	std::array<ofRectangle, 3> kinectStreamButtons;
	float inspectorwindow_setactivesize; // Para el size del setactive:

	void draw_conections();
	struct MappingParameterIndices
	{
		int topLeftX = -1;
		int topLeftY = -1;
		int topRightX = -1;
		int topRightY = -1;
		int bottomRightX = -1;
		int bottomRightY = -1;
		int bottomLeftX = -1;
		int bottomLeftY = -1;
		int feather = -1;
		bool valid() const;
	};
	struct MappingQuad
	{
		ofVec2f topLeft;
		ofVec2f topRight;
		ofVec2f bottomRight;
		ofVec2f bottomLeft;
	};
	MappingParameterIndices getMappingParameterIndices(JPbox *box) const;
	bool isAdvancedMappingShaderBox(JPbox *box) const;
	bool mappingTargetMatchesCurrentView() const;
	JPbox *getMappingEditBox();
	JPbox_shader *getAdvancedMappingEditBox();
	MappingQuad getMappingQuad(JPbox *box) const;
	bool isValidMappingQuad(const MappingQuad &quad) const;
	ofVec2f projectMappingPoint(const MappingQuad &quad, const ofVec2f &uv) const;
	ofRectangle getMappingPreviewRect(float width, float height) const;
	ofRectangle getMappingPanelPreviewRect() const;
	void drawMappingGrid(const MappingQuad &quad, float x, float y,
		float width, float height);
	void drawMappingHandles(const MappingQuad &quad, float x, float y,
		float width, float height, bool visible);
	void drawMappingPanel();
	void setupDefaultMappingPanelLayout();
	void clampMappingPanelLayout();
	enum MappingPanelAction
	{
		MAPPING_PANEL_GUIDES = 0,
		MAPPING_PANEL_GRID,
		MAPPING_PANEL_RENDER_GUIDES,
		MAPPING_PANEL_CLOSE
	};
	ofRectangle getMappingPanelActionBounds(
		MappingPanelAction action) const;
	bool mouseOverMappingPanel() const;
	bool mouseOverMappingPanelHeader() const;
	bool mouseOverMappingPanelResizeHandle() const;
	bool mouseOverMappingPanelCloseIcon() const;
	bool toggleMappingRenderGuides();
	bool updateMappingCorner(int corner, float x, float y, float width, float height);
	void markMappingParameterChanged();
	int getAdvancedMappingParameterLayer(const string &name) const;
	void drawAdvancedMappingParameterHeaders(JPbox *box);
	bool handleAdvancedMappingParameterHeaderClick(JPbox *box);

	enum AdvancedMappingTool
	{
		ADVANCED_MAPPING_PEN = 0,
		ADVANCED_MAPPING_MESH,
		ADVANCED_MAPPING_MOVE
	};
	// Which shape the move tool is acting on. Persists across drags so the
	// bounding box stays on screen between a move and a resize.
	enum AdvancedMappingMoveTarget
	{
		ADVANCED_MAPPING_TARGET_SURFACE = 0,
		ADVANCED_MAPPING_TARGET_MASK
	};
	enum AdvancedMappingDragKind
	{
		ADVANCED_MAPPING_DRAG_NONE = 0,
		ADVANCED_MAPPING_DRAG_MASK_ANCHOR,
		ADVANCED_MAPPING_DRAG_MASK_IN,
		ADVANCED_MAPPING_DRAG_MASK_OUT,
		ADVANCED_MAPPING_DRAG_SURFACE_CORNER,
		ADVANCED_MAPPING_DRAG_SURFACE_HANDLE,
		ADVANCED_MAPPING_DRAG_MOVE_SHAPE,
		ADVANCED_MAPPING_DRAG_SCALE_SHAPE,
		ADVANCED_MAPPING_DRAG_ROTATE_SHAPES,
		ADVANCED_MAPPING_DRAG_MASK_MARQUEE
	};
	// Toolbar order is the grouping the user sees: pick a layer, pick a tool,
	// shape what the tool selected, reference image, file in/out. Layers must
	// stay first and contiguous - the click and draw code identifies them by
	// index alone.
	enum AdvancedMappingToolbarAction
	{
		ADVANCED_MAPPING_LAYER_1 = 0,
		ADVANCED_MAPPING_LAYER_2,
		ADVANCED_MAPPING_LAYER_3,
		ADVANCED_MAPPING_LAYER_4,
		ADVANCED_MAPPING_TOOL_MESH,
		ADVANCED_MAPPING_TOOL_PEN,
		ADVANCED_MAPPING_TOOL_MOVE,
		ADVANCED_MAPPING_BEZIER,
		ADVANCED_MAPPING_SMOOTH,
		ADVANCED_MAPPING_FIT,
		ADVANCED_MAPPING_GUIDE,
		ADVANCED_MAPPING_SVG_IMPORT,
		ADVANCED_MAPPING_SVG_EXPORT,
		ADVANCED_MAPPING_TOOLBAR_COUNT
	};
	ofRectangle getAdvancedMappingToolbarBounds(
		AdvancedMappingToolbarAction action) const;
	ofRectangle getAdvancedMappingHeaderActionBounds(bool newShape) const;
	bool advancedMappingBezierActive(
		const JPbox_shader::AdvancedMappingLayer &layer) const;
	// Bounding box of the move tool's current target, in normalized space, and
	// its four corner handles in screen space. Returns false when the target
	// has no usable outline (an empty or two node mask).
	bool getAdvancedMappingMoveBox(
		const JPbox_shader::AdvancedMappingLayer &layer,
		ofRectangle &box) const;
	ofVec2f getAdvancedMappingRotationHandle(
		const ofRectangle &box, const ofRectangle &preview) const;
	void drawAdvancedMappingPanel();
	// interactive: only the editor panel passes true. The render window draws
	// the same overlay in another GL context, where editor chrome (move box,
	// handles, hover highlight) has no meaning and the mouse is not ours.
	void drawAdvancedMappingOverlay(float x, float y,
		float width, float height, bool includeHandles,
		bool interactive = false);
	bool updateAdvancedMappingMousePressed(int mouseButton);
	bool updateAdvancedMappingMouseDragged(int mouseButton);
	bool updateAdvancedMappingMouseReleased(int mouseButton);
	bool updateAdvancedMappingMouseScrolled(int x, int y, float scrollY);
	void clampAdvancedMappingView();
	ofVec2f projectAdvancedMappingPoint(
		const JPbox_shader::AdvancedMappingLayer &layer,
		const ofVec2f &uv) const;
	void markAdvancedMappingChanged(JPbox_shader *box,
		int layerIndex, bool maskChanged);

	bool mappingEditActive = false;
	bool mappingGuidesVisible = true;
	bool mappingGridVisible = false;
	bool mappingRenderGuidesVisible = false;
	int mappingTargetIndex = -1;
	vector<int> mappingTargetGroupPath;
	int mappingDraggedCorner = -1;
	float mappingPanelX = 24.0f;
	float mappingPanelY = 96.0f;
	float mappingPanelW = 560.0f;
	float mappingPanelH = 350.0f;
	bool mappingPanelDragging = false;
	bool mappingPanelResizing = false;
	bool mappingPanelPointerCaptured = false;
	ofVec2f mappingPanelDragStartMouse;
	ofVec2f mappingPanelDragStartPos;
	ofVec2f mappingPanelResizeStartSize;
	AdvancedMappingTool advancedMappingTool = ADVANCED_MAPPING_MESH;
	AdvancedMappingDragKind advancedMappingDragKind =
		ADVANCED_MAPPING_DRAG_NONE;
	int advancedMappingDragIndex = -1;
	int advancedMappingSelectedMaskContour = -1;
	int advancedMappingSelectedMaskNode = -1;
	vector<int> advancedMappingSelectedMaskContours;
	// Bezier edge handles are off until asked for, so a fresh surface is a
	// plain corner-pin quad. See advancedMappingBezierActive for how a layer
	// that already carries a curve overrides this.
	bool advancedMappingBezierEnabled = false;
	AdvancedMappingMoveTarget advancedMappingMoveTarget =
		ADVANCED_MAPPING_TARGET_SURFACE;
	// A move or scale writes snapshot + offset rather than accumulating a per
	// frame delta, so a dropped or replayed mouse frame lands in the same
	// place. The preview rect is captured too: clampMappingPanelLayout can
	// resize the panel mid drag, which would otherwise teleport the shape.
	JPbox_shader::AdvancedMappingLayer advancedMappingDragSnapshot;
	ofRectangle advancedMappingDragPreview;
	ofVec2f advancedMappingDragStartUv;
	ofVec2f advancedMappingScaleAnchor;
	ofVec2f advancedMappingScaleHandle;
	ofVec2f advancedMappingRotationPivot;
	float advancedMappingRotationStartAngle = 0.0f;
	vector<int> advancedMappingDragContours;
	ofVec2f advancedMappingMarqueeStart;
	ofVec2f advancedMappingMarqueeEnd;
	bool advancedMappingMarqueeAdditive = false;
	int advancedMappingDragLayer = -1;
	int advancedMappingDragContour = -1;
	float advancedMappingViewZoom = 1.0f;
	ofVec2f advancedMappingViewCenter = ofVec2f(0.5f, 0.5f);
	bool advancedMappingViewPanning = false;
	bool advancedMappingRightPanPending = false;
	int advancedMappingPendingDeleteContour = -1;
	int advancedMappingPendingDeleteNode = -1;
	ofVec2f advancedMappingViewPanStartMouse;
	ofVec2f advancedMappingViewPanStartCenter;

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
	bool cueApplyingCommit = false;
	ofFbo cueApplySnapshotFbo;
	ofVec2f cuePanelDragStartMouse;
	ofVec2f cuePanelDragStartPos;
	ofVec2f cuePanelResizeStartSize;
	ofVec2f selectionEnd;
	vector<int> selectedBoxIndices;
	// What was selected when the current marquee started. Empty for a plain
	// drag, so the marquee replaces; populated for a shift drag, so the marquee
	// unions on top. It has to be remembered separately because
	// updateBoxSelection runs every frame of the drag and rebuilds from the
	// rectangle - there is nothing left to add to by then.
	vector<int> selectionBase;

	// Vamos a ver si podemos emular un doble click.
	bool isDoubleClick;
	float lasttime_mouseclick;
	float duration_mouseclick;
	

	//PARA LO DEL SHADER 
	TransitionSR transition;
	// The two boxes whose parameters are currently morphing into each other,
	// or nullptr. Raw pointers because they are boxes this group owns; every
	// use re-checks membership so a deletion mid-fade cannot dangle.
	JPbox *morphOutgoing = nullptr;
	JPbox *morphIncoming = nullptr;
	void armParameterMorph(JPbox *outgoing, JPbox *incoming);
	void updateParameterMorph();
	void clearParameterMorph();

	//MODO SECUENCIA 

	float	lasttime_sequence;
	float durationGalleryMs;
	//Transition shader render : 
	JPParameter galleryDurationParam;
	JPSlider galleryDurationSlider;

	// Exposed slider dragging state
	int draggedExposedBoxIndex = -1;
	int draggedExposedParamIndex = -1;

	// Mapping from controller index to (childIndex, paramIndex) for exposed sliders in MAIN view
	// When openguinumber selects a PRESETBOX, these controllers link to the child's real parameters
	vector<pair<int, int>> exposedControllerMapping;
};
