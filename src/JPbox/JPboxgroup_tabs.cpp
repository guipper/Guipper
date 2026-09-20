#include "JPboxgroup.h"
#include "../JPutils/jp_text_input.h"
#include "../JPutils/jp_tooltip.h"
#include <algorithm>
#include <cmath>

vector<int> JPboxgroup::getDirectChildPresetIndices() const
{
	vector<int> indices;
	const vector<JPbox *> *boxList = &boxes;

	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr)
		{
			boxList = &preset->boxes;
		}
	}

	for (int i = 0; i < (int)boxList->size(); i++)
	{
		if ((*boxList)[i] != nullptr && (*boxList)[i]->getTipo() == JPbox::PRESETBOX)
		{
			indices.push_back(i);
		}
	}
	return indices;
}

// Breadcrumb helpers
static string getBreadcrumbNameAtLevel(const vector<int> &activeGroupPath, const vector<JPbox *> &boxes, int level)
{
	// level 0 = root (MAIN)
	if (level == 0) return "MAIN";
	if (level < 0 || level > (int)activeGroupPath.size()) return "?";
	if (activeGroupPath[0] < 0 || activeGroupPath[0] >= (int)boxes.size()) return "?";

	// Navigate path to find the box at this level
	JPbox *box = boxes[activeGroupPath[0]];
	if (box == nullptr) return "?";
	if (level == 1) return box->name;

	JPbox_preset *preset = (box->getTipo() == JPbox::PRESETBOX) ? static_cast<JPbox_preset *>(box) : nullptr;
	for (int d = 1; d < level && preset != nullptr; d++)
	{
		int idx = activeGroupPath[d];
		if (idx >= 0 && idx < (int)preset->boxes.size())
			box = preset->boxes[idx];
		else
			box = nullptr;
		preset = (box && box->getTipo() == JPbox::PRESETBOX) ? static_cast<JPbox_preset *>(box) : nullptr;
	}
	return (box != nullptr) ? box->name : "?";
}

JPboxgroup::GroupTabLayout JPboxgroup::groupTabLayout() const
{
	GroupTabLayout layout;
	const auto children = getDirectChildPresetIndices();
	const int depth = (int)activeGroupPath.size();
	vector<string> names = {"MAIN"};
	for (int level = 1; level <= depth; ++level)
		names.push_back(getBreadcrumbNameAtLevel(activeGroupPath, boxes, level));
	const auto *preset = getActivePreset();
	const auto &nodes = preset ? preset->boxes : boxes;
	for (int index : children)
		names.push_back(nodes[index]->name.empty() ? "Group" : nodes[index]->name);
	if (names.size() == 1) return layout;
	const int inspectorIndex = preset ? groupInspectorIndex : openguinumber;
	const bool inspectorVisible = inspectorIndex >= 0 && inspectorIndex < (int)nodes.size() && nodes[inspectorIndex];
	float rightEdge = ofGetWidth() - 8.0f;
	const auto inspector = getInspectorBounds();
	const float y = tabBarOffsetY + 8;
	if (inspectorVisible && inspector.width > 0 && inspector.y < y + 62 && inspector.getBottom() > y)
		rightEdge = std::min(rightEdge, inspector.x - 8);
	layout.bar = ofRectangle(8, y, std::max(0.0f, rightEdge - 8), children.empty() ? 28 : 62);
	if (layout.bar.width < 100) return layout;

	const string contextUid = preset ? preset->uid : "MAIN";
	const bool contextChanged = tabContextUid != contextUid;
	if (contextChanged) {
		if (!tabContextUid.empty()) groupTabPages[tabContextUid] = std::max(0, firstVisibleGroupTab - (int)tabLayoutPath.size() - 1);
		tabContextUid = contextUid;
		firstVisibleGroupTab = depth + 1 + groupTabPages[contextUid];
	}
	tabLayoutPath = activeGroupPath;
	int selectedTab = -1;
	string selectedUid;
	for (int i = 0; i < (int)children.size(); ++i) if (children[i] == inspectorIndex) {
		selectedTab = depth + 1 + i; selectedUid = nodes[children[i]]->uid; break;
	}
	const bool reveal = selectedUid != tabSelectionUid || std::abs(tabAvailableWidth-layout.bar.width)>0.5f;
	tabSelectionUid = selectedUid;
	tabAvailableWidth = layout.bar.width;
	auto width = [&](int i) { return std::min(210.0f, std::max(90.0f,
		jp_constants::p_font.stringWidth(names[i]) + 28.0f)); };
	const float rootWidth = 60;
	layout.items.push_back({0,names[0],ofRectangle(8,y,rootWidth,28)});
	// Breadcrumbs are a separate row. Keep MAIN and the deepest suffix when
	// space is limited; every ancestor is still reachable by stepping upward.
	float available = layout.bar.width-rootWidth-8;
	int firstCrumb = depth;
	float used = depth>0 ? std::min(width(depth),available) : 0;
	while (firstCrumb>1 && used+width(firstCrumb-1)+4<=available) {
		--firstCrumb; used+=width(firstCrumb)+4;
	}
	float x=8+rootWidth+8;
	for (int i=firstCrumb; i>0 && i<=depth; ++i) {
		const float w=std::min(width(i),layout.bar.getRight()-x);
		if(w<28) break;
		layout.items.push_back({i,names[i],ofRectangle(x,y,w,28)});x+=w+4;
	}
	const auto breadcrumbs=layout.items;
	if (children.empty()) return layout;
	firstVisibleGroupTab=ofClamp(firstVisibleGroupTab,depth+1,(int)names.size()-1);
	float total=0;
	for(int i=depth+1;i<(int)names.size();++i) total+=width(i)+4;
	const bool overflow=total>layout.bar.width;
	if(!overflow) firstVisibleGroupTab=depth+1;
	float right=layout.bar.getRight();
	if(overflow) {
		const float buttonWidth=layout.bar.width<300?24:28;
		layout.previous=ofRectangle(right-buttonWidth*2-4,y+34,buttonWidth,28);
		layout.next=ofRectangle(right-buttonWidth,y+34,buttonWidth,28);
		right-=buttonWidth*2+8;
	}
	auto placeItems=[&] {
		layout.items=breadcrumbs;
		float x=layout.bar.x;
		int last=firstVisibleGroupTab-1;
		for(int i=firstVisibleGroupTab;i<(int)names.size();++i) {
			const float available=right-x;
			if(available<28) break;
			const float w=std::min(width(i),available);
			if(w<width(i)&&i!=firstVisibleGroupTab) break;
			layout.items.push_back({i,names[i],ofRectangle(x,y+34,w,28)});
			x+=w+4;last=i;
		}
		layout.canPrevious=overflow&&firstVisibleGroupTab>depth+1;
		layout.canNext=overflow&&last<(int)names.size()-1;
	};
	placeItems();
	if(reveal&&selectedTab>depth) {
		if(selectedTab<firstVisibleGroupTab){firstVisibleGroupTab=selectedTab;placeItems();}
		while(firstVisibleGroupTab<selectedTab&&std::none_of(layout.items.begin(),layout.items.end(),
			[selectedTab](const GroupTabItem&i){return i.index==selectedTab;})) {++firstVisibleGroupTab;placeItems();}
	}
	return layout;
}

bool JPboxgroup::scrollGroupTabs(float delta)
{
	const auto layout = groupTabLayout();
	if (tabRenaming) return true;
	if (delta > 0 && layout.canPrevious) --firstVisibleGroupTab;
	if (delta < 0 && layout.canNext) ++firstVisibleGroupTab;
	return true;
}

void JPboxgroup::drawTabs()
{
	const auto layout = groupTabLayout();
	if (layout.items.empty()) return;
	const int depth = (int)activeGroupPath.size();
	const auto children = getDirectChildPresetIndices();
	const int inspectorIndex = isGroupViewActive() ? groupInspectorIndex : openguinumber;
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(COL_BG_TAB, 245);
	ofDrawRectangle(layout.bar);
	for (const auto &item : layout.items) {
		const auto &r = item.bounds;
		const bool active = item.index == depth;
		const int child = item.index - depth - 1;
		const bool selected = child >= 0 && child < (int)children.size() &&
			(children[child] == inspectorIndex || isBoxSelected(children[child]));
		const bool hover = r.inside(ofGetMouseX(), ofGetMouseY());
		ofSetColor(selected ? COL_ACCENT_CYAN_DARK :
			(active || hover ? COL_BG_HOVER : COL_TAB_INACTIVE_BG));
		ofDrawRectRounded(r.x, r.y, r.width, r.height, 3);
		if (active) {
			ofSetColor(COL_ACCENT_GREEN_BR);
			ofDrawRectangle(r.x + 6, r.getBottom() - 3, r.width - 12, 2);
		}
		if (tabRenaming && item.index == tabRenameTabIndex) {
			auto f = jp_text_input::field("tab-name", "tabs",
				ofRectangle(r.x + 4, r.y + 3, r.width - 8, r.height - 7),
				jp_constants::p_font, tabRenameBuffer, true);
			f.limit = 64;
			f.cancel = [this]{ cancelTabRename(); };
			f.commit = [this]{ commitTabRename(); return !tabRenaming; };
			f.validate = [](const string &v)->string { return v.empty() ?
				jp_text_input::message("Enter a name", "Ingresá un nombre") : ""; };
			jp_text_input::controller().add(std::move(f));
		} else {
			string label = item.name;
			const float available = r.width - (item.index == 0 ? 20 : 28);
			if (jp_constants::p_font.stringWidth(label) > available) {
				while (!label.empty() && jp_constants::p_font.stringWidth(label + "...") > available) {
					// Remove one UTF-8 code point, never leave a partial character.
					size_t end = label.size() - 1;
					while (end > 0 && (static_cast<unsigned char>(label[end]) & 0xc0) == 0x80) --end;
					label.resize(end);
				}
				label += "...";
			}
			ofSetColor(active || selected ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
			jp_constants::p_font.drawString(label, r.x + 10, r.y + 19);
			if (item.index > depth) {
				// Child groups lead forward; ancestors remain simple navigation labels.
				ofSetColor(selected ? COL_ACCENT_CYAN : COL_TEXT_DIM);
				ofDrawLine(r.getRight()-10, r.y+11, r.getRight()-7, r.y+14);
				ofDrawLine(r.getRight()-7, r.y+14, r.getRight()-10, r.y+17);
			}
		}
		string hint = item.name + " — " + (active && depth > 0 ?
			jp_text_input::message("Double-click to rename", "Doble clic para renombrar") :
			(item.index <= depth ? jp_text_input::message("Return to group", "Volver al grupo") :
			jp_text_input::message("Open group", "Abrir grupo")));
		jp_tooltip::draw(hint, r.x, r.y, r.width, r.height);
	}
	for (int direction : {-1, 1}) {
		const auto &r = direction < 0 ? layout.previous : layout.next;
		if (r.width == 0) continue;
		const bool enabled = direction < 0 ? layout.canPrevious : layout.canNext;
		ofSetColor(enabled && r.inside(ofGetMouseX(), ofGetMouseY()) ? COL_BG_HOVER : COL_BG_TAB);
		ofDrawRectRounded(r.x, r.y, r.width, r.height, 3);
		ofSetColor(enabled ? COL_TEXT_PRIMARY : COL_TEXT_DIM);
		const float cx = r.getCenter().x, cy = r.getCenter().y;
		ofDrawLine(cx-direction*3, cy-5, cx+direction*3, cy);
		ofDrawLine(cx+direction*3, cy, cx-direction*3, cy+5);
		jp_tooltip::draw(jp_text_input::message("Scroll groups (or use the wheel)",
			"Recorrer grupos (o usar la rueda)"), r.x, r.y, r.width, r.height);
	}
	ofPopStyle();
}

int JPboxgroup::getTabAtScreenPos(int screenX, int screenY) const
{
	for (const auto &item : groupTabLayout().items)
		if (item.bounds.inside(screenX, screenY)) return item.index;
	return -1;
}

JPbox_preset *JPboxgroup::getActivePreset() const
{
	if (activeGroupPath.empty()) return nullptr;

	// Checked like every deeper level below. Only this first one was trusted,
	// which is exactly the one that survives a clear() with a stale path.
	if (activeGroupPath[0] < 0 || activeGroupPath[0] >= (int)boxes.size())
	{
		return nullptr;
	}
	JPbox *box = boxes[activeGroupPath[0]];
	if (box == nullptr || box->getTipo() != JPbox::PRESETBOX) return nullptr;
	JPbox_preset *preset = static_cast<JPbox_preset *>(box);

	for (size_t depth = 1; depth < activeGroupPath.size(); depth++)
	{
		int idx = activeGroupPath[depth];
		if (idx < 0 || idx >= (int)preset->boxes.size() || preset->boxes[idx] == nullptr) return nullptr;
		if (preset->boxes[idx]->getTipo() != JPbox::PRESETBOX) return nullptr;
		preset = static_cast<JPbox_preset *>(preset->boxes[idx]);
	}
	return preset;
}

bool JPboxgroup::navigateToBreadcrumbLevel(int level)
{
	// level = number of path elements to keep (0 = MAIN, 1 = first level, etc.)
	if (level < 0 || level > (int)activeGroupPath.size())
	{
		return false;
	}

	// Save current viewport to the preset being exited (if any)
	if (isGroupViewActive())
	{
		JPbox_preset *currentPreset = getActivePreset();
		if (currentPreset != nullptr)
		{
			currentPreset->viewportZoom = viewportZoom;
			currentPreset->viewportPan = viewportPan;
		}
	}
	// Also save to tabZooms for MAIN level fallback
	ensureTabStateSize();
	int oldTabIndex = isGroupViewActive() ? (int)activeGroupPath.size() : 0;
	if (oldTabIndex < (int)tabZooms.size())
	{
		tabZooms[oldTabIndex] = viewportZoom;
		tabPans[oldTabIndex] = viewportPan;
	}

	if (level == 0)
	{
		// Go to MAIN
		activeGroupPath.clear();
		openguinumber = -1;
		groupInspectorIndex = -1;
		groupPreviewBoxIndex = -1;
		clearSelection();

		// Restore MAIN zoom
		if (0 < (int)tabZooms.size())
		{
			viewportZoom = tabZooms[0];
			viewportPan = tabPans[0];
		}
		else
		{
			viewportZoom = 1.0f;
			viewportPan = ofVec2f(0, 0);
		}
		return true;
	}

	// Truncate path to keep only 'level' elements
	activeGroupPath.resize(level);
	openguinumber = -1;
	groupInspectorIndex = -1;
	groupPreviewBoxIndex = -1;
	clearSelection();

	// Load viewport from the preset at this path level
	JPbox_preset *targetPreset = getActivePreset();
	if (targetPreset != nullptr)
	{
		viewportZoom = targetPreset->viewportZoom;
		viewportPan = targetPreset->viewportPan;
	}
	else
	{
		viewportZoom = 1.0f;
		viewportPan = ofVec2f(0, 0);
	}
	// Also update tabZooms for consistency
	if (level < (int)tabZooms.size())
	{
		tabZooms[level] = viewportZoom;
		tabPans[level] = viewportPan;
	}

	return true;
}

bool JPboxgroup::navigateToChildPreset(int childIndex)
{
	vector<int> childIndices = getDirectChildPresetIndices();
	if (childIndex < 0 || childIndex >= (int)childIndices.size())
	{
		return false;
	}

	int realIndex = childIndices[childIndex];

	// Save current viewport to the preset being exited (if any)
	if (isGroupViewActive())
	{
		JPbox_preset *currentPreset = getActivePreset();
		if (currentPreset != nullptr)
		{
			currentPreset->viewportZoom = viewportZoom;
			currentPreset->viewportPan = viewportPan;
		}
	}
	// Also save to tabZooms for MAIN level fallback
	ensureTabStateSize();
	int oldTabIndex = isGroupViewActive() ? (int)activeGroupPath.size() : 0;
	if (oldTabIndex < (int)tabZooms.size())
	{
		tabZooms[oldTabIndex] = viewportZoom;
		tabPans[oldTabIndex] = viewportPan;
	}

	// Build the new path: current activeGroupPath + child's real index
	vector<int> newPath = activeGroupPath;
	newPath.push_back(realIndex);

	// Verify the target exists and is a valid preset
	JPbox *box = boxes[newPath[0]];
	if (box == nullptr || box->getTipo() != JPbox::PRESETBOX) return false;
	JPbox_preset *preset = static_cast<JPbox_preset *>(box);
	for (size_t d = 1; d < newPath.size(); d++)
	{
		int idx = newPath[d];
		if (idx < 0 || idx >= (int)preset->boxes.size() || preset->boxes[idx] == nullptr) return false;
		if (d < newPath.size() - 1)
		{
			if (preset->boxes[idx]->getTipo() != JPbox::PRESETBOX) return false;
			preset = static_cast<JPbox_preset *>(preset->boxes[idx]);
		}
	}

	activeGroupPath = newPath;
	openguinumber = -1;
	groupInspectorIndex = -1;
	groupPreviewBoxIndex = -1;
	clearSelection();

	// Load viewport from the target preset
	int newLevel = (int)newPath.size();
	JPbox_preset *targetPreset = getActivePreset();
	if (targetPreset != nullptr)
	{
		viewportZoom = targetPreset->viewportZoom;
		viewportPan = targetPreset->viewportPan;
	}
	else
	{
		viewportZoom = 1.0f;
		viewportPan = ofVec2f(0, 0);
	}
	// Also update tabZooms for consistency
	if (newLevel < (int)tabZooms.size())
	{
		tabZooms[newLevel] = viewportZoom;
		tabPans[newLevel] = viewportPan;
	}

	return true;
}

bool JPboxgroup::handleTabClick()
{
	const auto layout = groupTabLayout();
	if (layout.previous.inside(ofGetMouseX(), ofGetMouseY())) return scrollGroupTabs(1);
	if (layout.next.inside(ofGetMouseX(), ofGetMouseY())) return scrollGroupTabs(-1);
	int tabIndex = getTabAtScreenPos(ofGetMouseX(), ofGetMouseY());
	if (tabIndex < 0)
	{
		return layout.bar.inside(ofGetMouseX(), ofGetMouseY());
	}

	int pathLen = (int)activeGroupPath.size();
	vector<int> childIndices = getDirectChildPresetIndices();

	// A canvas click or a different group must never count as a rename click.
	JPbox *clicked = nullptr;
	if (tabIndex == pathLen && pathLen > 0) clicked = getActivePreset();
	if (tabIndex > pathLen) {
		const auto *parent = getActivePreset();
		const auto &nodes = parent ? parent->boxes : boxes;
		clicked = nodes[childIndices[tabIndex - pathLen - 1]];
	}
	const uint64_t now = ofGetSystemTimeMillis();
	const bool renameClick = clicked && clicked->uid == lastGroupTabClickUid &&
		now - lastGroupTabClickTime < duration_mouseclick;
	lastGroupTabClickUid = clicked ? clicked->uid : "";
	lastGroupTabClickTime = now;

	// Tab index breakdown:
	// 0 = MAIN
	// 1..pathLen = breadcrumb levels (clickable to go back)
	// pathLen+1.. = child presets (clickable to go forward)

	if (tabIndex == 0)
	{
		if (isGroupViewActive()) navigateToBreadcrumbLevel(0);
		return true;
	}

	if (tabIndex >= 1 && tabIndex <= pathLen)
	{
		// Handle double-click on the last breadcrumb tab (current level) -> inline rename
		if (renameClick && tabIndex == pathLen && pathLen > 0)
		{
			int realIdx = activeGroupPath[pathLen - 1];
			const vector<JPbox *> *boxList = nullptr;

			if (pathLen == 1)
			{
				// Direct child of JPboxgroup (top-level preset)
				boxList = &boxes;
			}
			else
			{
				// Traverse activeGroupPath up to pathLen-1 to get the parent preset
				JPbox *parentBox = boxes[activeGroupPath[0]];
				if (parentBox != nullptr && parentBox->getTipo() == JPbox::PRESETBOX)
				{
					JPbox_preset *parentPreset = static_cast<JPbox_preset *>(parentBox);
					bool valid = true;
					for (int d = 1; d < pathLen - 1 && valid; d++)
					{
						int idx = activeGroupPath[d];
						if (idx >= 0 && idx < (int)parentPreset->boxes.size() && parentPreset->boxes[idx] != nullptr &&
							parentPreset->boxes[idx]->getTipo() == JPbox::PRESETBOX)
						{
							parentPreset = static_cast<JPbox_preset *>(parentPreset->boxes[idx]);
						}
						else
						{
							valid = false;
						}
					}
					if (valid)
					{
						boxList = &parentPreset->boxes;
					}
				}
			}

			if (boxList != nullptr && realIdx >= 0 && realIdx < (int)boxList->size() && (*boxList)[realIdx] != nullptr)
			{
				tabRenaming = true;
				tabRenameTabIndex = tabIndex;
				tabRenameBuffer = (*boxList)[realIdx]->name;
				tabRenameCursor = tabRenameBuffer.size();
			}
			return true;
		}

		// Selecting the current tab preserves the user's framing.
		if (tabIndex != pathLen) navigateToBreadcrumbLevel(tabIndex);
		return true;
	}

	// Child preset tab
	int childIndex = tabIndex - pathLen - 1;
	if (childIndex >= 0 && childIndex < (int)childIndices.size())
	{
		// Navigate into child preset
		navigateToChildPreset(childIndex);
		return true;
	}

	return false;
}

void JPboxgroup::ensureTabStateSize()
{
	vector<int> childIndices = getDirectChildPresetIndices();
	int pathLen = (int)activeGroupPath.size();
	int neededTabs = 1 + pathLen + (int)childIndices.size();
	// Also allocate for potential deeper paths
	int maxPossibleDepth = pathLen + 1 + (int)childIndices.size();
	neededTabs = std::max(neededTabs, maxPossibleDepth);

	while ((int)tabZooms.size() < neededTabs)
	{
		tabZooms.push_back(1.0f);
		tabPans.push_back(ofVec2f(0, 0));
	}
}

void JPboxgroup::cancelTabRename()
{
	tabRenaming = false;
	tabRenameTabIndex = -1;
	tabRenameBuffer.clear();
}


void JPboxgroup::commitTabRename()
{
	if (!tabRenaming) return;
	{
		if (!tabRenameBuffer.empty())
		{
			// Find the box and apply the new name
			int pathLen = (int)activeGroupPath.size();
			vector<int> childIndices = getDirectChildPresetIndices();
			int childIndex = tabRenameTabIndex - pathLen - 1;
			bool renamed = false;

			if (childIndex >= 0 && childIndex < (int)childIndices.size())
			{
				// Case 1: Child tab rename (tabRenameTabIndex > pathLen)
				int realIdx = childIndices[childIndex];
				const vector<JPbox *> *boxList = &boxes;
				JPbox_preset *ownerPreset = nullptr;
				if (isGroupViewActive())
				{
					ownerPreset = getActivePreset();
					if (ownerPreset != nullptr)
					{
						boxList = &ownerPreset->boxes;
					}
				}
				if (realIdx >= 0 && realIdx < (int)boxList->size() && (*boxList)[realIdx] != nullptr)
				{
					const string oldName =
						(*boxList)[realIdx]->name;
					(*boxList)[realIdx]->name = tabRenameBuffer;
					if (ownerPreset != nullptr)
					{
						ownerPreset
							->renameExposedTextureInputTarget(
								oldName, tabRenameBuffer);
					}
					renamed = true;
				}
			}
			else if (tabRenameTabIndex >= 1 && tabRenameTabIndex <= pathLen && pathLen > 0)
			{
				// Case 2: Breadcrumb tab rename (tabRenameTabIndex in 1..pathLen)
				int realIdx = activeGroupPath[pathLen - 1];
				const vector<JPbox *> *boxList = nullptr;
				JPbox_preset *ownerPreset = nullptr;

				if (pathLen == 1)
				{
					boxList = &boxes;
				}
				else
				{
					JPbox *parentBox = boxes[activeGroupPath[0]];
					if (parentBox != nullptr && parentBox->getTipo() == JPbox::PRESETBOX)
					{
						JPbox_preset *parentPreset = static_cast<JPbox_preset *>(parentBox);
						bool valid = true;
						for (int d = 1; d < pathLen - 1 && valid; d++)
						{
							int idx = activeGroupPath[d];
							if (idx >= 0 && idx < (int)parentPreset->boxes.size() && parentPreset->boxes[idx] != nullptr &&
								parentPreset->boxes[idx]->getTipo() == JPbox::PRESETBOX)
							{
								parentPreset = static_cast<JPbox_preset *>(parentPreset->boxes[idx]);
							}
							else
							{
								valid = false;
							}
						}
						if (valid)
						{
							boxList = &parentPreset->boxes;
							ownerPreset = parentPreset;
						}
					}
				}
				if (boxList != nullptr && realIdx >= 0 && realIdx < (int)boxList->size() && (*boxList)[realIdx] != nullptr)
				{
					const string oldName =
						(*boxList)[realIdx]->name;
					(*boxList)[realIdx]->name = tabRenameBuffer;
					if (ownerPreset != nullptr)
					{
						ownerPreset
							->renameExposedTextureInputTarget(
								oldName, tabRenameBuffer);
					}
					renamed = true;
				}
			}
		}
		tabRenaming = false;
		tabRenameTabIndex = -1;
		tabRenameBuffer.clear();
		return;
	}
	cancelTabRename();
}
