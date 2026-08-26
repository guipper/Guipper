#pragma once

#include "defines.h"
#include "ofMain.h"
#include "jp_box.h"
#include "jp_media_state.h"
#include "../JPutils/jp_parametergroup.h"
#include "../JPutils/jp_fbohandler.h"
#include <array>
#include <utility>
#include <vector>
//#include "Shaderrender.h"

//#include "JPbox/JPboxgroup.h"
// Esta caja la vamos a usar para ponerle objetos adentro. Con este template de caja despues hacemos las demas.

class JPbox_shader : public JPbox
{
public:
	static constexpr int ADVANCED_MAPPING_LAYER_COUNT = 4;

	struct AdvancedMappingNode
	{
		ofVec2f anchor;
		ofVec2f inHandle;
		ofVec2f outHandle;
		bool smooth = false;
	};

	struct AdvancedMappingContour
	{
		std::vector<AdvancedMappingNode> nodes;
		bool closed = false;
	};

	// How the source fills its mapped quad. Stretch is the default because it
	// is what every existing composition was authored against.
	enum AdvancedMappingFit
	{
		ADVANCED_MAPPING_FIT_STRETCH = 0,
		ADVANCED_MAPPING_FIT_CONTAIN,
		ADVANCED_MAPPING_FIT_COVER,
		ADVANCED_MAPPING_FIT_COUNT
	};

	struct AdvancedMappingLayer
	{
		std::array<ofVec2f, 4> corners;
		std::array<ofVec2f, 8> edgeHandles;
		std::vector<AdvancedMappingContour> masks;
		bool inspectorExpanded = true;
		// Kept out of the parameter list on purpose: it reaches the shader as a
		// uniform int, which the uniform parser ignores, so it adds no slider
		// and cannot disturb the positional order savefiles depend on.
		int fitMode = ADVANCED_MAPPING_FIT_STRETCH;
	};

	struct AdvancedMappingState
	{
		std::array<AdvancedMappingLayer,
			ADVANCED_MAPPING_LAYER_COUNT> layers;
		int selectedLayer = 0;
		std::string guideImagePath;
		bool guideVisible = false;
		float guideOpacity = 0.55f;
	};

	// ------------------------------------------------------- mapping undo
	//
	// A snapshot ring, not a command ring - the opposite choice from the graph
	// and the paint document, and for the opposite reason. The whole mapping
	// state is four layers of twelve points plus their mask contours: a few KB,
	// already a value type of std::array and std::vector of POD, already copied
	// wholesale by copyCustomStateFrom. Describing each corner drag as an
	// invertible command would be more code and more ways to be wrong for no
	// memory saved.
	//
	// The simple corner-pin tier has no struct at all - it lives in eight shader
	// parameters - so the snapshot carries those alongside, and one entry covers
	// whichever tier the box is using.
	struct MappingSnapshot
	{
		AdvancedMappingState advanced;
		std::vector<std::pair<int, float>> simpleCorners;
	};

	// Bounded by count alone: unlike a detached box or a paint stroke, an entry
	// here has no payload that can grow without bound.
	static constexpr std::size_t kMaxMappingHistory = 100;

	// Records `before` as the state to come back to, given what the box holds
	// now as the state to come back FROM. No-op when the two are equal, so a
	// click that changed nothing does not become a step.
	void pushMappingSnapshot(const MappingSnapshot &before);
	MappingSnapshot captureMappingSnapshot() const;
	bool mappingUndo();
	bool mappingRedo();
	bool canMappingUndo() const { return mappingCursor > 0; }
	bool canMappingRedo() const
	{
		return mappingCursor + 1 < mappingHistory.size();
	}
	void clearMappingHistory();

	JPbox_shader(); // constructor declared
	~JPbox_shader();

	// string dir;
	// JPFbohandlerGroup fbohandlergroup;

	// METODOS HEREDADOS :
	int frameNum;
	void reload();
	void reloadShaderonly();
	void setup2(string _dir, string _nombre);
	void setup(string _dir, string _nombre);
	void setup(ofTrueTypeFont &_font,
			   string dir,
			   string _nombre);
	void update();
	void draw();
	void updateFBO();
	void draw_outlet();
	void clear();
	void setPos(float _x, float _y)
	{
		JPdragobject::setPos(_x, _y);
		setfbohandler_nodepos();
	}

	// METODOS Y VARIABLES PROPIAS DE LA CLASE :
	void setfbohandler_nodepos();
	void update_NonglobalUniforms();
	void update_globalUniforms(); // GLOBAL UNIFORMS
	// JPParameterGroup getUniformsToJPParameterGroup(string _dir, string _name);
	void setUniforms(JPParameterGroup &_parameters, JPFbohandlerGroup &_fbohandlergroup, string _dir, string _name);
	bool isAdvancedMappingShader() const;
	AdvancedMappingState *getAdvancedMappingState();
	const AdvancedMappingState *getAdvancedMappingState() const;
	void markAdvancedMappingMaskDirty(int layerIndex = -1);
	bool loadAdvancedMappingGuide(const std::string &path);
	bool hasAdvancedMappingGuide() const;
	const ofImage *getAdvancedMappingGuide() const;
	bool importAdvancedMappingSvg(int layerIndex,
		const std::string &path, std::string &error);
	bool exportAdvancedMappingSvg(int layerIndex,
		const std::string &path, std::string &error) const;
	void saveCustomState(ofXml &boxNode) const override;
	void loadCustomState(const ofXml &boxNode) override;
	void copyCustomStateFrom(const JPbox *source) override;
	// ofFbo fbo;
	ofShader shader;

	//LIVECODING THINGS : 
	//bool showCode;
	ofBuffer buffer;

private:
	AdvancedMappingState advancedMappingState;
	// mappingHistory[0, mappingCursor) are behind the current state;
	// [mappingCursor, end) are ahead of it. Same cursor discipline as the other
	// two rings in the project.
	std::vector<MappingSnapshot> mappingHistory;
	std::size_t mappingCursor = 0;
	void applyMappingSnapshot(const MappingSnapshot &snapshot);
	bool sameMappingSnapshot(const MappingSnapshot &a,
		const MappingSnapshot &b) const;
	bool advancedMappingInitialized = false;
	std::array<ofFbo, ADVANCED_MAPPING_LAYER_COUNT>
		advancedMappingMasks;
	std::array<bool, ADVANCED_MAPPING_LAYER_COUNT>
		advancedMappingMaskDirty;
	ofImage advancedMappingGuide;

	void initializeAdvancedMappingState();
	int getAdvancedMappingInletIndex(int layerIndex) const;
	void updateAdvancedMappingUniforms();
	void rebuildAdvancedMappingMask(int layerIndex);
	void clearAdvancedMappingResources();
};
