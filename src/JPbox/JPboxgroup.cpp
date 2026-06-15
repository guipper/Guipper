#include "JPboxgroup.h"
#include <filesystem>
#include <algorithm>
#include <functional>


JPboxgroup::JPboxgroup() {}
JPboxgroup::~JPboxgroup()
{
	clearCue();
}

ofVec2f JPboxgroup::screenToCanvas(const ofVec2f &screen) const
{
	return (screen - viewportPan) / viewportZoom;
}

ofVec2f JPboxgroup::canvasToScreen(const ofVec2f &canvas) const
{
	return canvas * viewportZoom + viewportPan;
}

ofVec2f JPboxgroup::screenDeltaToCanvas(const ofVec2f &screenDelta) const
{
	return screenDelta / viewportZoom;
}

void JPboxgroup::zoomViewport(const ofVec2f &screenAnchor, float zoomFactor)
{
	ofVec2f canvasAnchor = screenToCanvas(screenAnchor);
	viewportZoom = ofClamp(viewportZoom * zoomFactor, 0.25f, 3.0f);
	viewportPan = screenAnchor - canvasAnchor * viewportZoom;
}

void JPboxgroup::panViewport(const ofVec2f &screenDelta)
{
	viewportPan += screenDelta;
}

string JPboxgroup::makeNameFromDirectory(const string &directory) const
{
	string nombre = directory.substr(directory.find_last_of("/\\") + 1, directory.size());
	nombre = nombre.substr(0, nombre.find(".mov"));
	nombre = nombre.substr(0, nombre.find(".mkv"));
	nombre = nombre.substr(0, nombre.find(".mp4"));
	nombre = nombre.substr(0, nombre.find(".avi"));
	nombre = nombre.substr(0, nombre.find(".vob"));
	nombre = nombre.substr(0, nombre.find(".flv"));
	nombre = nombre.substr(0, nombre.find(".jpg"));
	nombre = nombre.substr(0, nombre.find(".png"));
	nombre = nombre.substr(0, nombre.find(".jpeg"));
	nombre = nombre.substr(0, nombre.find(".frag"));
	nombre = nombre.substr(0, nombre.find(".xml"));
	if (directory.find("cam") != std::string::npos)
	{
		nombre = "CAMARITA";
	}
#ifdef SPOUT
	else if (directory.find("spoutReceiver") != std::string::npos)
	{
		nombre = "SPOUT";
	}
#endif
	else if (directory.find("framedifference") != std::string::npos)
	{
		nombre = "frameDif";
	}
#ifdef NDI
	else if (directory.find("ndiReceiver") != std::string::npos)
	{
		nombre = "NDI";
	}
#endif
	return nombre;
}

JPbox *JPboxgroup::createBoxForDirectory(const string &directory, string &name) const
{
	JPbox *bx = nullptr;
	if (directory.find(".frag") != std::string::npos)
	{
		bx = new JPbox_shader();
	}
	else if (directory.find(".jpg") != std::string::npos ||
			 directory.find(".png") != std::string::npos ||
			 directory.find(".jpeg") != std::string::npos)
	{
		bx = new JPbox_image();
	}
	else if (directory.find(".mov") != std::string::npos ||
			 directory.find(".mkv") != std::string::npos ||
			 directory.find(".mp4") != std::string::npos ||
			 directory.find(".flv") != std::string::npos ||
			 directory.find(".vob") != std::string::npos ||
			 directory.find(".avi") != std::string::npos)
	{
		bx = new JPbox_video();
	}
	else if (directory.find(".xml") != std::string::npos)
	{
		bx = new JPbox_preset();
	}
	else if (directory.find("cam") != std::string::npos)
	{
		bx = new JPbox_cam();
	}
#ifdef SPOUT
	else if (directory.find("spoutReceiver") != std::string::npos)
	{
		bx = new JPbox_spout();
	}
#endif
	else if (directory.find("framedifference") != std::string::npos)
	{
		bx = new JPbox_framedifference();
	}
#ifdef NDI
	else if (directory.find("ndiReceiver") != std::string::npos)
	{
		bx = new JPbox_ndi();
	}
#endif
	return bx;
}

string JPboxgroup::makeUniqueBoxName(const string &baseName) const
{
	string nombre = baseName;
	string nombreaux = nombre;
	bool existenombre = false;
	int counter = 2;
	do
	{
		existenombre = false;
		for (int i = 0; i < boxes.size(); i++)
		{
			if (boxes[i] != nullptr && nombre.compare(boxes[i]->name) == 0)
			{
				existenombre = true;
			}
		}
		if (existenombre)
		{
			nombre = nombreaux + ofToString(counter);
			counter++;
		}
	} while (existenombre);
	return nombre;
}


void JPboxgroup::setup(ofTrueTypeFont &_font, int &_activerender)
{
	font_p = &_font;
	activerender = &_activerender;

	//	cout << "WIIIIII " << jp_constants::renderWidth << endl;

	inspectorwindow_width = 450;
	inspectorwindow_x = ofGetWidth() - inspectorwindow_width / 2;
	inspectorwindow_y = inspectorwindow_height / 2;
	inspectorwindow_sepy = 30;
	inspectorwindow_height = 0; // Le tiro un valor solo para ver que onda.

	setinspectorsetactiveparams();

	// boxesdrawing.allocate(ofGetWidth(), ofGetHeight());

	offsetx = 0;
	offsety = 0;
	viewportZoom = 1.0f;
	viewportPan = ofVec2f(0, 0);
	viewportPanning = false;

	shaderboxagarrado = false;
	ouletagarrado = false;
	cualestaagarrado = -1;
	outlet_cualestaagarrado = -1;
	cueState = CueState();
	cueFullscreenPreview = false;
	cueMonitorMode = CUE_MONITOR_FINAL_OUTPUT;
	pendingCueRebuild = false;

	duration_mouseclick = 200;
	isDoubleClick = false;
	controllerselected = -1;

	transition.setup();
	lasttime_sequence = ofGetElapsedTimeMillis();
	activeSequence = false;
	durationGalleryMs = 1200.0f;
	setupGalleryDurationSlider();
	setupDefaultCuePanelLayout();
}
void JPboxgroup::draw()
{
	// boxesdrawing.draw(0, 0, ofGetWidth(), ofGetHeight());
	// boxesdrawing.draw(offsetx, offsety, ofGetWidth(), ofGetHeight());
	drawCuePreview();

	// Determine active box vector, render index, and inspector index
	vector<JPbox *> *activeBoxesPtr = &boxes;
	int activeRenderDisplayIndex = *activerender;
	int activeInspectorIndex = openguinumber;
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr)
		{
			activeBoxesPtr = &preset->boxes;
			activeRenderDisplayIndex = preset->activeRender;
			activeInspectorIndex = groupInspectorIndex;
		}
	}
	vector<JPbox *> &activeBoxes = *activeBoxesPtr;

	ofPushMatrix();
	ofTranslate(viewportPan.x, viewportPan.y);
	ofScale(viewportZoom, viewportZoom);
	JPdragobject::setMouseOverride(screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY())));

	// Draw connections (main view uses the dedicated function, group view draws inline)
	if (isGroupViewActive())
	{
		ofSetLineWidth(2);
		for (int i = (int)activeBoxes.size() - 1; i >= 0; i--)
		{
			for (int k = (int)activeBoxes.size() - 1; k >= 0; k--)
			{
				for (int l = activeBoxes[k]->fbohandlergroup.getSize() - 1; l >= 0; l--)
				{
					if (activeBoxes[k]->fbohandlergroup.getFboName(l) == activeBoxes[i]->name)
					{
						if (activeBoxes[i]->outletActiveFlag) {
							activeBoxes[i]->triangleangle = atan2(JPdragobject::getMouseY() - activeBoxes[i]->outlet_y,
								JPdragobject::getMouseX() - activeBoxes[i]->outlet_x);
						}
						else {
							if (activeBoxes[k]->fbohandlergroup.getSize() > 0) {
								activeBoxes[i]->triangleangle = atan2(activeBoxes[k]->fbohandlergroup.getPosY(l) - activeBoxes[i]->outlet_y,
									activeBoxes[k]->fbohandlergroup.getPosX(l) - activeBoxes[i]->outlet_x);
							}
							else {
								activeBoxes[i]->triangleangle = atan2(JPdragobject::getMouseY() - activeBoxes[i]->outlet_y,
									JPdragobject::getMouseX() - activeBoxes[i]->outlet_x);
							}
						}
						ofDrawLine(activeBoxes[k]->fbohandlergroup.getPosX(l),
							activeBoxes[k]->fbohandlergroup.getPosY(l),
							activeBoxes[i]->outlet_x + activeBoxes[i]->outlet_size / 2, activeBoxes[i]->outlet_y);
					}
				}
			}
		}
	}
	else
	{
		draw_conections();
	}

	// Selection rectangle (shared for both views)
	if (draw_SelectionRect) {
		ofSetColor(0, 180, 220, 45);
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofFill();
		ofVec2f center = ofVec2f((selectionEnd.x + lastMouseClick.x) / 2, (selectionEnd.y + lastMouseClick.y) / 2);
		float w = abs(selectionEnd.x - lastMouseClick.x);
		float h = abs(selectionEnd.y - lastMouseClick.y);
		ofDrawRectangle(center.x, center.y, w, h);
		ofNoFill();
		ofSetLineWidth(2);
		ofSetColor(0, 220, 255, 210);
		ofDrawRectangle(center.x, center.y, w, h);
		ofFill();
		ofSetLineWidth(1);
		ofSetRectMode(OF_RECTMODE_CORNER);
	}

	// Iterate over active boxes (works for both main and group view)
	for (int i = 0; i < (int)activeBoxes.size(); i++)
	{
		float x = activeBoxes[i]->x;
		float y = activeBoxes[i]->y + activeBoxes[i]->height / 2 - 8;
		ofSetRectMode(OF_RECTMODE_CENTER);

		// Green box: active render indicator
		{
			int displayIndex = activeRenderDisplayIndex;
			// Cue staged render only applies to main view
			if (!isGroupViewActive() && hasCue() &&
				cueState.stagedActiveRenderIndex >= 0 &&
				cueState.stagedActiveRenderIndex < (int)activeBoxes.size())
			{
				displayIndex = cueState.stagedActiveRenderIndex;
			}
			if (displayIndex == i) {
				ofSetColor(40, 210, 90, 210);
				ofRectMode(CENTER);
				ofRectRounded(x, y - activeBoxes[i]->height / 2 + 10, activeBoxes[i]->width * 1.1, activeBoxes[i]->height * 1.1, 10);
			}
		}

		// Main view specific: bypass/onoff blocking during selection rect
		if (!isGroupViewActive())
		{
			activeBoxes[i]->bypass.activable2 = !draw_SelectionRect;
			activeBoxes[i]->onoff.activable2 = !draw_SelectionRect;
		}

		// Main view specific: cue draft overlay
		if (!isGroupViewActive())
		{
			JPbox *draftBox = getCueDraftBoxForRealIndex(i);
			bool cueDraftBox = isCueSourceIndex(i);
			bool cueDirtyBox = isCueDraftDirty(i);
			if (cueDraftBox)
			{
				activeBoxes[i]->setBackgroundOverride(cueDirtyBox ? ofColor(238, 190, 24, 245) : ofColor(196, 178, 105, 235),
													   cueDirtyBox ? ofColor(255, 230, 85, 255) : ofColor(255, 205, 70, 225));
			}
			else
			{
				activeBoxes[i]->clearBackgroundOverride();
			}
			if (draftBox != nullptr)
			{
				bool realBypass = activeBoxes[i]->getBypass();
				bool realOnOff = activeBoxes[i]->getonoff();
				bool draftBypassBeforeDraw = draftBox->getBypass();
				bool draftOnOffBeforeDraw = draftBox->getonoff();
				activeBoxes[i]->setBypass(draftBypassBeforeDraw);
				activeBoxes[i]->setonoff(draftOnOffBeforeDraw);
				activeBoxes[i]->draw();
				activeBoxes[i]->clearBackgroundOverride();
				bool draftBypassAfterDraw = activeBoxes[i]->getBypass();
				bool draftOnOffAfterDraw = activeBoxes[i]->getonoff();
				if (draftBypassAfterDraw != draftBypassBeforeDraw)
				{
					draftBox->setBypass(draftBypassAfterDraw);
					markCueDraftDirty(i, CUE_DIRTY_BYPASS_PAUSE);
				}
				if (draftOnOffAfterDraw != draftOnOffBeforeDraw)
				{
					draftBox->setonoff(draftOnOffAfterDraw);
					markCueDraftDirty(i, CUE_DIRTY_BYPASS_PAUSE);
				}
				activeBoxes[i]->setBypass(realBypass);
				activeBoxes[i]->setonoff(realOnOff);
			}
			else
			{
				activeBoxes[i]->draw();
			}
		}
		else
		{
			// Group view: simple draw, no cue draft
			activeBoxes[i]->draw();
		}

		// Cyan outline: multi-selected box (shared)
		if (isBoxSelected(i))
		{
			ofPushStyle();
			ofSetRectMode(OF_RECTMODE_CENTER);
			ofNoFill();
			ofSetLineWidth(4);
			ofSetColor(0, 220, 255, 255);
			ofDrawRectRounded(activeBoxes[i]->x, activeBoxes[i]->y, activeBoxes[i]->width + 14, activeBoxes[i]->height + 14, 10);
			ofFill();
			ofPopStyle();
		}

		// Inspector outline (openguinumber for main, groupInspectorIndex for group)
		if (activeInspectorIndex == i)
		{
			ofPushStyle();
			ofSetRectMode(OF_RECTMODE_CENTER);
			ofNoFill();
			ofSetLineWidth(3);
			ofSetColor(0, 220, 255, 255);
			ofDrawRectRounded(x, y - activeBoxes[i]->height / 2 + 10, activeBoxes[i]->width * 1.22, activeBoxes[i]->height * 1.22, 10);
			ofPopStyle();
		}

		// Main view specific: cue added "NEW" label
		if (!isGroupViewActive() && isCueAddedRealIndex(i))
		{
			ofPushStyle();
			ofSetColor(30, 20, 0, 230);
			ofDrawRectangle(x - activeBoxes[i]->width / 2 + 6, y - activeBoxes[i]->height / 2 + 16, 30, 13);
			ofSetColor(255, 230, 80, 255);
			ofDrawBitmapString("NEW", x - activeBoxes[i]->width / 2 + 9, y - activeBoxes[i]->height / 2 + 27);
			ofPopStyle();
		}

		ofDrawBitmapString(ofToString(i), x, y);
	}
	JPdragobject::clearMouseOverride();
	ofPopMatrix();
	drawTabs();
	draw_paramswindow();
	drawGalleryDurationSlider();

}
void JPboxgroup::drawCuePreview()
{
	JPbox *previewBox = getCuePreviewBox();
	if (previewBox == nullptr)
	{
		return;
	}

	clampCuePanelLayout();
	const float pad = 12.0f;
	const float headerH = 30.0f;
	const float handleSize = 16.0f;
	const float iconSize = 18.0f;
	const float iconGap = 8.0f;
	const float previewAreaX = cuePanelX + pad;
	const float previewAreaY = cuePanelY + headerH + pad * 0.5f;
	const float previewAreaW = cuePanelW - pad * 2.0f;
	const float previewAreaH = cuePanelH - headerH - pad * 1.5f;
	float previewW = previewAreaW;
	float previewH = previewW * 9.0f / 16.0f;
	if (previewH > previewAreaH)
	{
		previewH = previewAreaH;
		previewW = previewH * 16.0f / 9.0f;
	}
	const float previewX = previewAreaX + (previewAreaW - previewW) * 0.5f;
	const float previewY = previewAreaY + (previewAreaH - previewH) * 0.5f;

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(18, 20, 22, 230);
	ofDrawRectRounded(cuePanelX, cuePanelY, cuePanelW, cuePanelH, 6);
	ofSetColor(30, 25, 12, 235);
	ofDrawRectRounded(cuePanelX, cuePanelY, cuePanelW, headerH, 6);
	ofNoFill();
	ofSetLineWidth(2);
	ofSetColor(255, 180, 0, 230);
	ofDrawRectRounded(cuePanelX, cuePanelY, cuePanelW, cuePanelH, 6);
	ofFill();

	ofSetColor(255, 180, 0);
	string displayName = previewBox->name;
	if (cueMonitorMode == CUE_MONITOR_SELECTED_BOX &&
		openguinumber >= 0 && openguinumber < boxes.size())
	{
		displayName = boxes[openguinumber]->name;
	}
	else if (isCueDraftMode() &&
			 cueState.draftOutputRealIndex >= 0 &&
			 cueState.draftOutputRealIndex < boxes.size())
	{
		displayName = boxes[cueState.draftOutputRealIndex]->name;
	}
	if (cueFullscreenPreview && boxes.size() > 0 && *activerender >= 0 && *activerender < boxes.size())
	{
		displayName = boxes[*activerender]->name;
	}
	if (isCueDraftMode() && !cueFullscreenPreview)
	{
		displayName += getCueDirtySummary();
	}
	string cueName = displayName;
	string panelMode = cueFullscreenPreview ? "LIVE OUTPUT" :
					   (cueMonitorMode == CUE_MONITOR_SELECTED_BOX ? "CUE SELECT" :
						(isCueDraftMode() ? "CUE OUTPUT" : "CUE PREVIEW"));
	string cuePrefix = panelMode + " - ";
	string title = cuePrefix + cueName;
	float maxNameWidth = cuePanelW - pad * 2.0f - iconSize * 4.0f - iconGap * 4.0f - jp_constants::p_font.stringWidth(cuePrefix);
	while (!cueName.empty() && jp_constants::p_font.stringWidth(cueName) > maxNameWidth)
	{
		cueName.pop_back();
	}
	if (cueName != displayName)
	{
		cueName += "...";
	}
	title = cuePrefix + cueName;
	jp_constants::p_font.drawString(title, cuePanelX + pad, cuePanelY + 21);

	const float closeX = cuePanelX + cuePanelW - pad - iconSize;
	const float fullX = closeX - iconGap - iconSize;
	const float monitorX = fullX - iconGap - iconSize;
	const float applyX = monitorX - iconGap - iconSize;
	const float iconY = cuePanelY + (headerH - iconSize) * 0.5f;
	ofNoFill();
	ofSetLineWidth(1.5f);
	ofSetColor(255, 215, 120, 230);
	ofDrawRectRounded(monitorX, iconY, iconSize, iconSize, 3);
	ofDrawRectRounded(applyX, iconY, iconSize, iconSize, 3);
	ofDrawRectRounded(fullX, iconY, iconSize, iconSize, 3);
	if (cueFullscreenPreview)
	{
		ofFill();
		ofSetColor(255, 180, 0, 210);
		ofDrawRectRounded(fullX + 1, iconY + 1, iconSize - 2, iconSize - 2, 3);
		ofNoFill();
	}
	ofColor monitorGlyphColor(255, 215, 120, 230);
	ofColor swapGlyphColor = cueFullscreenPreview ? ofColor(24, 20, 10, 245) : ofColor(255, 215, 120, 230);
	ofSetColor(monitorGlyphColor);
	if (cueMonitorMode == CUE_MONITOR_SELECTED_BOX)
	{
		ofDrawRectangle(monitorX + 4, iconY + 4, iconSize - 8, iconSize - 8);
		ofDrawLine(monitorX + 5, iconY + 5, monitorX + 8, iconY + 5);
		ofDrawLine(monitorX + 5, iconY + 5, monitorX + 5, iconY + 8);
		ofDrawLine(monitorX + iconSize - 5, iconY + 5, monitorX + iconSize - 8, iconY + 5);
		ofDrawLine(monitorX + iconSize - 5, iconY + 5, monitorX + iconSize - 5, iconY + 8);
		ofDrawLine(monitorX + 5, iconY + iconSize - 5, monitorX + 8, iconY + iconSize - 5);
		ofDrawLine(monitorX + 5, iconY + iconSize - 5, monitorX + 5, iconY + iconSize - 8);
		ofDrawLine(monitorX + iconSize - 5, iconY + iconSize - 5, monitorX + iconSize - 8, iconY + iconSize - 5);
		ofDrawLine(monitorX + iconSize - 5, iconY + iconSize - 5, monitorX + iconSize - 5, iconY + iconSize - 8);
		ofDrawLine(monitorX + iconSize * 0.5f - 2, iconY + iconSize * 0.5f, monitorX + iconSize * 0.5f + 2, iconY + iconSize * 0.5f);
		ofDrawLine(monitorX + iconSize * 0.5f, iconY + iconSize * 0.5f - 2, monitorX + iconSize * 0.5f, iconY + iconSize * 0.5f + 2);
	}
	else
	{
		ofDrawRectangle(monitorX + 4, iconY + 4, iconSize - 8, iconSize - 8);
		ofFill();
		ofDrawCircle(monitorX + iconSize * 0.5f, iconY + iconSize * 0.5f, 2.2f);
		ofNoFill();
	}
	ofDrawLine(applyX + 4, iconY + 10, applyX + 8, iconY + 14);
	ofDrawLine(applyX + 8, iconY + 14, applyX + iconSize - 4, iconY + 4);
	ofSetColor(swapGlyphColor);
	ofDrawRectangle(fullX + 4, iconY + 4, 6, 5);
	ofDrawRectangle(fullX + 8, iconY + 9, 6, 5);
	ofDrawLine(fullX + 5, iconY + 12, fullX + 13, iconY + 5);
	ofDrawLine(fullX + 13, iconY + 5, fullX + 10, iconY + 5);
	ofDrawLine(fullX + 13, iconY + 5, fullX + 13, iconY + 8);
	ofDrawLine(fullX + 13, iconY + 7, fullX + 5, iconY + 14);
	ofDrawLine(fullX + 5, iconY + 14, fullX + 8, iconY + 14);
	ofDrawLine(fullX + 5, iconY + 14, fullX + 5, iconY + 11);
	ofSetColor(255, 215, 120, 230);
	ofDrawLine(closeX + 4, iconY + 4, closeX + iconSize - 4, iconY + iconSize - 4);
	ofDrawLine(closeX + iconSize - 4, iconY + 4, closeX + 4, iconY + iconSize - 4);
	ofFill();

	ofSetColor(255);
	if (cueFullscreenPreview)
	{
		drawLiveOutput(previewX, previewY, previewW, previewH);
	}
	else
	{
		previewBox->fbo.draw(previewX, previewY, previewW, previewH);
	}
	ofSetColor(cueFullscreenPreview ? ofColor(0, 220, 255, 235) : ofColor(255, 180, 0, 235));
	ofDrawRectangle(previewX, previewY, previewW, 22);
	ofSetColor(18, 20, 22, 245);
	jp_constants::p_font.drawString(panelMode, previewX + 8, previewY + 16);

	ofSetColor(255, 180, 0, 220);
	ofDrawLine(cuePanelX + cuePanelW - handleSize, cuePanelY + cuePanelH - 4,
			   cuePanelX + cuePanelW - 4, cuePanelY + cuePanelH - handleSize);
	ofDrawLine(cuePanelX + cuePanelW - handleSize * 0.65f, cuePanelY + cuePanelH - 4,
			   cuePanelX + cuePanelW - 4, cuePanelY + cuePanelH - handleSize * 0.65f);
	ofPopStyle();
}
void JPboxgroup::setupDefaultCuePanelLayout()
{
	const float margin = 24.0f;
	const float headerH = 30.0f;
	const float minW = 260.0f;
	cuePanelW = std::max(minW, ofGetWidth() * 0.4f);
	cuePanelH = headerH + (cuePanelW - margin) * 9.0f / 16.0f;
	cuePanelX = margin;
	cuePanelY = ofGetHeight() - cuePanelH - margin;
	clampCuePanelLayout();
}

void JPboxgroup::clampCuePanelLayout()
{
	const float minW = 260.0f;
	const float minH = 170.0f;
	const float margin = 8.0f;
	float maxW = std::max(minW, ofGetWidth() - margin * 2.0f);
	float maxH = std::max(minH, ofGetHeight() - margin * 2.0f);

	cuePanelW = ofClamp(cuePanelW, minW, maxW);
	cuePanelH = ofClamp(cuePanelH, minH, maxH);
	cuePanelX = ofClamp(cuePanelX, margin, std::max(margin, ofGetWidth() - cuePanelW - margin));
	cuePanelY = ofClamp(cuePanelY, margin, std::max(margin, ofGetHeight() - cuePanelH - margin));
}

bool JPboxgroup::mouseOverCueHeader() const
{
	const float headerH = 30.0f;
	return ofGetMouseX() >= cuePanelX &&
		   ofGetMouseX() <= cuePanelX + cuePanelW &&
		   ofGetMouseY() >= cuePanelY &&
		   ofGetMouseY() <= cuePanelY + headerH;
}

bool JPboxgroup::mouseOverCueResizeHandle() const
{
	const float handleSize = 24.0f;
	return ofGetMouseX() >= cuePanelX + cuePanelW - handleSize &&
		   ofGetMouseX() <= cuePanelX + cuePanelW &&
		   ofGetMouseY() >= cuePanelY + cuePanelH - handleSize &&
		   ofGetMouseY() <= cuePanelY + cuePanelH;
}

bool JPboxgroup::mouseOverCueCloseIcon() const
{
	const float pad = 12.0f;
	const float headerH = 30.0f;
	const float iconSize = 18.0f;
	const float closeX = cuePanelX + cuePanelW - pad - iconSize;
	const float iconY = cuePanelY + (headerH - iconSize) * 0.5f;
	return ofGetMouseX() >= closeX &&
		   ofGetMouseX() <= closeX + iconSize &&
		   ofGetMouseY() >= iconY &&
		   ofGetMouseY() <= iconY + iconSize;
}

bool JPboxgroup::mouseOverCueFullscreenIcon() const
{
	const float pad = 12.0f;
	const float headerH = 30.0f;
	const float iconSize = 18.0f;
	const float iconGap = 8.0f;
	const float closeX = cuePanelX + cuePanelW - pad - iconSize;
	const float fullX = closeX - iconGap - iconSize;
	const float iconY = cuePanelY + (headerH - iconSize) * 0.5f;
	return ofGetMouseX() >= fullX &&
		   ofGetMouseX() <= fullX + iconSize &&
		   ofGetMouseY() >= iconY &&
		   ofGetMouseY() <= iconY + iconSize;
}

bool JPboxgroup::mouseOverCueApplyIcon() const
{
	const float pad = 12.0f;
	const float headerH = 30.0f;
	const float iconSize = 18.0f;
	const float iconGap = 8.0f;
	const float closeX = cuePanelX + cuePanelW - pad - iconSize;
	const float fullX = closeX - iconGap - iconSize;
	const float monitorX = fullX - iconGap - iconSize;
	const float applyX = monitorX - iconGap - iconSize;
	const float iconY = cuePanelY + (headerH - iconSize) * 0.5f;
	return ofGetMouseX() >= applyX &&
		   ofGetMouseX() <= applyX + iconSize &&
		   ofGetMouseY() >= iconY &&
		   ofGetMouseY() <= iconY + iconSize;
}

bool JPboxgroup::mouseOverCueMonitorModeIcon() const
{
	const float pad = 12.0f;
	const float headerH = 30.0f;
	const float iconSize = 18.0f;
	const float iconGap = 8.0f;
	const float closeX = cuePanelX + cuePanelW - pad - iconSize;
	const float fullX = closeX - iconGap - iconSize;
	const float monitorX = fullX - iconGap - iconSize;
	const float iconY = cuePanelY + (headerH - iconSize) * 0.5f;
	return ofGetMouseX() >= monitorX &&
		   ofGetMouseX() <= monitorX + iconSize &&
		   ofGetMouseY() >= iconY &&
		   ofGetMouseY() <= iconY + iconSize;
}
void JPboxgroup::setupGalleryDurationSlider()
{
	const float sliderWidth = 320.0f;
	const float sliderHeight = 20.0f;
	const float sliderX = sliderWidth * 0.5f + 30.0f;
	const float sliderY = 28.0f;

	const float clampedDuration = ofClamp(durationGalleryMs, 0.0f, 4200.0f);
	galleryDurationParam.setup(clampedDuration, "durationgallery_ms");
	galleryDurationParam.min = 0.0f;
	galleryDurationParam.max = 4200.0f;
	galleryDurationParam.movtype = JPParameter::STANDART;

	galleryDurationSlider.setup(sliderX, sliderY, sliderWidth, sliderHeight,
		0.0f, 4200.0f, clampedDuration, "durationgallery_ms");
	galleryDurationSlider.setParametersPointer(&galleryDurationParam);
	galleryDurationSlider.activable2 = true;
}
void JPboxgroup::drawGalleryDurationSlider()
{
	if (!activeSequence)
	{
		return;
	}

	if (!galleryDurationSlider.activeFlag)
	{
		const float clampedDuration = ofClamp(durationGalleryMs, 0.0f, 4200.0f);
		galleryDurationParam.floatValue = clampedDuration;
		galleryDurationParam.floatLerpValue = clampedDuration;
	}

	galleryDurationSlider.activable2 = true;
	galleryDurationSlider.draw();

	const float newDuration = ofClamp(galleryDurationParam.floatValue, 0.0f, 4200.0f);
	durationGalleryMs = newDuration;
}
void JPboxgroup::draw_activerender()
{
	if (boxes.size() >= 1)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		//

		//if (boxes.size() > 2){
			transition.draw(0, 0, ofGetWidth() * 2., ofGetHeight() * 2.);
		//}
		//else {
			//boxes[*activerender]->fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
		//}
	}
}
void JPboxgroup::draw_activerender(float _width, float _height)
{
	drawLiveOutput(0, 0, _width, _height);
}

void JPboxgroup::drawNodeEditorBackground(float _width, float _height)
{
	JPbox *previewBox = getCuePreviewBox();
	if (cueFullscreenPreview && previewBox != nullptr)
	{
		ofSetColor(255, 255);
		ofSetRectMode(OF_RECTMODE_CORNER);
		previewBox->fbo.draw(0, 0, _width, _height);
		return;
	}
	drawLiveOutput(0, 0, _width, _height);
}

void JPboxgroup::drawLiveOutput(float x, float y, float w, float h)
{
	if (boxes.size() >= 1)
	{
		ofSetRectMode(OF_RECTMODE_CORNER);
		//boxes[*activerender]->fbo.draw(0, 0, w, h);

		//QUE ONDA QUE LO TENGO QUE DIBUJAR DIFERENTE!?
		if (boxes.size() > 2) {
			transition.draw(x, y, w, h);
		}
		else {
			boxes[*activerender]->fbo.draw(x, y, w, h);
		}
		transition.draw(x, y, w, h);
	}
}
void JPboxgroup::draw_paramswindow()
{

	JPbox *inspectorBox = getInspectorBox();
	if (inspectorBox != nullptr)
	{
		/*//CUADRADO VERDE
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofSetColor(255, 255);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);

		//CUADRADO NEGRO ENCIMA
		float ancho2 = inspectorwindow_width * 0.98;
		float alto2 = inspectorwindow_height * 0.98;
		ofSetColor(0);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, ancho2, alto2);
		*/
		ofSetColor(120, 255);
		// constants_img::background.draw(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);

		string name = getCueDraftBoxForRealIndex(openguinumber) != nullptr ? inspectorBox->name + " DRAFT" : inspectorBox->name;
		ofSetColor(255);

		// DIBUJAR EL NOMBRE DEL SHADER
		// jp_constants::p2_font
		jp_constants::h_font.drawString(name,
										inspectorwindow_x - jp_constants::h_font.stringWidth(name) / 2,
										inspectorwindow_sepy); // El y de esto esta puesto medio frutanga

		// RDM button to the right of the shader name
		{
			float nameW = jp_constants::h_font.stringWidth(name);
			float nameRight = inspectorwindow_x - nameW / 2 + nameW + 12;
			float btnW = 48;
			float btnH = 22;
			float btnY = inspectorwindow_sepy - btnH / 2 + 1;
			inspectorrandom.x = nameRight + btnW / 2;
			inspectorrandom.y = btnY;
			inspectorrandom.width = btnW;
			inspectorrandom.height = btnH;

			// Draw RDM button background
			if (inspectorrandom.mouseOver()) {
				ofSetColor(120, 60, 180);
			} else {
				ofSetColor(80, 40, 120);
			}
			ofDrawRectRounded(nameRight, btnY, btnW, btnH, 3.0f);
			ofSetColor(200, 140, 255);
			ofNoFill();
			ofSetLineWidth(1.0f);
			ofDrawRectRounded(nameRight, btnY, btnW, btnH, 3.0f);
			ofFill();
			ofSetLineWidth(1.0f);
			ofSetColor(255);
			jp_constants::p_font.drawString("RDM",
				nameRight + btnW / 2 - jp_constants::p_font.stringWidth("RDM") / 2,
				btnY + btnH / 2 + jp_constants::p_font.stringHeight("RDM") / 2 - 2);
		}

		int index = 0; // INDICE PARA LOS BOTONES :
		for (int i = 0; i < controllers.size(); i++)
		{
			controllers[i]->draw();
			/*if (controllers[i]->controllertype == controllers[i]->SLIDER) {
				botones_modo[index]->draw();
				//OSEA SI NO ES EL REGULAR :
				if(botones_modo[index]->
				!= 0){
					botones_speed[index]->draw();
					if (!botones_speed[index]->boolValue) {
						sliders_speed[index]->draw();
					}
				}
				index++;
				if (botones_speed[index]->boolValue) {
					sliders_speed[index]->draw()
				}
			}*/
		}
	}
}
void JPboxgroup::draw_conections()
{
	// DRAW CONECTIONS :
	ofSetLineWidth(2);
	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		for (int k = boxes.size() - 1; k >= 0; k--)
		{
			for (int l = boxes[k]->fbohandlergroup.getSize() - 1; l >= 0; l--)
			{
				if (boxes[k]->fbohandlergroup.getFboName(l) ==
					boxes[i]->name)
				{

					

					// boxes[i]->triangleangle+= 1;

					//Es muy caro llamar atan2 todos los frames por todas las cajitas? 
					//Creo que hay una manera de optimizar este codigo
					if (boxes[i]->outletActiveFlag) {
						boxes[i]->triangleangle = atan2(JPdragobject::getMouseY() - boxes[i]->outlet_y,
						JPdragobject::getMouseX() - boxes[i]->outlet_x);
					}else{
						if(boxes[k]->fbohandlergroup.getSize() > 0){
							boxes[i]->triangleangle = atan2(boxes[k]->fbohandlergroup.getPosY(l) - boxes[i]->outlet_y,
							boxes[k]->fbohandlergroup.getPosX(l) - boxes[i]->outlet_x);
						}else {
							boxes[i]->triangleangle = atan2(JPdragobject::getMouseY() - boxes[i]->outlet_y,
								JPdragobject::getMouseX() - boxes[i]->outlet_x);
						}
					}

					ofDrawLine(boxes[k]->fbohandlergroup.getPosX(l),
						boxes[k]->fbohandlergroup.getPosY(l),
						boxes[i]->outlet_x + boxes[i]->outlet_size / 2, boxes[i]->outlet_y);
				}
				else
				{
				}
			}
		}
	}
}
void JPboxgroup::update(){

	// bool unoagarrado = false;
	update_paramswindow();
	ofVec2f canvasMouse = screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY()));
	
	// Update sub-boxes when in group view mode
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr)
		{
			for (int i = (int)preset->boxes.size() - 1; i >= 0; i--)
			{
				preset->boxes[i]->update();
				// Handle box grabbing for sub-boxes
				if (ofGetMousePressed() && !viewportPanning){
					JPdragobject::setMouseOverride(canvasMouse);
					if (preset->boxes[i]->mouseOverOutlet() && !ouletagarrado && !shaderboxagarrado){
						preset->boxes[i]->activeFlag = false;
						preset->boxes[i]->outletActiveFlag = true;
						ouletagarrado = true;
						shaderboxagarrado = false;
						outlet_cualestaagarrado = i;
						cualestaagarrado = -1;
					}
					else if (preset->boxes[i]->mouseOver() && !ouletagarrado && !shaderboxagarrado){
						// Select sub-box for inspector (use groupInspectorIndex, NOT openguinumber)
						if (groupInspectorIndex != i)
						{
							groupInspectorIndex = i;
							groupPreviewBoxIndex = -1;
							setControllers();
						}
						cualestaagarrado = i;
						outlet_cualestaagarrado = -1;
						ouletagarrado = false;
						shaderboxagarrado = true;
						preset->boxes[i]->activeFlag = true;
					}
					JPdragobject::clearMouseOverride();
				}
			}
		}
		// Do NOT return here - main boxes must keep updating even in group view
	}

	float lerpAmount = 0.3;
	for (int i = boxes.size() - 1; i >= 0; i--){
		boxes[i]->update();
		// boxes[i]->parameters.update(); //La mutie y no paso nada
		for (int k = boxes[i]->parameters.getSize() - 1; k >= 0; k--){
		}
		if (openguinumber == i)
		{
			for (int k = boxes[i]->parameters.getSize() - 1; k >= 0; k--)
			{
				// VAMOS A PROBAR MUTEAR ESTO A VER
				// controllers.at(k)->value = boxes[i]->parameters.getFloatValue(k);
			}
		}
		// ESTO ES PARA QUE EL SLIDER QUE REPRESENTA LA BARRA DE TIEMPO DE LOS VIDEOS
		// SE ACTUALICE
		if (boxes[i]->getTipo() == boxes[i]->VIDEOBOX && openguinumber == i &&
			!controllers.at(6)->mouseOver()){
			controllers.at(6)->value = boxes[i]->parameters.getFloatValue(6);
		}

		// PARA AGARRAR LAS CAJITAS :
		if (ofGetMousePressed() && !draw_SelectionRect && !viewportPanning){
			JPdragobject::setMouseOverride(canvasMouse);
			if (boxes[i]->mouseOverOutlet() && !ouletagarrado && !shaderboxagarrado){
				boxes[i]->activeFlag = false;
				boxes[i]->outletActiveFlag = true;
				ouletagarrado = true;
				shaderboxagarrado = false;
				outlet_cualestaagarrado = i;
				cualestaagarrado = -1;
			}
			else if (boxes[i]->mouseOver() && !ouletagarrado && !shaderboxagarrado){
				cualestaagarrado = i;
				outlet_cualestaagarrado = -1;
				ouletagarrado = false;
				shaderboxagarrado = true;
				boxes[i]->activeFlag = true;
			}
			JPdragobject::clearMouseOverride();
		}
		// PARA QUE RECARGUE EL SHADER AUTOMATICAMENTE PAP�.
		if (!jp_constants::systemDialog_open && boxes[i]->getTipo() == boxes[i]->SHADERBOX){
			if (ofFile(boxes[i]->dir).exists()){
				auto lasttimemodified = std::filesystem::last_write_time(ofToDataPath(boxes[i]->dir));
				if (lasttimemodified != boxes[i]->datemodified){
					//	cout << "RELOAD SHADER " << endl;
					// cout << "-------------------------------------" << endl;
					boxes[i]->datemodified = lasttimemodified;

					// UF ESTO ESTA ATADO CON ALAMBRE MUY FUERTE. ACA HAY UN BUG QUE LO QUE HACE ES QUE NO RECARGUE BIEN EL SHADER.
					// BASICAMENTE LO QUE SUCEDE ES QUE CUANDO VOLVES A CARGAR Y GUARDAR A VECES NO LEVANTA LOS PARAMETROS
					// ENTONCES LE DIGO QUE LO REINICIE HASTA QUE LA CANTIDAD DE PARAMETROS SEA COMO LA CORRECTA DIGAMOS.
					// OSEA TECNICAMENTE EXISTE LA POSIBILIDAD 0.00000000000000000001% DE QUE NUNCA CARGUE BIEN Y ENTRE EN UN LOOP INFINITO DE MUERTE Y DESTRUCCION.
					// OSEA AHORA AL MENOS CARGA BIEN SIEMPRE. LO QUE NO PUEDO HACER ES QUE ME VUELVA A CARGAR LOS VALORES QUE TENIA CON LOS RENDER QUE TENIA.

					// cout << "Parameter a size :" << boxes[i]->parameters.getSize() << endl;
					JPbox *aux;

					boxes[i]->reload();
					cout << "RElOAD SHADER" << endl;
					// cout << "Parameter d size :" << boxes[i]->parameters.getSize() << endl;

					// boxes[i]->reloadShaderonly();
					// Este contador de uniforms hace que no crashee nada.
					int counter = 0;
					/*for (int j = 0; j < boxes[i]->parameters.getSize(); j++) {
						counter++;
					}*/
					// cout << "CONTADOR DE UNIFORMS " << counter << endl;

					if (openguinumber == i){
						setControllers();
					}
					cout << "-------------------------------------" << endl;
				}
			}
		}
	}
	processPendingCueRebuild();
	processPendingCueApply();
	if (isCueDraftMode())
	{
		updateCueDraftGraph();
	}
	// SUELTA LAS CAJITAS Y EL SELECTION RECT
	if (!ofGetMousePressed())
	{
		shaderboxagarrado = false;
		ouletagarrado = false;
		cualestaagarrado = -1;
		outlet_cualestaagarrado = -1;
		draw_SelectionRect = false;
		viewportPanning = false;
		lastMouseClick = canvasMouse;
	}
	// ESTO HABLA DE LO MAL QUE PROGRAMAS : MIRA MIRA LO QUE ES ESTO SE FUE A LA MEIRDA EL CODIGO :
	// LA VARIABLE NEEDSUPDATE DETERMINA SI EL BOTON FUE APRETADO Y SI NECEITA ACTUALIZARSE LO HACE Y LA VUELVE A SETEAR A FALSE
	// ACA LO QUE PASA ES QUE ESTO SOLUCIONA EL TEMITA DEL SYNC CUANDO SE DESPLIEGA EL BOTON PARA QUE NO QUEDE EN CUALQUIERA
	// OSEA EL NEEDSUPDATE COMUNICA QUE EL BOTON DEL COLLAPSE DE LOS SLIDERS FUE APRETADO.
	JPbox *inspectorBox = getInspectorBox();
	if (inspectorBox != nullptr)
	{
		for (int k = 0; k < inspectorBox->parameters.getSize(); k++)
		{
			if (inspectorBox->parameters.parameters[k]->needsUpdate)
			{
				inspectorBox->parameters.parameters[k]->update();
				markCueDraftDirty(openguinumber);
				setControllers();

				inspectorBox->parameters.parameters[k]->needsUpdate = false;
			}
		}
	}

	if (!boxes.empty())
	{
		transition.update(); //ACTUALIZO EL TRANSITION
	}
		//activeSequence = true;
		const float sequenceIntervalMs = std::max(durationGalleryMs, 16.0f);
		if (activeSequence && 
			ofGetElapsedTimeMillis() - lasttime_sequence > sequenceIntervalMs && 
			boxes.size() > 2) {
			lasttime_sequence = ofGetElapsedTimeMillis();
			cout << "SEQ ACTIVADA. CAMBIO A " << *activerender << endl;
			cout << "boxes.size() " << boxes.size() << endl;

			int idx = *activerender + 1;
			if (*activerender > boxes.size() - 2) {
				idx = 0;
			}
			else {
				idx = *activerender + 1;
			}
			requestSetActiveRender(idx);
			// Keep all boxes running in cycle mode so animated sources don't "freeze"
			// while waiting to become active again.
		}
}
void JPboxgroup::setDurationGalleryMs(float _ms)
{
	durationGalleryMs = ofClamp(_ms, 0.0f, 4200.0f);
	galleryDurationParam.floatValue = durationGalleryMs;
	galleryDurationParam.floatLerpValue = durationGalleryMs;
}
float JPboxgroup::getDurationGalleryMs() const
{
	return durationGalleryMs;
}
void JPboxgroup::setActiveOnlyBox(int _val) {

	for (int i = boxes.size() - 1; i >= 0; i--) {
		if (i == _val) {
			boxes[i]->setonoff(true);
		}
		else {
			boxes[i]->setonoff(false);
		}
	}

}

void JPboxgroup::update_paramswindow()
{

	int index = 0; // INDICE PARA LOS BOTONES :

	// TODO ESTO PARA QUE TIPO AGARRES UN SOLO SLIDER A LA VEZ Y NO SE VUELVA LOCO
	if (!ofGetMousePressed())
	{
		controllerselected = -1;
	}
	bool ningunaAgarrada = true;
	for (int i = 0; i < controllers.size(); i++)
	{
		controllers[i]->update();
		if (controllers[i]->activeFlag)
		{
			controllerselected = i;
			ningunaAgarrada = false;
		}
	}
	if (ningunaAgarrada)
	{
		controllerselected = -1;
	}
	for (int i = 0; i < controllers.size(); i++)
	{
		if (controllerselected == i)
		{
			controllers[i]->activable2 = true;
		}
		else
		{
			controllers[i]->activable2 = false;
		}
	}
}
void JPboxgroup::update_resized(int w, int h)
{
	cout << "RESIZE" << endl;
	cout << "w " << w << endl;
	cout << "h " << h << endl;
	// cout << "render_width" << *render_width << endl;
	// cout << "render_height" << *render_height << endl;

	/*for (int i = boxes.size()-1; i >=0 ; i--) {
		boxes[i]->fbo.clear();
		//boxes[i]->shaderrender.fbo.allocate(*render_width, *render_height);
		boxes[i]->fbo.allocate(jp_constants::renderWidth,jp_constants::renderHeight);
	}*/

	inspectorwindow_x = ofGetWidth() - inspectorwindow_width / 2;
	inspectorwindow_y = inspectorwindow_height / 2;
	inspectorwindow_sepy = 30;
	inspectorwindow_height = 0;
	setinspectorsetactiveparams();
	setupGalleryDurationSlider();
	clampCuePanelLayout();

	// int i = controllers.size()-1; i >= 0; i--
	for (int i = 0; i < controllers.size(); i++)
	{
		controllers[i]->setPos(inspectorwindow_x, inspectorwindow_height);
		inspectorwindow_height += inspectorwindow_sepy;
	}

	// setControllers();
	// boxesdrawing.allocate(ofGetWidth(), ofGetHeight());
}
void JPboxgroup::setinspectorsetactiveparams()
{

	inspectorwindow_height = 0;
	inspectorwindow_setactivesize = 25;

	inspectorwindow_height += inspectorwindow_sepy * 2.0;

	inspectorsetactive.setup(ofGetWidth() - inspectorwindow_width * 1 / 4,
							 inspectorwindow_height,
							 inspectorwindow_setactivesize * 2.,
							 inspectorwindow_setactivesize);

	inspectorwindow_height += inspectorwindow_sepy;
	inspectorreload.setup(ofGetWidth() - inspectorwindow_width * 1 / 4,
						  inspectorwindow_height,
						  inspectorwindow_setactivesize * 2.,
						  inspectorwindow_setactivesize);

	inspectorwindow_height += inspectorwindow_sepy;
	inspectorrandom.setup(ofGetWidth() - inspectorwindow_width * 1 / 4,
						 inspectorwindow_height,
						 inspectorwindow_setactivesize * 2.,
						 inspectorwindow_setactivesize);

	inspectorwindow_height += inspectorwindow_sepy;
}
void JPboxgroup::update_mouseDragged(int mousebutton)
{
	ofVec2f screenMouse(ofGetMouseX(), ofGetMouseY());
	ofVec2f previousScreenMouse(ofGetPreviousMouseX(), ofGetPreviousMouseY());
	ofVec2f canvasMouse = screenToCanvas(screenMouse);
	ofVec2f previousCanvasMouse = screenToCanvas(previousScreenMouse);

	// Determine active box vector based on context (main vs group view)
	vector<JPbox *> *activeBoxesPtr = &boxes;
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset == nullptr) return;
		activeBoxesPtr = &preset->boxes;
	}
	vector<JPbox *> &activeBoxes = *activeBoxesPtr;

	if (mousebutton == OF_MOUSE_BUTTON_MIDDLE || mousebutton == OF_MOUSE_BUTTON_RIGHT)
	{
		viewportPanning = true;
		panViewport(screenMouse - previousScreenMouse);
		return;
	}

	if (draw_SelectionRect && mousebutton == OF_MOUSE_BUTTON_LEFT)
	{
		selectionEnd = canvasMouse;
		updateBoxSelection();
		return;
	}

	// Multi-drag: move selected boxes or the grabbed box
	if (cualestaagarrado != -1 && cualestaagarrado < (int)activeBoxes.size() && activeBoxes[cualestaagarrado]->activeFlag)
	{
		float deltaX = canvasMouse.x - previousCanvasMouse.x;
		float deltaY = canvasMouse.y - previousCanvasMouse.y;
		if (!selectedBoxIndices.empty() &&
			(isBoxSelected(cualestaagarrado) || std::find(selectedBoxIndices.begin(), selectedBoxIndices.end(), cualestaagarrado) != selectedBoxIndices.end()))
		{
			// Dragging a selected box — move all selected boxes
			for (int i = 0; i < (int)selectedBoxIndices.size(); i++)
			{
				int selectedIndex = selectedBoxIndices[i];
				if (selectedIndex >= 0 && selectedIndex < (int)activeBoxes.size())
				{
					activeBoxes[selectedIndex]->setPos(
						activeBoxes[selectedIndex]->x + deltaX,
						activeBoxes[selectedIndex]->y + deltaY);
				}
			}
		}
		else
		{
			activeBoxes[cualestaagarrado]->setPos(
				activeBoxes[cualestaagarrado]->x + deltaX,
				activeBoxes[cualestaagarrado]->y + deltaY);
		}
	}

	// Connection dragging: release outlet over an input
	JPdragobject::setMouseOverride(canvasMouse);
	for (int i = (int)activeBoxes.size() - 1; i >= 0; i--)
	{
		for (int k = (int)activeBoxes.size() - 1; k >= 0; k--)
		{
			for (int l = activeBoxes[k]->fbohandlergroup.getSize() - 1; l >= 0; l--)
			{
				if (activeBoxes[k]->fbohandlergroup.mouseOver(l) &&
					activeBoxes[i]->outletActiveFlag)
				{
					if (activeBoxes[k]->fbohandlergroup.getFboName(l) == activeBoxes[i]->name)
					{
						continue;
					}
					if (!isGroupViewActive() && isCueDraftMode())
					{
						commitCueDraftLink(k, l, i);
					}
					else
					{
						activeBoxes[k]->fbohandlergroup.setFboPointer(&activeBoxes[i]->fbo,
																		&activeBoxes[i]->name, l);
						if (!isGroupViewActive())
						{
							requestCueRebuild();
						}
					}
				}
			}
		}
	}
	JPdragobject::clearMouseOverride();
	// Para los sliders :
	JPbox *inspectorBox = getInspectorBox();
	if (!shaderboxagarrado && !ouletagarrado && cualestaagarrado == -1 && outlet_cualestaagarrado == -1 && inspectorBox != nullptr)
	{
		// Con esto detecto que no se toque ningun slider de mas: osea que no puedas estar tocando dos sliders a la vez :
		bool slideragarrado = false;
		// Si invertimos el for en este crashea. habra que cambiarlo en otro lugar tambien?
		for (int i = 0; i < controllers.size(); i++)
		{
			if (controllers[i]->activeFlag)
			{
				slideragarrado = true;

				if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT && inspectorBox->parameters.getMovType(i) == 0)
				{
					// float valf = ofLerp(controllers[i]->value, boxes[openguinumber]->parameters.getFloatValue(i), 0.1);
					// boxes[openguinumber]->parameters.setFloatValue(valf, i);

					///				float slidervaluewidth = width * 3 / 4;
					/*slider_value.setup(x, y,
						slidervaluewidth,
						height * 8 / 10,
						0.0,
						1.0,
						value,
						name);
						*/
					// controllers

					// Esto es porque si esta en movtype 0 no actualiza para que no pise con el OSC , entonces hay que actualizarlo manualmente
					// Esta es la actualizaci�n manual. Acordate que si el movtype esta en 0 el valor NO SE ACTUALIZA.
					controllers[i]->value = ofMap(ofGetMouseX(), controllers[i]->x - (controllers[i]->width * 3 / 4) / 2,
												  controllers[i]->x + (controllers[i]->width * 3 / 4) / 2, 0.0, 1.0, true);

					inspectorBox->parameters.setFloatValue(controllers[i]->value, i);
					inspectorBox->parameters.setFloatLerpValue(controllers[i]->value, i);
					markCueDraftDirty(openguinumber);
					// boxes[openguinumber]->parameters.setSpeed(controllers[i]->speed, i);
					// boxes[openguinumber]->parameters.setMin(controllers[i]->min, i);
					// boxes[openguinumber]->parameters.setMax(controllers[i]->max, i);
				}
			}
		}
		// Si invertimos el for en este crashea. habra que cambiarlo en otro lugar tambien?
		if (!slideragarrado && inspectorBox != nullptr)
		{
			for (int i = 0; i < controllers.size(); i++)
			{
				if ((controllers[i]->mouseOver()) && mousebutton == OF_MOUSE_BUTTON_LEFT)
				{
					if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
					{
						controllers[i]->activeFlag = true;
					}
				}
			}
		}
		if (!slideragarrado && !ouletagarrado)
		{
			offsetx = ofGetMouseX();
			offsety = ofGetMouseY();
		}
	}
}
void JPboxgroup::update_mousePressed(int mouseButton)
{
	////SET OPEN GUI NUMBER :
	ofVec2f canvasMouse = screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY()));

	float dif = ofGetSystemTimeMillis() - lasttime_mouseclick;
	// cout << "Diference " << dif << endl;
	isDoubleClick = (ofGetSystemTimeMillis() - lasttime_mouseclick < duration_mouseclick);
	lasttime_mouseclick = ofGetSystemTimeMillis();
	draw_SelectionRect = false;

	// Handle tab clicks (left button only)
	if (mouseButton == OF_MOUSE_BUTTON_LEFT && handleTabClick())
	{
		return;
	}

	// In group view mode: handle click on sub-box and deselect on empty space click
	if (isGroupViewActive() && mouseButton == OF_MOUSE_BUTTON_LEFT)
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr)
		{
			JPdragobject::setMouseOverride(canvasMouse);
			bool hitBox = false;
			int clickedIndex = -1;
			for (int i = 0; i < (int)preset->boxes.size(); i++)
			{
				if (preset->boxes[i]->mouseOver() && !preset->boxes[i]->mouseOverOutlet())
				{
					hitBox = true;
					clickedIndex = i;
					break;
				}
			}
			JPdragobject::clearMouseOverride();
			if (hitBox && clickedIndex >= 0)
			{
				// Match main view behavior: only clear selection if clicking a non-selected box
				if (!isBoxSelected(clickedIndex))
				{
					clearSelection();
				}
				// Single click: select for inspector (blue box, like openguinumber in main view)
				groupInspectorIndex = clickedIndex;
				groupPreviewBoxIndex = -1;
				setControllers();
				// Double-click: set active render (green box) and switch main output
				if (isDoubleClick)
				{
					preset->activeRender = clickedIndex;
					if (!activeGroupPath.empty())
					{
						requestSetActiveRender(activeGroupPath[0]);
					}
				}
				return; // Don't process main boxes
			}
			else if (!hitBox && !mouseOverGui())
			{
				groupInspectorIndex = -1;
				groupPreviewBoxIndex = -1;
				setControllers();
				clearSelection();
				draw_SelectionRect = true;
				lastMouseClick = canvasMouse;
				selectionEnd = lastMouseClick;
			}
		}
	}

	bool arafue = false; // POR SI NO TOCO NINGUN ELEMENTO;

	if (mouseButton == OF_MOUSE_BUTTON_RIGHT && isCueDraftMode() && !mouseOverGui())
	{
		JPdragobject::setMouseOverride(canvasMouse);
		for (int i = boxes.size() - 1; i >= 0; i--)
		{
			if (boxes[i]->mouseOver() && (isCueDraftDirty(i) || isCueAddedRealIndex(i)))
			{
				JPdragobject::clearMouseOverride();
				revertCueDraftBox(i);
				viewportPanning = false;
				return;
			}
		}
		JPdragobject::clearMouseOverride();
	}

	if ((mouseButton == OF_MOUSE_BUTTON_MIDDLE || mouseButton == OF_MOUSE_BUTTON_RIGHT) && !mouseOverGui())
	{
		viewportPanning = true;
		return;
	}

	// SI EL MOUSE ESTA DENTRO DEL INSPECTOR WINDOW PAPA.
	JPbox *inspectorBox = getInspectorBox();
	if (mouseOverGui() && inspectorBox != nullptr)
	{
		arafue = true;
		if (inspectorsetactive.mouseGrab())
		{
			if (!promoteCueToActive())
			{
				requestSetActiveRender(openguinumber);
			}
		}
		if (inspectorreload.mouseGrab())
		{
			// reloadActiveshader();
		}
		if (inspectorrandom.mouseGrab())
		{
			for (int i = 0; i < inspectorBox->parameters.getSize(); i++)
			{
				if (inspectorBox->parameters.getType(i) == JPParameter::FLOAT)
				{
					float mn = inspectorBox->parameters.getMin(i);
					float mx = inspectorBox->parameters.getMax(i);
					inspectorBox->parameters.setFloatValue(ofRandom(mn, mx), i);
					// Also zero out lerp target so it snaps immediately
					inspectorBox->parameters.setFloatLerpValue(inspectorBox->parameters.getFloatValue(i), i);
				}
			}
		}
		// Checkeo todos los controles.
		bool isovercontrol = false;
		// ESTE PARECE QUE NO HACE QUE CRASHEE COMO EL RESTO DE LOS CONTROLADORES
		int index = 0; // INDEX PARA RECORRER LOS BOTONES :
		int activeone = -1;
		for (int i = 0; i < controllers.size(); i++)
		{
			if (controllers[i]->mouseOver())
			{
				// cout << "MOUSE OVER " << controllers[i]->name << endl;
				isovercontrol = true;
				if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
				{
					controllers[i]->activeFlag = true;
					activeone = i;
				}
				else if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.BOOL)
				{
					inspectorBox->parameters.setBoolValue(controllers[i]->boolValue, i);
					markCueDraftDirty(openguinumber);
				}
			}
			if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
			{
				index++;
			}
		}
		// PONGO EN FALSE TODOS LOS QUE NO TENGO ACTIVOS:
		for (int i = 0; i < controllers.size(); i++)
		{
			if (i != activeone)
			{
				controllers[i]->activeFlag = false;
			}
			else
			{
				// Esto es porque si esta en movtype 0 no actualiza para que no pise con el OSC , entonces hay que actualizarlo manualmente
				// Esta es la actualizaci�n manual. Acordate que si el movtype esta en 0 el valor NO SE ACTUALIZA.
				controllers[i]->value = ofMap(ofGetMouseX(), controllers[i]->x - (controllers[i]->width * 3 / 4) / 2,
											  controllers[i]->x + (controllers[i]->width * 3 / 4) / 2, 0.0, 1.0, true);
				inspectorBox->parameters.setFloatValue(controllers[i]->value, i);
				inspectorBox->parameters.setFloatLerpValue(controllers[i]->value, i);
				markCueDraftDirty(openguinumber);
			}
		}
		if (mouseButton == 2 && isDoubleClick)
		{
			cout << "DOBLE CLICK " << endl;
			for (int i = 0; i < inspectorBox->parameters.getSize(); i++)
			{
				if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
				{
					float rdm = ofRandom(1);
					inspectorBox->parameters.setFloatLerpValue(rdm, i);
					inspectorBox->parameters.setFloatValue(rdm, i);
					markCueDraftDirty(openguinumber);

				}
			}
			setControllers();
		}
		// POR ACA VA LA COSA POR AHROA
		for (int i = 0; i < controllers.size(); i++)
		{
			if (controllers[i]->overboton_collapse &&
				inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
			{
				// CAMBIA MOVTYPE
				// Bueno todo esto esta medio raro pero funciona digamos.
				cout << "Cambia movtype " << endl;
				cout << "BOTON OVER " << controllers[i]->name << endl;
			}
		}
	}
	else
	{
		randomcnt = 0;
		arafue = false;
		JPdragobject::setMouseOverride(canvasMouse);
		for (int i = boxes.size() - 1; i >= 0; i--)
		{
			// Esto esta raro:
			if (boxes[i]->mouseOverOutlet())
			{
				arafue = true;
				clearSelection();
				boxes[i]->activeFlag = false;
				boxes[i]->outletActiveFlag = true;
				ouletagarrado = true;
				shaderboxagarrado = false;
				outlet_cualestaagarrado = i;
				cualestaagarrado = -1;
			}
			else if (boxes[i]->mouseOver())
			{
				arafue = true;
				if (!isBoxSelected(i))
				{
					clearSelection();
				}
				openguinumber = i;
				if (!mouseOverGui())
				{
					setControllers();
				}
		if (isDoubleClick)
		{
			//*activerender = i;
			requestSetActiveRender(i);
		}
	}
		}
		JPdragobject::clearMouseOverride();
	}
	if (!arafue)
	{
		openguinumber = -1;
		if (mouseButton == OF_MOUSE_BUTTON_LEFT)
		{
			clearSelection();
			draw_SelectionRect = true;
			lastMouseClick = canvasMouse;
			selectionEnd = lastMouseClick;
		}
	}
	if (openguinumber != -1)
	{
		setControllers();
	}
}
void JPboxgroup::update_mouseReleased(int mouseButton)
{
	// Determine active box vector based on context (main vs group view)
	vector<JPbox *> *activeBoxesPtr = &boxes;
	JPbox_preset *activePreset = nullptr;
	if (isGroupViewActive())
	{
		activePreset = getActivePreset();
		if (activePreset == nullptr) return;
		activeBoxesPtr = &activePreset->boxes;
	}
	vector<JPbox *> &activeBoxes = *activeBoxesPtr;

	if (mouseButton == OF_MOUSE_BUTTON_MIDDLE || mouseButton == OF_MOUSE_BUTTON_RIGHT)
	{
		viewportPanning = false;
		return;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT)
	{
		if (shaderboxagarrado && cualestaagarrado != -1 && cualestaagarrado < (int)activeBoxes.size())
		{
			activeBoxes[cualestaagarrado]->activeFlag = false;
		}
		if (ouletagarrado && outlet_cualestaagarrado != -1 && outlet_cualestaagarrado < (int)activeBoxes.size())
		{
			activeBoxes[outlet_cualestaagarrado]->outletActiveFlag = false;
		}
		shaderboxagarrado = false;
		ouletagarrado = false;
		cualestaagarrado = -1;
		outlet_cualestaagarrado = -1;
		viewportPanning = false;
		if (draw_SelectionRect)
		{
			selectionEnd = screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY()));
			updateBoxSelection();
			draw_SelectionRect = false;
		}
	}
}
bool JPboxgroup::update_cueMousePressed(int mouseButton)
{
	if (mouseButton != OF_MOUSE_BUTTON_LEFT || getCuePreviewBox() == nullptr)
	{
		return false;
	}

	if (mouseOverCueApplyIcon())
	{
		cuePanelApplyArmed = true;
		return true;
	}

	if (mouseOverCueMonitorModeIcon())
	{
		cuePanelApplyArmed = false;
		cueMonitorMode = cueMonitorMode == CUE_MONITOR_FINAL_OUTPUT ? CUE_MONITOR_SELECTED_BOX : CUE_MONITOR_FINAL_OUTPUT;
		return true;
	}

	if (mouseOverCueCloseIcon())
	{
		cuePanelApplyArmed = false;
		setCueBoxByIndex(-1);
		return true;
	}

	if (mouseOverCueFullscreenIcon())
	{
		cuePanelApplyArmed = false;
		cueFullscreenPreview = !cueFullscreenPreview;
		return true;
	}

	if (mouseOverCueResizeHandle())
	{
		cuePanelApplyArmed = false;
		cuePanelResizing = true;
		cuePanelDragging = false;
		cuePanelDragStartMouse = ofVec2f(ofGetMouseX(), ofGetMouseY());
		cuePanelResizeStartSize = ofVec2f(cuePanelW, cuePanelH);
		return true;
	}

	if (mouseOverCueHeader())
	{
		cuePanelApplyArmed = false;
		cuePanelDragging = true;
		cuePanelResizing = false;
		cuePanelDragStartMouse = ofVec2f(ofGetMouseX(), ofGetMouseY());
		cuePanelDragStartPos = ofVec2f(cuePanelX, cuePanelY);
		return true;
	}

	return false;
}

bool JPboxgroup::update_cueMouseDragged(int mouseButton)
{
	if (mouseButton != OF_MOUSE_BUTTON_LEFT)
	{
		return false;
	}

	ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	ofVec2f delta = mouse - cuePanelDragStartMouse;

	if (cuePanelApplyArmed)
	{
		return true;
	}

	if (cuePanelDragging)
	{
		cuePanelX = cuePanelDragStartPos.x + delta.x;
		cuePanelY = cuePanelDragStartPos.y + delta.y;
		clampCuePanelLayout();
		return true;
	}

	if (cuePanelResizing)
	{
		cuePanelW = cuePanelResizeStartSize.x + delta.x;
		cuePanelH = cuePanelResizeStartSize.y + delta.y;
		clampCuePanelLayout();
		return true;
	}

	return false;
}

bool JPboxgroup::update_cueMouseReleased(int mouseButton)
{
	if (mouseButton != OF_MOUSE_BUTTON_LEFT)
	{
		return false;
	}

	if (cuePanelApplyArmed)
	{
		bool applyNow = mouseOverCueApplyIcon();
		cuePanelApplyArmed = false;
		if (applyNow)
		{
			requestCueApply();
		}
		return true;
	}

	bool wasInteracting = cuePanelDragging || cuePanelResizing;
	cuePanelDragging = false;
	cuePanelResizing = false;
	if (wasInteracting)
	{
		clampCuePanelLayout();
	}
	return wasInteracting;
}

bool JPboxgroup::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
	if (scrollY == 0 || mouseOverGui())
	{
		return false;
	}

	float zoomFactor = scrollY > 0 ? 1.1f : 1.0f / 1.1f;
	float oldZoom = viewportZoom;
	zoomViewport(ofVec2f(x, y), zoomFactor);
	return viewportZoom != oldZoom;
}
void JPboxgroup::updateTransition(int _idx) {

//	cout << "UPDATE TRANSITION " << endl;
	if (boxes.size() >= 1) {
		_idx = ofClamp(_idx, 0, int(boxes.size()) - 1);
		bool activeRenderChanged = _idx != *activerender;
		if (&boxes[*activerender]->fbo != 0) {
			transition.setFboPointer1(&boxes[*activerender]->fbo);
		}
		*activerender = _idx;

		if (&boxes[*activerender]->fbo != 0) {
			transition.setFboPointer2(&boxes[*activerender]->fbo);
		}
		transition.setLerpValue(0);
		if (activeRenderChanged)
		{
			requestCueRebuild();
		}
	}
}

bool JPboxgroup::requestSetActiveRender(int index, bool activeOnly)
{
	if (boxes.empty() || index < 0 || index >= boxes.size() || boxes[index] == nullptr)
	{
		return false;
	}
	if (hasCue())
	{
		bool staged = setCueStagedActiveRenderIndex(index);
		if (staged)
		{
			markCueDraftDirty(index, CUE_DIRTY_STAGED_ACTIVE);
		}
		return staged;
	}
	updateTransition(index);
	if (activeOnly)
	{
		setActiveOnlyBox(index);
	}
	return true;
}
void JPboxgroup::draw_cursorrect() {}
void JPboxgroup::save(string outputPath)
{
	ofXml xml;

	auto activerender_save = xml.appendChild("activerender");
	activerender_save.set(*activerender);

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
		data.appendChild("onoff").set(boxes[i]->getonoff());
		data.appendChild("bypass").set(boxes[i]->getBypass());
		// boxes[i]->parameters.coutData();
		if (boxes[i]->parameters.getSize() > 0)
		{
			auto parameters = data.appendChild("parameters");
			for (int k = 0; k < boxes[i]->parameters.getSize(); k++)
			{

				if (boxes[i]->parameters.getType(k) == boxes[i]->parameters.BOOL)
				{
					auto param = parameters.appendChild("param");
					param.appendChild("name").set(boxes[i]->parameters.getName(k));
					param.appendChild("value").set(boxes[i]->parameters.getBoolValue(k));
				}
				else
				{
					// string name = boxes[i]->parameters.getName(k);
					auto param = parameters.appendChild("param");
					// param.set(boxes[i]->parameters.getFloatValue(k));
					param.appendChild("name").set(boxes[i]->parameters.getName(k));
					param.appendChild("min").set(boxes[i]->parameters.getMin(k));
					param.appendChild("max").set(boxes[i]->parameters.getMax(k));
					param.appendChild("value").set(boxes[i]->parameters.getFloatValue(k));
					param.appendChild("movtype").set(boxes[i]->parameters.getMovType(k));
					param.appendChild("speed").set(boxes[i]->parameters.getSpeed(k));
				}
			}
		}
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
	}

	ofFilePath::createEnclosingDirectory(outputPath);
	xml.save(outputPath);
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
void JPboxgroup::load(string _dirinput)
{
	clear();
	ofXml xml;

	ofDirectory dir(_dirinput);

	// if(dir.doesDirectoryExist(_dirinput)){

	xml.load(_dirinput);
	// Carga inicial de las cajitas :
	auto boxloader = xml.find("/box");

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

		JPbox *bx;
		if (directory.getValue().find(".frag") != std::string::npos)
		{
			bx = new JPbox_shader();
		}
		else if (directory.getValue().find(".jpg") != std::string::npos ||
				 directory.getValue().find(".png") != std::string::npos ||
				 directory.getValue().find(".jpeg") != std::string::npos)
		{
			bx = new JPbox_image();
		}
		else if (directory.getValue().find(".mov") != std::string::npos ||
				 directory.getValue().find(".mkv") != std::string::npos ||
				 directory.getValue().find(".mp4") != std::string::npos ||
				 directory.getValue().find(".flv") != std::string::npos ||
				 directory.getValue().find(".vob") != std::string::npos ||
				 directory.getValue().find(".avi") != std::string::npos)
		{
			bx = new JPbox_video();
		}
		else if (directory.getValue().find("cam") != std::string::npos)
		{
			bx = new JPbox_cam();
		}
#ifdef NDI
		else if (directory.getValue().find("ndiReceiver") != std::string::npos) {
			bx = new JPbox_ndi();
		}
#endif
#ifdef SPOUT
		else if (directory.getValue().find("spoutReceiver") != std::string::npos)
		{
			bx = new JPbox_spout();
		}
#endif

		else if (directory.getValue().find(".xml") != std::string::npos)
		{
			bx = new JPbox_preset();
		}
		else if (directory.getValue().find("framedifference") != std::string::npos)
		{
			bx = new JPbox_framedifference();
		}

		bx->setup(directory.getValue(), nombre.getValue());
		bx->setPos(x.getIntValue(), y.getIntValue());
		bx->setonoff(onoff ? onoff.getBoolValue() : true);
		bx->setBypass(bypass ? bypass.getBoolValue() : false);

		int index = 0;
		auto parameters = box.getChild("parameters").getChildren();
		// cout << "PARAMETER SIZE SB " << sb->parameters.getSize() << endl;

		for (auto &param : parameters) 
		{

			if (bx->parameters.getType(index) == bx->parameters.FLOAT)
			{
				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setMin(param.getChild("min").getFloatValue(), index);
				bx->parameters.setMax(param.getChild("max").getFloatValue(), index);
				bx->parameters.setFloatLerpValue(param.getChild("value").getFloatValue(), index);
				bx->parameters.setFloatValue(param.getChild("value").getFloatValue(), index);
				bx->parameters.setmovetype(param.getChild("movtype").getIntValue(), index);
				bx->parameters.setSpeed(param.getChild("speed").getFloatValue(), index);
			}
			else if (bx->parameters.getType(index) == bx->parameters.BOOL)
			{
				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setBoolValue(param.getChild("value").getBoolValue(), index);
			}
			index++;
		}

	
#ifdef SPOUT
		if (bx->getTipo() == 4) {
			cout << "RELOAD CAJA DE SPOUT " << endl;
			bx->reload();
		}
#endif

		boxes.push_back(bx);
	}
	// Una vez que cargo todas las cajitas les cargamos los links :
	// Mira lo que esta este algoritmo para levantar los links entre cajitas papa !!!
	int index1 = 0;
	int index2 = 0;
	cout << "COMIENZA LINKS DE LOS FBO " << endl;
	for (auto &box : boxloader)
	{
		auto fboslinks = box.getChild("fboslinks").getChildren();
		index2 = 0;
		for (auto &fbolink : fboslinks)
		{
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
						boxes[index1]->fbohandlergroup.setFboPointer(fbopointer, fbopointername, index2);
					}
				}
			}
			index2++;
		}
		index1++;
	}

	//CLAUSULA DE SEGURIDAD : 
	*activerender = int(ofClamp(xml.getChild("activerender").getIntValue(),0,boxes.size()-1));
	cout << "TERMINA LINKS DE LOS FBO " << endl;

	updateTransition(*activerender);




	//}
	// activerender_loader.getIntValue();
	// activerender = activerender_loader.getIntValue();



}
void JPboxgroup::setControllers(){

	for (int i = 0; i < controllers.size(); i++)
	{
		delete controllers[i];
		controllers[i] = nullptr;
	}
	controllers.clear();

	JPbox *inspectorBox = getInspectorBox();
	if (inspectorBox == nullptr)
	{
		return;
	}

	inspectorwindow_height = 0;
	float slider_width = inspectorwindow_width * 3 / 4;
	float slider_height = inspectorwindow_sepy * 7 / 10;

	inspectorwindow_height = font_p->stringHeight(inspectorBox->name);
	inspectorwindow_height += inspectorwindow_sepy * 1.;

	// El espacio que ponemos para dibujar el reload shader. Cosa que ya sacamos.
	/*if(boxes[openguinumber]->getTipo() == boxes[openguinumber]->SHADERBOX){
		inspectorwindow_height += inspectorwindow_setactivesize;
	}*/
	// Espacio para dibujar el set active render
	inspectorwindow_height += inspectorwindow_sepy * 0.5;
	/*inspectorwindow_height += inspectorwindow_setactivesize;
	inspectorwindow_height += inspectorwindow_sepy * 0.5;
	*/

	// FIJATE QUE ESTO SI LA PRIMERA CONDICION NO SE CUMPLE NI EVALUA LA SEGUNDA. PARA PODER EVALUAR LA SEGUNDA LA PRIMERA TIENE QUE SER TRU
	if (inspectorBox->parameters.getSize() > 0 && inspectorBox->parameters.getMovType(0) != 0)
	{
		inspectorwindow_height += inspectorwindow_sepy * 0.5;
	}

	for (int k = 0; k < inspectorBox->parameters.getSize(); k++)
	{
		if (inspectorBox->parameters.getType(k) == inspectorBox->parameters.FLOAT)
		{

			float complexsliderheight = inspectorwindow_sepy * 1.0;
			if (inspectorBox->parameters.getMovType(k) != 0)
			{
				complexsliderheight = inspectorwindow_sepy * 2.0;
			}
			if (k > 0)
			{
				if (inspectorBox->parameters.getMovType(k) != 0 &&
					inspectorBox->parameters.getMovType(k - 1) == 0)
				{
					inspectorwindow_height += inspectorwindow_sepy * 0.5;
				}
				if (inspectorBox->parameters.getMovType(k) == 0 &&
					inspectorBox->parameters.getMovType(k - 1) != 0)
				{
					inspectorwindow_height -= inspectorwindow_sepy * 0.5;
				}
			}
			// boxes[openguinumber]->parameters.setFloatValue(0.0, k);

			// boxes[openguinumber]->parameters.parameters[k]->floatValue = 0.5;

			// boxes[openguinumber]->parameters.parameters[k]->floatLerpValue = 0.5;

			// JPParameter* as = boxes[openguinumber]->parameters.parameters[k];
			JPComplexSlider *sl = new JPComplexSlider();
			sl->setup(inspectorwindow_x,
					  inspectorwindow_height, inspectorwindow_width, complexsliderheight,
					  inspectorBox->parameters.parameters[k]);

			controllers.push_back(sl);

			if (k != inspectorBox->parameters.getSize() - 1)
			{
				// inspectorwindow_height -= inspectorwindow_sepy * 0.5;
				inspectorwindow_height += complexsliderheight;
			}
			else
			{
				inspectorwindow_height += complexsliderheight * .5;
			}
		}
		else if (inspectorBox->parameters.getType(k) == inspectorBox->parameters.BOOL)
		{
			float complexsliderheight = inspectorwindow_sepy * 1.0;
			JPToogle *toogle = new JPToogle();
			toogle->setParametersPointer(inspectorBox->parameters.getJParameter(k));
			toogle->setup(inspectorwindow_x,
						  inspectorwindow_height, slider_width, slider_height, inspectorBox->parameters.getName(k), inspectorBox->parameters.getBoolValue(k));
			controllers.push_back(toogle);
			// Esto es para que no lo agregue si es el ultimo elemento :
			if (k != inspectorBox->parameters.getSize() - 1)
			{
				inspectorwindow_height += complexsliderheight;
			}
		}
	}
	inspectorwindow_y = inspectorwindow_height / 2;
}
void JPboxgroup::reloadActiveshader()
{
	if (boxes.size() > 0){
		if (openguinumber != -1){
			// cout << "Active Render " << *activerender <<endl;
			// cout << "Active Render " << *activerender << endl;
			boxes[openguinumber]->reload();
			setControllers();
		}
		else{
			// cout << "Active Render " << *activerender << endl;
			// cout << "Active Render " << *activerender << endl;
			boxes[*activerender]->reload();
		}
	}
}
void JPboxgroup::listenToOsc(string _dir, float _val){
	// string nombre = dir.substr(dir.find_last_of("/\\") + 1, dir.size());
	// string dir = _dir;
	string shadername = _dir.substr(_dir.find_first_of("/") + 1, _dir.find_last_of("/") - 1);
	string parametername = _dir.substr(_dir.find_last_of("/") + 1, _dir.size());
	//cout << _dir << endl;
	if (_dir == "/setactiverender") {
		//cout << "SETEA EL RENDER ACTIVO " << _val << endl;
		if(!boxes.empty() && _val < boxes.size() && _val >= 0){
			//*activerender = floor(_val);
			requestSetActiveRender(floor(_val));

	}
}

	if (_dir == "/nextshader") {
		if (boxes.empty()) {
			return;
		}

		int base = openguinumber;
		if (base < 0 || base > boxes.size() - 1) {
			base = *activerender;
		}
		int val = base + 1;
		if(val > boxes.size()-1){
			val = 0;
		}

		openguinumber = val;
		setControllers();
	}

	if (_dir == "/prevshader") {
		if (boxes.empty()) {
			return;
		}

		int base = openguinumber;
		if (base < 0 || base > boxes.size() - 1) {
			base = *activerender;
		}
		int val = base - 1;
		if (val < 0) {
			val = boxes.size()-1;
		}
		openguinumber = val;
		setControllers();
	}


	if (_dir == "/nextshader_gallerymode") {
		if (boxes.empty()) {
			return;
		}

		int val = openguinumber + 1;
		if (val > boxes.size() - 1) {
			val = 0;
		}
		openguinumber = val;
		setControllers();
		requestSetActiveRender(floor(openguinumber), true);
	}

	if (_dir == "/prevshader_gallerymode") {
		if (boxes.empty()) {
			return;
		}

		int val = openguinumber - 1;
		if (val < 0) {
			val = boxes.size() - 1;
		}
		openguinumber = val;
		setControllers();
		requestSetActiveRender(floor(openguinumber), true);
	}



	if (_dir == "/setactiveshader") {
		if (promoteCueToActive())
		{
			return;
		}
		if (!boxes.empty() && openguinumber >= 0 && openguinumber < boxes.size()) {
			requestSetActiveRender(floor(openguinumber));
		}
	}

	if (_dir == "/setactivecycle") {
		activeSequence = !activeSequence;
		if (activeSequence) {
			for (int i = 0; i < boxes.size(); i++) {
				boxes[i]->setonoff(true);
			}
		}
	}

	if (_dir == "/disablegallerymode") {
		activeSequence = false;
		for (int i = 0; i < boxes.size(); i++) {
			boxes[i]->setonoff(true);
		}
	}

	if (_dir == "/addmirrorsquad") {
		addBox("data/shaders/imageprocessing/mirrorquad.frag");
	}

	if (_dir == "/setdurationgalleryms") {
		setDurationGalleryMs(_val);
	}

			//LEO POR NOMBRE DE EFECTO Y LE TIRO AL EFECTO ESE
	for (int i = 0; i < boxes.size(); i++){
		if (boxes[i]->name == shadername){	
			JPbox *targetBox = getEditableBoxForRealIndex(i);
			if (targetBox == nullptr)
			{
				continue;
			}
			//cout << "Parameter name " <<parametername << endl;
			if (parametername == "onoff") {
				//cout << "LLEGA ON OFF" << endl;
				if (_val == 0) {
					targetBox->setonoff(false);
					markCueDraftDirty(i, CUE_DIRTY_BYPASS_PAUSE);
				}
				else if (_val == 1) {
					targetBox->setonoff(true);
					markCueDraftDirty(i, CUE_DIRTY_BYPASS_PAUSE);
				}
			}
			// cout << "COINCIDE EL NOMBRE " << endl;
			for (int k = 0; k < targetBox->parameters.getSize(); k++){
				if (targetBox->parameters.getName(k) == parametername){
					// cout << "COINCIDE EL PARAMETRO " << endl;
					if (targetBox->parameters.getType(k) == targetBox->parameters.FLOAT){
						targetBox->parameters.setFloatValue(_val, k);
						targetBox->parameters.setFloatLerpValue(_val, k);
						markCueDraftDirty(i);
						// ESTO ES PARA QUE SOLO MODIFIQUE EL VALOR DEL SLIDER SOLO SI ESTA ABIERTO ESE COSO
						if (openguinumber == i){
							controllers[k]->value = _val; // ACa la cantidad de controllers siempre va a ser igual a la cantidad de parameters size;
						}
					}
				}
			}
		}
	}
	//LEO POR NOMBRE DEL OPENGUIQUE ESTA ACTIVO
	if (shadername == "openguinumber"){
		JPbox *inspectorBox = getInspectorBox();
		string index = "NULL";
		// cout << "parametername.size()" << parametername.size() << endl;
		// NO TENGO NI PUTA IDEA QUE HACES ESTE CODIGO DE ACA :  ONDA . PORQUE SI ES IGUAL A 6 O A / O SEA QUE CARAJO
		if (parametername.size() == 6){
			index = parametername.at(5);
		}
		if (parametername.size() == 7){
			index = parametername.at(5);
			index.push_back(parametername.at(6));
		}
		int Intindex = ofToInt(index);
		if (Intindex < controllers.size() && inspectorBox != nullptr &&
			inspectorBox->parameters.getMovType(Intindex) == 0){
			inspectorBox->parameters.setFloatValue(_val, Intindex);
			inspectorBox->parameters.setFloatLerpValue(_val, Intindex);
			markCueDraftDirty(openguinumber);
			controllers[Intindex]->value = _val;
		}
	}
}
vector<string> JPboxgroup::getBoxNames() const
{
	vector<string> names;
	for (int i = 0; i < boxes.size(); i++)
	{
		names.push_back(boxes[i]->name);
	}
	return names;
}
int JPboxgroup::findBoxIndexByName(string boxName) const
{
	for (int i = 0; i < boxes.size(); i++)
	{
		if (boxes[i]->name == boxName)
		{
			return i;
		}
	}
	return -1;
}
JPbox *JPboxgroup::findBoxByName(string boxName) const
{
	int index = findBoxIndexByName(boxName);
	if (index >= 0)
	{
		return boxes[index];
	}
	return nullptr;
}
bool JPboxgroup::hasBoxName(string boxName) const
{
	return findBoxByName(boxName) != nullptr;
}
bool JPboxgroup::toggleBypassForBox(string boxName)
{
	int index = findBoxIndexByName(boxName);
	JPbox *box = getEditableBoxForRealIndex(index);
	if (box != nullptr)
	{
		box->setBypass(!box->getBypass());
		markCueDraftDirty(index, CUE_DIRTY_BYPASS_PAUSE);
		return true;
	}
	return false;
}
bool JPboxgroup::togglePauseForBox(string boxName)
{
	int index = findBoxIndexByName(boxName);
	JPbox *box = getEditableBoxForRealIndex(index);
	if (box != nullptr)
	{
		box->setonoff(!box->getonoff());
		markCueDraftDirty(index, CUE_DIRTY_BYPASS_PAUSE);
		return true;
	}
	return false;
}
bool JPboxgroup::setBypassForBox(string boxName, bool value)
{
	int index = findBoxIndexByName(boxName);
	JPbox *box = getEditableBoxForRealIndex(index);
	if (box != nullptr)
	{
		box->setBypass(value);
		markCueDraftDirty(index, CUE_DIRTY_BYPASS_PAUSE);
		return true;
	}
	return false;
}
bool JPboxgroup::setPauseForBox(string boxName, bool value)
{
	int index = findBoxIndexByName(boxName);
	JPbox *box = getEditableBoxForRealIndex(index);
	if (box != nullptr)
	{
		box->setonoff(value);
		markCueDraftDirty(index, CUE_DIRTY_BYPASS_PAUSE);
		return true;
	}
	return false;
}
bool JPboxgroup::selectOpenBoxByName(string boxName)
{
	int index = findBoxIndexByName(boxName);
	if (index >= 0)
	{
		openguinumber = index;
		setControllers();
		return true;
	}
	return false;
}
bool JPboxgroup::selectOpenBoxByIndex(int index)
{
	if (index >= 0 && index < boxes.size())
	{
		openguinumber = index;
		setControllers();
		return true;
	}
	return false;
}
bool JPboxgroup::setCueFromSelected()
{
	return setCueByIndex(openguinumber);
}

bool JPboxgroup::setCueByIndex(int index)
{
	if (index < 0 || index >= getCueTargetBoxSize())
	{
		clearCue();
		return false;
	}

	bool wasInspectorTarget = isCueDraftMode() && openguinumber == cueState.sourceIndex;
	clearCue();
	if (wasInspectorTarget && openguinumber >= 0 && openguinumber < getCueTargetBoxSize())
	{
		setControllers();
	}
	if (getCueTargetActiveRender() >= 0 && getCueTargetActiveRender() < getCueTargetBoxSize())
	{
		if (cueApplySnapshotFbo.getWidth() != getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getWidth() ||
			cueApplySnapshotFbo.getHeight() != getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getHeight())
		{
			cueApplySnapshotFbo.allocate(getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getWidth(), getCueTargetBoxAt(getCueTargetActiveRender())->fbo.getHeight());
		}
		cueApplySnapshotFbo.begin();
		ofClear(0, 0, 0, 0);
		ofSetColor(255, 255);
		getCueTargetBoxAt(getCueTargetActiveRender())->fbo.draw(0, 0, cueApplySnapshotFbo.getWidth(), cueApplySnapshotFbo.getHeight());
		cueApplySnapshotFbo.end();
	}

	if (beginCueDraftForBoxIndex(index))
	{
		return true;
	}

	return false;
}

bool JPboxgroup::toggleCueByIndex(int index)
{
	cout << "toggleCueByIndex(" << index << ") targetBoxSize=" << getCueTargetBoxSize() << endl;
	if (index < 0 || index >= getCueTargetBoxSize())
	{
		cout << "  -> index out of range, clearCue" << endl;
		clearCue();
		return true;
	}
	cout << "  hasCue=" << hasCue() << " sourceIndex=" << cueState.sourceIndex << endl;
	if (hasCue() && cueState.sourceIndex == index)
	{
		cout << "  -> same index, clearing" << endl;
		clearCue();
		return true;
	}
	cout << "  -> calling setCueByIndex" << endl;
	return setCueByIndex(index);
}

void JPboxgroup::clearCue()
{
	bool wasInspectorTarget = isCueDraftMode() && openguinumber == cueState.sourceIndex;
	removeCueAddedBoxesFromRealGraph();
	clearCueDraft();
	cueState.mode = CUE_NONE;
	cueState.sourceIndex = -1;
	cueState.previewIndex = -1;
	cueState.stagedActiveRenderIndex = -1;
	cueState.targetPreset = nullptr;
	cueFullscreenPreview = false;
	cueMonitorMode = CUE_MONITOR_FINAL_OUTPUT;
	cuePanelApplyArmed = false;
	pendingCueApply = false;
	pendingCueRebuild = false;
	if (wasInspectorTarget && openguinumber >= 0 && openguinumber < boxes.size())
	{
		setControllers();
	}
}

bool JPboxgroup::applyCue()
{
	if (isCueDraftMode())
	{
		return applyCueDraftToSource();
	}
	if (isCueNormalPreviewMode())
	{
		if (cueState.stagedActiveRenderIndex >= 0 &&
			cueState.stagedActiveRenderIndex < getCueTargetBoxSize() &&
			cueState.stagedActiveRenderIndex != getCueTargetActiveRender())
		{
			updateTransition(cueState.stagedActiveRenderIndex);
		}
		return true;
	}
	return false;
}

bool JPboxgroup::hasCue() const
{
	return cueState.mode != CUE_NONE;
}

bool JPboxgroup::setCueBoxByIndex(int index)
{
	return setCueByIndex(index);
}

bool JPboxgroup::setCueBoxByName(string boxName)
{
	return setCueByIndex(findBoxIndexByName(boxName));
}

bool JPboxgroup::toggleCueBoxByIndex(int index)
{
	cout << "toggleCueBoxByIndex(" << index << ") called" << endl;
	// When in group view, set the target preset for CUE operations
	if (isGroupViewActive())
	{
		cueState.targetPreset = getActivePreset();
		cout << "  -> targetPreset set for group view" << endl;
	}
	else
	{
		cueState.targetPreset = nullptr;
	}
	return toggleCueByIndex(index);
}

bool JPboxgroup::hasCueBox() const
{
	return hasCue();
}

// --- CUE target helpers ---
vector<JPbox *>& JPboxgroup::getCueTargetBoxes()
{
	return (cueState.targetPreset != nullptr) ? cueState.targetPreset->boxes : boxes;
}

int JPboxgroup::getCueTargetBoxSize() const
{
	return (cueState.targetPreset != nullptr) ? (int)cueState.targetPreset->boxes.size() : (int)boxes.size();
}

JPbox *JPboxgroup::getCueTargetBoxAt(int index) const
{
	if (index < 0) return nullptr;
	if (cueState.targetPreset != nullptr)
	{
		return (index < (int)cueState.targetPreset->boxes.size()) ? cueState.targetPreset->boxes[index] : nullptr;
	}
	return (index < (int)boxes.size()) ? boxes[index] : nullptr;
}

int &JPboxgroup::getCueTargetActiveRender()
{
	return (cueState.targetPreset != nullptr) ? cueState.targetPreset->activeRender : *activerender;
}

bool JPboxgroup::promoteCueToActive()
{
	return requestCueApply();
}

bool JPboxgroup::requestCueApply()
{
	if (!hasCue())
	{
		return false;
	}
	pendingCueApply = true;
	return true;
}

void JPboxgroup::processPendingCueApply()
{
	if (!pendingCueApply)
	{
		return;
	}
	pendingCueApply = false;
	applyCue();
}

void JPboxgroup::requestCueRebuild()
{
	if (hasCue())
	{
		pendingCueRebuild = true;
	}
}

void JPboxgroup::processPendingCueRebuild()
{
	if (!pendingCueRebuild)
	{
		return;
	}
	pendingCueRebuild = false;
	rebuildCueAfterGraphChange();
}

bool JPboxgroup::rebuildCueAfterGraphChange()
{
	if (!hasCue())
	{
		return false;
	}

	if (isCueNormalPreviewMode())
	{
		if (cueState.sourceIndex < 0 || cueState.sourceIndex >= getCueTargetBoxSize() ||
			cueState.previewIndex < 0 || cueState.previewIndex >= getCueTargetBoxSize() ||
			getCueTargetBoxAt(cueState.sourceIndex) == nullptr || getCueTargetBoxAt(cueState.previewIndex) == nullptr)
		{
			clearCue();
			return false;
		}
		return true;
	}

	if (!isCueDraftMode())
	{
		return false;
	}
	// Rebuild the draft graph — check all draft real indices are still valid
	for (int i = 0; i < (int)cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex < 0 || realIndex >= getCueTargetBoxSize() ||
			getCueTargetBoxAt(realIndex) == nullptr || cueState.draftBoxes[i] == nullptr)
		{
			clearCue();
			return false;
		}
	}

	if (!isCueDraftMode())
	{
		return false;
	}

	struct DraftSnapshot
	{
		int realIndex = -1;
		string name;
		JPParameterGroup parameters;
		bool onoff = true;
		bool bypass = false;
		vector<string> linkNames;
		vector<bool> linkSet;
		unsigned int dirtyFlags = CUE_DIRTY_NONE;
	};

	vector<DraftSnapshot> snapshots;
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex < 0 || realIndex >= boxes.size() ||
			i < 0 || i >= cueState.draftBoxes.size() ||
			boxes[realIndex] == nullptr || cueState.draftBoxes[i] == nullptr)
		{
			continue;
		}
		DraftSnapshot snapshot;
		snapshot.realIndex = realIndex;
		snapshot.name = boxes[realIndex]->name;
		snapshot.parameters = cueState.draftBoxes[i]->parameters;
		snapshot.onoff = cueState.draftBoxes[i]->getonoff();
		snapshot.bypass = cueState.draftBoxes[i]->getBypass();
		for (int linkIndex = 0; linkIndex < cueState.draftBoxes[i]->fbohandlergroup.getSize(); linkIndex++)
		{
			bool isSet = cueState.draftBoxes[i]->fbohandlergroup.getisPointerSet(linkIndex);
			snapshot.linkSet.push_back(isSet);
			snapshot.linkNames.push_back(isSet ? cueState.draftBoxes[i]->fbohandlergroup.getFboName(linkIndex) : "");
		}
		snapshot.dirtyFlags = getCueDraftDirtyFlags(realIndex);
		if (snapshot.dirtyFlags != CUE_DIRTY_NONE)
		{
			snapshots.push_back(snapshot);
		}
	}

	int sourceIndex = cueState.sourceIndex;
	int openIndex = openguinumber;
	int keepStagedActiveRenderIndex = cueState.stagedActiveRenderIndex;
	bool keepFullscreenPreview = cueFullscreenPreview;
	CueMonitorMode keepMonitorMode = cueMonitorMode;

	if (sourceIndex < 0 || sourceIndex >= boxes.size() ||
		boxes[sourceIndex] == nullptr)
	{
		clearCue();
		return false;
	}

	if (!buildCueDraftGraph(sourceIndex))
	{
		return false;
	}

	cueFullscreenPreview = keepFullscreenPreview;
	cueMonitorMode = keepMonitorMode;
	if (keepStagedActiveRenderIndex >= 0 && keepStagedActiveRenderIndex < boxes.size())
	{
		setCueStagedActiveRenderIndex(keepStagedActiveRenderIndex);
	}
	cueState.dirtyDraftRealIndices.clear();
	for (int i = 0; i < cueState.draftDirtyFlags.size(); i++)
	{
		cueState.draftDirtyFlags[i] = CUE_DIRTY_NONE;
	}

	for (int i = 0; i < snapshots.size(); i++)
	{
		int realIndex = snapshots[i].realIndex;
		if (realIndex < 0 || realIndex >= boxes.size() ||
			boxes[realIndex] == nullptr ||
			boxes[realIndex]->name != snapshots[i].name)
		{
			realIndex = findBoxIndexByName(snapshots[i].name);
		}

		JPbox *draftBox = getCueDraftBoxForRealIndex(realIndex);
		if (draftBox == nullptr)
		{
			continue;
		}

		copyParametersByNameOrIndex(draftBox->parameters, snapshots[i].parameters);
		draftBox->setonoff(snapshots[i].onoff);
		draftBox->setBypass(snapshots[i].bypass);
		for (int linkIndex = 0; linkIndex < snapshots[i].linkSet.size() &&
								   linkIndex < draftBox->fbohandlergroup.getSize(); linkIndex++)
		{
			if (!snapshots[i].linkSet[linkIndex])
			{
				draftBox->fbohandlergroup.deleteFboPointer(linkIndex);
				continue;
			}
			int linkedRealIndex = findBoxIndexByName(snapshots[i].linkNames[linkIndex]);
			JPbox *linkedDraft = getCueDraftBoxForRealIndex(linkedRealIndex);
			if (linkedDraft != nullptr)
			{
				draftBox->fbohandlergroup.setFboPointer(&linkedDraft->fbo, &linkedDraft->name, linkIndex);
			}
			else if (linkedRealIndex >= 0 && linkedRealIndex < boxes.size() && boxes[linkedRealIndex] != nullptr)
			{
				draftBox->fbohandlergroup.setFboPointer(&boxes[linkedRealIndex]->fbo, &boxes[linkedRealIndex]->name, linkIndex);
			}
		}
		markCueDraftDirty(realIndex, snapshots[i].dirtyFlags);
	}
	for (int i = 0; i < cueState.cueAddedRealIndices.size(); i++)
	{
		markCueDraftDirty(cueState.cueAddedRealIndices[i], CUE_DIRTY_ADDED);
	}

	openguinumber = openIndex;
	if (openguinumber >= 0 && openguinumber < boxes.size())
	{
		setControllers();
	}
	rewireCueDraftGraph();
	updateCueDraftGraph();
	return true;
}

bool JPboxgroup::beginCueDraftForActiveShader()
{
	return beginCueDraftForBoxIndex(getCueTargetActiveRender());
}

void JPboxgroup::clearCueDraft()
{
	for (int i = 0; i < cueState.draftBoxes.size(); i++)
	{
		if (cueState.draftBoxes[i] != nullptr)
		{
			cueState.draftBoxes[i]->clear();
			delete cueState.draftBoxes[i];
			cueState.draftBoxes[i] = nullptr;
		}
	}
	cueState.draftBoxes.clear();
	cueState.draftRealIndices.clear();
	cueState.dirtyDraftRealIndices.clear();
	cueState.draftDirtyFlags.clear();
	cueState.draftBaselineParameters.clear();
	cueState.draftBaselineOnOff.clear();
	cueState.draftBaselineBypass.clear();
	cueState.draftInspectorRealIndex = -1;
	cueState.draftSourceBox = nullptr;
	cueState.draftOutputBox = nullptr;
	cueState.draftOutputRealIndex = -1;
	cueState.stagedActiveRenderIndex = -1;
}

bool JPboxgroup::applyCueDraftToSource()
{
	if (!isCueDraftMode())
	{
		return false;
	}
	int sourceIndex = cueState.sourceIndex;
	bool draftWasInspectorTarget = getCueDraftBoxForRealIndex(openguinumber) != nullptr;
	int stagedActiveIndex = cueState.stagedActiveRenderIndex;
	int targetSize = getCueTargetBoxSize();
	int targetActiveRender = getCueTargetActiveRender();
	if (stagedActiveIndex < 0 || stagedActiveIndex >= targetSize)
	{
		stagedActiveIndex = targetActiveRender;
	}
	bool activeRenderChanged = stagedActiveIndex != targetActiveRender;
	if (cueState.dirtyDraftRealIndices.empty() && !activeRenderChanged)
	{
		return true;
	}
	if (targetActiveRender >= 0 && targetActiveRender < targetSize &&
		(cueApplySnapshotFbo.getWidth() != getCueTargetBoxAt(targetActiveRender)->fbo.getWidth() ||
		 cueApplySnapshotFbo.getHeight() != getCueTargetBoxAt(targetActiveRender)->fbo.getHeight()))
	{
		cueApplySnapshotFbo.allocate(getCueTargetBoxAt(targetActiveRender)->fbo.getWidth(), getCueTargetBoxAt(targetActiveRender)->fbo.getHeight());
	}
	if (targetActiveRender >= 0 && targetActiveRender < targetSize)
	{
		cueApplySnapshotFbo.begin();
		ofClear(0, 0, 0, 0);
		ofSetColor(255, 255);
		getCueTargetBoxAt(targetActiveRender)->fbo.draw(0, 0, cueApplySnapshotFbo.getWidth(), cueApplySnapshotFbo.getHeight());
		cueApplySnapshotFbo.end();
	}

	vector<int> dirtyIndices = cueState.dirtyDraftRealIndices;
	vector<int> deletedIndices = getCueDirtyIndices(CUE_DIRTY_DELETED);
	std::sort(deletedIndices.begin(), deletedIndices.end(), std::greater<int>());
	vector<JPbox*> &targetBoxes = getCueTargetBoxes();
	for (int i = 0; i < dirtyIndices.size(); i++)
	{
		int realIndex = dirtyIndices[i];
		unsigned int flags = getCueDraftDirtyFlags(realIndex);
		if ((flags & CUE_DIRTY_DELETED) != 0)
		{
			continue;
		}
		int draftIndex = findCueDraftCloneIndexForRealIndex(realIndex);
		if (realIndex < 0 || realIndex >= targetSize ||
			draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
			targetBoxes[realIndex] == nullptr || cueState.draftBoxes[draftIndex] == nullptr)
		{
			continue;
		}
		if ((flags & (CUE_DIRTY_PARAMS | CUE_DIRTY_ADDED)) != 0)
		{
			copyParametersByNameOrIndex(targetBoxes[realIndex]->parameters, cueState.draftBoxes[draftIndex]->parameters);
		}
		if ((flags & (CUE_DIRTY_BYPASS_PAUSE | CUE_DIRTY_ADDED)) != 0)
		{
			targetBoxes[realIndex]->setonoff(cueState.draftBoxes[draftIndex]->getonoff());
			targetBoxes[realIndex]->setBypass(cueState.draftBoxes[draftIndex]->getBypass());
		}
		if ((flags & (CUE_DIRTY_LINKS | CUE_DIRTY_ADDED)) != 0)
		{
			copyCueDraftLinksToReal(realIndex);
		}
	}
	cueApplyingCommit = true;
	for (int i = 0; i < deletedIndices.size(); i++)
	{
		int realIndex = deletedIndices[i];
		if (realIndex >= 0 && realIndex < targetSize && !isCueAddedRealIndex(realIndex))
		{
			// Use main deleteBoxAtIndex for preset boxes, it handles the correct vector
			deleteBoxAtIndex(realIndex);
			if (stagedActiveIndex == realIndex)
			{
				stagedActiveIndex = targetActiveRender;
			}
			else if (stagedActiveIndex > realIndex)
			{
				stagedActiveIndex--;
			}
		}
	}
	cueApplyingCommit = false;
	cueState.cueAddedRealIndices.clear();
	updateRealBoxesForCueApply();
	if (stagedActiveIndex >= 0 && stagedActiveIndex < targetSize)
	{
		getCueTargetActiveRender() = stagedActiveIndex;
		transition.setFboPointer1(&cueApplySnapshotFbo);
		transition.setFboPointer2(&targetBoxes[stagedActiveIndex]->fbo);
		transition.setLerpValue(0);
	}

	bool keepFullscreenPreview = cueFullscreenPreview;
	CueMonitorMode keepMonitorMode = cueMonitorMode;
	int rebuildSourceIndex = sourceIndex;
	if (rebuildSourceIndex < 0 || rebuildSourceIndex >= targetSize || targetBoxes[rebuildSourceIndex] == nullptr)
	{
		rebuildSourceIndex = stagedActiveIndex;
	}
	if (rebuildSourceIndex < 0 || rebuildSourceIndex >= targetSize || targetBoxes[rebuildSourceIndex] == nullptr)
	{
		rebuildSourceIndex = targetActiveRender;
	}
	if (rebuildSourceIndex >= 0 && rebuildSourceIndex < targetSize && buildCueDraftGraph(rebuildSourceIndex))
	{
		cueFullscreenPreview = keepFullscreenPreview;
		cueMonitorMode = keepMonitorMode;
		setCueStagedActiveRenderIndex(stagedActiveIndex);
	}
	else
	{
		clearCue();
		return true;
	}
	if (draftWasInspectorTarget && openguinumber >= 0 && openguinumber < boxes.size())
	{
		setControllers();
	}
	return true;
}

JPbox *JPboxgroup::getInspectorBox()
{
	// In group view mode, return sub-box from the active preset (uses separate groupInspectorIndex)
	if (isGroupViewActive() && groupInspectorIndex >= 0)
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr && groupInspectorIndex < (int)preset->boxes.size())
		{
			return preset->boxes[groupInspectorIndex];
		}
	}

	JPbox *draftBox = getCueDraftBoxForRealIndex(openguinumber);
	if (draftBox != nullptr)
	{
		cueState.draftInspectorRealIndex = openguinumber;
		return draftBox;
	}
	if (openguinumber >= 0 && openguinumber < boxes.size())
	{
		return boxes[openguinumber];
	}
	return nullptr;
}

JPbox *JPboxgroup::getCuePreviewBox()
{
	if (cueMonitorMode == CUE_MONITOR_SELECTED_BOX &&
		openguinumber >= 0 && openguinumber < getCueTargetBoxSize())
	{
		JPbox *draftBox = getCueDraftBoxForRealIndex(openguinumber);
		if (draftBox != nullptr)
		{
			return draftBox;
		}
		return getCueTargetBoxAt(openguinumber);
	}
	if (isCueDraftMode())
	{
		if (cueState.draftOutputBox != nullptr)
		{
			return cueState.draftOutputBox;
		}
		return cueState.draftSourceBox;
	}
	if (isCueNormalPreviewMode() &&
		cueState.previewIndex >= 0 && cueState.previewIndex < getCueTargetBoxSize())
	{
		return getCueTargetBoxAt(cueState.previewIndex);
	}
	return nullptr;
}

JPbox *JPboxgroup::getCueDraftSourceBox()
{
	return cueState.draftSourceBox;
}

JPbox *JPboxgroup::getCueDraftBoxForRealIndex(int index) const
{
	if (!isCueDraftMode())
	{
		return nullptr;
	}
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex >= 0 && draftIndex < cueState.draftBoxes.size())
	{
		return cueState.draftBoxes[draftIndex];
	}
	return nullptr;
}

JPbox *JPboxgroup::getEditableBoxForRealIndex(int index)
{
	if (index < 0 || index >= boxes.size())
	{
		return nullptr;
	}
	JPbox *draftBox = getCueDraftBoxForRealIndex(index);
	if (draftBox != nullptr)
	{
		return draftBox;
	}
	return boxes[index];
}

bool JPboxgroup::beginCueDraftForBoxIndex(int index)
{
	if (index < 0 || index >= getCueTargetBoxSize() || getCueTargetBoxSize() == 0 ||
		getCueTargetActiveRender() < 0 || getCueTargetActiveRender() >= getCueTargetBoxSize())
	{
		return false;
	}
	if (getCueTargetBoxAt(index) == nullptr)
	{
		return false;
	}
	return buildCueDraftGraph(index);
}

bool JPboxgroup::buildCueDraftGraph(int sourceIndex)
{
	if (sourceIndex < 0 || sourceIndex >= getCueTargetBoxSize() ||
		getCueTargetBoxAt(sourceIndex) == nullptr)
	{
		clearCueDraft();
		cueState.mode = CUE_NONE;
		cueState.sourceIndex = -1;
		cueState.previewIndex = -1;
		return false;
	}

	bool draftWasInspectorTarget = isCueDraftMode() && openguinumber == cueState.sourceIndex;
	clearCueDraft();

	vector<JPbox*> &targetBoxes = getCueTargetBoxes();
	for (int realIndex = 0; realIndex < (int)targetBoxes.size(); realIndex++)
	{
		if (targetBoxes[realIndex] == nullptr)
		{
			continue;
		}
		JPbox *draft = cloneBoxForCueDraft(realIndex);
		if (draft == nullptr)
		{
			clearCueDraft();
			cueState.mode = CUE_NONE;
			cueState.sourceIndex = -1;
			cueState.previewIndex = -1;
			if (draftWasInspectorTarget && openguinumber >= 0 && openguinumber < boxes.size())
			{
				setControllers();
			}
			return false;
		}
		cueState.draftBoxes.push_back(draft);
		cueState.draftRealIndices.push_back(realIndex);
		cueState.draftDirtyFlags.push_back(CUE_DIRTY_NONE);
		cueState.draftBaselineParameters.push_back(draft->parameters);
		cueState.draftBaselineOnOff.push_back(draft->getonoff());
		cueState.draftBaselineBypass.push_back(draft->getBypass());
		if (realIndex == sourceIndex)
		{
			cueState.draftSourceBox = draft;
		}
	}

	cueState.mode = cueState.draftSourceBox != nullptr ? CUE_DRAFT_CHAIN : CUE_NONE;
	cueState.sourceIndex = sourceIndex;
	cueState.previewIndex = -1;
	cueState.stagedActiveRenderIndex = getCueTargetActiveRender();
	setCueStagedActiveRenderIndex(cueState.stagedActiveRenderIndex);
	cueFullscreenPreview = false;
	rewireCueDraftGraph();
	updateCueDraftGraph();
	if (getCueDraftBoxForRealIndex(openguinumber) != nullptr)
	{
		setControllers();
	}
	return isCueDraftMode();
}

bool JPboxgroup::collectCueDraftPath(int currentIndex, int activeIndex, vector<int> &path, vector<bool> &visiting)
{
	if (currentIndex < 0 || currentIndex >= boxes.size() || visiting[currentIndex])
	{
		return false;
	}
	if (currentIndex == activeIndex)
	{
		if (std::find(path.begin(), path.end(), currentIndex) == path.end())
		{
			path.insert(path.begin(), currentIndex);
		}
		return true;
	}

	visiting[currentIndex] = true;
	bool foundPath = false;
	string currentName = boxes[currentIndex]->name;
	for (int consumerIndex = 0; consumerIndex < boxes.size(); consumerIndex++)
	{
		if (consumerIndex == currentIndex || boxes[consumerIndex] == nullptr)
		{
			continue;
		}
		for (int linkIndex = 0; linkIndex < boxes[consumerIndex]->fbohandlergroup.getSize(); linkIndex++)
		{
			if (boxes[consumerIndex]->fbohandlergroup.getisPointerSet(linkIndex) &&
				boxes[consumerIndex]->fbohandlergroup.getFboName(linkIndex) == currentName &&
				collectCueDraftPath(consumerIndex, activeIndex, path, visiting))
			{
				foundPath = true;
			}
		}
	}
	visiting[currentIndex] = false;

	if (foundPath && std::find(path.begin(), path.end(), currentIndex) == path.end())
	{
		path.insert(path.begin(), currentIndex);
	}
	return foundPath;
}

JPbox *JPboxgroup::cloneBoxForCueDraft(int index)
{
	if (index < 0 || index >= getCueTargetBoxSize() || getCueTargetBoxAt(index) == nullptr)
	{
		return nullptr;
	}

	JPbox *source = getCueTargetBoxAt(index);
	JPbox *draft = nullptr;
	const int type = source->getTipo();
	if (type == source->SHADERBOX ||
		type == source->FRAMEDIFFERENCEBOX ||
		type == source->PRESETBOX)
	{
		string draftName = source->name + "_cue_draft";
		draft = createBoxForDirectory(source->dir, draftName);
		if (draft == nullptr)
		{
			return nullptr;
		}
		draft->setup(source->dir, draftName);
	}
	else
	{
		draft = new JPbox();
		draft->setup(source->dir, source->name + "_cue_draft");
	}
	copyEditableBoxState(draft, source);
	draft->name = source->name;
	return draft;
}

void JPboxgroup::copyEditableBoxState(JPbox *destination, JPbox *source)
{
	if (destination == nullptr || source == nullptr)
	{
		return;
	}
	destination->parameters = source->parameters;
	destination->setonoff(source->getonoff());
	destination->setBypass(source->getBypass());
	destination->setPos(source->x, source->y);
}

int JPboxgroup::findCueDraftCloneIndexForRealIndex(int index) const
{
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		if (cueState.draftRealIndices[i] == index)
		{
			return i;
		}
	}
	return -1;
}

bool JPboxgroup::isCueSourceIndex(int index) const
{
	if (isCueDraftMode())
	{
		return isCueDraftRealIndex(index);
	}
	return hasCue() && cueState.sourceIndex == index;
}

bool JPboxgroup::isCueDraftRealIndex(int index) const
{
	return findCueDraftCloneIndexForRealIndex(index) >= 0;
}

bool JPboxgroup::isRealIndexDraftEditable(int index) const
{
	return isCueDraftMode() && isCueDraftRealIndex(index);
}

bool JPboxgroup::isCueDraftDirty(int index) const
{
	unsigned int flags = getCueDraftDirtyFlags(index);
	return (flags & ~CUE_DIRTY_STAGED_ACTIVE) != CUE_DIRTY_NONE;
}

unsigned int JPboxgroup::getCueDraftDirtyFlags(int index) const
{
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex < 0 || draftIndex >= cueState.draftDirtyFlags.size())
	{
		return CUE_DIRTY_NONE;
	}
	return cueState.draftDirtyFlags[draftIndex];
}

bool JPboxgroup::isCueDeletedRealIndex(int index) const
{
	return (getCueDraftDirtyFlags(index) & CUE_DIRTY_DELETED) != 0;
}

bool JPboxgroup::isCueDraftMode() const
{
	return cueState.mode == CUE_DRAFT_CHAIN;
}

bool JPboxgroup::isCueNormalPreviewMode() const
{
	return cueState.mode == CUE_NORMAL_PREVIEW;
}

bool JPboxgroup::setCueStagedActiveRenderIndex(int index)
{
	if (!hasCue() || index < 0 || index >= getCueTargetBoxSize() || getCueTargetBoxAt(index) == nullptr)
	{
		return false;
	}
	cueState.stagedActiveRenderIndex = index;
	if (cueState.draftOutputBox == nullptr && index >= 0 && index < getCueTargetBoxSize())
	{
		cueState.draftOutputBox = getCueTargetBoxAt(index);
	}
	markCueDraftDirty(cueState.sourceIndex, CUE_DIRTY_STAGED_ACTIVE);
	return false;
}

void JPboxgroup::markCueDraftDirty(int index, unsigned int flags)
{
	if (!isCueDraftMode() || !isCueDraftRealIndex(index))
	{
		return;
	}
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex >= 0 && draftIndex < cueState.draftDirtyFlags.size())
	{
		cueState.draftDirtyFlags[draftIndex] |= flags;
	}
	if (std::find(cueState.dirtyDraftRealIndices.begin(),
				  cueState.dirtyDraftRealIndices.end(),
				  index) == cueState.dirtyDraftRealIndices.end())
	{
		cueState.dirtyDraftRealIndices.push_back(index);
	}
}

void JPboxgroup::removeCueDraftDirty(int index, unsigned int flags)
{
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex >= 0 && draftIndex < cueState.draftDirtyFlags.size())
	{
		if (flags == 0)
		{
			cueState.draftDirtyFlags[draftIndex] = CUE_DIRTY_NONE;
		}
		else
		{
			cueState.draftDirtyFlags[draftIndex] &= ~flags;
		}
	}
	if (getCueDraftDirtyFlags(index) == CUE_DIRTY_NONE)
	{
		cueState.dirtyDraftRealIndices.erase(
			std::remove(cueState.dirtyDraftRealIndices.begin(),
						cueState.dirtyDraftRealIndices.end(),
						index),
			cueState.dirtyDraftRealIndices.end());
	}
}

bool JPboxgroup::isCueAddedRealIndex(int index) const
{
	return std::find(cueState.cueAddedRealIndices.begin(),
					 cueState.cueAddedRealIndices.end(),
					 index) != cueState.cueAddedRealIndices.end();
}

void JPboxgroup::addCueAddedRealIndex(int index)
{
	if (index < 0 || index >= boxes.size())
	{
		return;
	}
	if (!isCueAddedRealIndex(index))
	{
		cueState.cueAddedRealIndices.push_back(index);
	}
	markCueDraftDirty(index, CUE_DIRTY_ADDED);
}

vector<int> JPboxgroup::getCueDirtyIndices(unsigned int mask) const
{
	vector<int> result;
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		if (i >= cueState.draftDirtyFlags.size())
		{
			continue;
		}
		unsigned int flags = cueState.draftDirtyFlags[i];
		if (flags == CUE_DIRTY_NONE)
		{
			continue;
		}
		if (mask != 0 && (flags & mask) == 0)
		{
			continue;
		}
		result.push_back(cueState.draftRealIndices[i]);
	}
	return result;
}

string JPboxgroup::getCueDirtySummary() const
{
	int params = 0;
	int bypassPause = 0;
	int links = 0;
	int added = 0;
	int deleted = 0;
	bool stagedActive = false;
	for (int i = 0; i < cueState.draftDirtyFlags.size(); i++)
	{
		unsigned int flags = cueState.draftDirtyFlags[i];
		if (flags & CUE_DIRTY_PARAMS) params++;
		if (flags & CUE_DIRTY_BYPASS_PAUSE) bypassPause++;
		if (flags & CUE_DIRTY_LINKS) links++;
		if (flags & CUE_DIRTY_ADDED) added++;
		if (flags & CUE_DIRTY_DELETED) deleted++;
		if (flags & CUE_DIRTY_STAGED_ACTIVE) stagedActive = true;
	}
	vector<string> parts;
	if (params > 0) parts.push_back("params " + ofToString(params));
	if (bypassPause > 0) parts.push_back("pause " + ofToString(bypassPause));
	if (links > 0) parts.push_back("links " + ofToString(links));
	if (added > 0) parts.push_back("new " + ofToString(added));
	if (deleted > 0) parts.push_back("delete " + ofToString(deleted));
	if (stagedActive) parts.push_back("active");
	if (parts.empty())
	{
		return "";
	}
	string summary = " (";
	for (int i = 0; i < parts.size(); i++)
	{
		if (i > 0)
		{
			summary += ", ";
		}
		summary += parts[i];
	}
	summary += ")";
	return summary;
}

bool JPboxgroup::revertCueDraftBox(int index)
{
	if (!isCueDraftMode() || index < 0 || index >= boxes.size())
	{
		return false;
	}
	if (isCueAddedRealIndex(index))
	{
		string deletedName = boxes[index]->name;
		for (int k = boxes.size() - 1; k >= 0; k--)
		{
			if (k == index || boxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < boxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (boxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					boxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		boxes[index]->clear();
		delete boxes[index];
		boxes[index] = nullptr;
		boxes.erase(boxes.begin() + index);
		cueState.cueAddedRealIndices.erase(
			std::remove(cueState.cueAddedRealIndices.begin(),
						cueState.cueAddedRealIndices.end(),
						index),
			cueState.cueAddedRealIndices.end());
		removeCueDraftDirty(index);
		for (int &addedIndex : cueState.cueAddedRealIndices)
		{
			if (addedIndex > index)
			{
				addedIndex--;
			}
		}
		for (int &dirtyIndex : cueState.dirtyDraftRealIndices)
		{
			if (dirtyIndex > index)
			{
				dirtyIndex--;
			}
		}
		if (openguinumber == index)
		{
			openguinumber = -1;
			for (int c = 0; c < controllers.size(); c++)
			{
				delete controllers[c];
				controllers[c] = nullptr;
			}
			controllers.clear();
		}
		else if (openguinumber > index)
		{
			openguinumber--;
		}
		if (*activerender > index)
		{
			(*activerender)--;
		}
		*activerender = boxes.empty() ? 0 : ofClamp(*activerender, 0, int(boxes.size()) - 1);
		requestCueRebuild();
		return true;
	}
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
		draftIndex >= cueState.draftBaselineParameters.size() ||
		boxes[index] == nullptr || cueState.draftBoxes[draftIndex] == nullptr)
	{
		return false;
	}
	cueState.draftBoxes[draftIndex]->parameters = cueState.draftBaselineParameters[draftIndex];
	cueState.draftBoxes[draftIndex]->setonoff(cueState.draftBaselineOnOff[draftIndex]);
	cueState.draftBoxes[draftIndex]->setBypass(cueState.draftBaselineBypass[draftIndex]);
	removeCueDraftDirty(index);
	if (openguinumber == index)
	{
		setControllers();
	}
	rewireCueDraftGraph();
	updateCueDraftGraph();
	return true;
}

void JPboxgroup::removeCueAddedBoxesFromRealGraph()
{
	if (cueState.cueAddedRealIndices.empty())
	{
		return;
	}
	vector<int> added = cueState.cueAddedRealIndices;
	std::sort(added.begin(), added.end(), std::greater<int>());
	added.erase(std::unique(added.begin(), added.end()), added.end());
	cueState.cueAddedRealIndices.clear();
	for (int i = 0; i < added.size(); i++)
	{
		int index = added[i];
		if (index < 0 || index >= boxes.size() || boxes[index] == nullptr)
		{
			continue;
		}
		string deletedName = boxes[index]->name;
		for (int k = boxes.size() - 1; k >= 0; k--)
		{
			if (k == index || boxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < boxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (boxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					boxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		boxes[index]->clear();
		delete boxes[index];
		boxes[index] = nullptr;
		boxes.erase(boxes.begin() + index);
		if (openguinumber == index)
		{
			openguinumber = -1;
			for (int c = 0; c < controllers.size(); c++)
			{
				delete controllers[c];
				controllers[c] = nullptr;
			}
			controllers.clear();
		}
		else if (openguinumber > index)
		{
			openguinumber--;
		}
		if (*activerender > index)
		{
			(*activerender)--;
		}
	}
	if (!boxes.empty())
	{
		*activerender = ofClamp(*activerender, 0, int(boxes.size()) - 1);
	}
	else
	{
		*activerender = 0;
	}
}

bool JPboxgroup::commitCueDraftLink(int targetRealIndex, int linkIndex, int sourceRealIndex)
{
	if (!isCueDraftMode() ||
		targetRealIndex < 0 || targetRealIndex >= boxes.size() ||
		sourceRealIndex < 0 || sourceRealIndex >= boxes.size())
	{
		return false;
	}
	JPbox *targetDraft = getCueDraftBoxForRealIndex(targetRealIndex);
	JPbox *sourceDraft = getCueDraftBoxForRealIndex(sourceRealIndex);
	if (targetDraft == nullptr ||
		linkIndex < 0 ||
		linkIndex >= targetDraft->fbohandlergroup.getSize() ||
		boxes[sourceRealIndex] == nullptr)
	{
		return false;
	}
	if (sourceDraft != nullptr)
	{
		targetDraft->fbohandlergroup.setFboPointer(&sourceDraft->fbo, &sourceDraft->name, linkIndex);
	}
	else
	{
		targetDraft->fbohandlergroup.setFboPointer(&boxes[sourceRealIndex]->fbo, &boxes[sourceRealIndex]->name, linkIndex);
	}
	markCueDraftDirty(targetRealIndex, CUE_DIRTY_LINKS);
	updateCueDraftGraph();
	return true;
}

void JPboxgroup::copyCueDraftLinksToReal(int realIndex)
{
	int draftIndex = findCueDraftCloneIndexForRealIndex(realIndex);
	if (draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
		realIndex < 0 || realIndex >= boxes.size() ||
		cueState.draftBoxes[draftIndex] == nullptr ||
		boxes[realIndex] == nullptr)
	{
		return;
	}
	JPbox *draftBox = cueState.draftBoxes[draftIndex];
	int maxLinks = std::min(draftBox->fbohandlergroup.getSize(), boxes[realIndex]->fbohandlergroup.getSize());
	for (int linkIndex = 0; linkIndex < maxLinks; linkIndex++)
	{
		if (!draftBox->fbohandlergroup.getisPointerSet(linkIndex))
		{
			boxes[realIndex]->fbohandlergroup.deleteFboPointer(linkIndex);
			continue;
		}
		string linkedName = draftBox->fbohandlergroup.getFboName(linkIndex);
		int linkedRealIndex = findBoxIndexByName(linkedName);
		if (linkedRealIndex >= 0 && linkedRealIndex < boxes.size() && boxes[linkedRealIndex] != nullptr)
		{
			boxes[realIndex]->fbohandlergroup.setFboPointer(&boxes[linkedRealIndex]->fbo,
															&boxes[linkedRealIndex]->name,
															linkIndex);
		}
		else
		{
			boxes[realIndex]->fbohandlergroup.deleteFboPointer(linkIndex);
		}
	}
}

void JPboxgroup::rewireCueDraftGraph()
{
	for (int draftIndex = 0; draftIndex < cueState.draftBoxes.size(); draftIndex++)
	{
		int realIndex = cueState.draftRealIndices[draftIndex];
		if (realIndex < 0 || realIndex >= boxes.size() || cueState.draftBoxes[draftIndex] == nullptr)
		{
			continue;
		}
		if ((getCueDraftDirtyFlags(realIndex) & CUE_DIRTY_LINKS) != 0)
		{
			continue;
		}
		for (int linkIndex = 0; linkIndex < cueState.draftBoxes[draftIndex]->fbohandlergroup.getSize() &&
			 linkIndex < boxes[realIndex]->fbohandlergroup.getSize(); linkIndex++)
		{
			if (!boxes[realIndex]->fbohandlergroup.getisPointerSet(linkIndex))
			{
				continue;
			}
			string linkedName = boxes[realIndex]->fbohandlergroup.getFboName(linkIndex);
			int linkedRealIndex = findBoxIndexByName(linkedName);
			int linkedDraftIndex = findCueDraftCloneIndexForRealIndex(linkedRealIndex);
			if (linkedDraftIndex >= 0 && linkedDraftIndex < cueState.draftBoxes.size())
			{
				cueState.draftBoxes[draftIndex]->fbohandlergroup.setFboPointer(&cueState.draftBoxes[linkedDraftIndex]->fbo,
																			   &cueState.draftBoxes[linkedDraftIndex]->name,
																			   linkIndex);
			}
			else if (linkedRealIndex >= 0 && linkedRealIndex < boxes.size())
			{
				cueState.draftBoxes[draftIndex]->fbohandlergroup.setFboPointer(&boxes[linkedRealIndex]->fbo,
																			   &boxes[linkedRealIndex]->name,
																			   linkIndex);
			}
		}
	}
}

void JPboxgroup::updateCueDraftGraph()
{
	for (int i = 0; i < cueState.draftBoxes.size(); i++)
	{
		if (cueState.draftBoxes[i] == nullptr)
		{
			continue;
		}
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex >= 0 && realIndex < boxes.size() && boxes[realIndex] != nullptr)
		{
			int type = boxes[realIndex]->getTipo();
			if (type != boxes[realIndex]->SHADERBOX &&
				type != boxes[realIndex]->FRAMEDIFFERENCEBOX &&
				type != boxes[realIndex]->PRESETBOX)
			{
				cueState.draftBoxes[i]->update();
				cueState.draftBoxes[i]->fbo.begin();
				ofClear(0, 0, 0, 0);
				ofSetColor(255, 255);
				boxes[realIndex]->fbo.draw(0, 0,
										   cueState.draftBoxes[i]->fbo.getWidth(),
										   cueState.draftBoxes[i]->fbo.getHeight());
				cueState.draftBoxes[i]->fbo.end();
				continue;
			}
		}
		if (cueState.draftBoxes[i] != nullptr)
		{
			cueState.draftBoxes[i]->update();
		}
	}
}

void JPboxgroup::updateRealBoxesForCueApply()
{
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex >= 0 && realIndex < boxes.size() && boxes[realIndex] != nullptr)
		{
			boxes[realIndex]->update();
		}
	}
}

void JPboxgroup::copyParametersByNameOrIndex(JPParameterGroup &destination, JPParameterGroup &source)
{
	for (int srcIndex = 0; srcIndex < source.getSize(); srcIndex++)
	{
		int dstIndex = -1;
		string srcName = source.getName(srcIndex);
		for (int i = 0; i < destination.getSize(); i++)
		{
			if (destination.getName(i) == srcName)
			{
				dstIndex = i;
				break;
			}
		}
		if (dstIndex < 0 && srcIndex < destination.getSize())
		{
			dstIndex = srcIndex;
		}
		if (dstIndex < 0 || dstIndex >= destination.getSize() ||
			destination.getType(dstIndex) != source.getType(srcIndex))
		{
			continue;
		}
		if (source.getType(srcIndex) == source.FLOAT)
		{
			destination.setFloatValue(source.getFloatValue(srcIndex), dstIndex);
			destination.setFloatLerpValue(source.getLerpValue(srcIndex), dstIndex);
			destination.setMin(source.getMin(srcIndex), dstIndex);
			destination.setMax(source.getMax(srcIndex), dstIndex);
			destination.setSpeed(source.getSpeed(srcIndex), dstIndex);
			destination.setmovetype(source.getMovType(srcIndex), dstIndex);
		}
		else if (source.getType(srcIndex) == source.BOOL)
		{
			destination.setBoolValue(source.getBoolValue(srcIndex), dstIndex);
		}
	}
}
void JPboxgroup::setCuePanelLayout(float x, float y, float w, float h)
{
	cuePanelX = x;
	cuePanelY = y;
	cuePanelW = w;
	cuePanelH = h;
	clampCuePanelLayout();
}
void JPboxgroup::getCuePanelLayout(float &x, float &y, float &w, float &h) const
{
	x = cuePanelX;
	y = cuePanelY;
	w = cuePanelW;
	h = cuePanelH;
}
int JPboxgroup::getMaxParameterCount() const
{
	int maxCount = 0;
	for (int b = 0; b < boxes.size(); b++)
	{
		maxCount = std::max(maxCount, boxes[b]->parameters.getSize());
	}
	return maxCount;
}
bool JPboxgroup::setOpenBoxParameterAtIndex(int parameterIndex, float value)
{
	JPbox *inspectorBox = getInspectorBox();
	if (inspectorBox == nullptr ||
		parameterIndex < 0 ||
		parameterIndex >= inspectorBox->parameters.getSize())
	{
		return false;
	}

	int type = inspectorBox->parameters.getType(parameterIndex);
	if (type == inspectorBox->parameters.FLOAT)
	{
		inspectorBox->parameters.setFloatValue(value, parameterIndex);
		inspectorBox->parameters.setFloatLerpValue(value, parameterIndex);
		markCueDraftDirty(openguinumber);
		if (parameterIndex < controllers.size())
		{
			controllers[parameterIndex]->value = value;
		}
		return true;
	}
	if (type == inspectorBox->parameters.BOOL)
	{
		bool boolValue = value > 0.5f;
		inspectorBox->parameters.setBoolValue(boolValue, parameterIndex);
		markCueDraftDirty(openguinumber);
		if (parameterIndex < controllers.size())
		{
			controllers[parameterIndex]->boolValue = boolValue;
			controllers[parameterIndex]->activeFlag = false;
		}
		return true;
	}
	return false;
}
bool JPboxgroup::setLastBoxOnOff(bool value)
{
	if (boxes.empty())
	{
		return false;
	}
	boxes.back()->setonoff(value);
	return true;
}
bool JPboxgroup::mouseOverGui()
{

	if (ofGetMouseX() > inspectorwindow_x - inspectorwindow_width / 2 && ofGetMouseX() < inspectorwindow_x + inspectorwindow_width / 2 && ofGetMouseY() > inspectorwindow_y - inspectorwindow_height / 2 && ofGetMouseY() < inspectorwindow_y + inspectorwindow_height / 2)
	{

		return true;
	}
	else
	{
		return false;
	}
}
void JPboxgroup::addBox(string directory, float _x, float _y)
{
	// When in group view, add any box type to the active preset's sub-boxes
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr)
		{
			string nombre = makeUniqueBoxName(makeNameFromDirectory(directory));
			JPbox *bx = createBoxForDirectory(directory, nombre);
			if (bx == nullptr)
			{
				return;
			}
			bx->setup(directory, nombre);
			bx->setonoff(true);
			bx->setPos(_x, _y);
			preset->boxes.push_back(bx);
			cout << "addBox: added sub-box \"" << nombre << "\" to active preset (group view)" << endl;
			return;
		}
	}

	string nombre = makeUniqueBoxName(makeNameFromDirectory(directory));
	JPbox *bx = createBoxForDirectory(directory, nombre);
	if (bx == nullptr)
	{
		return;
	}

	bx->setup(directory, nombre);
	bx->setonoff(true);
	bx->setPos(_x, _y);
	boxes.push_back(bx);
	if (isCueDraftMode())
	{
		int newIndex = int(boxes.size()) - 1;
		addCueAddedRealIndex(newIndex);
		openguinumber = newIndex;
	}
	requestCueRebuild();
}
void JPboxgroup::addBox(string directory)
{
	ofVec2f canvasMouse = screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY()));
	addBox(directory, canvasMouse.x, canvasMouse.y);
}
void JPboxgroup::triggerCodeOnActiveShader() {

	if (openguinumber != -1){
		boxes[openguinumber]->showCode = !boxes[openguinumber]->showCode;
	}
	/*for (int i = 0; i < boxes.size(); i++) {

		if (*activerender == i) {

			boxes[*activerender]->showCode = !boxes[*activerender]->showCode;

		}
	}*/
}
/*DEPRECATED:*/
void JPboxgroup::setupShaderRendersFromDataFolder()
{

	string path = "shaders";
	ofDirectory dir(path);
	dir.listDir();

	if (dir.isDirectory())
	{
		for (int i = 0; i < dir.size(); i++)
		{
			string compofolder_name = dir.getName(i);
			string compofolder_path = dir.getPath(i);
			// cout << " " << compofolder_path << endl;
			ofDirectory dir2(compofolder_path);
			if (dir2.isDirectory())
			{
				dir2.listDir();
				for (int k = 0; k < dir2.size(); k++)
				{
					string compofolder_name2 = dir2.getName(k);
					string compofolder_path2 = dir2.getPath(k);
					// cout << compofolder_path2 << endl;

					JPbox_shader test;
					test.setup(*font_p,
							   compofolder_path2,
							   compofolder_name2);
					test.setPos(ofRandom(ofGetWidth() * 1 / 4, ofGetWidth() * 3 / 4),
								ofRandom(ofGetHeight() * 1 / 4, ofGetHeight() * 3 / 4));
				}
			}
		}
	}
	// cout << "--------------------------------" << endl;
}
void JPboxgroup::clear()
{
	clearSelection();
	clearCue();
	transition.setFboPointer1(nullptr);
	transition.setFboPointer2(nullptr);
	activeSequence = false;

	for (int i = boxes.size() - 1; i >= 0; i--)
	{
		boxes[i]->clear();
		delete boxes[i];
		boxes[i] = nullptr;
	}

	for (size_t i = 0; i < controllers.size(); i++)
	{
		delete controllers[i];
		controllers[i] = nullptr;
	}

	openguinumber = -1;
	*activerender = 0;
	boxes.clear();
	controllers.clear();
}

void JPboxgroup::clearSelection()
{
	selectedBoxIndices.clear();
	draw_SelectionRect = false;
}

bool JPboxgroup::boxIntersectsSelection(JPbox *box) const
{
	if (box == nullptr)
	{
		return false;
	}
	float selectionLeft = std::min(lastMouseClick.x, selectionEnd.x);
	float selectionRight = std::max(lastMouseClick.x, selectionEnd.x);
	float selectionTop = std::min(lastMouseClick.y, selectionEnd.y);
	float selectionBottom = std::max(lastMouseClick.y, selectionEnd.y);
	float boxLeft = box->x - box->width / 2;
	float boxRight = box->x + box->width / 2;
	float boxTop = box->y - box->height / 2;
	float boxBottom = box->y + box->height / 2;
	return selectionLeft <= boxRight &&
		   selectionRight >= boxLeft &&
		   selectionTop <= boxBottom &&
		   selectionBottom >= boxTop;
}

void JPboxgroup::updateBoxSelection()
{
	selectedBoxIndices.clear();
	vector<JPbox *> &activeBoxes = isGroupViewActive() ? getActivePreset()->boxes : boxes;
	for (int i = 0; i < (int)activeBoxes.size(); i++)
	{
		if (boxIntersectsSelection(activeBoxes[i]))
		{
			selectedBoxIndices.push_back(i);
		}
	}
}

bool JPboxgroup::isBoxSelected(int index) const
{
	return std::find(selectedBoxIndices.begin(), selectedBoxIndices.end(), index) != selectedBoxIndices.end();
}

bool JPboxgroup::deleteBoxAtIndex(int index)
{
	if (index < 0 || index >= boxes.size())
	{
		return false;
	}
	if (isCueDraftMode() && !cueApplyingCommit)
	{
		if (isCueAddedRealIndex(index))
		{
			return revertCueDraftBox(index);
		}
		JPbox *draftBox = getCueDraftBoxForRealIndex(index);
		if (draftBox != nullptr)
		{
			draftBox->setonoff(false);
			draftBox->setBypass(true);
		}
		markCueDraftDirty(index, CUE_DIRTY_DELETED);
		updateCueDraftGraph();
		return true;
	}
	bool deletedCueSource = hasCue() && cueState.sourceIndex == index;
	bool deletedCuePreview = isCueNormalPreviewMode() && cueState.previewIndex == index;
	bool needsCueIndexShift = isCueNormalPreviewMode() && cueState.sourceIndex > index;
	bool needsPreviewIndexShift = isCueNormalPreviewMode() && cueState.previewIndex > index;
	if (!cueApplyingCommit && (isCueDraftMode() || deletedCueSource || deletedCuePreview))
	{
		clearCue();
	}

	string deletedName = boxes[index]->name;
	for (int k = boxes.size() - 1; k >= 0; k--)
	{
		if (k == index)
		{
			continue;
		}
		for (int l = 0; l < boxes[k]->fbohandlergroup.getSize(); l++)
		{
			if (boxes[k]->fbohandlergroup.getFboName(l) == deletedName)
			{
				boxes[k]->fbohandlergroup.deleteFboPointer(l);
			}
		}
	}

	boxes[index]->clear();
	delete boxes[index];
	boxes[index] = nullptr;
	boxes.erase(boxes.begin() + index);

	if (boxes.empty())
	{
		openguinumber = -1;
		clearCue();
		*activerender = 0;
		activeSequence = false;
		transition.setFboPointer1(nullptr);
		transition.setFboPointer2(nullptr);
	}
	else
	{
		if (openguinumber == index)
		{
			openguinumber = -1;
			for (int i = 0; i < controllers.size(); i++)
			{
				delete controllers[i];
				controllers[i] = nullptr;
			}
			controllers.clear();
		}
		else if (openguinumber > index)
		{
			openguinumber--;
		}
		if (needsCueIndexShift)
		{
			cueState.sourceIndex--;
		}
		if (needsPreviewIndexShift)
		{
			cueState.previewIndex--;
		}

		if (*activerender == index)
		{
			*activerender = std::min(index, int(boxes.size()) - 1);
		}
		else if (*activerender > index)
		{
			(*activerender)--;
		}
		*activerender = ofClamp(*activerender, 0, int(boxes.size()) - 1);
		if (!cueApplyingCommit)
		{
			updateTransition(*activerender);
			requestCueRebuild();
		}
	}
	return true;
}

bool JPboxgroup::deleteSelectedBoxes()
{
	if (selectedBoxIndices.empty())
	{
		return false;
	}
	std::sort(selectedBoxIndices.begin(), selectedBoxIndices.end(), std::greater<int>());
	selectedBoxIndices.erase(std::unique(selectedBoxIndices.begin(), selectedBoxIndices.end()), selectedBoxIndices.end());
	for (int i = 0; i < selectedBoxIndices.size(); i++)
	{
		deleteBoxAtIndex(selectedBoxIndices[i]);
	}
	clearSelection();
	return true;
}

void JPboxgroup::groupSelectedBoxes()
{
	if (selectedBoxIndices.size() < 2)
	{
		return;
	}

	// Calculate average position of selected boxes
	float avgX = 0, avgY = 0;
	for (int idx : selectedBoxIndices)
	{
		if (idx >= 0 && idx < boxes.size() && boxes[idx] != nullptr)
		{
			avgX += boxes[idx]->x;
			avgY += boxes[idx]->y;
		}
	}
	avgX /= (float)selectedBoxIndices.size();
	avgY /= (float)selectedBoxIndices.size();

	// Sort unique, descending for safe deletion
	vector<int> sortedIndices = selectedBoxIndices;
	std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
	sortedIndices.erase(std::unique(sortedIndices.begin(), sortedIndices.end()), sortedIndices.end());

	// Build XML in the EXACT format that JPbox_preset::setup() expects
	// (same format as JPboxgroup::save() but only for selected boxes)
	ofXml xml;
	xml.appendChild("activerender").set(0);

	for (int si : sortedIndices)
	{
		if (si < 0 || si >= boxes.size() || boxes[si] == nullptr) continue;

		JPbox *box = boxes[si];
		auto data = xml.appendChild("box");
		data.appendChild("nombre").set(box->name);
		data.appendChild("x").set((int)box->x);
		data.appendChild("y").set((int)box->y);
		data.appendChild("directory").set(box->dir);

		// Parameters
		if (box->parameters.getSize() > 0)
		{
			auto parameters = data.appendChild("parameters");
			for (int k = 0; k < box->parameters.getSize(); k++)
			{
				auto param = parameters.appendChild("param");
				param.appendChild("name").set(box->parameters.getName(k));
				if (box->parameters.getType(k) == box->parameters.BOOL)
				{
					param.appendChild("value").set(box->parameters.getBoolValue(k));
				}
				else
				{
					param.appendChild("min").set(box->parameters.getMin(k));
					param.appendChild("max").set(box->parameters.getMax(k));
					param.appendChild("value").set(box->parameters.getFloatValue(k));
					param.appendChild("movtype").set(box->parameters.getMovType(k));
					param.appendChild("speed").set(box->parameters.getSpeed(k));
				}
			}
		}

		// FBO links (preserved between grouped boxes)
		if (box->fbohandlergroup.getPointerSetsSize() > 0)
		{
			auto fboslinks = data.appendChild("fboslinks");
			for (int k = 0; k < box->fbohandlergroup.getSize(); k++)
			{
				if (box->fbohandlergroup.getisPointerSet(k))
				{
					fboslinks.appendChild(box->fbohandlergroup.getName(k))
						.set(box->fbohandlergroup.getFboName(k));
				}
			}
		}
	}

	// Save to temp XML file in data/groups/
	string outputDir = "data/groups/";
	string timestamp = ofGetTimestampString();
	string outputPath = outputDir + "group_" + timestamp + ".xml";
	ofFilePath::createEnclosingDirectory(outputPath);
	xml.save(outputPath);
	cout << "groupSelectedBoxes: saved to " << outputPath << endl;

	// Clear selection and delete the original boxes
	clearSelection();
	for (int i = 0; i < (int)sortedIndices.size(); i++)
	{
		deleteBoxAtIndex(sortedIndices[i]);
	}

	// Add the preset using the EXACT same path as loading any preset from disk
	// This calls createBoxForDirectory -> JPbox_preset -> JPbox_preset::setup()
	// which loads the XML, creates child boxes, restores params and links
	addBox(outputPath, avgX, avgY);

	// Set the newly created preset as the active render (output)
	int newIdx = (int)boxes.size() - 1;
	*activerender = newIdx;

	// Reset transition so it doesn't draw from deleted boxes' FBOs
	transition.setFboPointer1(&boxes[newIdx]->fbo);
	transition.setFboPointer2(&boxes[newIdx]->fbo);
	transition.setLerpValue(0);

	cout << "groupSelectedBoxes: done, boxes size=" << boxes.size() << " activerender=" << *activerender << endl;
}

void JPboxgroup::deleteSelectedShader()
{
	// In group view mode, delete the selected sub-boxes from the preset
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset == nullptr) return;

		// Priority 1: delete multi-selected boxes
		if (!selectedBoxIndices.empty())
		{
			// Sort descending so indices remain valid during deletion
			vector<int> sortedIndices = selectedBoxIndices;
			std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
			sortedIndices.erase(std::unique(sortedIndices.begin(), sortedIndices.end()), sortedIndices.end());
			clearSelection();

			for (int idx : sortedIndices)
			{
				if (idx < 0 || idx >= (int)preset->boxes.size()) continue;

				string deletedName = preset->boxes[idx]->name;

				// Remove FBO links pointing to the deleted box
				for (int k = (int)preset->boxes.size() - 1; k >= 0; k--)
				{
					if (k == idx) continue;
					for (int l = 0; l < preset->boxes[k]->fbohandlergroup.getSize(); l++)
					{
						if (preset->boxes[k]->fbohandlergroup.getFboName(l) == deletedName)
						{
							preset->boxes[k]->fbohandlergroup.deleteFboPointer(l);
						}
					}
				}

				preset->boxes[idx]->clear();
				delete preset->boxes[idx];
				preset->boxes[idx] = nullptr;
				preset->boxes.erase(preset->boxes.begin() + idx);
			}

			// After bulk delete, check if empty
			if (preset->boxes.empty())
			{
				preset->activeRender = 0;
				preset->onoff.boolValue = false;
				activeGroupPath.clear();
				activeTab = 0;
				groupInspectorIndex = -1;
				groupPreviewBoxIndex = -1;
				setControllers();
				return;
			}
			else
			{
				preset->activeRender = ofClamp(preset->activeRender, 0, (int)preset->boxes.size() - 1);
			}

			groupInspectorIndex = -1;
			setControllers();
			return;
		}

		// Priority 2: delete single inspected box
		if (preset != nullptr && groupInspectorIndex >= 0 && groupInspectorIndex < (int)preset->boxes.size())
		{
			int idx = groupInspectorIndex;
			string deletedName = preset->boxes[idx]->name;

			// Remove FBO links pointing to the deleted box
			for (int k = (int)preset->boxes.size() - 1; k >= 0; k--)
			{
				if (k == idx) continue;
				for (int l = 0; l < preset->boxes[k]->fbohandlergroup.getSize(); l++)
				{
					if (preset->boxes[k]->fbohandlergroup.getFboName(l) == deletedName)
					{
						preset->boxes[k]->fbohandlergroup.deleteFboPointer(l);
					}
				}
			}

			// Delete and remove
			preset->boxes[idx]->clear();
			delete preset->boxes[idx];
			preset->boxes[idx] = nullptr;
			preset->boxes.erase(preset->boxes.begin() + idx);

			// Adjust preset's activeRender
			if (preset->boxes.empty())
			{
				preset->activeRender = 0;
				preset->onoff.boolValue = false; // Prevent updateFBO from crashing on empty boxes
				// All sub-boxes deleted — pop back to main view
				activeGroupPath.clear();
				activeTab = 0;
				groupInspectorIndex = -1;
				groupPreviewBoxIndex = -1;
				setControllers();
				return;
			}
			else
			{
				preset->activeRender = ofClamp(preset->activeRender, 0, (int)preset->boxes.size() - 1);
			}

			groupInspectorIndex = -1;
			setControllers();
		}
		return;
	}

	if (deleteSelectedBoxes())
	{
		return;
	}


	// YA LO ENCONTRAMOS ESE BUG :
	/*Vamos a dejar esto aca por las dudas, que me reinicie todos los dibujos cuando limpio uno.
	esto es para solucionar el tema ese de que cuando borro un fboPointer, en vez de borrarlo es como
	si me pusiera otro shader como fboPointer.  Y si por alguna raz�n le haces un clear a todos los fbos entonces
	es como si reiniciara los punteros dentro del fbohandlergroup.fbos,
	Sin embargo. Si hubiera muchisimas cajitas, asumo que hacerle un clear y un allocate a todas las cajas
	es un proceso sumamente lento. pero es lo mismo que hace en el resize as� que no s�, es posible que a futuro
	tenga que solucionarlo. Esta modificaci�n es parte del proceso por encontrar ese bug que cada tanto(todav�a
	no s� porque aparece, y hace que crashee la app, as� que medio que estamos como doblecheckeando todo e investigando
	donde mierda puede estar ese bug.
	*/

	/*for (int i = boxes.size() - 1; i >= 0; i--) {
		boxes[i]->fbo.clear();
		boxes[i]->fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
	}*/


	for (int i = 0; i < boxes.size(); i++)
	{
		JPdragobject::setMouseOverride(screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY())));
		if (boxes[i]->mouseOver())
		{
			JPdragobject::clearMouseOverride();
			deleteBoxAtIndex(i);
			break;
		}
		JPdragobject::clearMouseOverride();
	}

	openguinumber = -1;
}

void JPboxgroup::copySelectedBoxes()
{
	clipboardXml.clear();

	// Determine which boxes to copy
	vector<int> srcIndices;

	if (isGroupViewActive())
	{
		// In group view: copy the sub-box at groupInspectorIndex
		JPbox_preset *preset = getActivePreset();
		if (preset == nullptr || groupInspectorIndex < 0 || groupInspectorIndex >= (int)preset->boxes.size())
		{
			cout << "copySelectedBoxes: nothing selected in group view" << endl;
			return;
		}
		srcIndices.push_back(groupInspectorIndex);
	}
	else
	{
		// In main view: use selectedBoxIndices, or fall back to openguinumber
		if (!selectedBoxIndices.empty())
		{
			srcIndices = selectedBoxIndices;
		}
		else if (openguinumber >= 0 && openguinumber < (int)boxes.size())
		{
			srcIndices.push_back(openguinumber);
		}
		else
		{
			cout << "copySelectedBoxes: nothing selected" << endl;
			return;
		}
	}

	// Build XML from source boxes (same format as save())
	ofXml xml;
	xml.appendChild("activerender").set(0);

	for (int si : srcIndices)
	{
		JPbox *box;
		if (isGroupViewActive())
		{
			JPbox_preset *preset = getActivePreset();
			if (si < 0 || si >= (int)preset->boxes.size() || preset->boxes[si] == nullptr) continue;
			box = preset->boxes[si];
		}
		else
		{
			if (si < 0 || si >= (int)boxes.size() || boxes[si] == nullptr) continue;
			box = boxes[si];
		}

		auto data = xml.appendChild("box");
		data.appendChild("nombre").set(box->name);
		data.appendChild("x").set((int)box->x);
		data.appendChild("y").set((int)box->y);
		data.appendChild("directory").set(box->dir);
		data.appendChild("onoff").set(box->getonoff());
		data.appendChild("bypass").set(box->getBypass());

		// Parameters
		if (box->parameters.getSize() > 0)
		{
			auto parameters = data.appendChild("parameters");
			for (int k = 0; k < box->parameters.getSize(); k++)
			{
				auto param = parameters.appendChild("param");
				param.appendChild("name").set(box->parameters.getName(k));
				if (box->parameters.getType(k) == box->parameters.BOOL)
				{
					param.appendChild("value").set(box->parameters.getBoolValue(k));
				}
				else
				{
					param.appendChild("min").set(box->parameters.getMin(k));
					param.appendChild("max").set(box->parameters.getMax(k));
					param.appendChild("value").set(box->parameters.getFloatValue(k));
					param.appendChild("movtype").set(box->parameters.getMovType(k));
					param.appendChild("speed").set(box->parameters.getSpeed(k));
				}
			}
		}

		// FBO links (between copied boxes)
		if (box->fbohandlergroup.getPointerSetsSize() > 0)
		{
			auto fboslinks = data.appendChild("fboslinks");
			for (int k = 0; k < box->fbohandlergroup.getSize(); k++)
			{
				if (box->fbohandlergroup.getisPointerSet(k))
				{
					fboslinks.appendChild(box->fbohandlergroup.getName(k))
						.set(box->fbohandlergroup.getFboName(k));
				}
			}
		}
	}

	clipboardXml = xml.toString();
	cout << "copySelectedBoxes: copied " << srcIndices.size() << " box(es) to clipboard" << endl;
}

void JPboxgroup::pasteBoxes()
{
	if (clipboardXml.empty())
	{
		cout << "pasteBoxes: clipboard is empty" << endl;
		return;
	}

	ofXml xml;
	if (!xml.parse(clipboardXml))
	{
		cout << "pasteBoxes: failed to parse clipboard XML" << endl;
		return;
	}

	auto boxloader = xml.find("/box");
	if (boxloader.empty())
	{
		cout << "pasteBoxes: no boxes in clipboard" << endl;
		return;
	}

	// Determine where to paste
	JPbox_preset *targetPreset = nullptr;
	if (isGroupViewActive())
	{
		targetPreset = getActivePreset();
		if (targetPreset == nullptr)
		{
			cout << "pasteBoxes: in group view but no active preset" << endl;
			return;
		}
	}

	// Calculate paste position at mouse cursor in canvas coordinates
	ofVec2f pastePos = screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY()));

	// First pass: create all boxes and add to destination
	// We add them one by one so makeUniqueBoxName checks against existing boxes each time
	vector<JPbox *> newBoxes;
	vector<string> srcNames; // original names for FBO link reconnection

	int pasteIndex = 0;
	for (auto &box : boxloader)
	{
		auto nombre = box.getChild("nombre");
		auto x = box.getChild("x");
		auto y = box.getChild("y");
		auto directory = box.getChild("directory");
		auto onoff = box.getChild("onoff");
		auto bypass = box.getChild("bypass");

		if (!nombre || !directory) continue;

		string dir = directory.getValue();

		// Reuse the same addBox pattern but without requiring an xml file on disk
		string nombreFinal = makeUniqueBoxName(makeNameFromDirectory(dir));
		JPbox *bx = createBoxForDirectory(dir, nombreFinal);
		if (bx == nullptr) continue;

		if (targetPreset != nullptr)
		{
			bx->setup(dir, nombreFinal);
			bx->setonoff(true);
			bx->setPos(pastePos.x + pasteIndex * 30.0f, pastePos.y + pasteIndex * 30.0f);
			targetPreset->boxes.push_back(bx);
		}
		else
		{
			bx->setup(dir, nombreFinal);
			bx->setonoff(true);
			bx->setPos(pastePos.x + pasteIndex * 30.0f, pastePos.y + pasteIndex * 30.0f);
			boxes.push_back(bx);
		}

		// Restore onoff/bypass from copied state
		if (onoff) bx->setonoff(onoff.getBoolValue());
		if (bypass) bx->setBypass(bypass.getBoolValue());

		// Restore parameters (only if the child exists)
		{
			auto paramsChild = box.getChild("parameters");
			if (paramsChild)
			{
				int paramIndex = 0;
				auto parameters = paramsChild.getChildren();
				for (auto &param : parameters)
				{
					if (paramIndex >= bx->parameters.getSize()) break;

					if (bx->parameters.getType(paramIndex) == bx->parameters.FLOAT)
					{
						bx->parameters.setName(param.getChild("name").getValue());
						bx->parameters.setMin(param.getChild("min").getFloatValue(), paramIndex);
						bx->parameters.setMax(param.getChild("max").getFloatValue(), paramIndex);
						bx->parameters.setFloatLerpValue(param.getChild("value").getFloatValue(), paramIndex);
						bx->parameters.setFloatValue(param.getChild("value").getFloatValue(), paramIndex);
						bx->parameters.setmovetype(param.getChild("movtype").getIntValue(), paramIndex);
						bx->parameters.setSpeed(param.getChild("speed").getFloatValue(), paramIndex);
					}
					else if (bx->parameters.getType(paramIndex) == bx->parameters.BOOL)
					{
						bx->parameters.setName(param.getChild("name").getValue());
						bx->parameters.setBoolValue(param.getChild("value").getBoolValue(), paramIndex);
					}
					paramIndex++;
				}
			}
		}

		srcNames.push_back(nombre.getValue());
		newBoxes.push_back(bx);
		pasteIndex++;
	}

	if (newBoxes.empty())
	{
		cout << "pasteBoxes: no valid boxes to paste" << endl;
		return;
	}

	// Second pass: reconnect FBO links using name mapping (old name -> new name)
	pasteIndex = 0;
	for (auto &box : boxloader)
	{
		if (pasteIndex >= (int)newBoxes.size()) break;
		JPbox *newBox = newBoxes[pasteIndex];

		auto fbosChild = box.getChild("fboslinks");
		if (!fbosChild)
		{
			pasteIndex++;
			continue;
		}
		auto fboslinks = fbosChild.getChildren();
		int linkIndex = 0;
		for (auto &fbolink : fboslinks)
		{
			string linkedName = fbolink.getValue();
			if (newBox->fbohandlergroup.getSize() <= linkIndex)
			{
				linkIndex++;
				continue;
			}

			// Find the box in our newly pasted set whose old name matches
			if (targetPreset != nullptr)
			{
				for (size_t k = 0; k < targetPreset->boxes.size(); k++)
				{
					if (targetPreset->boxes[k] == newBox) continue;
					// Check the original name by looking at srcNames at matching index
					// For pasted boxes within the same preset, names are already unique
					string candidateName = targetPreset->boxes[k]->name;
					for (size_t si = 0; si < srcNames.size(); si++)
					{
						if (newBoxes[si] == targetPreset->boxes[k])
						{
							candidateName = srcNames[si];
							break;
						}
					}
					if (candidateName == linkedName && newBox->fbohandlergroup.getSize() > linkIndex)
					{
						newBox->fbohandlergroup.setFboPointer(
							&targetPreset->boxes[k]->fbo,
							&targetPreset->boxes[k]->name,
							linkIndex);
						break;
					}
				}
			}
			else
			{
				for (size_t k = 0; k < boxes.size(); k++)
				{
					if (boxes[k] == newBox) continue;
					// Match by original name via srcNames
					string candidateName = boxes[k]->name;
					for (size_t si = 0; si < srcNames.size(); si++)
					{
						if (newBoxes[si] == boxes[k])
						{
							candidateName = srcNames[si];
							break;
						}
					}
					if (candidateName == linkedName && newBox->fbohandlergroup.getSize() > linkIndex)
					{
						newBox->fbohandlergroup.setFboPointer(
							&boxes[k]->fbo,
							&boxes[k]->name,
							linkIndex);
						break;
					}
				}
			}
			linkIndex++;
		}
		pasteIndex++;
	}

	requestCueRebuild();
	cout << "pasteBoxes: pasted " << newBoxes.size() << " box(es)" << endl;
}

ofTexture *JPboxgroup::getActiveTexture()
{
	if (boxes.size() >= 1)
	{
		return &boxes[*activerender]->fbo.getTexture();
		// boxes[*activerender]->shaderrender.fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	}
	return nullptr;
}
int JPboxgroup::getBoxesSize()
{
	return boxes.size();
}
ofFbo *JPboxgroup::getActiverender()
{
	if (boxes.size() >= 1)
	{
		return &boxes[*activerender]->fbo;
		// boxes[*activerender]->shaderrender.fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	}
	return nullptr;
}
int JPboxgroup::getActiverenderNum() {
	return *activerender;
}
/*ofFbo JPboxgroup::getActiverender() {
	if (boxes.size() >= 1) {
		return boxes[*activerender]->shaderrender.fbo;
		//boxes[*activerender]->shaderrender.fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	}
}*/

// ============================================================
// TAB SYSTEM
// ============================================================

vector<vector<int>> JPboxgroup::collectAllPresetPaths() const
{
	vector<vector<int>> paths;

	// Helper to recursively find presets
	function<void(const vector<int> &basePath, const vector<JPbox *> &boxList)> findPresets;
	findPresets = [&](const vector<int> &basePath, const vector<JPbox *> &boxList)
	{
		for (int i = 0; i < (int)boxList.size(); i++)
		{
			if (boxList[i] != nullptr && boxList[i]->getTipo() == JPbox::PRESETBOX)
			{
				vector<int> path = basePath;
				path.push_back(i);
				paths.push_back(path);
				// Recurse into nested presets
				JPbox_preset *innerPreset = static_cast<JPbox_preset *>(boxList[i]);
				findPresets(path, innerPreset->boxes);
			}
		}
	};

	findPresets(vector<int>(), boxes);
	return paths;
}

void JPboxgroup::drawTabs()
{
	vector<vector<int>> presetPaths = collectAllPresetPaths();
	int totalTabs = 1 + (int)presetPaths.size(); // main + preset tabs
	if (totalTabs <= 1)
	{
		return; // No preset boxes, no tabs to show
	}

	const float tabBarX = 0;
	const float tabBarY = tabBarOffsetY;
	const float tabHeight = 28;
	const float tabMinWidth = 90;
	const float pad = 8;
	const float gap = 2;
	const float tabH = tabHeight;
	float x = tabBarX + pad;
	const float y = tabBarY + pad;

	// Draw tab bar background
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(18, 18, 22, 210);
	ofDrawRectRounded(x, y, ofGetWidth() - pad * 2, tabH + gap * 2, 4);
	ofPopStyle();

	// Draw "Main" tab (activeTab == 0)
	drawSingleTab(x, y, tabH, "MAIN", activeTab == 0);

	// Measure main tab width
	float mainTabW = std::max(tabMinWidth, jp_constants::p_font.stringWidth("MAIN") + 20);
	x += mainTabW + gap;

	// Draw preset group tabs
	for (int ti = 0; ti < (int)presetPaths.size(); ti++)
	{
		const vector<int> &path = presetPaths[ti];
		// Get the name from the deepest preset in the path
		JPbox *labelBox = boxes[path[0]];
		JPbox_preset *labelPreset = (labelBox && labelBox->getTipo() == JPbox::PRESETBOX)
			? static_cast<JPbox_preset *>(labelBox) : nullptr;
		for (size_t d = 1; d < path.size() && labelPreset != nullptr; d++)
		{
			if (path[d] < (int)labelPreset->boxes.size())
				labelBox = labelPreset->boxes[path[d]];
			else
				labelBox = nullptr;
			labelPreset = (labelBox && labelBox->getTipo() == JPbox::PRESETBOX)
				? static_cast<JPbox_preset *>(labelBox) : nullptr;
		}
		string tabName = (labelBox != nullptr) ? labelBox->name : "";
		if (tabName.empty())
		{
			tabName = "Group " + ofToString(ti);
		}
		float tabW = std::max(tabMinWidth, jp_constants::p_font.stringWidth(tabName) + 24);
		drawSingleTab(x, y, tabH, tabName, activeTab == (ti + 1));
		x += tabW + gap;
	}
}

void JPboxgroup::drawSingleTab(float x, float y, float h, const string &label, bool active)
{
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);

	const float tabMinWidth = 90;
	float textW = jp_constants::p_font.stringWidth(label);
	float tabW = std::max(tabMinWidth, textW + 24);

	if (active)
	{
		ofSetColor(40, 180, 80, 235);
	}
	else
	{
		ofSetColor(35, 35, 42, 225);
	}
	ofDrawRectRounded(x, y, tabW, h, 3);

	// Border
	ofNoFill();
	ofSetLineWidth(1);
	if (active)
	{
		ofSetColor(60, 220, 100, 255);
	}
	else
	{
		ofSetColor(55, 55, 65, 200);
	}
	ofDrawRectRounded(x, y, tabW, h, 3);
	ofFill();

	// Text
	ofSetColor(active ? 255 : 200);
	jp_constants::p_font.drawString(label, x + (tabW - textW) * 0.5f, y + h * 0.5f + 5);

	ofPopStyle();
}

int JPboxgroup::getTabAtScreenPos(int screenX, int screenY) const
{
	vector<vector<int>> presetPaths = collectAllPresetPaths();
	int totalTabs = 1 + (int)presetPaths.size();
	const float tabBarY = tabBarOffsetY;
	const float tabHeight = 28;
	if (totalTabs <= 1 || screenY < tabBarY || screenY > tabBarY + tabHeight + 8 + tabBarOffsetY)
	{
		return -1;
	}

	const float tabBarX = 0;
	const float tabMinWidth = 90;
	const float pad = 8;
	const float gap = 2;
	const float tabH = tabHeight;
	float x = tabBarX + pad;

	// Check Main tab
	float mainTabW = std::max(tabMinWidth, jp_constants::p_font.stringWidth("MAIN") + 20);
	if (screenX >= x && screenX <= x + mainTabW)
	{
		return 0; // Main tab
	}
	x += mainTabW + gap;

	// Check preset tabs
	for (int ti = 0; ti < (int)presetPaths.size(); ti++)
	{
		const vector<int> &path = presetPaths[ti];
		// Get name from the deepest preset in the path
		JPbox *labelBox = boxes[path[0]];
		JPbox_preset *labelPreset = (labelBox && labelBox->getTipo() == JPbox::PRESETBOX)
			? static_cast<JPbox_preset *>(labelBox) : nullptr;
		for (size_t d = 1; d < path.size() && labelPreset != nullptr; d++)
		{
			if (path[d] < (int)labelPreset->boxes.size())
				labelBox = labelPreset->boxes[path[d]];
			else
				labelBox = nullptr;
			labelPreset = (labelBox && labelBox->getTipo() == JPbox::PRESETBOX)
				? static_cast<JPbox_preset *>(labelBox) : nullptr;
		}
		string tabName = (labelBox != nullptr) ? labelBox->name : "";
		if (tabName.empty())
		{
			tabName = "Group " + ofToString(ti);
		}
		float tabW = std::max(tabMinWidth, jp_constants::p_font.stringWidth(tabName) + 24);
		if (screenX >= x && screenX <= x + tabW)
		{
			return ti + 1;
		}
		x += tabW + gap;
	}
	return -1;
}

JPbox_preset *JPboxgroup::getActivePreset() const
{
	if (activeGroupPath.empty()) return nullptr;

	// Navigate the path to find the target preset
	JPbox *box = boxes[activeGroupPath[0]];
	if (box == nullptr || box->getTipo() != JPbox::PRESETBOX) return nullptr;
	JPbox_preset *preset = static_cast<JPbox_preset *>(box);

	// For nested presets, go deeper
	for (size_t depth = 1; depth < activeGroupPath.size(); depth++)
	{
		int idx = activeGroupPath[depth];
		if (idx < 0 || idx >= (int)preset->boxes.size() || preset->boxes[idx] == nullptr) return nullptr;
		if (preset->boxes[idx]->getTipo() != JPbox::PRESETBOX) return nullptr;
		preset = static_cast<JPbox_preset *>(preset->boxes[idx]);
	}
	return preset;
}

bool JPboxgroup::handleTabClick()
{
	int tabIndex = getTabAtScreenPos(ofGetMouseX(), ofGetMouseY());
	if (tabIndex < 0)
	{
		return false;
	}

	// Handle double-click on an ALREADY active tab → rename
	if (tabIndex == activeTab && isDoubleClick)
	{
		vector<vector<int>> presetPaths = collectAllPresetPaths();
		int ti = tabIndex - 1;
		if (tabIndex > 0 && ti >= 0 && ti < (int)presetPaths.size())
		{
			// Navigate path to find the preset box
			const vector<int> &path = presetPaths[ti];
			JPbox *targetBox = boxes[path[0]];
			JPbox_preset *targetPreset = (targetBox && targetBox->getTipo() == JPbox::PRESETBOX)
				? static_cast<JPbox_preset *>(targetBox) : nullptr;
			for (size_t d = 1; d < path.size() && targetPreset != nullptr; d++)
			{
				if (path[d] < (int)targetPreset->boxes.size())
					targetBox = targetPreset->boxes[path[d]];
				else
					targetBox = nullptr;
				targetPreset = (targetBox && targetBox->getTipo() == JPbox::PRESETBOX)
					? static_cast<JPbox_preset *>(targetBox) : nullptr;
			}
			if (targetBox != nullptr)
			{
				string newName = ofSystemTextBoxDialog("Rename group", targetBox->name);
				if (!newName.empty())
				{
					targetBox->name = newName;
				}
			}
		}
		return true;
	}

	if (tabIndex == activeTab)
	{
		// Clicking the same tab: just reset zoom
		viewportZoom = 1.0f;
		viewportPan = ofVec2f(0, 0);
		// Store the reset state for this tab
		ensureTabStateSize();
		if (tabIndex < (int)tabZooms.size())
		{
			tabZooms[tabIndex] = viewportZoom;
			tabPans[tabIndex] = viewportPan;
		}
		return true;
	}

	// Save current tab's zoom/pan before switching
	ensureTabStateSize();
	if (activeTab < (int)tabZooms.size())
	{
		tabZooms[activeTab] = viewportZoom;
		tabPans[activeTab] = viewportPan;
	}

	activeTab = tabIndex;
	clearSelection();

	// Restore new tab's zoom/pan (or reset if first visit)
	if (activeTab < (int)tabZooms.size())
	{
		viewportZoom = tabZooms[activeTab];
		viewportPan = tabPans[activeTab];
	}
	else
	{
		viewportZoom = 1.0f;
		viewportPan = ofVec2f(0, 0);
		ensureTabStateSize();
		if (activeTab < (int)tabZooms.size())
		{
			tabZooms[activeTab] = viewportZoom;
			tabPans[activeTab] = viewportPan;
		}
	}

	if (activeTab == 0)
	{
		// Switch to main view
		activeGroupPath.clear();
		openguinumber = -1;
		groupInspectorIndex = -1;
		groupPreviewBoxIndex = -1;
	}
	else
	{
		// Switch to group view - show sub-boxes
		vector<vector<int>> presetPaths = collectAllPresetPaths();
		int ti = activeTab - 1;
		if (ti >= 0 && ti < (int)presetPaths.size())
		{
			activeGroupPath = presetPaths[ti];
			openguinumber = -1;
			groupInspectorIndex = -1;
			groupPreviewBoxIndex = -1;
		}
	}

	return true;
}

void JPboxgroup::ensureTabStateSize()
{
	int totalTabs = 1 + (int)collectAllPresetPaths().size();
	while ((int)tabZooms.size() < totalTabs)
	{
		tabZooms.push_back(1.0f);
		tabPans.push_back(ofVec2f(0, 0));
	}
}
