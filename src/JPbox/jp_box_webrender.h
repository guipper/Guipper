#pragma once
#include "jp_box.h"
#include <memory>
#include <string>

class JPbox_webrender : public JPbox
{
public:
	JPbox_webrender();
	~JPbox_webrender() override;

	using JPbox::setup;
	void setup(string _directory, string _nombre) override;
	void update() override;
	void draw() override;
	void reload() override;
	void clear() override;
	void updateFBO() override;
	void setCaptureActive(bool active) override;

	void saveCustomState(ofXml &boxNode) const override;
	void loadCustomState(const ofXml &boxNode) override;
	void copyCustomStateFrom(const JPbox *source) override;

	// Navigation & interaction
	void navigate(const std::string &rawUrl);
	void reloadPage();
	void goBack();
	void goForward();
	void triggerMouseClick();
	void setMuted(bool muted);
	bool isMuted() const { return mutedState; }
	bool hasActiveUrl() const { return !activeUrl.empty(); }
	std::string statusLabel() const;

	// Inspector UI
	float inspectorHeight() const { return 194.0f; }
	void layoutInspector(float left, float top, float width);
	void drawInspector();
	bool clickInspector(float x, float y);

	std::string urlText;
	bool urlFocused = false;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;

	std::string activeUrl;
	bool mutedState = false;
	bool showCursorOverlay = false;
	bool captureActive = true;

	ofTexture webTexture;
	uint64_t uploadedSerial = 0;
	bool hasRenderedWebFrame = false;

	// Parameter indices
	int scalex = -1;
	int scaley = -1;
	int offsetx = -1;
	int offsety = -1;
	int strech = -1;
	int scaleRatioIndex = -1;
	int scrollYIndex = -1;
	int scrollXIndex = -1;
	int zoomIndex = -1;
	int mouseXIndex = -1;
	int mouseYIndex = -1;

	float lastSentScrollX = -1.0f;
	float lastSentScrollY = -1.0f;
	float lastSentZoom = -1.0f;
	float lastSentMouseX = -1.0f;
	float lastSentMouseY = -1.0f;
	uint64_t lastInputSendMs = 0;

	// Inspector rectangles
	ofRectangle urlFieldRect;
	ofRectangle loadButtonRect;
	ofRectangle reloadButtonRect;
	ofRectangle backButtonRect;
	ofRectangle forwardButtonRect;
	ofRectangle clickButtonRect;
	ofRectangle muteButtonRect;
	ofRectangle cursorButtonRect;
	ofRectangle statusRect;

	void ensureSessionRunning();
	void drawPlaceholderToFbo();
};

