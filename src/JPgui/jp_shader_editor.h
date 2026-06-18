#pragma once

#include "defines.h"
#include "ofMain.h"
#include <vector>
#include <string>

using namespace std;

// ============================================================
// JPShaderEditor — In-app GLSL shader code editor
// ============================================================
// Features:
//   - Multi-tab editor with close buttons
//   - GLSL syntax highlighting (keywords, types, functions, etc.)
//   - Vertical scroll + horizontal scroll
//   - Zoom in/out (Ctrl + mouse wheel)
//   - Ctrl+S to save file (triggers auto-reload via datemodified)
//   - Cursor: click to position, arrow keys, Home/End, PageUp/Down
//   - Line numbers gutter
//   - Status bar (line:col, modified indicator, zoom level)
// ============================================================

class JPShaderEditor
{
public:
	// ---- Public types ----
	struct EditorTab
	{
		string filePath;        // Full path to .frag file
		string shaderName;      // Display name for the tab
		vector<string> lines;   // File content, one string per line
		int cursorLine = 0;     // 0-based cursor line
		int cursorCol = 0;      // 0-based cursor column
		int scrollLine = 0;     // First visible line (vertical scroll)
		int scrollCol = 0;      // First visible column (horizontal scroll)
		float fontSize = 16.0f; // Current font size for this tab
		bool modified = false;  // Unsaved changes flag
		int targetBoxIndex = -1;// Index in boxes[] if opened from inspector, -1 if from shader index

		// Selection
		int selStartLine = -1;
		int selStartCol = -1;
		int selEndLine = -1;
		int selEndCol = -1;
		bool hasSelection() const { return selStartLine >= 0 && selEndLine >= 0; }
		void clearSelection() { selStartLine = selEndLine = -1; }
	};

	// ---- Lifecycle ----
	JPShaderEditor();
	~JPShaderEditor();

	void setup();
	void draw();
	void exit();

	// ---- Tab management ----
	void openShader(string filePath, string shaderName, int boxIndex = -1);
	void closeTab(int index);
	void closeActiveTab();
	int  findTabByPath(const string& path);

	// ---- Visibility ----
	bool isVisible() const { return visible; }
	void setVisible(bool v) { visible = v; }
	void show() { visible = true; }
	void hide() { visible = false; }
	bool wantsKeyCapture() const { return visible && activeTab >= 0; }
	bool justOpened() const { return _justOpened; }
	void clearJustOpened() { _justOpened = false; }

	// ---- Input ----
	void keyPressed(int key);
	void keycodePressed(int keycode);
	void mousePressed(int x, int y, int button);
	void mouseDragged(int x, int y, int button);
	void mouseScrolled(int x, int y, float scrollX, float scrollY);

	// ---- Save ----
	bool saveCurrentTab();

	// ---- Layout constants ----
	static constexpr float MARGIN = 40.0f;
	static constexpr float TOP_BAR_H = 32.0f;
	static constexpr float TAB_BAR_H = 28.0f;
	static constexpr float STATUS_BAR_H = 22.0f;
	static constexpr float LINE_NUMBER_W = 48.0f;
	static constexpr float SCROLLBAR_W = 10.0f;

private:
	// ---- State ----
	vector<EditorTab> tabs;
	int activeTab = -1;
	bool visible = false;

	// ---- Font ----
	ofTrueTypeFont editorFont;
	float currentFontSize = 16.0f;
	const float MIN_FONT_SIZE = 10.0f;
	const float MAX_FONT_SIZE = 28.0f;
	void reloadFont(float size);

	// ---- Rendering ----
	void drawTopBar();
	void drawTabBar();
	void drawCodeArea();
	void drawStatusBar();
	void drawLineNumbers(int tabIndex, float codeX, float codeY, float codeW, float codeH);
	void drawCodeLines(int tabIndex, float codeX, float codeY, float codeW, float codeH);
	void drawScrollbar(int tabIndex, float scrollX, float codeY, float codeH);
	void drawCursor(int tabIndex, float codeX, float codeY);

	// ---- Syntax Highlighting ----
	struct ColorSegment
	{
		int start;
		int len;
		ofColor color;
	};
	vector<ColorSegment> tokenizeLine(const string& line);
	ofColor getTokenColor(const string& token, bool isPreprocessor, bool isComment, bool isNumber);

	// ---- Layout helpers ----
	float getCharWidth() const;
	float getLineHeight() const;
	int   getVisibleLines(float codeH) const;
	int   getVisibleCols(float codeW) const;

	// ---- Cursor / Scroll ----
	void ensureCursorVisible(int tabIndex, float codeW, float codeH);
	void clampCursor(int tabIndex);

	// ---- File I/O ----
	vector<string> loadFileLines(const string& path);
	void writeFileLines(const string& path, const vector<string>& lines);

	// ---- Blink ----
	float lastBlinkTime = 0.0f;
	bool  cursorVisible = true;
	bool  _justOpened = false;       // Set by openShader, cleared by ofApp

	// Internal clipboard (avoids dependency on OS clipboard API)
	string _clipboard;

	// ---- Selection helpers ----
	void deleteSelection(int tabIndex);
	void getSelectedText(string& out, int tabIndex) const;
};
