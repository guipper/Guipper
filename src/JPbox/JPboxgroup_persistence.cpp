#include "../JPutils/jp_storage.h"
#include "JPboxgroup.h"
#include "../JPutils/jp_parameter_xml.h"
#include "jp_media.h"
#include "jp_box_factory.h"
#include <algorithm>
#include <set>
#include <thread>
#include <chrono>

namespace {
JPboxgroup::LoadResult validateStoredTree(const ofXml& xml, std::set<std::string>& stack, std::string& detail) {
    using Result = JPboxgroup::LoadResult;
    const auto format = xml.getChild("guipper_format");
    if (format && format.getValue() != "1") return Result::UnsupportedVersion;
    if (!xml.getChild("activerender") && !xml.getChild("box")) return Result::InvalidComposition;
    for (auto node : xml.getChildren("box")) {
        const auto source = jp_normalizePath(node.getChild("directory").getValue());
        if (ofTrim(source).empty()) { detail = node.getChild("nombre").getValue(); return Result::InvalidComposition; }
        if ((jp_media::isImage(source) || jp_media::isVideo(source)) && !ofFile::doesFileExist(source)) {
            detail=source;return Result::AssetError;
        }
        if (ofToLower(ofFilePath::getFileExt(source)) == "frag") {
            const auto bytes = ofBufferFromFile(source);
            if (bytes.size() == 0 || jp_uniform_parser::parse(bytes.getText()).hasErrors()) {
                detail = source; ofLogError("session") << "Cannot read/parse shader: " << ofToDataPath(source, true);
                return Result::AssetError;
            }
        }
        if (ofToLower(ofFilePath::getFileExt(source)) == "xml") {
            const auto path = std::filesystem::absolute(ofToDataPath(source, true)).lexically_normal().string();
            if (stack.size() >= 64 || !stack.insert(path).second) { detail = source; return Result::InvalidComposition; }
            ofXml child;
            if (!child.load(path)) { detail = source; return Result::AssetError; }
            const auto result = validateStoredTree(child, stack, detail);
            stack.erase(path);
            if (result != Result::Success) return result;
        }
    }
    return Result::Success;
}
bool validBuiltTree(JPbox* box, std::string& detail) {
    if (auto* shader = dynamic_cast<JPbox_shader*>(box))
        {
            GLint linked = GL_FALSE;
            if (!shader->shader.isLoaded() || !shader->shader.getProgram()) { detail = box->dir; return false; }
            glGetProgramiv(shader->shader.getProgram(), GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE) { detail = box->dir; return false; }
        }
    if (auto* preset = dynamic_cast<JPbox_preset*>(box))
        for (auto* child : preset->boxes) if (!validBuiltTree(child, detail)) return false;
    return true;
}
}

// Validate a curated group in isolation before assigning or inserting it.
bool JPboxgroup::validateGroupFile(const string& path, string* errorDetail) {
    string ignored;
    string& lastLoadErrorDetail = errorDetail ? *errorDetail : ignored;
    lastLoadErrorDetail.clear();
    ofXml xml;
    if (!xml.load(path) || !xml.getChild("box")) { lastLoadErrorDetail = path; return false; }
    std::set<std::string> stack{std::filesystem::absolute(ofToDataPath(path,true)).lexically_normal().string()};
    if (validateStoredTree(xml,stack,lastLoadErrorDetail) != LoadResult::Success) return false;
    JPbox_preset candidate;
    try {
        candidate.setup(path,"group-validation");
        const bool valid=!candidate.boxes.empty() && validBuiltTree(&candidate,lastLoadErrorDetail);
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
std::shared_ptr<JPboxgroup::PreparedSession> JPboxgroup::parseSession(const string &path)
{
    auto candidate=std::make_shared<PreparedSession>(); candidate->path=path;
    if(!candidate->xml.load(path)) { candidate->result=LoadResult::ReadError; candidate->error=path; return candidate; }
    std::set<string> stack;
    stack.insert(std::filesystem::absolute(ofToDataPath(path,true)).lexically_normal().string());
    candidate->result=validateStoredTree(candidate->xml,stack,candidate->error);
    if(candidate->result!=LoadResult::Success) return candidate;
    for(auto node:candidate->xml.getChildren("box")) {
        candidate->declarations.push_back(node);
        const auto directory=jp_normalizePath(node.getChild("directory").getValue());
        if(ofToLower(ofFilePath::getFileExt(directory))=="xml") {
            auto child=parseSession(directory);child->presetContext=true;
            if(child->result!=LoadResult::Success) {candidate->result=child->result;candidate->error=child->error;return candidate;}
            candidate->groups.push_back(std::move(child));
        } else candidate->groups.push_back(nullptr);
    }
    jp_quick_image::loadStack(candidate->xml,candidate->finalLayers);
    return candidate;
}
JPboxgroup::LoadResult JPboxgroup::buildSessionNode(PreparedSession &candidate, ofXml box, PreparedSession *group)
{
    auto &boxes=candidate.nodes;
    auto &loadedBoxNodes=candidate.loaded;
    auto &legacyOverlays=candidate.legacy;
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
        if (!bx) { candidate.error=directory.getValue(); return LoadResult::AssetError; }

        boxes.push_back(bx);
        try {
            if(group) static_cast<JPbox_preset *>(bx)->setupPrepared(jp_normalizePath(directory.getValue()),nombre.getValue(),group->xml,group->nodes);
            else bx->setup(jp_normalizePath(directory.getValue()), nombre.getValue());
        }
        catch (const std::exception& error) {
            ofLogError("session") << error.what();
            return LoadResult::AssetError;
        }
        if (!validBuiltTree(bx, candidate.error)) return LoadResult::AssetError;
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
			candidate.presetContext?jp_parameter_xml::LoadContext::Preset:jp_parameter_xml::LoadContext::Composition);
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
    return LoadResult::Success;
}
JPboxgroup::LoadResult JPboxgroup::buildNextSessionResource(PreparedSession &candidate)
{
    if(candidate.cursor>=candidate.declarations.size()) return LoadResult::Success;
    auto group=candidate.groups[candidate.cursor];
    if(group && group->cursor<group->declarations.size()) {
        auto result=buildNextSessionResource(*group);
        if(result!=LoadResult::Success) candidate.error=group->error;
        return result;
    }
    if(group && !group->linked) linkPreparedSession(*group);
    const auto declaration=candidate.declarations[candidate.cursor++];
    return buildSessionNode(candidate,declaration,group.get());
}
void JPboxgroup::linkPreparedSession(PreparedSession &candidate)
{
    auto &boxes=candidate.nodes;
    auto &loadedBoxNodes=candidate.loaded;
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

    candidate.linked=true;
}
JPboxgroup::LoadResult JPboxgroup::commitPreparedSession(PreparedSession &candidate)
{
    auto &boxes=candidate.nodes;
    auto &candidateFinal=candidate.finalLayers;
    auto &xml=candidate.xml;
    auto &legacyOverlays=candidate.legacy;
    using LegacyOverlay=PreparedSession::LegacyOverlay;
    const int nextRender = boxes.empty() ? 0 :
        ofClamp(xml.getChild("activerender").getIntValue(), 0, int(boxes.size()) - 1);
    // Capture only after validation/setup succeeded. A failed load must not
    // replace or restart an already visible transition.
    ofFbo outgoing = captureSessionOutput();
    const float fadeSeconds = std::max(0.001f, getTransitionDurationMs() / 1000.f);
    std::unique_ptr<RetainedScene> retained;
    // A second request starts from the visible mixed frame, not from B.
    if (!sessionFadeActive && mainTransitionState().progress() >= 1.f &&
        TransitionSR::preferences().quality != jp_transition::Quality::Capture) {
        clearParameterMorph();
        clearCue();
        retained = std::make_unique<RetainedScene>();
        retained->active = *activerender;
        retained->nodes.swap(this->boxes);
        retained->finalLayers = std::move(finalQuickImages);
    }
    clear();
    outgoingScene = std::move(retained);
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
    configureSceneTransition();
	return LoadResult::Success;
}
JPboxgroup::LoadResult JPboxgroup::load(string path)
{
    lastLoadErrorDetail.clear();
    auto candidate=parseSession(path);
    if(candidate->result==LoadResult::Success)
        while(candidate->cursor<candidate->declarations.size()) {
            candidate->result=buildNextSessionResource(*candidate);
            if(candidate->result!=LoadResult::Success) break;
        }
    if(candidate->result!=LoadResult::Success) { lastLoadErrorDetail=candidate->error; return candidate->result; }
    linkPreparedSession(*candidate);
    return commitPreparedSession(*candidate);
}
void JPboxgroup::requestSessionLoad(string path, std::function<void(LoadResult)> completion)
{
    ++sessionRequestTicket;
    pendingSession.reset();
    requestedSessionPath=std::move(path);
    sessionCompletion=std::move(completion);
    sessionPreparationStarted=ofGetElapsedTimef();
    lastLoadErrorDetail.clear();
}
void JPboxgroup::pollSessionLoad()
{
    if(requestedSessionPath.empty()) return;
    auto finish=[&](LoadResult result) {
        auto callback=std::move(sessionCompletion);
        auto candidate=std::move(pendingSession);
        requestedSessionPath.clear();
        if(result==LoadResult::Success && candidate) result=commitPreparedSession(*candidate);
        else if(candidate) lastLoadErrorDetail=candidate->error;
        if(callback) callback(result);
    };
    if(ofGetElapsedTimef()-sessionPreparationStarted>=10.) {
        if(pendingSession && pendingSession->error.empty()) pendingSession->error=requestedSessionPath;
        else lastLoadErrorDetail=requestedSessionPath;
        finish(LoadResult::AssetError); return;
    }
    if(!pendingSession) {
        if(!parsingSession.valid()) {
            const auto path=requestedSessionPath;
            // packaged_task futures do not block the rendering thread when a
            // request is cancelled. The worker owns only CPU-side parsed data.
            const auto ticket=sessionRequestTicket;
            std::packaged_task<std::shared_ptr<PreparedSession>()> task([path,ticket]{auto result=parseSession(path);result->ticket=ticket;return result;});
            parsingSession=task.get_future(); std::thread(std::move(task)).detach();
            return;
        }
        if(parsingSession.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        try { pendingSession=parsingSession.get(); }
        catch(const std::exception &e) { lastLoadErrorDetail=e.what(); finish(LoadResult::ReadError); return; }
        if(pendingSession->path!=requestedSessionPath || pendingSession->ticket!=sessionRequestTicket) { pendingSession.reset(); return; }
        if(pendingSession->result!=LoadResult::Success) { finish(pendingSession->result); return; }
    }
    auto &candidate=*pendingSession;
    // One resource per frame, including nested presets. A single driver
    // compilation can still block; timings identify that remaining spike.
    if(candidate.cursor<candidate.declarations.size()) {
        const auto start=ofGetElapsedTimef();
        try { candidate.result=buildNextSessionResource(candidate); }
        catch(const std::exception &error) {candidate.error=error.what();candidate.result=LoadResult::AssetError;}
        ofLogNotice("transition-prepare") << candidate.path << " node=" << candidate.cursor << " ms=" << (ofGetElapsedTimef()-start)*1000.;
        if(candidate.result!=LoadResult::Success) finish(candidate.result);
        return;
    }
    if(!candidate.linked) linkPreparedSession(candidate);
    bool ready=true;
    // A paused destination still needs one valid frame before presentation.
    // Restore transport flags after warming; nothing is written to the project.
    vector<std::pair<JPbox *,bool>> transports;
    std::function<void(JPbox *)> warm=[&](JPbox *node) {
        transports.emplace_back(node,node->getonoff());node->setonoff(true);node->setRenderPinned(true);node->setRenderThisFrame(true);
        if(auto *group=dynamic_cast<JPbox_preset *>(node))for(auto *child:group->boxes)warm(child);
    };
    for(auto *node:candidate.nodes)warm(node);
    for(auto it=candidate.nodes.rbegin();it!=candidate.nodes.rend();++it) (*it)->update();
    for(auto state:transports)state.first->setonoff(state.second);
    std::function<void(JPbox *)> inspect=[&](JPbox *node) {
        if(auto *media=dynamic_cast<JPMediaInspectable *>(node)) if(!media->mediaReady()) { ready=false;candidate.error=node->dir; }
        if(auto *video=dynamic_cast<JPbox_video *>(node)) if(!video->movie.isLoaded() || !video->movie.getTexture().isAllocated()) {ready=false;candidate.error=node->dir;}
        if(auto *camera=dynamic_cast<JPbox_cam *>(node)) if(!camera->transitionReady()) {ready=false;candidate.error=node->dir;}
        if(auto *group=dynamic_cast<JPbox_preset *>(node)) for(auto *child:group->boxes) inspect(child);
        if(!node->fbo.isAllocated()) {ready=false;candidate.error=node->dir;}
    };
    for(auto *node:candidate.nodes) inspect(node);
    if(ready) { ofLogNotice("transition-prepare") << "ready ms=" << (ofGetElapsedTimef()-sessionPreparationStarted)*1000.; finish(LoadResult::Success); }
}
