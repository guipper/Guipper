#include "JPboxgroup.h"
#include "../JPutils/jp_constants.h"
#include "../JPutils/jp_tooltip.h"
#include <algorithm>

// The FINAL stack panel: the list of what is composited on top of the output.
//
// It used to be an image editor - files dropped in, moved and scaled on a
// preview, with their own transport. All of that now lives where it belongs: a
// layer is a BOX in the graph, so the file, the transport and any placement are
// the box's own, and this panel only answers "what is on top, and in what
// order".
namespace
{
	ofRectangle closeButton(float panelX, float panelY, float panelW)
	{
		return ofRectangle(panelX + panelW - 30.0f, panelY + 7.0f, 24.0f, 20.0f);
	}

	JPQuickImageLayerState *layerById(JPQuickImageStackState *stack,
		uint64_t id)
	{
		if (stack == nullptr) return nullptr;
		for (auto &layer : stack->layers) if (layer.id == id) return &layer;
		return nullptr;
	}
}

JPQuickImageStackState *JPboxgroup::quickImageStack()
{
	return &finalQuickImages;
}

const JPQuickImageStackState *JPboxgroup::quickImageStack() const
{
	return &finalQuickImages;
}

bool JPboxgroup::toggleQuickImageEditor()
{
	quickImagePanelOpen = !quickImagePanelOpen;
	if (!quickImagePanelOpen)
	{
		quickImageDrag = QUICK_IMAGE_DRAG_NONE;
		return true;
	}
	quickImageListScroll = 0;
	if (quickImageSelectedId == 0 && !finalQuickImages.layers.empty())
		quickImageSelectedId = finalQuickImages.layers.back().id;
	return true;
}

void JPboxgroup::clearQuickImageEditor()
{
	quickImagePanelOpen = false;
	quickImageDrag = QUICK_IMAGE_DRAG_NONE;
	quickImageSelectedId = 0;
}

ofRectangle JPboxgroup::getQuickImagePanelBounds() const
{
	return quickImagePanelOpen ? ofRectangle(quickImagePanelX,
		quickImagePanelY, quickImagePanelW, quickImagePanelH) : ofRectangle();
}

ofRectangle JPboxgroup::quickImagePreviewRect() const
{
	const float areaX = quickImagePanelX + 12.0f;
	const float areaY = quickImagePanelY + 38.0f;
	const float areaW = quickImagePanelW - 24.0f;
	const float areaH = std::max(120.0f, quickImagePanelH * 0.56f);
	const float canvasW = std::max(1, jp_constants::renderWidth);
	const float canvasH = std::max(1, jp_constants::renderHeight);
	const float scale = std::min(areaW / canvasW, areaH / canvasH);
	const float width = canvasW * scale, height = canvasH * scale;
	return ofRectangle(areaX + (areaW - width) * 0.5f,
		areaY + (areaH - height) * 0.5f, width, height);
}

void JPboxgroup::pushFinalQuickImageHistory(
	const JPQuickImageStackState &before)
{
	if (before == finalQuickImages) return;
	if (finalQuickImageHistory.empty())
	{
		finalQuickImageHistory.push_back(before);
		finalQuickImageHistoryCursor = 0;
	}
	else
	{
		finalQuickImageHistory.resize(finalQuickImageHistoryCursor + 1);
		finalQuickImageHistory[finalQuickImageHistoryCursor] = before;
	}
	finalQuickImageHistory.push_back(finalQuickImages);
	finalQuickImageHistoryCursor = finalQuickImageHistory.size() - 1;
	while (finalQuickImageHistory.size() > kMaxQuickImageHistory)
	{
		finalQuickImageHistory.erase(finalQuickImageHistory.begin());
		if (finalQuickImageHistoryCursor > 0) --finalQuickImageHistoryCursor;
	}
}

bool JPboxgroup::quickImageUndoShortcut(bool redo)
{
	if (!quickImagePanelOpen) return false;
	if (finalQuickImageHistory.empty()) return true;
	if (redo)
	{
		if (finalQuickImageHistoryCursor + 1 >= finalQuickImageHistory.size())
			return true;
		++finalQuickImageHistoryCursor;
	}
	else
	{
		if (finalQuickImageHistoryCursor == 0) return true;
		--finalQuickImageHistoryCursor;
	}
	finalQuickImages = finalQuickImageHistory[finalQuickImageHistoryCursor];
	jp_quick_image::normalize(finalQuickImages);
	if (layerById(&finalQuickImages, quickImageSelectedId) == nullptr)
		quickImageSelectedId = finalQuickImages.layers.empty() ? 0 :
			finalQuickImages.layers.back().id;
	return true;
}

ofFbo *JPboxgroup::finalCompositeFboOrNull()
{
	return finalCompositeActive && finalQuickImageFbo.isAllocated() ?
		&finalQuickImageFbo : nullptr;
}

const ofFbo *JPboxgroup::finalCompositeFboOrNull() const
{
	return const_cast<JPboxgroup *>(this)->finalCompositeFboOrNull();
}

// The final image every output shows: the active render (or the crossfade),
// then the FINAL stack on top. Composing into an FBO rather than onto a window
// is what makes the stack reach Spout, NDI and the PNG export, not just the
// preview.
void JPboxgroup::renderFinalComposite()
{
	finalCompositeActive = false;
	if (finalQuickImages.layers.empty() || boxes.empty() ||
		activerender == nullptr || *activerender < 0 ||
		*activerender >= (int)boxes.size()) return;
	const int width = std::max(1, jp_constants::renderWidth);
	const int height = std::max(1, jp_constants::renderHeight);
	if (!finalQuickImageFbo.isAllocated() ||
		finalQuickImageFbo.getWidth() != width ||
		finalQuickImageFbo.getHeight() != height)
		finalQuickImageFbo.allocate(width, height, GL_RGBA);
	finalQuickImageFbo.begin();
	ofClear(0, 0, 0, 0);
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofEnableBlendMode(OF_BLENDMODE_DISABLED);
	ofSetColor(255);
	if (transition.getLerpValue() < 1.0f && transition.isSourceAllocated())
		transition.draw(0, 0, width, height);
	else boxes[*activerender]->fbo.draw(0, 0, width, height);
	ofEnableAlphaBlending();
	// In list order, so the last layer of the stack is drawn last and lands on
	// top - which is why the panel shows the list upside down, newest first.
	finalQuickImageRenderer.draw(finalQuickImages, width, height, 0);
	ofPopStyle();
	finalQuickImageFbo.end();
	finalCompositeActive = true;
}

// Row geometry. The list is drawn TOP LAYER FIRST: the row at the top of the
// panel is the layer that covers the others, which is the only reading that
// matches what you see in the preview.
int JPboxgroup::quickImageVisibleRows() const
{
	const float listY = quickImagePreviewRect().getBottom() + 12.0f;
	return std::max(1, (int)((getQuickImagePanelBounds().getBottom() -
		listY - 10.0f) / 28.0f));
}

void JPboxgroup::drawQuickImagePanel()
{
	if (!quickImagePanelOpen) return;
	JPQuickImageStackState *stack = quickImageStack();
	const ofRectangle panel = getQuickImagePanelBounds();
	const ofRectangle preview = quickImagePreviewRect();
	const ofRectangle close = closeButton(panel.x, panel.y, panel.width);
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofEnableAlphaBlending();
	ofSetColor(0, 110); ofDrawRectRounded(panel.x + 4, panel.y + 5,
		panel.width, panel.height, 7);
	ofSetColor(COL_BG_TAB, 250); ofDrawRectRounded(panel, 7);
	ofNoFill(); ofSetColor(COL_ACCENT_CYAN); ofSetLineWidth(1.5f);
	ofDrawRectRounded(panel, 7); ofFill();
	ofSetColor(COL_ACCENT_CYAN);
	jp_constants::p_font.drawString("FINAL", panel.x + 11, panel.y + 22);
	ofSetColor(COL_ACCENT_RED);
	ofDrawLine(close.x + 6, close.y + 5, close.getRight() - 6,
		close.getBottom() - 5);
	ofDrawLine(close.getRight() - 6, close.y + 5, close.x + 6,
		close.getBottom() - 5);

	ofSetColor(COL_BG_DARK); ofDrawRectangle(preview);
	if (finalQuickImageFbo.isAllocated())
	{
		ofSetColor(255); finalQuickImageFbo.draw(preview);
	}
	ofNoFill(); ofSetColor(COL_BORDER_MUTED); ofDrawRectangle(preview); ofFill();

	if (stack->layers.empty())
	{
		ofSetColor(COL_TEXT_MUTED);
		jp_constants::p_font.drawString(
			"Send a box here with FINAL in its inspector",
			panel.x + 14, preview.getBottom() + 30);
		ofPopStyle();
		return;
	}

	const float listY = preview.getBottom() + 12.0f;
	const float rowH = 28.0f;
	const int rows = quickImageVisibleRows();
	const int count = (int)stack->layers.size();
	const int maxScroll = std::max(0, count - rows);
	quickImageListScroll = ofClamp(quickImageListScroll, 0, maxScroll);
	for (int slot = 0; slot < rows; ++slot)
	{
		// Top row is the LAST layer of the stack, i.e. the one drawn on top.
		const int i = count - 1 - (slot + quickImageListScroll);
		if (i < 0) break;
		const auto &layer = stack->layers[i];
		const ofRectangle row(panel.x + 12, listY + slot * rowH,
			panel.width - 24, rowH - 3);
		ofSetColor(layer.id == quickImageSelectedId ?
			ofColor(COL_ACCENT_CYAN, 50) : COL_BG_INPUT);
		ofDrawRectRounded(row, 3);
		ofSetColor(layer.visible ? COL_ACCENT_CYAN : COL_TEXT_MUTED);
		ofDrawCircle(row.x + 10, row.getCenter().y, 4);
		string label = layer.name;
		// Which box is really drawn: in a patched chain that is not the box the
		// layer names, and there is nowhere else to see it.
		if (const ResolvedOverlay *resolved = finalOverlayFor(layer))
		{
			if (resolved->texture == nullptr) label += "  (not drawing)";
			else if (resolved->terminal != nullptr &&
				resolved->terminal != resolved->source)
				label += "  > " + resolved->terminal->name;
		}
		const float labelRoom = (row.getRight() - 76.0f) - (row.x + 21.0f);
		while (label.size() > 1 &&
			jp_constants::p_font.stringWidth(label) > labelRoom)
			label.pop_back();
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString(label, row.x + 21, row.y + 17);
		ofSetColor(i + 1 < count ? COL_TEXT_SECONDARY : COL_TEXT_MUTED);
		jp_constants::p_font.drawString("^", row.getRight() - 62, row.y + 17);
		ofSetColor(i > 0 ? COL_TEXT_SECONDARY : COL_TEXT_MUTED);
		jp_constants::p_font.drawString("v", row.getRight() - 43, row.y + 17);
		ofSetColor(COL_ACCENT_RED);
		jp_constants::p_font.drawString("x", row.getRight() - 20, row.y + 17);
	}
	ofPopStyle();
}

bool JPboxgroup::update_quickImageMousePressed(int mouseButton)
{
	if (!quickImagePanelOpen || mouseButton != OF_MOUSE_BUTTON_LEFT ||
		!getQuickImagePanelBounds().inside(ofGetMouseX(), ofGetMouseY()))
		return false;
	JPQuickImageStackState *stack = quickImageStack();
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	const ofRectangle panel = getQuickImagePanelBounds();
	if (closeButton(panel.x, panel.y, panel.width).inside(mouse))
	{
		clearQuickImageEditor(); return true;
	}
	if (mouse.y <= panel.y + 31.0f)
	{
		quickImageDrag = QUICK_IMAGE_DRAG_PANEL;
		quickImageDragMouse = mouse;
		quickImagePanelDragOrigin.set(quickImagePanelX, quickImagePanelY);
		return true;
	}
	const float listY = quickImagePreviewRect().getBottom() + 12.0f;
	const float rowH = 28.0f;
	const int rows = quickImageVisibleRows();
	const int count = (int)stack->layers.size();
	for (int slot = 0; slot < rows; ++slot)
	{
		const int i = count - 1 - (slot + quickImageListScroll);
		if (i < 0) break;
		ofRectangle row(panel.x + 12, listY + slot * rowH,
			panel.width - 24, rowH - 3);
		if (!row.inside(mouse)) continue;
		JPQuickImageStackState before = *stack;
		quickImageSelectedId = stack->layers[i].id;
		if (mouse.x < row.x + 20)
			stack->layers[i].visible = !stack->layers[i].visible;
		// Up is up: swapping with the NEXT entry moves the layer later in the
		// draw order, which is exactly one step closer to the top of the list.
		else if (mouse.x >= row.getRight() - 70 &&
			mouse.x < row.getRight() - 52 && i + 1 < count)
			std::swap(stack->layers[i], stack->layers[i + 1]);
		else if (mouse.x >= row.getRight() - 52 &&
			mouse.x < row.getRight() - 31 && i > 0)
			std::swap(stack->layers[i], stack->layers[i - 1]);
		else if (mouse.x >= row.getRight() - 31)
			stack->layers.erase(stack->layers.begin() + i);
		pushFinalQuickImageHistory(before);
		return true;
	}
	return true;
}

bool JPboxgroup::update_quickImageMouseDragged(int mouseButton)
{
	if (!quickImagePanelOpen || mouseButton != OF_MOUSE_BUTTON_LEFT ||
		quickImageDrag != QUICK_IMAGE_DRAG_PANEL) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	quickImagePanelX = quickImagePanelDragOrigin.x + mouse.x - quickImageDragMouse.x;
	quickImagePanelY = quickImagePanelDragOrigin.y + mouse.y - quickImageDragMouse.y;
	quickImagePanelX = ofClamp(quickImagePanelX, 0.0f,
		std::max(0.0f, ofGetWidth() - quickImagePanelW));
	quickImagePanelY = ofClamp(quickImagePanelY, 0.0f,
		std::max(0.0f, ofGetHeight() - quickImagePanelH));
	return true;
}

bool JPboxgroup::update_quickImageMouseReleased(int mouseButton)
{
	if (mouseButton != OF_MOUSE_BUTTON_LEFT ||
		quickImageDrag == QUICK_IMAGE_DRAG_NONE) return false;
	quickImageDrag = QUICK_IMAGE_DRAG_NONE;
	return true;
}

bool JPboxgroup::quickImageKeyPressed(int key)
{
	if (!quickImagePanelOpen || (key != OF_KEY_DEL && key != OF_KEY_BACKSPACE))
		return false;
	JPQuickImageStackState *stack = quickImageStack();
	if (quickImageSelectedId == 0) return true;
	JPQuickImageStackState before = *stack;
	stack->layers.erase(std::remove_if(stack->layers.begin(), stack->layers.end(),
		[&](const JPQuickImageLayerState &layer) {
			return layer.id == quickImageSelectedId;
		}), stack->layers.end());
	quickImageSelectedId = stack->layers.empty() ? 0 : stack->layers.back().id;
	pushFinalQuickImageHistory(before);
	return true;
}

bool JPboxgroup::update_quickImageMouseScrolled(int x, int y, float scrollY)
{
	if (!quickImagePanelOpen || scrollY == 0.0f ||
		!getQuickImagePanelBounds().inside(x, y)) return false;
	JPQuickImageStackState *stack = quickImageStack();
	if (y < quickImagePreviewRect().getBottom() + 12.0f) return true;
	const int maxScroll = std::max(0,
		(int)stack->layers.size() - quickImageVisibleRows());
	quickImageListScroll = ofClamp(quickImageListScroll +
		(scrollY > 0.0f ? 1 : -1), 0, maxScroll);
	return true;
}
