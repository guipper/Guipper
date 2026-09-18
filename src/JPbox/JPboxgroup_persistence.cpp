#include "../JPutils/jp_storage.h"
#include "JPboxgroup.h"
#include "../JPutils/jp_parameter_xml.h"
#include "jp_media.h"
#include "jp_box_factory.h"
#include <algorithm>
#include <set>

namespace {
JPboxgroup::LoadResult validateStoredTree(const ofXml& xml, std::set<std::string>& stack) {
    using Result = JPboxgroup::LoadResult;
    const auto format = xml.getChild("guipper_format");
    if (format && format.getValue() != "1") return Result::UnsupportedVersion;
    if (!xml.getChild("activerender") && !xml.getChild("box")) return Result::InvalidComposition;
    for (auto node : xml.getChildren("box")) {
        const auto source = jp_normalizePath(node.getChild("directory").getValue());
        if (ofTrim(source).empty()) return Result::InvalidComposition;
        if (ofToLower(ofFilePath::getFileExt(source)) == "frag") {
            const auto bytes = ofBufferFromFile(source);
            if (bytes.size() == 0 || jp_uniform_parser::parse(bytes.getText()).hasErrors()) return Result::AssetError;
        }
        if (ofToLower(ofFilePath::getFileExt(source)) == "xml") {
            const auto path = std::filesystem::absolute(ofToDataPath(source, true)).lexically_normal().string();
            if (stack.size() >= 64 || !stack.insert(path).second) return Result::InvalidComposition;
            ofXml child;
            if (!child.load(path)) return Result::AssetError;
            const auto result = validateStoredTree(child, stack);
            stack.erase(path);
            if (result != Result::Success) return result;
        }
    }
    return Result::Success;
}
bool validBuiltTree(JPbox* box) {
    if (auto* shader = dynamic_cast<JPbox_shader*>(box))
        {
            GLint linked = GL_FALSE;
            if (!shader->shader.isLoaded() || !shader->shader.getProgram()) return false;
            glGetProgramiv(shader->shader.getProgram(), GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE) return false;
        }
    if (auto* preset = dynamic_cast<JPbox_preset*>(box))
        for (auto* child : preset->boxes) if (!validBuiltTree(child)) return false;
    return true;
}
}

// Validate a curated group in isolation before assigning or inserting it.
bool JPboxgroup::validateGroupFile(const string& path) {
    ofXml xml;
    if (!xml.load(path) || !xml.getChild("box")) return false;
    std::set<std::string> stack{std::filesystem::absolute(ofToDataPath(path,true)).lexically_normal().string()};
    if (validateStoredTree(xml,stack) != LoadResult::Success) return false;
    JPbox_preset candidate;
    try {
        candidate.setup(path,"group-validation");
        const bool valid=!candidate.boxes.empty() && validBuiltTree(&candidate);
        candidate.clear(); return valid;
    } catch (...) { candidate.clear(); return false; }
}

// Session orchestration: graph lifetime and link repair stay on JPboxgroup.
// Parameter field encoding is shared with presets and clipboard via the codec.
ofXml JPboxgroup::snapshotXml()
{
	ofXml xml;
	xml.appendChild("guipper_format").set(1);

	auto activerender_save = xml.appendChild("activerender");
	activerender_save.set(*activerender);
	jp_quick_image::saveStack(xml, finalQuickImages);

	for (int i = 0; i < boxes.size(); i++)
	{
		if (isCueAddedRealIndex(i))
		{
			continue;
		}

		// for (int i = boxes.size() - 1; i >= 0; i--) {
		auto data = xml.appendChild("box"); // or whatever name you want to.
		data.appendChild("nombre").set(boxes[i]->name);
		data.appendChild("x").set(boxes[i]->x);
		data.appendChild("y").set(boxes[i]->y);
		data.appendChild("directory").set(boxes[i]->dir);
		data.appendChild("uid").set(boxes[i]->uid);
		data.appendChild("tooutput").set(boxes[i]->getOutputCandidate());
		data.appendChild("onoff").set(boxes[i]->getonoff());
		data.appendChild("bypass").set(boxes[i]->getBypass());
		boxes[i]->saveCustomState(data);
		// boxes[i]->parameters.coutData();
		jp_parameter_xml::save(data, boxes[i]->parameters);
		if (boxes[i]->fbohandlergroup.getPointerSetsSize() > 0)
		{
			auto fboslinks = data.appendChild("fboslinks");
			for (int k = 0; k < boxes[i]->fbohandlergroup.getSize(); k++)
			{
				if (boxes[i]->fbohandlergroup.getisPointerSet(k))
				{
					fboslinks.appendChild(boxes[i]->fbohandlergroup.getName(k))
						.set(boxes[i]->fbohandlergroup.getFboName(k));
				}
			}
		}
		// Save exposedParams for preset boxes
		if (boxes[i]->getTipo() == boxes[i]->PRESETBOX)
		{
			JPbox_preset *preset = dynamic_cast<JPbox_preset *>(boxes[i]);
			if (preset != nullptr && !preset->exposedParams.empty())
			{
				auto exposedNode = data.appendChild("exposedParams");
				for (int ci = 0; ci < (int)preset->exposedParams.size(); ci++)
				{
					for (int pi = 0; pi < (int)preset->exposedParams[ci].size(); pi++)
					{
						if (preset->exposedParams[ci][pi])
						{
							auto boxNode = exposedNode.appendChild("box");
							boxNode.set(ci);
							auto paramNode = boxNode.appendChild("param");
							paramNode.set(pi);
						}
					}
				}
			}
		}
	}



	return xml;
}
bool JPboxgroup::save(string outputPath)
{
	ofXml xml = snapshotXml();
	// Save current viewport zoom/pan to the active preset (if in group view)
	if (isGroupViewActive())
	{
		JPbox_preset *activePreset = getActivePreset();
		if (activePreset != nullptr)
		{
			activePreset->viewportZoom = viewportZoom;
			activePreset->viewportPan = viewportPan;
		}
	}

	// After saving the main project file, save all preset children to their own XML files
	for (int i = 0; i < (int)boxes.size(); i++)
	{
		if (boxes[i]->getTipo() == boxes[i]->PRESETBOX)
		{
			JPbox_preset *preset = dynamic_cast<JPbox_preset *>(boxes[i]);
			if (preset != nullptr)
			{
				if (!preset->save()) return false;
			}
		}
	}
	return jp::saveXml(xml, outputPath);
}
void JPboxgroup::load2(string _dirinput)
{
	JPbox_preset *presetbox = new JPbox_preset();

	// NO SE COMO HACERLO EN UNA SOLA PASADA PERO EN 2 RE FUNCA ASI QUE MIRA QUE PIOLA EH
	/*string nombre = _dirinput.substr(_dirinput.find_last_of("/\\") + 1, _dirinput.size());
	nombre = nombre.substr(0, nombre.find(".xml"));
	cout << "nombre " << nombre << endl;
	*/
	/*string name = _dirinput;
		   name = name.substr(5, name.find(".xml"));
	cout << "POSITION .XML " << name.find(".xml") << endl;
	cout << "name " << name << endl;*/
	/*presetbox->setup(ofGetMouseX(),ofGetMouseY(), _dirinput);
	presetbox->setPos(ofGetMouseX(), ofGetMouseY());
	boxes.push_back(presetbox);

	*activerender = 0;*/
}
JPboxgroup::LoadResult JPboxgroup::load(string _dirinput)
{
	// Parse exactly once before touching any live state. ofXml retains the
	// parsed document for the reconstruction below, even if the file changes.
	ofXml xml;
	if (!xml.load(_dirinput))
	{
		ofLogError("session") << "Cannot read composition XML: " << _dirinput;
		return LoadResult::ReadError;
	}
    std::set<std::string> stack;
    stack.insert(std::filesystem::absolute(ofToDataPath(_dirinput, true)).lexically_normal().string());
    const auto validation = validateStoredTree(xml, stack);
    if (validation != LoadResult::Success) return validation;
	// Legacy compositions may omit activerender. An explicitly empty project
	// saved by Guipper contains activerender, so it remains a valid load.
	if (!xml.getChild("activerender") && !xml.getChild("box"))
	{
		ofLogError("session") << "Not a Guipper composition: " << _dirinput;
		return LoadResult::InvalidComposition;
	}
	for (const auto &node : xml.getChildren("box"))
	{
		if (ofTrim(node.getChild("directory").getValue()).empty())
		{
			ofLogError("session") << "Box has no source directory: " << _dirinput;
			return LoadResult::InvalidComposition;
		}
	}
    vector<JPbox*> boxes;
    struct CandidateCleanup {
        vector<JPbox*>& nodes;
        ~CandidateCleanup() { for (auto* node : nodes) { node->clear(); delete node; } }
    } cleanup{boxes};
    decltype(finalQuickImages) candidateFinal;
    jp_quick_image::loadStack(xml, candidateFinal);
	// Carga inicial de las cajitas :
	auto boxloader = xml.find("/box");
	// Kept in lockstep with `boxes`, so the link pass below can pair a box
	// with the node it came from even when some nodes produce no box.
	vector<ofXml> loadedBoxNodes;
	// Boxes carrying the pre-stack GO TO FINAL flag, migrated into FINAL layers
	// once every box exists - the stack is loaded before them, and a layer has
	// to name a box that is already there.
	struct LegacyOverlay { JPbox *box; float opacity; int order; };
	vector<LegacyOverlay> legacyOverlays;

	cout << "******************************************************************" << endl;
	for (auto &box : boxloader)
	{

		auto nombre = box.getChild("nombre");
		auto x = box.getChild("x");
		auto y = box.getChild("y");
		auto directory = box.getChild("directory");
		auto onoff = box.getChild("onoff");
		auto bypass = box.getChild("bypass");
		// cout << "Nombre : " << nombre.getValue() << endl;
		// cout << "y : " << x.getValue() << endl;
		// cout << "x : " << y.getValue() << endl;
		// cout << "Directory : " << directory.getValue() << endl;

		JPbox *bx = jp_box_factory::create(directory.getValue(),
			jp_box_factory::Context::Stored);
		if (bx == nullptr)
		{
			// Nothing matched: a build without NDI/Spout, or a save that
			// references a box type this binary does not know about.
			ofLogWarning("JPboxgroup")
				<< "skipping box '" << nombre.getValue()
				<< "' with unsupported directory '"
				<< directory.getValue() << "'";
			continue;
		}

        boxes.push_back(bx);
        try { bx->setup(jp_normalizePath(directory.getValue()), nombre.getValue()); }
        catch (const std::exception& error) {
            ofLogError("session") << error.what();
            return LoadResult::AssetError;
        }
        if (!validBuiltTree(bx)) return LoadResult::AssetError;
		bx->setPos(x.getIntValue(), y.getIntValue());
		bx->setonoff(onoff ? onoff.getBoolValue() : true);
		bx->setBypass(bypass ? bypass.getBoolValue() : false);
		// Adopt the stored identity; a composition written before uids existed
		// simply keeps the one the constructor minted. Nothing is rewritten on
		// disk here - the live-output binding heals itself by falling back to
		// the box NAME, so a file the user never re-saves still resolves.
		auto uidNode = box.getChild("uid");
		if (uidNode && !uidNode.getValue().empty()) bx->uid = uidNode.getValue();
		auto toOutput = box.getChild("tooutput");
		bx->setOutputCandidate(toOutput ? toOutput.getBoolValue() : false);
		// Compositions written by the first cut of GO TO FINAL carry the flag
		// per box. Collected here and turned into FINAL stack layers once every
		// box exists, so those overlays survive the move to the stack.
		{
			const jp_finaloverlay::Legacy legacy =
				jp_finaloverlay::readLegacy(box);
			if (legacy.present)
				legacyOverlays.push_back({bx, legacy.opacity, legacy.order});
		}

		jp_parameter_xml::load(box, bx->parameters,
			jp_parameter_xml::LoadContext::Composition);
		bx->loadCustomState(box);


#ifdef SPOUT
		if (bx->getTipo() == 4) {
			cout << "RELOAD CAJA DE SPOUT " << endl;
			bx->reload();
		}
#endif

		loadedBoxNodes.push_back(box);

		// Load exposedParams for preset boxes from the main XML
		if (bx->getTipo() == bx->PRESETBOX)
		{
			JPbox_preset *preset = dynamic_cast<JPbox_preset *>(bx);
			if (preset != nullptr)
			{
				auto exposedNode = box.getChild("exposedParams");
				if (exposedNode)
				{
					auto boxNodes = exposedNode.getChildren();
					for (auto &boxNode : boxNodes)
					{
						int childIndex = boxNode.getIntValue();
						for (auto &paramNode : boxNode.getChildren())
						{
							int paramIndex = paramNode.getIntValue();
							if (childIndex >= 0 && childIndex < (int)preset->exposedParams.size() &&
								paramIndex >= 0 && paramIndex < (int)preset->exposedParams[childIndex].size())
							{
								preset->exposedParams[childIndex][paramIndex] = true;
							}
						}
					}
				}
			}
		}
	}
	// Una vez que cargo todas las cajitas les cargamos los links :
	// Mira lo que esta este algoritmo para levantar los links entre cajitas papa !!!
	// Walk the nodes that actually produced a box, not every node in the file.
	// Iterating boxloader here assumed the two ran in lockstep, so a single
	// skipped box shifted every later node onto the wrong box and silently
	// rewired the rest of the patch.
	int index1 = 0;
	cout << "COMIENZA LINKS DE LOS FBO " << endl;
	for (auto &box : loadedBoxNodes)
	{
		if (index1 >= (int)boxes.size())
		{
			break;
		}
		auto fboslinks = box.getChild("fboslinks").getChildren();
		for (auto &fbolink : fboslinks)
		{
			int linkIndex = boxes[index1]->fbohandlergroup.findIndexByName(
				fbolink.getName());
			if (linkIndex < 0)
			{
				continue;
			}
			for (int i = 0; i < boxes.size(); i++)
			{

				cout << "NOMBRE CAJA " << boxes[i]->name << endl ;
				cout << "NOMBRE FBO " << fbolink.getValue() << endl;
				if (boxes[i]->name == fbolink.getValue() && i != index1)
				{
					ofFbo *fbopointer = &boxes[i]->fbo;
					string *fbopointername = &boxes[i]->name;
					//if(fbopointer != nullptr){



					bool existe = false;


					for(int k = 0; k < boxes.size(); k++){
						if (boxes[k]->name ==  *fbopointername) {
							existe = true;
						}
					}


					//aca tendria que comparar que tipo si lee que tiene un link el shader tendria que tener esa misma cantidad de entradas
					//porque si tipo modificas el shader y le sacas un buffer e intentas levantar un archivo de guardado que tiene un buffer crashea
					//entonces le pongo lo de > 0 pero en realidad tendría que ser que
					if (existe && boxes[index1]->fbohandlergroup.getSize() > 0) {
						boxes[index1]->fbohandlergroup.setFboPointer(
							fbopointer, fbopointername, linkIndex);
					}
				}
			}
		}
		index1++;
	}

    const int nextRender = boxes.empty() ? 0 :
        ofClamp(xml.getChild("activerender").getIntValue(), 0, int(boxes.size()) - 1);
    // Capture only after validation/setup succeeded. A failed load must not
    // replace or restart an already visible transition.
    ofFbo outgoing = captureSessionOutput();
    const float fadeSeconds = std::max(0.001f, getTransitionDurationMs() / 1000.f);
    clear();
    sessionFadeSnapshot = std::move(outgoing);
    sessionFadeActive = sessionFadeSnapshot.isAllocated();
    sessionFadeDurationSeconds = fadeSeconds;
    this->boxes.swap(boxes);
    finalQuickImages = std::move(candidateFinal);
    finalQuickImageHistory.clear();
    finalQuickImageHistoryCursor = 0;
    *activerender = nextRender;

	// The old graph is gone. Do not arm a node fade with two identical inputs.
	transition.setLerpValue(1.0f);




	//}
	// activerender_loader.getIntValue();
	// activerender = activerender_loader.getIntValue();

	// Hand-edited XML, or the same group .xml placed twice, can deliver
	// duplicate identities. Shallowest box keeps the uid; the rest are re-minted.
	repairBoxUids();

	// Migration, after repairBoxUids so the layers name the identities the rest
	// of this session will use. Full frame, because that is what the per-box
	// overlay always was; the old order becomes stack order.
	if (!legacyOverlays.empty())
	{
		std::stable_sort(legacyOverlays.begin(), legacyOverlays.end(),
			[](const LegacyOverlay &a, const LegacyOverlay &b)
			{
				return a.order < b.order;
			});
		for (const LegacyOverlay &legacy : legacyOverlays)
		{
			if (legacy.box == nullptr) continue;
			JPQuickImageLayerState layer = jp_quick_image::makeBoxLayer(
				finalQuickImages, legacy.box->uid, legacy.box->name);
			layer.opacity = legacy.opacity;
			finalQuickImages.layers.push_back(layer);
		}
		ofLogNotice("finaloverlay") << "migrated " << legacyOverlays.size()
			<< " box overlay(s) into the FINAL stack";
	}

	// LAST, once the graph is populated: loadStack() runs before a single box
	// exists and cannot resolve anything. A composition saved before deleting
	// took its layers with it can carry rows whose source no longer exists, and
	// those rows draw nothing while looking exactly like a healthy one.
	pruneOrphanFinalLayers();
	return LoadResult::Success;
}
