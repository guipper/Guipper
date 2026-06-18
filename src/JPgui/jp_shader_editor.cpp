#include "jp_shader_editor.h"
#include "../JPutils/jp_constants.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

using namespace std;

// ============================================================
// COLOR PALETTE (VS Code Dark+ inspired)
// ============================================================
static const ofColor COL_BG(30, 30, 30, 220);       // Editor background (semi-transparent)
static const ofColor COL_GUTTER_BG(37, 37, 38, 220); // Line number gutter
static const ofColor COL_LINE_NUM(133, 133, 133);   // Line numbers
static const ofColor COL_LINE_NUM_ACTIVE(198, 198, 198); // Active line number
static const ofColor COL_CURRENT_LINE(42, 45, 46, 180);  // Current line highlight
static const ofColor COL_CURSOR(255, 255, 255);      // Cursor
static const ofColor COL_SELECTION(38, 79, 120);     // Selection
static const ofColor COL_DEFAULT(212, 212, 212);     // Default text
static const ofColor COL_KEYWORD(197, 134, 192);     // Flow: if, else, return, for, while...
static const ofColor COL_TYPE(86, 156, 214);         // Types: float, vec3, sampler2D...
static const ofColor COL_FUNCTION(220, 220, 170);    // Built-in functions: sin, mix, fract...
static const ofColor COL_NUMBER(181, 206, 168);      // Numbers
static const ofColor COL_COMMENT(106, 153, 85);      // Comments
static const ofColor COL_STRING(206, 145, 120);      // Strings
static const ofColor COL_PREPROC(155, 155, 155);     // Preprocessor: #ifdef, #define...
static const ofColor COL_UNIFORM(78, 201, 176);      // Storage: uniform, varying...
static const ofColor COL_TOP_BAR(50, 50, 55);        // Top bar background
static const ofColor COL_TAB_ACTIVE(55, 55, 60);     // Active tab (must differ from COL_BG)
static const ofColor COL_TAB_INACTIVE(37, 37, 42);   // Inactive tab
static const ofColor COL_TAB_BAR_BG(32, 32, 36);     // Tab bar background
static const ofColor COL_TAB_BORDER(70, 70, 75);     // Tab border
static const ofColor COL_TAB_MODIFIED(255, 200, 50); // Modified dot
static const ofColor COL_TAB_CLOSE_HOVER(200, 60, 60);// Close button hover
static const ofColor COL_BTN_SAVE(40, 140, 100);     // Save button
static const ofColor COL_BTN_CLOSE(160, 60, 60);     // Close button
static const ofColor COL_BTN_TEXT(240, 240, 240);    // Button text
static const ofColor COL_STATUS_BAR(0, 122, 204);    // Status bar
static const ofColor COL_STATUS_TEXT(255, 255, 255); // Status bar text
static const ofColor COL_SCROLLBAR_BG(60, 60, 60);   // Scrollbar background
static const ofColor COL_SCROLLBAR_THUMB(100, 100, 105); // Scrollbar thumb

// GLSL Keywords by category
static const vector<string> GLSL_TYPES = {
	"void", "float", "int", "bool", "vec2", "vec3", "vec4",
	"ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4",
	"mat2", "mat3", "mat4"
};

static const vector<string> GLSL_KEYWORDS = {
	"if", "else", "for", "while", "do", "return", "break", "continue",
	"switch", "case", "default", "discard"
};

static const vector<string> GLSL_STORAGE = {
	"uniform", "varying", "attribute", "in", "out", "inout", "const"
};

static const vector<string> GLSL_FUNCTIONS = {
	"sin", "cos", "tan", "asin", "acos", "atan",
	"sinh", "cosh", "tanh",
	"pow", "exp", "log", "log2", "sqrt", "inversesqrt",
	"abs", "sign", "floor", "ceil", "fract", "mod",
	"min", "max", "clamp", "mix", "step", "smoothstep",
	"length", "distance", "dot", "cross", "normalize", "reflect", "refract",
	"radians", "degrees",
	"texture2D", "texture2DRect", "texture", "texture2DLod",
	"lessThan", "greaterThan", "lessThanEqual", "greaterThanEqual",
	"equal", "notEqual", "any", "all", "not",
	"matrixCompMult"
};

// ---- Helpers ----

static bool isWordChar(char c) {
	return isalnum((unsigned char)c) || c == '_';
}

static bool isDigitChar(char c) {
	return isdigit((unsigned char)c) || c == '.';
}

static bool isHexChar(char c) {
	return isxdigit((unsigned char)c);
}

static string toLowerStr(const string& s) {
	string r = s;
	for (auto& c : r) c = tolower((unsigned char)c);
	return r;
}

static bool vecContains(const vector<string>& vec, const string& s) {
	string ls = toLowerStr(s);
	for (const auto& v : vec) {
		if (toLowerStr(v) == ls) return true;
	}
	return false;
}

// ============================================================
// Lifecycle
// ============================================================

JPShaderEditor::JPShaderEditor() {}
JPShaderEditor::~JPShaderEditor() {}

void JPShaderEditor::setup()
{
	reloadFont(MIN_FONT_SIZE);
	currentFontSize = MIN_FONT_SIZE;
	lastBlinkTime = ofGetElapsedTimef();
}

void JPShaderEditor::exit()
{
	tabs.clear();
}

void JPShaderEditor::reloadFont(float size)
{
	// Clamp size
	if (size < MIN_FONT_SIZE) size = MIN_FONT_SIZE;
	if (size > MAX_FONT_SIZE) size = MAX_FONT_SIZE;

	string fontPath = "font/consola.ttf";
	ofFile f(ofToDataPath(fontPath, true));
	if (!f.exists()) {
		// Fallback: try absolute Windows path
		fontPath = "C:/Windows/Fonts/consola.ttf";
	}

	editorFont.load(fontPath, (int)size);
	currentFontSize = size;
}

// ============================================================
// Tab management
// ============================================================

int JPShaderEditor::findTabByPath(const string& path)
{
	for (int i = 0; i < (int)tabs.size(); i++) {
		if (tabs[i].filePath == path) return i;
	}
	return -1;
}

void JPShaderEditor::openShader(string filePath, string shaderName, int boxIndex)
{
	// Check if already open
	int existing = findTabByPath(filePath);
	if (existing >= 0) {
		activeTab = existing;
		visible = true;
		_justOpened = true;
		return;
	}

	EditorTab tab;
	tab.filePath = filePath;
	tab.shaderName = shaderName;
	tab.lines = loadFileLines(filePath);
	tab.cursorLine = 0;
	tab.cursorCol = 0;
	tab.scrollLine = 0;
	tab.scrollCol = 0;
	tab.fontSize = currentFontSize;
	tab.modified = false;
	tab.targetBoxIndex = boxIndex;

	tabs.push_back(tab);
	activeTab = (int)tabs.size() - 1;
	visible = true;
	_justOpened = true;

	// Make sure font is set
	reloadFont(tab.fontSize);
}

void JPShaderEditor::closeTab(int index)
{
	if (index < 0 || index >= (int)tabs.size()) return;

	tabs.erase(tabs.begin() + index);

	if (tabs.empty()) {
		activeTab = -1;
		visible = false;
	} else if (activeTab >= (int)tabs.size()) {
		activeTab = (int)tabs.size() - 1;
	}
}

void JPShaderEditor::closeActiveTab()
{
	closeTab(activeTab);
}

// ============================================================
// Save
// ============================================================

bool JPShaderEditor::saveCurrentTab()
{
	if (activeTab < 0 || activeTab >= (int)tabs.size()) return false;

	EditorTab& tab = tabs[activeTab];
	writeFileLines(tab.filePath, tab.lines);
	tab.modified = false;

	// The auto-reload system in JPboxgroup::update() detects the
	// datemodified change and calls boxes[i]->reload() automatically.
	// We update the datemodified timestamp on the target box so the
	// next update cycle picks it up.

	return true;
}

// ============================================================
// File I/O
// ============================================================

vector<string> JPShaderEditor::loadFileLines(const string& path)
{
	vector<string> result;
	ifstream file(ofToDataPath(path, true));
	if (!file.is_open()) {
		// Try the path as-is (absolute)
		file.open(path);
	}
	if (file.is_open()) {
		string line;
		while (getline(file, line)) {
			// Remove \r if present (Windows line endings)
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			// Expand tab characters to 4 spaces
			string expanded;
			for (char c : line) {
				if (c == '\t') {
					expanded += "    ";
				} else {
					expanded += c;
				}
			}
			result.push_back(expanded);
		}
	}
	// Ensure at least one empty line
	if (result.empty()) {
		result.push_back("");
	}
	return result;
}

void JPShaderEditor::writeFileLines(const string& path, const vector<string>& lines)
{
	string realPath = ofToDataPath(path, true);
	ofFile f(realPath);
	if (!f.exists()) {
		// Use path as-is
		realPath = path;
	}
	ofstream file(realPath, ios::binary);
	if (file.is_open()) {
		for (size_t i = 0; i < lines.size(); i++) {
			file << lines[i];
			if (i < lines.size() - 1) file << "\n";
		}
	}
}

// ============================================================
// Syntax Highlighting
// ============================================================

ofColor JPShaderEditor::getTokenColor(const string& token, bool isPreprocessor, bool isComment, bool isNumber)
{
	if (isComment)    return COL_COMMENT;
	if (isPreprocessor) return COL_PREPROC;
	if (isNumber)     return COL_NUMBER;
	if (vecContains(GLSL_TYPES, token))     return COL_TYPE;
	if (vecContains(GLSL_KEYWORDS, token))  return COL_KEYWORD;
	if (vecContains(GLSL_STORAGE, token))   return COL_UNIFORM;
	if (vecContains(GLSL_FUNCTIONS, token)) return COL_FUNCTION;
	return COL_DEFAULT;
}

vector<JPShaderEditor::ColorSegment> JPShaderEditor::tokenizeLine(const string& line)
{
	vector<ColorSegment> result;
	int len = (int)line.size();
	if (len == 0) return result;

	// Detect preprocessor line
	bool isPreprocessorLine = false;
	for (int i = 0; i < len; i++) {
		if (line[i] == ' ' || line[i] == '\t') continue;
		if (line[i] == '#') { isPreprocessorLine = true; break; }
		break;
	}

	int i = 0;
	while (i < len) {
		// ---- Whitespace (group and draw as default-colored text) ----
		if (line[i] == ' ' || line[i] == '\t') {
			int start = i;
			while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
			result.push_back({start, i - start, COL_DEFAULT});
			continue;
		}

		// ---- Line comment: // ----
		if (i + 1 < len && line[i] == '/' && line[i + 1] == '/') {
			result.push_back({i, len - i, COL_COMMENT});
			return result;
		}

		// ---- Block comment: /* ... */ ----
		if (i + 1 < len && line[i] == '/' && line[i + 1] == '*') {
			int start = i;
			i += 2;
			while (i + 1 < len && !(line[i] == '*' && line[i + 1] == '/')) {
				i++;
			}
			if (i + 1 < len) i += 2; // Skip */
			result.push_back({start, i - start, COL_COMMENT});
			continue;
		}

		// ---- Preprocessor (whole line gets preprocessor color) ----
		if (line[i] == '#') {
			result.push_back({i, len - i, COL_PREPROC});
			return result;
		}

		// ---- Number ----
		if (isdigit((unsigned char)line[i]) ||
			(line[i] == '.' && i + 1 < len && isdigit((unsigned char)line[i + 1]))) {
			int start = i;
			// Hex: 0x...
			if (line[i] == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
				i += 2;
				while (i < len && isHexChar(line[i])) i++;
			} else {
				bool hasDot = false;
				while (i < len && (isdigit((unsigned char)line[i]) || line[i] == '.')) {
					if (line[i] == '.') {
						if (hasDot) break;
						hasDot = true;
					}
					i++;
				}
				// Scientific notation: 1.0e10
				if (i < len && (line[i] == 'e' || line[i] == 'E')) {
					i++;
					if (i < len && (line[i] == '+' || line[i] == '-')) i++;
					while (i < len && isdigit((unsigned char)line[i])) i++;
				}
			}
			result.push_back({start, i - start, COL_NUMBER});
			continue;
		}

		// ---- Word (identifier, keyword, type, function) ----
		if (isWordChar(line[i])) {
			int start = i;
			while (i < len && isWordChar(line[i])) i++;
			string token = line.substr(start, i - start);
			ofColor col = getTokenColor(token, isPreprocessorLine, false, false);
			result.push_back({start, i - start, col});
			continue;
		}

		// ---- Single character (operators, punctuation) ----
		result.push_back({i, 1, COL_DEFAULT});
		i++;
	}

	return result;
}

// ============================================================
// Drawing
// ============================================================

void JPShaderEditor::draw()
{
	if (!visible) return;

	// Save GL state to prevent rectMode leaks
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofFill();

	float sw = ofGetWidth();
	float sh = ofGetHeight();

	// Full screen overlay — 50% transparent so shader preview shows through
	ofSetColor(0, 0, 0, 128);
	ofDrawRectangle(0, 0, sw, sh);

	// Editor panel
	float ex = MARGIN;
	float ey = MARGIN;
	float ew = sw - MARGIN * 2;
	float eh = sh - MARGIN * 2;

	ofSetColor(COL_BG);
	ofDrawRectRounded(ex, ey, ew, eh, 6);

	// Layout zones
	float topBarY = ey;
	float tabBarY = topBarY + TOP_BAR_H;
	float codeY = tabBarY + TAB_BAR_H;
	float codeH = eh - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H;
	float statusY = codeY + codeH;

	drawTopBar();
	drawTabBar();
	drawCodeArea();
	drawStatusBar();

	// Blink cursor timer
	float now = ofGetElapsedTimef();
	if (now - lastBlinkTime > 0.53f) {
		cursorVisible = !cursorVisible;
		lastBlinkTime = now;
	}

	ofPopStyle();
}

void JPShaderEditor::drawTopBar()
{
	float sw = ofGetWidth();
	float ex = MARGIN;
	float ey = MARGIN;
	float ew = sw - MARGIN * 2;

	ofSetColor(COL_TOP_BAR);
	ofDrawRectRounded(ex, ey, ew, TOP_BAR_H, 6);

	// Title
	if (activeTab >= 0 && activeTab < (int)tabs.size()) {
		ofSetColor(240);
		string title = "EDITOR: " + tabs[activeTab].shaderName;
		if (tabs[activeTab].modified) title += " *";
		jp_constants::p_font.drawString(title, ex + 12, ey + TOP_BAR_H - 8);
	}

	// Close button (X)
	float btnW = 60;
	float btnH = 24;
	float closeX = ex + ew - btnW - 12;
	float closeY = ey + (TOP_BAR_H - btnH) / 2;

	ofSetColor(COL_BTN_CLOSE);
	ofDrawRectRounded(closeX, closeY, btnW, btnH, 3);
	ofSetColor(COL_BTN_TEXT);
	string closeLabel = "CLOSE";
	float clw = jp_constants::p_font.stringWidth(closeLabel);
	jp_constants::p_font.drawString(closeLabel,
		closeX + btnW / 2 - clw / 2,
		closeY + btnH / 2 + jp_constants::p_font.stringHeight(closeLabel) / 2 - 2);

	// Save button
	float saveX = closeX - btnW - 8;
	ofSetColor(COL_BTN_SAVE);
	ofDrawRectRounded(saveX, closeY, btnW, btnH, 3);
	ofSetColor(COL_BTN_TEXT);
	string saveLabel = "SAVE";
	float swl = jp_constants::p_font.stringWidth(saveLabel);
	jp_constants::p_font.drawString(saveLabel,
		saveX + btnW / 2 - swl / 2,
		closeY + btnH / 2 + jp_constants::p_font.stringHeight(saveLabel) / 2 - 2);
}

void JPShaderEditor::drawTabBar()
{
	float sw = ofGetWidth();
	float ex = MARGIN;
	float ey = MARGIN + TOP_BAR_H;
	float ew = sw - MARGIN * 2;

	ofSetColor(COL_TAB_BAR_BG);
	ofDrawRectangle(ex, ey, ew, TAB_BAR_H);

	// Bottom border
	ofSetColor(COL_TAB_BORDER);
	ofDrawLine(ex, ey + TAB_BAR_H, ex + ew, ey + TAB_BAR_H);

	float tabX = ex + 4;
	float tabH = TAB_BAR_H - 4;
	float tabY = ey + 2;
	float closeSize = 12;

	for (int i = 0; i < (int)tabs.size(); i++) {
		EditorTab& tab = tabs[i];

		// Tab label width
		string label = tab.shaderName;
		if (tab.modified) label = "● " + label;
		float labelW = jp_constants::p_font.stringWidth(label);
		float tabW = max(80.0f, labelW + 28); // minimum width + padding + close button space

		// Tab background
		if (i == activeTab) {
			ofSetColor(COL_TAB_ACTIVE);
		} else {
			ofSetColor(COL_TAB_INACTIVE);
		}
		ofDrawRectRounded(tabX, tabY, tabW, tabH, 3);

		// Tab label
		ofSetColor(i == activeTab ? ofColor(240) : ofColor(170));
		jp_constants::p_font.drawString(label, tabX + 6, tabY + tabH - 7);

		// Close button (×) — tiny area on the right
		float closeX = tabX + tabW - closeSize - 6;
		float closeY = tabY + (tabH - closeSize) / 2;

		// Check if mouse is over this close button
		bool hoverClose = false;
		if (ofGetMouseX() >= closeX - 2 && ofGetMouseX() <= closeX + closeSize + 2 &&
			ofGetMouseY() >= closeY - 2 && ofGetMouseY() <= closeY + closeSize + 2) {
			hoverClose = true;
		}

		ofSetColor(hoverClose ? COL_TAB_CLOSE_HOVER : ofColor(130));
		jp_constants::p_font.drawString("x", closeX, closeY + closeSize - 2);

		tabX += tabW + 2;

		// Stop if we ran out of space
		if (tabX + 80 > ex + ew) break;
	}
}

void JPShaderEditor::drawCodeArea()
{
	if (activeTab < 0 || activeTab >= (int)tabs.size()) return;

	float sw = ofGetWidth();
	float ex = MARGIN;
	float ey = MARGIN + TOP_BAR_H + TAB_BAR_H;
	float ew = sw - MARGIN * 2;
	float eh = sw - MARGIN * 2; // full height, will be computed below

	float sh = ofGetHeight();
	float codeH = sh - MARGIN * 2 - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H;
	float codeY = ey;
	float codeX = ex;
	float codeW = ew;

	// Draw code background
	ofSetColor(COL_BG);
	ofDrawRectangle(codeX, codeY, codeW, codeH);

	drawLineNumbers(activeTab, codeX, codeY, codeW, codeH);
	drawCodeLines(activeTab, codeX, codeY, codeW, codeH);
	drawScrollbar(activeTab, codeX + codeW - SCROLLBAR_W, codeY, codeH);
	drawCursor(activeTab, codeX, codeY);
}

void JPShaderEditor::drawLineNumbers(int tabIndex, float codeX, float codeY, float codeW, float codeH)
{
	EditorTab& tab = tabs[tabIndex];
	float lineH = tab.fontSize + 2.0f; // line height: font + 2px spacing

	// Gutter background
	ofSetColor(COL_GUTTER_BG);
	ofDrawRectangle(codeX, codeY, LINE_NUMBER_W, codeH);

	// Separator line
	ofSetColor(60);
	ofDrawLine(codeX + LINE_NUMBER_W, codeY, codeX + LINE_NUMBER_W, codeY + codeH);

	int totalLines = (int)tab.lines.size();
	float textY = codeY + lineH;

	for (int i = tab.scrollLine; i < totalLines; i++) {
		if (textY > codeY + codeH + lineH) break;

		bool isActiveLine = (i == tab.cursorLine);

		// Line number color
		ofSetColor(isActiveLine ? COL_LINE_NUM_ACTIVE : COL_LINE_NUM);

		string numStr = ofToString(i + 1);
		float nw = editorFont.stringWidth(numStr);
		editorFont.drawString(numStr,
			codeX + LINE_NUMBER_W - nw - 8,
			textY);

		textY += lineH;
	}
}

void JPShaderEditor::drawCodeLines(int tabIndex, float codeX, float codeY, float codeW, float codeH)
{
	EditorTab& tab = tabs[tabIndex];
	float lineH = tab.fontSize + 2.0f;

	// Clamp scroll to valid range
	int totalLines = (int)tab.lines.size();
	int visibleLines = (int)(codeH / lineH);
	int maxScroll = totalLines - visibleLines;
	if (maxScroll < 0) maxScroll = 0;
	if (tab.scrollLine > maxScroll) tab.scrollLine = maxScroll;
	if (tab.scrollLine < 0) tab.scrollLine = 0;

	float textX = codeX + LINE_NUMBER_W + 8;
	float textY = codeY + lineH;

	// Visible text area width
	float textW = codeW - LINE_NUMBER_W - SCROLLBAR_W - 8;

	// Current line highlight
	if (tab.cursorLine >= tab.scrollLine && tab.cursorLine < tab.scrollLine + visibleLines) {
		float hlY = codeY + (tab.cursorLine - tab.scrollLine) * lineH;
		ofSetColor(COL_CURRENT_LINE);
		ofDrawRectangle(textX, hlY, textW, lineH);
	}

	// Compute normalized selection range for drawing
	int selL1 = tab.selStartLine, selC1 = tab.selStartCol;
	int selL2 = tab.selEndLine, selC2 = tab.selEndCol;
	if (tab.hasSelection()) {
		if (selL1 > selL2 || (selL1 == selL2 && selC1 > selC2)) {
			swap(selL1, selL2); swap(selC1, selC2);
		}
	}

	for (int i = tab.scrollLine; i < totalLines; i++) {
		if (textY > codeY + codeH + lineH) break;

		const string& line = tab.lines[i];
		vector<ColorSegment> segments = tokenizeLine(line);

		// Horizontal scroll offset
		float baseX = textX - tab.scrollCol * getCharWidth();
		float drawX = baseX;

		// Draw selection highlight for this line
		if (tab.hasSelection() && i >= selL1 && i <= selL2) {
			int selStart = (i == selL1) ? selC1 : 0;
			int selEnd = (i == selL2) ? selC2 : (int)line.size();
			if (selStart < selEnd) {
				float selX = baseX + selStart * getCharWidth();
				float selW = (selEnd - selStart) * getCharWidth();
				ofSetColor(COL_SELECTION);
				ofDrawRectangle(selX, textY + 2, selW, lineH - 4);
			}
		}

		if (segments.empty() && !line.empty()) {
			ofSetColor(COL_DEFAULT);
			editorFont.drawString(line, drawX, textY);
		} else {
			float charW = getCharWidth();
			for (const auto& seg : segments) {
				if (seg.start + seg.len > (int)line.size()) break;
				string part = line.substr(seg.start, seg.len);
				float segX = baseX + seg.start * charW;
				ofSetColor(seg.color);
				editorFont.drawString(part, segX, textY);
			}
		}

		textY += lineH;
	}
}

void JPShaderEditor::drawCursor(int tabIndex, float codeX, float codeY)
{
	if (!cursorVisible) return;

	EditorTab& tab = tabs[tabIndex];
	float lineH = tab.fontSize + 2.0f;
	float charW = getCharWidth();

	// Only draw if cursor line is visible
	int visibleLines = getVisibleLines(ofGetHeight() - MARGIN * 2 - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H);
	if (tab.cursorLine < tab.scrollLine || tab.cursorLine >= tab.scrollLine + visibleLines) return;

	float cursorX = codeX + LINE_NUMBER_W + 8 + (tab.cursorCol - tab.scrollCol) * charW;
	float cursorY = codeY + (tab.cursorLine - tab.scrollLine) * lineH;

	// Clamp to visible area
	float textW = (ofGetWidth() - MARGIN * 2) - LINE_NUMBER_W - SCROLLBAR_W - 8;
	if (cursorX < codeX + LINE_NUMBER_W + 8) cursorX = codeX + LINE_NUMBER_W + 8;
	if (cursorX > codeX + LINE_NUMBER_W + 8 + textW) cursorX = codeX + LINE_NUMBER_W + 8 + textW;

	ofSetColor(COL_CURSOR);
	ofSetLineWidth(1.5f);
	ofDrawLine(cursorX, cursorY + 2, cursorX, cursorY + lineH - 2);
	ofSetLineWidth(1.0f);
}

void JPShaderEditor::drawScrollbar(int tabIndex, float scrollX, float codeY, float codeH)
{
	EditorTab& tab = tabs[tabIndex];
	int totalLines = (int)tab.lines.size();
	float lineH = tab.fontSize + 2.0f;
	int visibleLines = (int)(codeH / lineH);

	if (totalLines <= visibleLines) return;

	ofSetColor(COL_SCROLLBAR_BG);
	ofDrawRectRounded(scrollX, codeY, SCROLLBAR_W, codeH, 3);

	float thumbH = (float)visibleLines / totalLines * codeH;
	if (thumbH < 20) thumbH = 20;
	float thumbY = codeY + (float)tab.scrollLine / totalLines * codeH;

	ofSetColor(COL_SCROLLBAR_THUMB);
	ofDrawRectRounded(scrollX + 1, thumbY, SCROLLBAR_W - 2, thumbH, 2);
}

void JPShaderEditor::drawStatusBar()
{
	float sw = ofGetWidth();
	float sh = ofGetHeight();
	float ex = MARGIN;
	float ey = sh - MARGIN - STATUS_BAR_H;
	float ew = sw - MARGIN * 2;

	ofSetColor(COL_STATUS_BAR);
	ofDrawRectRounded(ex, ey, ew, STATUS_BAR_H, 6);

	if (activeTab >= 0 && activeTab < (int)tabs.size()) {
		EditorTab& tab = tabs[activeTab];

		ofSetColor(COL_STATUS_TEXT);

		// Left: line:col
		string pos = "Ln " + ofToString(tab.cursorLine + 1) +
			", Col " + ofToString(tab.cursorCol + 1);
		jp_constants::p_font.drawString(pos, ex + 12, ey + STATUS_BAR_H - 6);

		// Center: modified indicator
		if (tab.modified) {
			string mod = "MODIFIED";
			float mw = jp_constants::p_font.stringWidth(mod);
			ofSetColor(255, 200, 50);
			jp_constants::p_font.drawString(mod, ex + ew / 2 - mw / 2, ey + STATUS_BAR_H - 6);
		}

		// Right: zoom level + total lines + file name
		string info = "Zoom: " + ofToString((int)tab.fontSize) + "px  |  " +
			ofToString((int)tab.lines.size()) + " lines  |  " + tab.shaderName;
		float iw = jp_constants::p_font.stringWidth(info);
		ofSetColor(COL_STATUS_TEXT);
		jp_constants::p_font.drawString(info, ex + ew - iw - 12, ey + STATUS_BAR_H - 6);
	}
}

// ============================================================
// Layout helpers
// ============================================================

float JPShaderEditor::getCharWidth() const
{
	// Approximate char width for monospace font
	return editorFont.stringWidth("X");
}

float JPShaderEditor::getLineHeight() const
{
	return currentFontSize + 2.0f;
}

int JPShaderEditor::getVisibleLines(float codeH) const
{
	float lh = getLineHeight();
	if (lh <= 0) return 1;
	return (int)(codeH / lh);
}

int JPShaderEditor::getVisibleCols(float codeW) const
{
	float cw = getCharWidth();
	if (cw <= 0) return 1;
	return (int)(codeW / cw);
}

// ============================================================
// Cursor / Scroll
// ============================================================

void JPShaderEditor::clampCursor(int tabIndex)
{
	EditorTab& tab = tabs[tabIndex];
	if (tab.cursorLine < 0) tab.cursorLine = 0;
	if (tab.cursorLine >= (int)tab.lines.size())
		tab.cursorLine = max(0, (int)tab.lines.size() - 1);
	if (tab.cursorCol < 0) tab.cursorCol = 0;
	int lineLen = (int)tab.lines[tab.cursorLine].size();
	if (tab.cursorCol > lineLen) tab.cursorCol = lineLen;
}

void JPShaderEditor::ensureCursorVisible(int tabIndex, float codeW, float codeH)
{
	EditorTab& tab = tabs[tabIndex];
	int visLines = getVisibleLines(codeH);
	int visCols = getVisibleCols(codeW - LINE_NUMBER_W - SCROLLBAR_W - 8);

	// Vertical
	if (tab.cursorLine < tab.scrollLine) {
		tab.scrollLine = tab.cursorLine;
	} else if (tab.cursorLine >= tab.scrollLine + visLines) {
		tab.scrollLine = tab.cursorLine - visLines + 1;
	}
	if (tab.scrollLine < 0) tab.scrollLine = 0;

	// Horizontal
	if (tab.cursorCol < tab.scrollCol) {
		tab.scrollCol = tab.cursorCol;
	} else if (tab.cursorCol >= tab.scrollCol + visCols) {
		tab.scrollCol = tab.cursorCol - visCols + 4;
	}
	if (tab.scrollCol < 0) tab.scrollCol = 0;
}

// ============================================================
// Mouse input
// ============================================================

void JPShaderEditor::mousePressed(int x, int y, int button)
{
	if (!visible) return;

	float sw = ofGetWidth();
	float sh = ofGetHeight();
	float ex = MARGIN;
	float ey = MARGIN;
	float ew = sw - MARGIN * 2;
	float eh = sh - MARGIN * 2;

	// Outside editor panel?
	if (x < ex || x > ex + ew || y < ey || y > ey + eh) {
		// Click outside closes editor
		hide();
		return;
	}

	// ---- Top bar: Close / Save buttons ----
	float topBarY = ey;
	float btnW = 60;
	float btnH = 24;
	float closeX = ex + ew - btnW - 12;
	float closeY = ey + (TOP_BAR_H - btnH) / 2;
	float saveX = closeX - btnW - 8;

	if (x >= closeX && x <= closeX + btnW && y >= closeY && y <= closeY + btnH) {
		hide();
		return;
	}
	if (x >= saveX && x <= saveX + btnW && y >= closeY && y <= closeY + btnH) {
		saveCurrentTab();
		return;
	}

	// ---- Tab bar: tab clicks ----
	float tabBarY = ey + TOP_BAR_H;
	if (y >= tabBarY && y <= tabBarY + TAB_BAR_H) {
		// Tab bar clicks: switch tab or close tab
		float tabX = ex + 4;
		float tabH = TAB_BAR_H - 4;
		float tabY2 = tabBarY + 2;
		float closeSize = 12;

		for (int i = 0; i < (int)tabs.size(); i++) {
			string label = tabs[i].shaderName;
			if (tabs[i].modified) label = "● " + label;
			float labelW = jp_constants::p_font.stringWidth(label);
			float tabW = labelW + 28;
			float closeX2 = tabX + tabW - closeSize - 6;
			float closeY2 = tabY2 + (tabH - closeSize) / 2;

			// Check close button click first
			if (x >= closeX2 - 2 && x <= closeX2 + closeSize + 2 &&
				y >= closeY2 - 2 && y <= closeY2 + closeSize + 2) {
				closeTab(i);
				return;
			}

			// Check tab click
			if (x >= tabX && x <= tabX + tabW && y >= tabY2 && y <= tabY2 + tabH) {
				activeTab = i;
				reloadFont(tabs[i].fontSize);
				return;
			}

			tabX += tabW + 2;
			if (tabX + 80 > ex + ew) break;
		}
		return;
	}

	// ---- Code area: cursor positioning ----
	float codeY = ey + TOP_BAR_H + TAB_BAR_H;
	float codeH = eh - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H;

	if (y < codeY || y > codeY + codeH) return;
	if (activeTab < 0 || activeTab >= (int)tabs.size()) return;
	// Click on scrollbar
	if (x >= ex + ew - SCROLLBAR_W) return;
	// Click on gutter — no-op
	if (x < ex + LINE_NUMBER_W) return;

	EditorTab& tab = tabs[activeTab];
	float lineH = tab.fontSize + 2.0f;
	float charW = getCharWidth();

	// Compute line from click Y
	int clickedLine = tab.scrollLine + (int)((y - codeY) / lineH);
	if (clickedLine < 0) clickedLine = 0;
	if (clickedLine >= (int)tab.lines.size()) clickedLine = (int)tab.lines.size() - 1;

	// Compute column from click X (relative to code start)
	float codeStartX = ex + LINE_NUMBER_W + 8;
	int clickedCol = tab.scrollCol + (int)((x - codeStartX) / charW + 0.5f);
	if (clickedCol < 0) clickedCol = 0;
	if (clickedCol > (int)tab.lines[clickedLine].size())
		clickedCol = (int)tab.lines[clickedLine].size();

	tab.cursorLine = clickedLine;
	tab.cursorCol = clickedCol;
	tab.clearSelection();  // Clear selection on simple click (drag restarts it)
	cursorVisible = true;
	lastBlinkTime = ofGetElapsedTimef();
}

void JPShaderEditor::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
	if (!visible || activeTab < 0 || activeTab >= (int)tabs.size()) return;

	EditorTab& tab = tabs[activeTab];

	// Ctrl + scroll = zoom
	if (ofGetKeyPressed(OF_KEY_CONTROL)) {
		float newSize = tab.fontSize - scrollY * 2.0f;
		if (newSize < MIN_FONT_SIZE) newSize = MIN_FONT_SIZE;
		if (newSize > MAX_FONT_SIZE) newSize = MAX_FONT_SIZE;

		tab.fontSize = newSize;
		reloadFont(newSize);
		return;
	}

	// Shift + scroll = horizontal scroll
	if (ofGetKeyPressed(OF_KEY_SHIFT)) {
		tab.scrollCol -= (int)scrollY * 3;
		if (tab.scrollCol < 0) tab.scrollCol = 0;
		return;
	}

	// Normal scroll = vertical
	int oldScroll = tab.scrollLine;
	tab.scrollLine -= (int)scrollY * 3;

	// Clamp
	int maxScroll = (int)tab.lines.size() - getVisibleLines(ofGetHeight() - MARGIN * 2 - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H);
	if (maxScroll < 0) maxScroll = 0;
	if (tab.scrollLine < 0) tab.scrollLine = 0;
	if (tab.scrollLine > maxScroll) tab.scrollLine = maxScroll;
}

// ============================================================
// Keyboard input
// ============================================================

void JPShaderEditor::keyPressed(int key)
{
	if (!visible || activeTab < 0 || activeTab >= (int)tabs.size()) return;

	EditorTab& tab = tabs[activeTab];

	// Ensure lines array has at least one line
	if (tab.lines.empty()) {
		tab.lines.push_back("");
	}

	int lineLen = (int)tab.lines[tab.cursorLine].size();
	string& currentLine = tab.lines[tab.cursorLine];

	switch (key) {
	// ---- Navigation ----
	case OF_KEY_UP:
	{
		bool shift = ofGetKeyPressed(OF_KEY_SHIFT);
		if (shift && !tab.hasSelection()) {
			tab.selStartLine = tab.cursorLine;
			tab.selStartCol = tab.cursorCol;
		}
		if (tab.cursorLine > 0) {
			tab.cursorLine--;
			int newLen = (int)tab.lines[tab.cursorLine].size();
			if (tab.cursorCol > newLen) tab.cursorCol = newLen;
		}
		if (shift) { tab.selEndLine = tab.cursorLine; tab.selEndCol = tab.cursorCol; }
		else { tab.clearSelection(); }
		break;
	}

	case OF_KEY_DOWN:
	{
		bool shift = ofGetKeyPressed(OF_KEY_SHIFT);
		if (shift && !tab.hasSelection()) {
			tab.selStartLine = tab.cursorLine;
			tab.selStartCol = tab.cursorCol;
		}
		if (tab.cursorLine < (int)tab.lines.size() - 1) {
			tab.cursorLine++;
			int newLen = (int)tab.lines[tab.cursorLine].size();
			if (tab.cursorCol > newLen) tab.cursorCol = newLen;
		}
		if (shift) { tab.selEndLine = tab.cursorLine; tab.selEndCol = tab.cursorCol; }
		else { tab.clearSelection(); }
		break;
	}

	case OF_KEY_LEFT:
	{
		bool shift = ofGetKeyPressed(OF_KEY_SHIFT);
		if (shift && !tab.hasSelection()) {
			tab.selStartLine = tab.cursorLine;
			tab.selStartCol = tab.cursorCol;
		}
		if (tab.cursorCol > 0) {
			tab.cursorCol--;
		} else if (tab.cursorLine > 0) {
			// Move to end of previous line
			tab.cursorLine--;
			tab.cursorCol = (int)tab.lines[tab.cursorLine].size();
		}
		if (shift) { tab.selEndLine = tab.cursorLine; tab.selEndCol = tab.cursorCol; }
		else { tab.clearSelection(); }
		break;
	}

	case OF_KEY_RIGHT:
	{
		bool shift = ofGetKeyPressed(OF_KEY_SHIFT);
		if (shift && !tab.hasSelection()) {
			tab.selStartLine = tab.cursorLine;
			tab.selStartCol = tab.cursorCol;
		}
		int lineLen = (int)tab.lines[tab.cursorLine].size();
		if (tab.cursorCol < lineLen) {
			tab.cursorCol++;
		} else if (tab.cursorLine < (int)tab.lines.size() - 1) {
			// Move to start of next line
			tab.cursorLine++;
			tab.cursorCol = 0;
		}
		if (shift) { tab.selEndLine = tab.cursorLine; tab.selEndCol = tab.cursorCol; }
		else { tab.clearSelection(); }
		break;
	}

	case OF_KEY_HOME:
		tab.cursorCol = 0;
		break;

	case OF_KEY_END:
		tab.cursorCol = lineLen;
		break;

	case OF_KEY_PAGE_UP:
	{
		int visible = getVisibleLines(ofGetHeight() - MARGIN * 2 - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H);
		tab.cursorLine -= visible;
		if (tab.cursorLine < 0) tab.cursorLine = 0;
		tab.scrollLine -= visible;
		if (tab.scrollLine < 0) tab.scrollLine = 0;
		clampCursor(activeTab);
		break;
	}

	case OF_KEY_PAGE_DOWN:
	{
		int visible = getVisibleLines(ofGetHeight() - MARGIN * 2 - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H);
		tab.cursorLine += visible;
		tab.scrollLine += visible;
		clampCursor(activeTab);
		break;
	}

	// ---- Editing ----
	case OF_KEY_BACKSPACE:
		if (tab.hasSelection()) { deleteSelection(activeTab); break; }
		if (tab.cursorCol > 0) {
			currentLine.erase(currentLine.begin() + tab.cursorCol - 1);
			tab.cursorCol--;
			tab.modified = true;
		} else if (tab.cursorLine > 0) {
			// Merge with previous line
			tab.cursorCol = (int)tab.lines[tab.cursorLine - 1].size();
			tab.lines[tab.cursorLine - 1] += currentLine;
			tab.lines.erase(tab.lines.begin() + tab.cursorLine);
			tab.cursorLine--;
			tab.modified = true;
		}
		break;

	case OF_KEY_DEL:
		if (tab.hasSelection()) { deleteSelection(activeTab); break; }
		if (tab.cursorCol < lineLen) {
			currentLine.erase(currentLine.begin() + tab.cursorCol);
			tab.modified = true;
		} else if (tab.cursorLine < (int)tab.lines.size() - 1) {
			// Merge with next line
			currentLine += tab.lines[tab.cursorLine + 1];
			tab.lines.erase(tab.lines.begin() + tab.cursorLine + 1);
			tab.modified = true;
		}
		break;

	case OF_KEY_RETURN:
	{
		if (tab.hasSelection()) { deleteSelection(activeTab); }
		// Split line at cursor
		string rest = currentLine.substr(tab.cursorCol);
		currentLine = currentLine.substr(0, tab.cursorCol);
		tab.lines.insert(tab.lines.begin() + tab.cursorLine + 1, rest);
		tab.cursorLine++;
		tab.cursorCol = 0;
		tab.modified = true;
		break;
	}

	case OF_KEY_TAB:
	{
		if (tab.hasSelection()) { deleteSelection(activeTab); }
		// Insert 4 spaces
		currentLine.insert(tab.cursorCol, "    ");
		tab.cursorCol += 4;
		tab.modified = true;
		break;
	}

	case OF_KEY_ESC:
		hide();
		return;

	// ---- Printable characters ----
	default:
		if (key >= 32 && key <= 126) {
			if (tab.hasSelection()) { deleteSelection(activeTab); }
			currentLine.insert(currentLine.begin() + tab.cursorCol, (char)key);
			tab.cursorCol++;
			tab.modified = true;
		}
		break;
	}

	// After any edit, clamp cursor and ensure visible
	clampCursor(activeTab);
	float eh = ofGetHeight() - MARGIN * 2;
	float codeH = eh - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H;
	float ew = ofGetWidth() - MARGIN * 2;
	ensureCursorVisible(activeTab, ew, codeH);

	// Reset blink on keypress
	cursorVisible = true;
	lastBlinkTime = ofGetElapsedTimef();
}

// ============================================================
// Keyboard: Ctrl+key combinations (copy, paste, cut, select all)
// ============================================================

void JPShaderEditor::keycodePressed(int keycode)
{
	if (!visible || activeTab < 0 || activeTab >= (int)tabs.size()) return;
	EditorTab& tab = tabs[activeTab];

	// Ctrl+A (key=1): Select all
	if (keycode == 1) {
		tab.selStartLine = 0;
		tab.selStartCol = 0;
		tab.selEndLine = (int)tab.lines.size() - 1;
		tab.selEndCol = (int)tab.lines.back().size();
		return;
	}

	// Ctrl+C (key=3): Copy
	if (keycode == 3) {
		if (!tab.hasSelection()) {
			// Select current line if no selection
			tab.selStartLine = tab.cursorLine;
			tab.selStartCol = 0;
			tab.selEndLine = tab.cursorLine;
			tab.selEndCol = (int)tab.lines[tab.cursorLine].size();
		}
		string sel;
		getSelectedText(sel, activeTab);
		if (!sel.empty()) {
			_clipboard = sel;
		}
		return;
	}

	// Ctrl+V (key=22): Paste
	if (keycode == 22) {
		string clip = _clipboard;
		if (!clip.empty()) {
			// Delete selection if any
			if (tab.hasSelection()) {
				deleteSelection(activeTab);
			}
			// Split clipboard by newlines and insert
			vector<string> pasteLines;
			string cur;
			for (char c : clip) {
				if (c == '\n') { pasteLines.push_back(cur); cur.clear(); }
				else if (c != '\r') { cur += c; }
			}
			pasteLines.push_back(cur);

			if (pasteLines.size() == 1) {
				// Single line paste
				tab.lines[tab.cursorLine].insert(tab.cursorCol, pasteLines[0]);
				tab.cursorCol += (int)pasteLines[0].size();
			} else {
				// Multi-line paste
				string afterCursor = tab.lines[tab.cursorLine].substr(tab.cursorCol);
				tab.lines[tab.cursorLine] = tab.lines[tab.cursorLine].substr(0, tab.cursorCol) + pasteLines[0];
				for (int i = 1; i < (int)pasteLines.size(); i++) {
					tab.lines.insert(tab.lines.begin() + tab.cursorLine + i, pasteLines[i]);
				}
				tab.lines[tab.cursorLine + (int)pasteLines.size() - 1] += afterCursor;
				tab.cursorLine += (int)pasteLines.size() - 1;
				tab.cursorCol = (int)pasteLines.back().size();
			}
			tab.clearSelection();
			tab.modified = true;
			clampCursor(activeTab);
		}
		return;
	}

	// Ctrl+X (key=24): Cut
	if (keycode == 24) {
		if (tab.hasSelection()) {
			string sel;
			getSelectedText(sel, activeTab);
			if (!sel.empty()) {
				_clipboard = sel;
			}
			deleteSelection(activeTab);
		}
		return;
	}
}

// ============================================================
// Mouse drag — extend selection
// ============================================================

void JPShaderEditor::mouseDragged(int x, int y, int button)
{
	if (!visible || activeTab < 0 || activeTab >= (int)tabs.size()) return;

	EditorTab& tab = tabs[activeTab];
	float sw = ofGetWidth();
	float sh = ofGetHeight();
	float ex = MARGIN;
	float ey = MARGIN + TOP_BAR_H + TAB_BAR_H;
	float eh = sh - MARGIN * 2;
	float codeY = ey;
	float codeH = eh - TOP_BAR_H - TAB_BAR_H - STATUS_BAR_H;
	float codeStartX = ex + LINE_NUMBER_W + 8;

	// Check if within code area
	if (y < codeY || y > codeY + codeH || x < codeStartX) return;
	if (x >= ex + (sw - MARGIN * 2) - SCROLLBAR_W) return;

	float lineH = tab.fontSize + 2.0f;
	float charW = getCharWidth();

	int clickedLine = tab.scrollLine + (int)((y - codeY) / lineH);
	if (clickedLine < 0) clickedLine = 0;
	if (clickedLine >= (int)tab.lines.size()) clickedLine = (int)tab.lines.size() - 1;

	int clickedCol = tab.scrollCol + (int)((x - codeStartX) / charW + 0.5f);
	if (clickedCol < 0) clickedCol = 0;
	if (clickedCol > (int)tab.lines[clickedLine].size())
		clickedCol = (int)tab.lines[clickedLine].size();

	// Start or extend selection
	if (!tab.hasSelection()) {
		tab.selStartLine = tab.cursorLine;
		tab.selStartCol = tab.cursorCol;
	}
	tab.selEndLine = clickedLine;
	tab.selEndCol = clickedCol;

	tab.cursorLine = clickedLine;
	tab.cursorCol = clickedCol;
	cursorVisible = true;
	lastBlinkTime = ofGetElapsedTimef();
}

// ============================================================
// Selection helpers
// ============================================================

void JPShaderEditor::getSelectedText(string& out, int tabIndex) const
{
	if (tabIndex < 0 || tabIndex >= (int)tabs.size()) return;
	const EditorTab& tab = tabs[tabIndex];
	if (!tab.hasSelection()) return;

	int sl1 = tab.selStartLine, sc1 = tab.selStartCol;
	int sl2 = tab.selEndLine, sc2 = tab.selEndCol;
	if (sl1 > sl2 || (sl1 == sl2 && sc1 > sc2)) {
		swap(sl1, sl2); swap(sc1, sc2);
	}

	out.clear();
	for (int l = sl1; l <= sl2 && l < (int)tab.lines.size(); l++) {
		const string& line = tab.lines[l];
		int start = (l == sl1) ? sc1 : 0;
		int end = (l == sl2) ? sc2 : (int)line.size();
		if (start < 0) start = 0;
		if (end > (int)line.size()) end = (int)line.size();
		if (end > start) {
			out += line.substr(start, end - start);
		}
		if (l < sl2) out += "\n";
	}
}

void JPShaderEditor::deleteSelection(int tabIndex)
{
	if (tabIndex < 0 || tabIndex >= (int)tabs.size()) return;
	EditorTab& tab = tabs[tabIndex];
	if (!tab.hasSelection()) return;

	int sl1 = tab.selStartLine, sc1 = tab.selStartCol;
	int sl2 = tab.selEndLine, sc2 = tab.selEndCol;
	if (sl1 > sl2 || (sl1 == sl2 && sc1 > sc2)) {
		swap(sl1, sl2); swap(sc1, sc2);
	}

	// If single line
	if (sl1 == sl2) {
		string& line = tab.lines[sl1];
		if (sc1 >= 0 && sc2 <= (int)line.size() && sc2 > sc1) {
			line.erase(sc1, sc2 - sc1);
		}
		tab.cursorLine = sl1;
		tab.cursorCol = sc1;
	} else {
		// Multi-line: keep start of first line + end of last line
		string& firstLine = tab.lines[sl1];
		string& lastLine = tab.lines[sl2];
		firstLine = firstLine.substr(0, sc1) + lastLine.substr(sc2);
		// Remove lines between (in reverse)
		for (int l = sl2; l > sl1; l--) {
			tab.lines.erase(tab.lines.begin() + l);
		}
		tab.cursorLine = sl1;
		tab.cursorCol = sc1;
	}
	tab.clearSelection();
	tab.modified = true;
}
