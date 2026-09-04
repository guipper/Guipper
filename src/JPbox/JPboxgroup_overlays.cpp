#include "JPboxgroup.h"
#include "jp_graph_walk.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_tooltip.h"
#include <cmath>
#include <algorithm>
#include <functional>

// GO TO FINAL: a box flagged anywhere in the graph is composited on top of the
// final image, and what actually gets drawn is the LAST box of its effect
// chain - patch a shader after the image and the shader's output is what shows.
//
// Resolution happens once per frame and is thrown away again; see
// JPboxgroup::frameOverlays for why there is no cache.

namespace
{
	// Depth-first, carrying the container the way walkBoxTree deliberately does
	// not: findBoxByUid and friends only want the box, chain resolution needs
	// to know which siblings it may scan.
	const vector<JPbox *> *findOwningList(const vector<JPbox *> &list,
		const JPbox *target)
	{
		for (JPbox *box : list)
		{
			if (box == nullptr) continue;
			if (box == target) return &list;
			if (JPbox_preset *group = dynamic_cast<JPbox_preset *>(box))
			{
				const vector<JPbox *> *found =
					findOwningList(group->boxes, target);
				if (found != nullptr) return found;
			}
		}
		return nullptr;
	}

	// walkBoxTree lives in JPboxgroup.cpp's anonymous namespace and cannot be
	// reached from here; this is the same depth-first order, which is what the
	// order tie-break relies on.
	void visitBoxTree(const vector<JPbox *> &list,
		const std::function<void(JPbox *)> &visit)
	{
		for (JPbox *box : list)
		{
			if (box == nullptr) continue;
			visit(box);
			if (JPbox_preset *group = dynamic_cast<JPbox_preset *>(box))
				visitBoxTree(group->boxes, visit);
		}
	}

	// A group takes its children out of the graph with it, so "which boxes stop
	// existing when this one is deleted" is the whole subtree, not one uid.
	void collectSubtreeUids(const JPbox *box, vector<string> &out)
	{
		if (box == nullptr) return;
		if (!box->uid.empty()) out.push_back(box->uid);
		if (const JPbox_preset *group = dynamic_cast<const JPbox_preset *>(box))
			for (const JPbox *child : group->boxes)
				collectSubtreeUids(child, out);
	}
}

const vector<JPbox *> *JPboxgroup::owningList(const JPbox *box) const
{
	if (box == nullptr) return nullptr;
	return findOwningList(boxes, box);
}

bool JPboxgroup::feedsActiveRender(const JPbox *box) const
{
	if (box == nullptr || activerender == nullptr) return false;
	if (*activerender < 0 || *activerender >= (int)boxes.size()) return false;
	JPbox *current = boxes[*activerender];
	// Walk INTO the active render while it is a group: a group's FBO is its own
	// active child's pixels, so that child - and its active grandchild - are
	// already the final image just as much as the group is.
	while (current != nullptr)
	{
		if (current == box) return true;
		JPbox_preset *group = dynamic_cast<JPbox_preset *>(current);
		if (group == nullptr) break;
		if (group->activeRender < 0 ||
			group->activeRender >= (int)group->boxes.size()) break;
		current = group->boxes[group->activeRender];
	}
	return false;
}

void JPboxgroup::collectFinalOverlays()
{
	frameOverlays.clear();
	if (boxes.empty() || activerender == nullptr) return;

	// The FINAL stack is the list of what sits on top of the output. Resolving
	// it here, once, is what lets the renderer, the panel, the node chips and
	// the render scheduler all agree within a frame.
	for (const JPQuickImageLayerState &layer : finalQuickImages.layers)
	{
		if (layer.sourceUid.empty()) continue; // pure file layer, nothing to resolve
		JPbox *source = findBoxByUid(layer.sourceUid);
		if (source == nullptr) continue;       // box deleted: the file, if any, still draws
		// EVERY box-backed layer gets an entry, even one that must not draw.
		// Absence from this list is what tells the resolver "not a FINAL stack
		// layer, resolve it yourself" - so a layer dropped for being paused or
		// for feeding back would otherwise be drawn by that fallback anyway.
		ResolvedOverlay overlay;
		overlay.layerId = layer.id;
		overlay.source = source;
		overlay.terminal = source;
		if (layer.followChain)
		{
			const vector<JPbox *> *list = owningList(source);
			if (list != nullptr)
				overlay.terminal = jp_graphwalk::resolveChainTerminal(*list, source);
		}
		const bool drawable = overlay.terminal != nullptr &&
			// A paused box holds whatever frame it stopped on; compositing
			// that over a live output reads as a bug.
			source->getonoff() &&
			overlay.terminal->fbo.isAllocated() &&
			// Drawing the final image on top of itself is a feedback loop, and
			// it samples a texture the composite is writing this very frame.
			!feedsActiveRender(overlay.terminal);
		if (drawable) overlay.texture = &overlay.terminal->fbo.getTexture();
		frameOverlays.push_back(overlay);
	}
	// No sort: the stack IS the order, top of the list drawn last, and the
	// panel's up/down buttons are how it gets rearranged.
}

const JPboxgroup::ResolvedOverlay *JPboxgroup::finalOverlayFor(
	const JPQuickImageLayerState &layer) const
{
	if (layer.sourceUid.empty()) return nullptr;
	for (const ResolvedOverlay &overlay : frameOverlays)
		if (overlay.layerId == layer.id) return &overlay;
	return nullptr;
}

void JPboxgroup::applyRenderPins()
{
	visitBoxTree(boxes, [](JPbox *box) { box->setRenderPinned(false); });
	for (const ResolvedOverlay &overlay : frameOverlays)
	{
		// Pinning the terminal is enough: jp_renderschedule walks a marked
		// box' inlets, so the whole chain behind it comes along.
		if (overlay.terminal != nullptr) overlay.terminal->setRenderPinned(true);
	}
	// Same defect, one line: a quick-image layer fed by a box composites every
	// frame while that box, being nobody's active render, was rendering one
	// frame in four.
	auto pinStack = [this](const JPQuickImageStackState &stack)
	{
		for (const auto &layer : stack.layers)
		{
			if (layer.sourceUid.empty()) continue;
			if (JPbox *source = findBoxByUid(layer.sourceUid))
				source->setRenderPinned(true);
		}
	};
	pinStack(finalQuickImages);
}

// ------------------------------------------------------------ inspector card

void JPboxgroup::layoutFinalOverlayInspector(JPbox *box, float &cursorY)
{
	finalOverlayInspector.clear();
	// Zero size is the row's hidden-AND-unhittable convention, and returning
	// without touching cursorY is what keeps an unlisted box' panel identical
	// to before this feature.
	if (box == nullptr || !hasFinalLayerForBox(box)) return;
	const float x = inspectorBodyViewport.x + inspectorLayout.contentPadding;
	const float w = inspectorBodyViewport.width -
		inspectorLayout.contentPadding * 2.0f;
	const float h = 62.0f;
	finalOverlayInspector.card.set(x, cursorY, w, h);
	finalOverlayInspector.follow.set(x + 8, cursorY + 32, 84, 20);
	// Right of the follow switch, sharing its row: both answer "how does this
	// layer reach the output", so they read together.
	finalOverlayInspector.opacityBar.set(x + 100, cursorY + 34,
		std::max(40.0f, w - 152.0f), 16.0f);
	cursorY += h + inspectorLayout.rowGap;
}

void JPboxgroup::drawFinalOverlayInspector(JPbox *box)
{
	if (box == nullptr || finalOverlayInspector.card.width <= 0.0f) return;
	JPQuickImageLayerState *layer = finalLayerForBox(box);
	if (layer == nullptr) return;
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(COL_BG_INPUT);
	ofDrawRectRounded(finalOverlayInspector.card, 4.0f);
	ofNoFill();
	ofSetColor(COL_ACCENT_GOLD_DIM);
	ofDrawRectRounded(finalOverlayInspector.card, 4.0f);
	ofFill();

	// What is actually on screen. In a patched chain that is not this box, and
	// when the layer was excluded there is nothing on screen at all - saying so
	// beats a lit button over an empty output. Placement lives in the FINAL
	// panel, which is also where the stacking order is, so this says where in
	// the stack the layer sits rather than repeating a gizmo.
	int position = 0;
	for (size_t i = 0; i < finalQuickImages.layers.size(); ++i)
		if (finalQuickImages.layers[i].id == layer->id)
			position = (int)(finalQuickImages.layers.size() - i);
	const ResolvedOverlay *resolved = finalOverlayFor(*layer);
	string subject = "FINAL  #" + ofToString(position);
	bool live = resolved != nullptr && resolved->texture != nullptr;
	if (!live)
	{
		subject += !box->getonoff() ? "  (box is paused)" :
			(feedsActiveRender(box) ? "  (already the final output)" :
				"  (nothing to draw)");
	}
	else if (resolved->terminal != box)
	{
		string terminalName = resolved->terminal->name;
		const float nameRoom = finalOverlayInspector.card.width - 150.0f;
		while (terminalName.size() > 1 &&
			jp_constants::inspector_media_font.stringWidth(terminalName) > nameRoom)
			terminalName.pop_back();
		subject += "  " + terminalName;
	}
	ofSetColor(live ? COL_ACCENT_GOLD : COL_TEXT_MUTED);
	jp_constants::inspector_media_font.drawString(subject,
		finalOverlayInspector.card.x + 8,
		finalOverlayInspector.card.y + 20);

	const ofRectangle &bar = finalOverlayInspector.opacityBar;
	ofSetColor(COL_BG_DARK);
	ofDrawRectRounded(bar, 3.0f);
	ofSetColor(COL_ACCENT_GOLD);
	ofDrawRectRounded(ofRectangle(bar.x, bar.y,
		std::max(2.0f, bar.width * layer->opacity), bar.height), 3.0f);
	ofSetColor(COL_TEXT_SECONDARY);
	jp_constants::inspector_media_font.drawString(
		ofToString((int)std::round(layer->opacity * 100.0f)) + "%",
		bar.getRight() + 6, bar.getBottom() - 3);
	jp_tooltip::draw("Opacity of this layer over the final output", bar);

	const ofRectangle &follow = finalOverlayInspector.follow;
	const bool followHover = follow.inside(ofGetMouseX(), ofGetMouseY());
	ofSetColor(layer->followChain ? COL_ACCENT_CYAN_DIM :
		(followHover ? COL_BG_HOVER : COL_BG_DARK));
	ofDrawRectRounded(follow, 3.0f);
	ofSetColor(layer->followChain ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
	const string followLabel = layer->followChain ? "follow chain" : "pinned";
	jp_constants::inspector_media_font.drawString(followLabel,
		follow.getCenter().x -
			jp_constants::inspector_media_font.stringWidth(followLabel) * 0.5f,
		follow.getCenter().y + 4);
	jp_tooltip::draw(layer->followChain ?
		"Drawing the end of this box' chain - click to pin it to this box" :
		"Pinned to this box - click to follow its chain again", follow);
	ofPopStyle();
}

bool JPboxgroup::handleFinalOverlayInspectorClick()
{
	JPbox *box = getInspectorBox();
	if (box == nullptr || finalOverlayInspector.card.width <= 0.0f) return false;
	JPQuickImageLayerState *layer = finalLayerForBox(box);
	if (layer == nullptr) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	if (finalOverlayInspector.opacityBar.inside(mouse))
	{
		finalOverlayOpacityDragging = true;
		layer->opacity = ofClamp((mouse.x - finalOverlayInspector.opacityBar.x) /
			std::max(1.0f, finalOverlayInspector.opacityBar.width), 0.0f, 1.0f);
		return true;
	}
	if (finalOverlayInspector.follow.inside(mouse))
	{
		JPQuickImageStackState before = finalQuickImages;
		layer->followChain = !layer->followChain;
		pushFinalQuickImageHistory(before);
		return true;
	}
	return finalOverlayInspector.card.inside(mouse);
}

// --------------------------------------------------- membership in the stack

bool JPboxgroup::hasFinalLayerForBox(const JPbox *box) const
{
	return const_cast<JPboxgroup *>(this)->finalLayerForBox(box) != nullptr;
}

JPQuickImageLayerState *JPboxgroup::finalLayerForBox(const JPbox *box)
{
	if (box == nullptr || box->uid.empty()) return nullptr;
	for (auto &layer : finalQuickImages.layers)
		if (layer.sourceUid == box->uid) return &layer;
	return nullptr;
}

void JPboxgroup::addFinalLayerForBox(JPbox *box)
{
	if (box == nullptr || box->uid.empty()) return;
	if (hasFinalLayerForBox(box)) return;
	JPQuickImageStackState before = finalQuickImages;
	// Appended, so a box just sent to the final lands on TOP of what is
	// already there - the same reading as every layer stack.
	finalQuickImages.layers.push_back(
		jp_quick_image::makeBoxLayer(finalQuickImages, box->uid, box->name));
	quickImageSelectedId = finalQuickImages.layers.back().id;
	pushFinalQuickImageHistory(before);
}

void JPboxgroup::openFinalStackPanel()
{
	quickImagePanelOpen = true;
	quickImageListScroll = 0;
}

void JPboxgroup::removeFinalLayersForBox(const JPbox *box)
{
	if (box == nullptr || box->uid.empty()) return;
	JPQuickImageStackState before = finalQuickImages;
	auto &layers = finalQuickImages.layers;
	layers.erase(std::remove_if(layers.begin(), layers.end(),
		[&](const JPQuickImageLayerState &layer)
		{
			return layer.sourceUid == box->uid;
		}), layers.end());
	if (before == finalQuickImages) return;
	if (finalLayerById(quickImageSelectedId) == nullptr)
		quickImageSelectedId = layers.empty() ? 0 : layers.back().id;
	pushFinalQuickImageHistory(before);
}

// ------------------------------------------ layers leaving with their box

void JPboxgroup::captureFinalLayersForBox(const JPbox *box,
	vector<JPGraphDetachedFinalLayer> &out)
{
	if (box == nullptr || finalQuickImages.layers.empty()) return;
	vector<string> uids;
	collectSubtreeUids(box, uids);
	if (uids.empty()) return;

	auto &layers = finalQuickImages.layers;
	const size_t before = out.size();
	// Forward pass records the ORIGINAL index of each layer, so restoring can
	// insert them back in ascending order and land where they were. Erasing
	// happens afterwards, in one pass, for the same reason.
	for (size_t i = 0; i < layers.size(); ++i)
	{
		const JPQuickImageLayerState &layer = layers[i];
		if (layer.sourceUid.empty()) continue;
		if (std::find(uids.begin(), uids.end(), layer.sourceUid) == uids.end())
			continue;
		// The documented exception, kept: a layer that has a file of its own
		// degrades to that still image rather than leaving with the box.
		if (!layer.path.empty()) continue;
		JPGraphDetachedFinalLayer saved;
		saved.index = (int)i;
		saved.id = layer.id;
		saved.sourceUid = layer.sourceUid;
		saved.path = layer.path;
		saved.name = layer.name;
		saved.visible = layer.visible;
		saved.followChain = layer.followChain;
		saved.opacity = layer.opacity;
		saved.centerX = layer.center.x;
		saved.centerY = layer.center.y;
		saved.sizeX = layer.size.x;
		saved.sizeY = layer.size.y;
		saved.rotationDegrees = layer.rotationDegrees;
		saved.media = layer.media;
		out.push_back(saved);
	}
	if (out.size() == before) return;

	layers.erase(std::remove_if(layers.begin(), layers.end(),
		[&](const JPQuickImageLayerState &layer)
		{
			return !layer.sourceUid.empty() && layer.path.empty() &&
				std::find(uids.begin(), uids.end(), layer.sourceUid) !=
					uids.end();
		}), layers.end());
	forgetFinalStackHistory();
}

void JPboxgroup::restoreFinalLayers(
	const vector<JPGraphDetachedFinalLayer> &saved)
{
	if (saved.empty()) return;
	auto &layers = finalQuickImages.layers;
	for (const JPGraphDetachedFinalLayer &s : saved)
	{
		JPQuickImageLayerState layer;
		layer.id = s.id;
		layer.sourceUid = s.sourceUid;
		layer.path = s.path;
		layer.name = s.name;
		layer.visible = s.visible;
		layer.followChain = s.followChain;
		layer.opacity = s.opacity;
		layer.center.set(s.centerX, s.centerY);
		layer.size.set(s.sizeX, s.sizeY);
		layer.rotationDegrees = s.rotationDegrees;
		layer.media = s.media;
		const int index = (int)ofClamp((float)s.index, 0.0f,
			(float)layers.size());
		layers.insert(layers.begin() + index, layer);
		// The id was minted before the delete, so nextId is normally already
		// past it - unless a load() reset the counter in between.
		if (finalQuickImages.nextId <= s.id)
			finalQuickImages.nextId = s.id + 1;
	}
	forgetFinalStackHistory();
}

void JPboxgroup::dropFinalLayersForDestroyedBox(const JPbox *box)
{
	// For the paths that delete a box outright with no history entry to hold
	// its layers - the cue destroying what a cancelled cue had added. Same
	// removal, into a record nobody keeps.
	vector<JPGraphDetachedFinalLayer> discarded;
	captureFinalLayersForBox(box, discarded);
}

void JPboxgroup::pruneOrphanFinalLayers()
{
	auto &layers = finalQuickImages.layers;
	const size_t before = layers.size();
	layers.erase(std::remove_if(layers.begin(), layers.end(),
		[this](const JPQuickImageLayerState &layer)
		{
			if (layer.sourceUid.empty()) return false; // pure file layer
			if (!layer.path.empty()) return false;     // degrades to its image
			return findBoxByUid(layer.sourceUid) == nullptr;
		}), layers.end());
	if (layers.size() == before) return;
	if (finalLayerById(quickImageSelectedId) == nullptr)
		quickImageSelectedId = layers.empty() ? 0 : layers.back().id;
}

JPQuickImageLayerState *JPboxgroup::finalLayerById(uint64_t id)
{
	if (id == 0) return nullptr;
	for (auto &layer : finalQuickImages.layers)
		if (layer.id == id) return &layer;
	return nullptr;
}

// ------------------------------------------------------- MIDI entry points

// Resolved by NAME, like every other box-targeted MIDI action, but always to
// the LIVE box: the FINAL stack names boxes by uid and a cue draft clone is not
// in the graph the composite walks, so `getEditableBoxForRealIndex` - which the
// bypass and pause entry points use - would hand back a box the stack can never
// see. These run on the main thread: newMidiMessage only queues, and the queue
// is drained from ofApp::update.
JPbox *JPboxgroup::liveBoxByName(const string &boxName)
{
	const int index = findBoxIndexByName(boxName);
	if (index < 0 || index >= (int)boxes.size()) return nullptr;
	return boxes[index];
}

bool JPboxgroup::setFinalLayerForBoxName(const string &boxName, bool value)
{
	JPbox *box = liveBoxByName(boxName);
	if (box == nullptr) return false;
	if (value) addFinalLayerForBox(box);
	else removeFinalLayersForBox(box);
	return true;
}

bool JPboxgroup::toggleFinalLayerForBoxName(const string &boxName)
{
	JPbox *box = liveBoxByName(boxName);
	if (box == nullptr) return false;
	return setFinalLayerForBoxName(boxName, !hasFinalLayerForBox(box));
}


