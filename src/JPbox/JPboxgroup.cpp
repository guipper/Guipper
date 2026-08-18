#include "JPboxgroup.h"
#include "../JPgui/jp_gl_state.h"
#include "jp_media.h"
#include "../JPutils/jp_pointer.h"
#include "../JPutils/jp_audio.h"
#include "../JPgui/jp_shader_editor.h"
#include "../JPutils/jp_textfield.h"
#include "../JPutils/jp_tooltip.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <functional>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace
{
	using ProfileClock = std::chrono::steady_clock;
	float elapsedProfileMs(ProfileClock::time_point start)
	{
		return std::chrono::duration<float, std::milli>(
			ProfileClock::now() - start).count();
	}
	void saveParameterUserState(ofXml &node, JPParameter *parameter)
	{
		if (parameter == nullptr) return;
		node.appendChild("randomlocked").set(parameter->randomLocked);
		if (parameter->variabletype == JPParameter::FLOAT)
		{
			node.appendChild("rangeenabled").set(parameter->rangeEnabled);
			node.appendChild("defaultvalue").set(parameter->defaultFloatValue);
		}
		else if (parameter->variabletype == JPParameter::BOOL)
			node.appendChild("defaultbool").set(parameter->defaultBoolValue);
	}
	void loadParameterUserState(ofXml &node, JPParameter *parameter)
	{
		if (parameter == nullptr) return;
		auto locked = node.getChild("randomlocked");
		if (locked) parameter->randomLocked = locked.getBoolValue();
		auto value = node.getChild("defaultvalue");
		if (value && parameter->variabletype == JPParameter::FLOAT)
			parameter->defaultFloatValue = ofClamp(
				value.getFloatValue(), parameter->nativeMin, parameter->nativeMax);
		if (parameter->variabletype == JPParameter::FLOAT)
		{
			auto enabled = node.getChild("rangeenabled");
			// Custom limiting is opt-in. Legacy files keep their remembered
			// endpoints, but open on the native slider domain.
			parameter->setRangeEnabled(enabled ? enabled.getBoolValue() : false);
		}
		auto boolean = node.getChild("defaultbool");
		if (boolean && parameter->variabletype == JPParameter::BOOL)
			parameter->defaultBoolValue = boolean.getBoolValue();
	}
	string mediaTimeLabel(double seconds)
	{
		seconds = std::max(0.0, seconds);
		const int hours = (int)(seconds / 3600.0);
		const int mins = ((int)(seconds / 60.0)) % 60;
		const double secs = seconds - (int)(seconds / 60.0) * 60.0;
		std::ostringstream label;
		label << std::setfill('0');
		if (hours > 0) label << std::setw(2) << hours << ':';
		label << std::setw(2) << mins << ':' << std::fixed <<
			std::setw(6) << std::setprecision(3) << secs;
		return label.str();
	}
	bool parseMediaTime(const string &text, double duration, int frames, float &normalized)
	{
		try
		{
			double seconds = 0.0;
			if (!text.empty() && (text.back()=='f' || text.back()=='F'))
			{
				const int frame = std::stoi(text.substr(0,text.size()-1));
				normalized = frames > 1 ? frame/(float)(frames-1) : 0.0f; return true;
			}
			vector<string> parts = ofSplitString(text, ":", true, true);
			if(parts.size()==1) seconds=std::stod(parts[0]);
			else if(parts.size()==2) seconds=std::stod(parts[0])*60.0+std::stod(parts[1]);
			else if(parts.size()==3) seconds=std::stod(parts[0])*3600.0+std::stod(parts[1])*60.0+std::stod(parts[2]);
			else return false;
			normalized = duration > 0 ? (float)(seconds/duration) : 0.0f; return true;
		}
		catch (...) { return false; }
	}
	void copyFboStraight(ofFbo &source, ofFbo &destination)
	{
		if (!source.isAllocated() || !destination.isAllocated()) return;
		ofPushStyle();
		destination.begin();
		ofClear(0, 0, 0, 0);
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		ofSetColor(255);
		ofSetRectMode(OF_RECTMODE_CORNER);
		source.draw(0, 0, destination.getWidth(), destination.getHeight());
		destination.end();
		ofEnableAlphaBlending();
		ofPopStyle();
	}
	// Was constexpr false, so nobody could ever switch it on without a rebuild.
	// Now shares the one GUIPPER_HITBOX switch with the box buttons.
	inline bool kShowInspectorClickBoundsEnabled()
	{
		return jp_hitbox::debugEnabled();
	}

	string fitInspectorLabel(string text, float maxWidth)
	{
		if (maxWidth <= 0.0f)
		{
			return "";
		}
		if (jp_constants::p_font.stringWidth(text) <= maxWidth)
		{
			return text;
		}
		while (text.size() > 1 &&
			jp_constants::p_font.stringWidth(text + "..") > maxWidth)
		{
			text.pop_back();
		}
		return text.empty() ? "" : text + "..";
	}

	void drawInspectorClickBounds(const ofRectangle &bounds, bool enabled = true)
	{
		if (!kShowInspectorClickBoundsEnabled() || bounds.width <= 0.0f ||
			bounds.height <= 0.0f)
		{
			return;
		}

		const bool hovered = enabled &&
			bounds.inside(ofGetMouseX(), ofGetMouseY());
		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofNoFill();
		ofSetLineWidth(hovered ? 1.5f : 1.0f);
		ofSetColor(hovered ? COL_ACCENT_GOLD :
			(enabled ? ofColor(COL_ACCENT_CYAN, 145) :
				ofColor(COL_BORDER_MUTED, 90)));
		ofDrawRectRounded(bounds, 2.0f);
		ofPopStyle();
	}

	void drawInspectorClickBounds(const JPdragobject &control,
		bool enabled = true)
	{
		drawInspectorClickBounds(
			ofRectangle(
				control.x - control.width / 2.0f,
				control.y - control.height / 2.0f,
				control.width,
				control.height),
			enabled);
	}
}

void JPboxgroup::layoutMediaInspector(JPMediaInspectable *media, float &cursorY)
{
	if (media == nullptr) return;
	const float x=inspectorBodyViewport.x+inspectorLayout.contentPadding;
	const float w=inspectorBodyViewport.width-inspectorLayout.contentPadding*2.0f;
	const bool playable=media->mediaPlayable() || !media->mediaReady();
	mediaInspectorPlayableBuilt=playable;
	const float h=playable?176.0f:42.0f;
	mediaInspector.card.set(x,cursorY,w,h);
	const float gap=5.0f, button=24.0f, top=cursorY+9.0f;
	mediaInspector.fit.set(x+8,top,w-16,button);
	if(playable)
	{
		float bx=x+8;
		const float transportTop=top+32;
		auto placeTransport=[&](ofRectangle&r,float width){r.set(bx,transportTop,width,button);bx+=width+gap;};
		placeTransport(mediaInspector.restart,button);placeTransport(mediaInspector.previous,button);
		placeTransport(mediaInspector.play,32);placeTransport(mediaInspector.next,button);placeTransport(mediaInspector.direction,button);
		mediaInspector.loop.set(x+8,top+64,78,button);
		mediaInspector.speed.set(x+91,top+64,76,button);
		mediaInspector.mute.set(x+w-126,top+64,42,button);
		mediaInspector.volume.set(x+w-79,top+64,71,button);
		mediaInspector.timeline.set(x+8,top+100,w-16,18);
		const float rangeGroupWidth=108.0f,rangeButtonWidth=30.0f,rangeGap=3.0f;
		mediaInspector.inButton.set(x+8,top+126,rangeButtonWidth,24);
		mediaInspector.inField.set(mediaInspector.inButton.getRight()+rangeGap,top+126,
			rangeGroupWidth-rangeButtonWidth-rangeGap,24);
		mediaInspector.outButton.set(x+w-116,top+126,rangeButtonWidth,24);
		mediaInspector.outField.set(mediaInspector.outButton.getRight()+rangeGap,top+126,
			rangeGroupWidth-rangeButtonWidth-rangeGap,24);
	}
	else
	{
		mediaInspector.restart.set(0,0,0,0);mediaInspector.previous.set(0,0,0,0);
		mediaInspector.play.set(0,0,0,0);mediaInspector.next.set(0,0,0,0);
		mediaInspector.direction.set(0,0,0,0);mediaInspector.loop.set(0,0,0,0);
		mediaInspector.speed.set(0,0,0,0);mediaInspector.mute.set(0,0,0,0);
		mediaInspector.volume.set(0,0,0,0);mediaInspector.timeline.set(0,0,0,0);
		mediaInspector.inButton.set(0,0,0,0);mediaInspector.inField.set(0,0,0,0);
		mediaInspector.outButton.set(0,0,0,0);mediaInspector.outField.set(0,0,0,0);
	}
	cursorY+=h+inspectorLayout.rowGap;
}

void JPboxgroup::drawMediaInspector(JPMediaInspectable *target)
{
	if(target==nullptr || mediaInspector.card.width<=0) return;
	JPMediaState &s=target->mediaState();
	jp_media::normalize(s);
	auto button=[&](const ofRectangle&r,const string&label,bool active=false)
	{
		if(r.width<=0)return;
		const bool over=r.inside(ofGetMouseX(),ofGetMouseY());
		ofSetColor(active?COL_ACCENT_CYAN_DIM:(over?COL_BG_HOVER:COL_BG_INPUT));ofDrawRectRounded(r,3);
		ofNoFill();ofSetColor(active?COL_ACCENT_CYAN:COL_BORDER_MUTED);ofDrawRectRounded(r,3);ofFill();
		ofSetColor(active?COL_TEXT_PRIMARY:COL_TEXT_SECONDARY);
		jp_constants::inspector_media_font.drawString(label,r.getCenter().x-jp_constants::inspector_media_font.stringWidth(label)/2,r.getCenter().y+3);
	};
	enum class MediaIcon { Restart, Previous, PlayPause, Next, Direction };
	auto iconButton=[&](const ofRectangle&r,MediaIcon icon,bool active=false)
	{
		button(r,"",active);
		const float cx=r.getCenter().x,cy=r.getCenter().y;
		ofSetColor(active?COL_TEXT_PRIMARY:COL_TEXT_SECONDARY);
		ofSetLineWidth(1.4f);
		if(icon==MediaIcon::Restart)
		{
			ofPolyline arc;
			for(int i=0;i<=16;++i)
			{
				const float a=ofDegToRad(-45.0f+i*17.0f);
				arc.addVertex(cx+std::cos(a)*6.0f,cy+std::sin(a)*6.0f);
			}
			arc.draw();
			ofDrawTriangle(cx+4.2f,cy-6.0f,cx+9.0f,cy-5.2f,cx+6.2f,cy-1.5f);
		}
		else if(icon==MediaIcon::Previous)
		{
			ofDrawLine(cx-6,cy-6,cx-6,cy+6);
			ofDrawTriangle(cx-4,cy,cx+5,cy-6,cx+5,cy+6);
		}
		else if(icon==MediaIcon::PlayPause)
		{
			if(active)
			{
				ofDrawRectangle(cx-5,cy-6,3,12);
				ofDrawRectangle(cx+2,cy-6,3,12);
			}
			else ofDrawTriangle(cx-4,cy-7,cx+7,cy,cx-4,cy+7);
		}
		else if(icon==MediaIcon::Next)
		{
			ofDrawTriangle(cx+4,cy,cx-5,cy-6,cx-5,cy+6);
			ofDrawLine(cx+6,cy-6,cx+6,cy+6);
		}
		else
		{
			const float direction=active?-1.0f:1.0f;
			ofDrawLine(cx-7*direction,cy,cx+7*direction,cy);
			ofDrawTriangle(cx+7*direction,cy,
				cx+2*direction,cy-4,cx+2*direction,cy+4);
		}
		ofSetLineWidth(1.0f);
	};
	ofSetColor(ofColor(COL_BG_PANEL,245));ofDrawRectRounded(mediaInspector.card,4);
	ofNoFill();ofSetColor(ofColor(COL_BORDER_MUTED,180));ofDrawRectRounded(mediaInspector.card,4);ofFill();
	static const char *fits[]={"CUSTOM","FIT","FILL","STRETCH","ORIGINAL"};
	const int fitIndex=std::clamp((int)s.fitMode,0,4);
	ofSetColor(COL_BG_INPUT);ofDrawRectRounded(mediaInspector.fit,3);
	const float fitSegment=mediaInspector.fit.width/5.0f;
	ofSetColor(COL_ACCENT_CYAN_DIM);
	ofDrawRectRounded(mediaInspector.fit.x+fitSegment*fitIndex,
		mediaInspector.fit.y,fitSegment,mediaInspector.fit.height,3);
	for(int i=0;i<5;++i)
	{
		if(i>0){ofSetColor(COL_BORDER_MUTED);ofDrawLine(mediaInspector.fit.x+fitSegment*i,
			mediaInspector.fit.y+4,mediaInspector.fit.x+fitSegment*i,mediaInspector.fit.getBottom()-4);}
		ofSetColor(i==fitIndex?COL_TEXT_PRIMARY:COL_TEXT_SECONDARY);
		const string label=fits[i];
		jp_constants::inspector_media_font.drawString(label,
			mediaInspector.fit.x+fitSegment*(i+0.5f)-
			jp_constants::inspector_media_font.stringWidth(label)/2,
			mediaInspector.fit.getCenter().y+3);
	}
	ofNoFill();ofSetColor(COL_ACCENT_CYAN);ofDrawRectRounded(mediaInspector.fit,3);ofFill();
	jp_tooltip::draw("Object fit",mediaInspector.fit.x,mediaInspector.fit.y,
		mediaInspector.fit.width,mediaInspector.fit.height);
	if(mediaInspector.play.width<=0)return;
	iconButton(mediaInspector.restart,MediaIcon::Restart);
	iconButton(mediaInspector.previous,MediaIcon::Previous);
	iconButton(mediaInspector.play,MediaIcon::PlayPause,s.playing);
	iconButton(mediaInspector.next,MediaIcon::Next);
	iconButton(mediaInspector.direction,MediaIcon::Direction,s.reverse);
	jp_tooltip::draw("Restart",mediaInspector.restart.x,mediaInspector.restart.y,mediaInspector.restart.width,mediaInspector.restart.height);
	jp_tooltip::draw("Previous frame",mediaInspector.previous.x,mediaInspector.previous.y,mediaInspector.previous.width,mediaInspector.previous.height);
	jp_tooltip::draw(s.playing?"Pause":"Play",mediaInspector.play.x,mediaInspector.play.y,mediaInspector.play.width,mediaInspector.play.height);
	jp_tooltip::draw("Next frame",mediaInspector.next.x,mediaInspector.next.y,mediaInspector.next.width,mediaInspector.next.height);
	jp_tooltip::draw(s.reverse?"Play forward":"Play reverse",mediaInspector.direction.x,mediaInspector.direction.y,mediaInspector.direction.width,mediaInspector.direction.height);
	static const char *loops[]={"ONCE","LOOP","PING"};button(mediaInspector.loop,loops[std::clamp((int)s.loopMode,0,2)],true);
	button(mediaInspector.speed,ofToString(s.rate,2)+"x");
	if(target->mediaHasAudio()){button(mediaInspector.mute,s.muted?"MUTE":"AUD",s.muted);button(mediaInspector.volume,ofToString((int)(s.volume*100)));}
	auto meter=[&](const ofRectangle&r,float n){ofSetColor(COL_ACCENT_CYAN);ofDrawRectangle(r.x+4,r.getBottom()-3,(r.width-8)*ofClamp(n,0.0f,1.0f),1.5f);};
	meter(mediaInspector.speed,(s.rate-.25f)/3.75f);
	if(target->mediaHasAudio())meter(mediaInspector.volume,s.volume);
	const ofRectangle&t=mediaInspector.timeline;
	ofSetColor(COL_BG_INPUT);ofDrawRectRounded(t,3);
	const float inX=t.x+t.width*s.rangeIn,outX=t.x+t.width*s.rangeOut,posX=t.x+t.width*s.position;
	ofSetColor(ofColor(COL_ACCENT_CYAN,80));ofDrawRectangle(inX,t.y,outX-inX,t.height);
	ofSetColor(COL_ACCENT_GOLD);ofDrawLine(inX,t.y,inX,t.getBottom());ofDrawLine(outX,t.y,outX,t.getBottom());
	ofSetColor(COL_TEXT_PRIMARY);ofDrawLine(posX,t.y-2,posX,t.getBottom()+2);
	auto field=[&](const ofRectangle&r,const string&prefix,float n,int focus)
	{
		button(r,prefix+((mediaTimeFieldFocus==focus)?mediaTimeFieldBuffer:mediaTimeLabel(n*target->mediaDurationSeconds())),mediaTimeFieldFocus==focus);
	};
	button(mediaInspector.inButton,"IN");button(mediaInspector.outButton,"OUT");
	field(mediaInspector.inField,"",s.rangeIn,1);field(mediaInspector.outField,"",s.rangeOut,2);
	jp_tooltip::draw("Set IN at playhead",mediaInspector.inButton.x,mediaInspector.inButton.y,
		mediaInspector.inButton.width,mediaInspector.inButton.height);
	jp_tooltip::draw("Set OUT at playhead",mediaInspector.outButton.x,mediaInspector.outButton.y,
		mediaInspector.outButton.width,mediaInspector.outButton.height);
	if(!target->mediaReady()){ofSetColor(COL_ACCENT_GOLD);jp_constants::inspector_media_font.drawString(target->mediaStatus(),t.x,t.y-4);}
}

void JPboxgroup::recordProfileValue(float &average, float &peak,
	float sampleMs)
{
	average = average <= 0.0f ? sampleMs : ofLerp(average, sampleMs, 0.08f);
	peak = std::max(sampleMs, peak * 0.995f);
}

void JPboxgroup::scheduleTopLevelRenders()
{
	const int boxCount = (int)boxes.size();
	// Roots: everything the SCREEN depends on this frame. jp_renderschedule
	// walks their inputs and drops the rest to the staggered preview rate.
	vector<int> roots;

	if (activerender != nullptr) roots.push_back(*activerender);

	// A normal CUE preview displays a real (usually non-active) graph box.
	// Treat that preview exactly like a fixed live output so animation and all
	// of its upstream inputs remain full-rate while the panel is open.
	if (isCueNormalPreviewMode()) roots.push_back(cueState.previewIndex);

	// Keep both transition inputs live only for the crossfade. Once it reaches
	// its target, the active render already covers the second input and the old
	// branch can return to thumbnail rate.
	if (transition.getLerpValue() < 1.0f)
	{
		const ofFbo *transitionInputs[] = {
			transition.getFirstInput(), transition.getSecondInput()
		};
		for (const ofFbo *input : transitionInputs)
		{
			for (int i = 0; input != nullptr && i < boxCount; ++i)
			{
				if (&boxes[i]->fbo == input) roots.push_back(i);
			}
		}
	}

	// A live-output window may intentionally show a non-active box.
	for (const string &name : requiredRenderSources)
	{
		for (int i = 0; i < boxCount; ++i)
		{
			if (boxes[i]->name == name)
			{
				roots.push_back(i);
				break;
			}
		}
	}

	jp_renderschedule::apply(boxes, roots, ofGetFrameNum(), activeSequence);
}


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
	string nombre = jp_media::stem(directory);
	/*
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
	*/
	if (directory.find("camdepth") != std::string::npos)
	{
		// Checked first for the same reason the box dispatch is: "camdepth"
		// contains "cam", so the looser test below would name it CAMARITA.
		nombre = "CAM DEPTH";
	}
	else if (directory.find("cam") != std::string::npos)
	{
		nombre = "CAMARITA";
	}
	else if (directory.find("kinect2") != std::string::npos)
	{
		nombre = "KINECT2";
	}
	else if (directory.find("pointercloud") != std::string::npos)
	{
		nombre = "PointerCloud";
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
	else if (jp_media::isImage(directory))
	{
		bx = new JPbox_image();
	}
	else if (jp_media::isVideo(directory))
	{
		bx = new JPbox_video();
	}
	else if (directory.find(".xml") != std::string::npos)
	{
		bx = new JPbox_preset();
	}
	else if (directory.find("kinect2") != std::string::npos)
	{
		bx = new JPbox_kinect2();
	}
	else if (directory.find("pointercloud") != std::string::npos)
	{
		bx = new JPbox_pointercloud();
	}
	// BEFORE the plain "cam" test: "camdepth" contains "cam", so the
	// looser check would swallow it and build a CAMARITA instead.
	else if (directory.find("camdepth") != std::string::npos)
	{
		bx = new JPbox_camdepth();
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
	return makeUniqueBoxName(baseName, boxes);
}

string JPboxgroup::makeUniqueBoxName(const string &baseName, const vector<JPbox *> &checkBoxes) const
{
	string nombre = baseName;
	string nombreaux = nombre;
	bool existenombre = false;
	int counter = 2;
	do
	{
		existenombre = false;
		for (int i = 0; i < checkBoxes.size(); i++)
		{
			if (checkBoxes[i] != nullptr && nombre.compare(checkBoxes[i]->name) == 0)
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

vector<JPbox *> *JPboxgroup::getCurrentViewBoxes()
{
	if (!isGroupViewActive())
	{
		return &boxes;
	}
	JPbox_preset *preset = getActivePreset();
	return preset != nullptr ? &preset->boxes : nullptr;
}

int *JPboxgroup::getCurrentViewActiveRenderPointer()
{
	if (!isGroupViewActive())
	{
		return activerender;
	}
	JPbox_preset *preset = getActivePreset();
	return preset != nullptr ? &preset->activeRender : nullptr;
}

string JPboxgroup::makeNextGroupName(
	const vector<JPbox *> &siblings) const
{
	const string prefix = "group";
	int highestSuffix = 0;
	for (JPbox *box : siblings)
	{
		if (box == nullptr ||
			box->name.rfind(prefix, 0) != 0 ||
			box->name.size() == prefix.size())
		{
			continue;
		}
		const string suffix = box->name.substr(prefix.size());
		const bool numeric = std::all_of(
			suffix.begin(), suffix.end(),
			[](unsigned char character) {
				return std::isdigit(character) != 0;
			});
		if (numeric)
		{
			highestSuffix = std::max(
				highestSuffix, ofToInt(suffix));
		}
	}
	return prefix + ofToString(highestSuffix + 1);
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
	setupDefaultMappingPanelLayout();
}
void JPboxgroup::draw()
{
	// boxesdrawing.draw(0, 0, ofGetWidth(), ofGetHeight());
	// boxesdrawing.draw(offsetx, offsety, ofGetWidth(), ofGetHeight());

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
			activeRenderDisplayIndex = getCurrentViewActiveRenderIndex();
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
		JPbox_preset *linkOwnerPreset = cueTargetsCurrentView()
			? getDraftPresetForCurrentView()
			: getActivePreset();
		ofSetLineWidth(2);
		for (int i = (int)activeBoxes.size() - 1; i >= 0; i--)
		{
			for (int k = (int)activeBoxes.size() - 1; k >= 0; k--)
			{
				// During a cue on this group, draw the draft's staged links.
				JPbox *linkSrcK = activeBoxes[k];
				if (cueTargetsCurrentView()) {
					JPbox *dk = getCueDraftBoxForRealIndex(k);
					if (dk != nullptr) linkSrcK = dk;
				}
				for (int l = activeBoxes[k]->fbohandlergroup.getSize() - 1; l >= 0; l--)
				{
					if (linkOwnerPreset != nullptr &&
						l < linkSrcK->fbohandlergroup.getSize() &&
						linkOwnerPreset->isExposedTextureInputTarget(
							linkSrcK->name,
							linkSrcK->fbohandlergroup.getName(l)))
					{
						continue;
					}
					if (l < linkSrcK->fbohandlergroup.getSize() &&
						linkSrcK->fbohandlergroup.getFboName(l) == activeBoxes[i]->name)
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
		ofSetColor(COL_ACCENT_CYAN, 45);
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofFill();
		ofVec2f center = ofVec2f((selectionEnd.x + lastMouseClick.x) / 2, (selectionEnd.y + lastMouseClick.y) / 2);
		float w = abs(selectionEnd.x - lastMouseClick.x);
		float h = abs(selectionEnd.y - lastMouseClick.y);
		ofDrawRectangle(center.x, center.y, w, h);
		ofNoFill();
		ofSetLineWidth(2);
		ofSetColor(COL_ACCENT_CYAN, 210);
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
			// Cue staged render applies whenever the cue targets the current graph.
			if (!isGroupViewActive() && cueTargetsCurrentView() &&
				cueState.stagedActiveRenderIndex >= 0 &&
				cueState.stagedActiveRenderIndex < (int)activeBoxes.size())
			{
				displayIndex = cueState.stagedActiveRenderIndex;
			}
			if (displayIndex == i) {
				ofSetColor(COL_ACCENT_GREEN_BR, 210);
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
		else
		{
			// Group view: ensure bypass/onoff buttons are clickable
			activeBoxes[i]->bypass.activable2 = true;
			activeBoxes[i]->onoff.activable2 = true;
		}

		// Cue draft overlay — shown whenever the cue targets the current graph
		// (main graph or the active preset in group view).
		const bool cueStagesVisibleNodes = isCueDraftMode() &&
			(!isGroupViewActive() || getDraftPresetForCurrentView() != nullptr);
		if (cueTargetsCurrentView() || cueStagesVisibleNodes)
		{
			JPbox *draftBox = getCueDraftBoxForCurrentViewIndex(i);
			const int cueDirtyIndex = isGroupViewActive() &&
				!activeGroupPath.empty() ? activeGroupPath[0] : i;
			bool cueDraftBox = draftBox != nullptr;
			bool cueDirtyBox = isCueDraftDirty(cueDirtyIndex);
			if (cueDraftBox)
			{
				activeBoxes[i]->setBackgroundOverride(cueDirtyBox ? ofColor(COL_ACCENT_CYAN_DARK, 245) : ofColor(COL_ACCENT_CYAN_DARK, 200),
													   cueDirtyBox ? ofColor(COL_ACCENT_CYAN, 255) : ofColor(COL_ACCENT_CYAN_DIM, 225));
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
					markCueDraftDirty(cueDirtyIndex,
						CUE_DIRTY_BYPASS_PAUSE);
				}
				if (draftOnOffAfterDraw != draftOnOffBeforeDraw)
				{
					draftBox->setonoff(draftOnOffAfterDraw);
					markCueDraftDirty(cueDirtyIndex,
						CUE_DIRTY_BYPASS_PAUSE);
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
			// No cue targets this graph: plain draw.
			activeBoxes[i]->draw();
		}

		// GUIPPER_HITBOX=1: outline the box's selectable area, its two toggles,
		// its texture OUT and every texture IN. Here rather than inside
		// JPbox::draw so it paints over the controls, not beneath them.
		activeBoxes[i]->drawHitboxDebug();

		// Cyan outline: multi-selected box (shared)
		if (isBoxSelected(i))
		{
			ofPushStyle();
			ofSetRectMode(OF_RECTMODE_CENTER);
			ofNoFill();
			ofSetLineWidth(4);
			ofSetColor(COL_ACCENT_CYAN, 255);
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
			ofSetColor(COL_ACCENT_CYAN, 255);
			ofDrawRectRounded(x, y - activeBoxes[i]->height / 2 + 10, activeBoxes[i]->width * 1.22, activeBoxes[i]->height * 1.22, 10);
			ofPopStyle();
		}

		// Cue-added "NEW" label (when the cue targets the current graph)
		if (cueTargetsCurrentView() && isCueAddedRealIndex(i))
		{
			ofPushStyle();
			ofSetColor(COL_BG_DARK, 230);
			ofDrawRectangle(x - activeBoxes[i]->width / 2 + 6, y - activeBoxes[i]->height / 2 + 16, 30, 13);
			ofSetColor(COL_ACCENT_CYAN, 255);
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
	// The cue panel is hit-tested before the canvas, so it has to be painted
	// over the canvas too. It used to be drawn first, which meant boxes covered
	// a panel that still swallowed their clicks.
	drawCuePreview();
	drawMappingPanel();

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
	ofSetColor(COL_BG_TAB, 230);
	ofDrawRectRounded(cuePanelX, cuePanelY, cuePanelW, cuePanelH, 6);
	ofSetColor(COL_BG_PANEL, 235);
	ofDrawRectRounded(cuePanelX, cuePanelY, cuePanelW, headerH, 6);
	ofNoFill();
	ofSetLineWidth(2);
	ofSetColor(COL_ACCENT_GOLD, 230);
	ofDrawRectRounded(cuePanelX, cuePanelY, cuePanelW, cuePanelH, 6);
	ofFill();

	ofSetColor(COL_ACCENT_GOLD);
	string displayName = previewBox->name;
	if (cueMonitorMode == CUE_MONITOR_SELECTED_BOX &&
		cueSelectedIndex() >= 0 && cueSelectedIndex() < getCueTargetBoxSize())
	{
		displayName = getCueTargetBoxAt(cueSelectedIndex())->name;
	}
	else if (isCueDraftMode() &&
			 cueState.draftOutputRealIndex >= 0 &&
			 cueState.draftOutputRealIndex < getCueTargetBoxSize())
	{
		displayName = getCueTargetBoxAt(cueState.draftOutputRealIndex)->name;
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
	ofSetColor(COL_ACCENT_GOLD_DIM, 230);
	ofDrawRectRounded(monitorX, iconY, iconSize, iconSize, 3);
	ofDrawRectRounded(applyX, iconY, iconSize, iconSize, 3);
	ofDrawRectRounded(fullX, iconY, iconSize, iconSize, 3);
	if (cueFullscreenPreview)
	{
		ofFill();
		ofSetColor(COL_ACCENT_GOLD, 210);
		ofDrawRectRounded(fullX + 1, iconY + 1, iconSize - 2, iconSize - 2, 3);
		ofNoFill();
	}
	ofColor monitorGlyphColor(COL_ACCENT_GOLD_DIM, 230);
	ofColor swapGlyphColor = cueFullscreenPreview ? ofColor(COL_BG_DARK, 245) : ofColor(COL_ACCENT_GOLD_DIM, 230);
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
	ofSetColor(COL_ACCENT_GOLD_DIM, 230);
	ofDrawLine(closeX + 4, iconY + 4, closeX + iconSize - 4, iconY + iconSize - 4);
	ofDrawLine(closeX + iconSize - 4, iconY + 4, closeX + 4, iconY + iconSize - 4);
	ofFill();
	jp_tooltip::draw("Monitor selected box or final output", monitorX, iconY, iconSize, iconSize);
	jp_tooltip::draw("Apply cue changes", applyX, iconY, iconSize, iconSize);
	jp_tooltip::draw("Toggle fullscreen preview", fullX, iconY, iconSize, iconSize);
	jp_tooltip::draw("Close cue preview", closeX, iconY, iconSize, iconSize);

	ofSetColor(255);
	ofEnableAlphaBlending();
	if (cueFullscreenPreview)
	{
		drawLiveOutput(previewX, previewY, previewW, previewH);
	}
	else
	{
		previewBox->fbo.draw(previewX, previewY, previewW, previewH);
	}
	ofSetColor(cueFullscreenPreview ? ofColor(COL_ACCENT_CYAN, 235) : ofColor(COL_ACCENT_GOLD, 235));
	ofDrawRectangle(previewX, previewY, previewW, 22);
	ofSetColor(COL_BG_TAB, 245);
	jp_constants::p_font.drawString(panelMode, previewX + 8, previewY + 16);

	ofSetColor(COL_ACCENT_GOLD, 220);
	ofDrawLine(cuePanelX + cuePanelW - handleSize, cuePanelY + cuePanelH - 4,
			   cuePanelX + cuePanelW - 4, cuePanelY + cuePanelH - handleSize);
	ofDrawLine(cuePanelX + cuePanelW - handleSize * 0.65f, cuePanelY + cuePanelH - 4,
			   cuePanelX + cuePanelW - 4, cuePanelY + cuePanelH - handleSize * 0.65f);
	jp_tooltip::draw("Resize cue preview", cuePanelX + cuePanelW - handleSize,
				cuePanelY + cuePanelH - handleSize, handleSize, handleSize);
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
	galleryDurationParam.nativeMin = 0.0f;
	galleryDurationParam.nativeMax = 4200.0f;
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
	drawLiveOutput(0, 0, ofGetWidth() * 2.0f, ofGetHeight() * 2.0f);
}
void JPboxgroup::draw_activerender(float _width, float _height)
{
	drawLiveOutput(0, 0, _width, _height);
}

bool JPboxgroup::drawLiveOutputSource(bool followMainActive,
	const string &sourceBoxUid, float _width, float _height,
	const ofRectangle &normCrop, float bezelCanvasPx,
	ofRectangle *outEffectiveNorm)
{
	// crop -> canvas pixels -> uniform bezel inset -> clamp, then back to
	// normalized. Done against the real texture size, so it is computed per
	// source below rather than once here.
	auto effectiveFor = [&](float texW, float texH) {
		ofRectangle rect(normCrop.x * texW, normCrop.y * texH,
			normCrop.width * texW, normCrop.height * texH);
		const float rawLeft = rect.x + bezelCanvasPx;
		const float rawTop = rect.y + bezelCanvasPx;
		const float rawRight = rect.getRight() - bezelCanvasPx;
		const float rawBottom = rect.getBottom() - bezelCanvasPx;
		// Clamp rather than skip: a one pixel stretched output is diagnosable,
		// a black window reads as a crash.
		const float left = ofClamp(rawLeft, 0.0f, std::max(0.0f, texW - 1.0f));
		const float top = ofClamp(rawTop, 0.0f, std::max(0.0f, texH - 1.0f));
		const float right = ofClamp(rawRight, left + 1.0f, texW);
		const float bottom = ofClamp(rawBottom, top + 1.0f, texH);
		rect.set(left, top, right - left, bottom - top);
		if (texW > 0.0f && texH > 0.0f && outEffectiveNorm != nullptr)
			outEffectiveNorm->set(rect.x / texW, rect.y / texH,
				rect.width / texW, rect.height / texH);
		return rect;
	};
	const bool cropped = !(normCrop.x <= 0.0f && normCrop.y <= 0.0f &&
		normCrop.width >= 1.0f && normCrop.height >= 1.0f) ||
		bezelCanvasPx != 0.0f;
	if (outEffectiveNorm != nullptr)
		outEffectiveNorm->set(0.0f, 0.0f, 1.0f, 1.0f);

	if (followMainActive)
	{
		if (boxes.empty() || activerender == nullptr ||
			*activerender < 0 || *activerender >= (int)boxes.size())
		{
			return false;
		}
		if (!cropped)
		{
			drawLiveOutput(0.0f, 0.0f, _width, _height);
			return true;
		}
		const bool transitioning = transition.getLerpValue() < 1.0f &&
			transition.isSourceAllocated();
		ofFbo &active = boxes[*activerender]->fbo;
		const float texW = transitioning ? transition.getSourceWidth() : active.getWidth();
		const float texH = transitioning ? transition.getSourceHeight() : active.getHeight();
		const ofRectangle rect = effectiveFor(texW, texH);
		drawLiveOutput(0.0f, 0.0f, _width, _height,
			ofRectangle(rect.x / texW, rect.y / texH,
				rect.width / texW, rect.height / texH));
		return true;
	}

	// By uid, and therefore across the whole tree: a box nested in a group is
	// a perfectly good output source and a name lookup could never reach one.
	JPbox *source = findBoxByUid(sourceBoxUid);
	if (source == nullptr || !source->fbo.isAllocated())
	{
		return false;
	}

	ofPushStyle();
	ofEnableAlphaBlending();
	ofSetColor(255);
	ofSetRectMode(OF_RECTMODE_CORNER);
	if (!cropped)
	{
		source->fbo.draw(0.0f, 0.0f, _width, _height);
		ofPopStyle();
		return true;
	}
	const ofRectangle rect = effectiveFor(source->fbo.getWidth(),
		source->fbo.getHeight());
	source->fbo.getTexture().drawSubsection(0.0f, 0.0f, _width, _height,
		rect.x, rect.y, rect.width, rect.height);
	ofPopStyle();
	return true;
}

bool JPboxgroup::MappingParameterIndices::valid() const
{
	return topLeftX >= 0 && topLeftY >= 0 &&
		topRightX >= 0 && topRightY >= 0 &&
		bottomRightX >= 0 && bottomRightY >= 0 &&
		bottomLeftX >= 0 && bottomLeftY >= 0 && feather >= 0;
}

JPboxgroup::MappingParameterIndices
JPboxgroup::getMappingParameterIndices(JPbox *box) const
{
	MappingParameterIndices indices;
	if (box == nullptr)
	{
		return indices;
	}

	for (int i = 0; i < box->parameters.getSize(); i++)
	{
		const string name = box->parameters.getName(i);
		if (name == "top_left_x") indices.topLeftX = i;
		else if (name == "top_left_y") indices.topLeftY = i;
		else if (name == "top_right_x") indices.topRightX = i;
		else if (name == "top_right_y") indices.topRightY = i;
		else if (name == "bottom_right_x") indices.bottomRightX = i;
		else if (name == "bottom_right_y") indices.bottomRightY = i;
		else if (name == "bottom_left_x") indices.bottomLeftX = i;
		else if (name == "bottom_left_y") indices.bottomLeftY = i;
		else if (name == "feather") indices.feather = i;
	}
	return indices;
}

bool JPboxgroup::isMappingShaderBox(JPbox *box) const
{
	if (box == nullptr || box->getTipo() != box->SHADERBOX)
	{
		return false;
	}

	if (isAdvancedMappingShaderBox(box))
	{
		return true;
	}

	// The mapping controls are useful before an input is connected, so the
	// parameter contract is the capability check. The textura1 link remains
	// optional until the box is wired in the graph.
	return getMappingParameterIndices(box).valid();
}

bool JPboxgroup::mappingTargetMatchesCurrentView() const
{
	return mappingTargetIndex == getCurrentViewSelectedIndex() &&
		mappingTargetGroupPath == activeGroupPath;
}

JPbox *JPboxgroup::getMappingEditBox()
{
	if (!mappingEditActive || !mappingTargetMatchesCurrentView())
	{
		return nullptr;
	}
	JPbox *box = getInspectorBox();
	return isMappingShaderBox(box) ? box : nullptr;
}

JPboxgroup::MappingQuad JPboxgroup::getMappingQuad(JPbox *box) const
{
	MappingQuad quad;
	const MappingParameterIndices indices = getMappingParameterIndices(box);
	if (!indices.valid())
	{
		return quad;
	}

	auto valueAt = [&](int index) {
		return box->parameters.getFloatValue(index);
	};
	quad.topLeft.set(valueAt(indices.topLeftX), valueAt(indices.topLeftY));
	quad.topRight.set(valueAt(indices.topRightX), valueAt(indices.topRightY));
	quad.bottomRight.set(valueAt(indices.bottomRightX), valueAt(indices.bottomRightY));
	quad.bottomLeft.set(valueAt(indices.bottomLeftX), valueAt(indices.bottomLeftY));
	return quad;
}

bool JPboxgroup::isValidMappingQuad(const MappingQuad &quad) const
{
	auto cross = [](const ofVec2f &a, const ofVec2f &b) {
		return a.x * b.y - a.y * b.x;
	};
	const float c0 = cross(quad.topRight - quad.topLeft,
		quad.bottomRight - quad.topRight);
	const float c1 = cross(quad.bottomRight - quad.topRight,
		quad.bottomLeft - quad.bottomRight);
	const float c2 = cross(quad.bottomLeft - quad.bottomRight,
		quad.topLeft - quad.bottomLeft);
	const float c3 = cross(quad.topLeft - quad.bottomLeft,
		quad.topRight - quad.topLeft);
	const float minCross = std::min(std::min(c0, c1), std::min(c2, c3));
	const float maxCross = std::max(std::max(c0, c1), std::max(c2, c3));
	return (minCross > 0.00001f || maxCross < -0.00001f) &&
		std::isfinite(minCross) && std::isfinite(maxCross);
}

ofVec2f JPboxgroup::projectMappingPoint(const MappingQuad &quad,
	const ofVec2f &uv) const
{
	// Use the same bilinear four-corner warp as mapping.frag. This keeps the
	// editor grid skewed to all four corners without introducing projective
	// depth into the preview.
	const float topWeight = 1.0f - uv.y;
	const float bottomWeight = uv.y;
	return quad.topLeft * (1.0f - uv.x) * topWeight +
		quad.topRight * uv.x * topWeight +
		quad.bottomRight * uv.x * bottomWeight +
		quad.bottomLeft * (1.0f - uv.x) * bottomWeight;
}

ofRectangle JPboxgroup::getMappingPreviewRect(float width, float height) const
{
	if (width <= 0.0f || height <= 0.0f)
	{
		return ofRectangle(0.0f, 0.0f, width, height);
	}

	const float outputAspect = jp_constants::renderHeight > 0 ?
		jp_constants::renderWidth / (float)jp_constants::renderHeight :
		width / height;
	float previewWidth = width;
	float previewHeight = previewWidth / outputAspect;
	if (previewHeight > height)
	{
		previewHeight = height;
		previewWidth = previewHeight * outputAspect;
	}
	return ofRectangle((width - previewWidth) * 0.5f,
		(height - previewHeight) * 0.5f, previewWidth, previewHeight);
}

void JPboxgroup::drawMappingGrid(const MappingQuad &quad,
	float x, float y, float width, float height)
{
	if (!mappingGridVisible || !isValidMappingQuad(quad))
	{
		return;
	}

	auto toScreen = [&](const ofVec2f &uv) {
		const ofVec2f point = projectMappingPoint(quad, uv);
		return ofVec2f(x + point.x * width, y + point.y * height);
	};
	auto drawClippedLine = [&](ofVec2f start, ofVec2f end) {
		// Clip each grid segment against the four edges of the crop. The
		// quad is validated as convex above, so half-plane clipping keeps
		// the overlay local even while corners are being dragged.
		const ofVec2f points[4] = {
			quad.topLeft, quad.topRight, quad.bottomRight, quad.bottomLeft};
		const ofVec2f center = (points[0] + points[1] +
			points[2] + points[3]) * 0.25f;
		float lower = 0.0f;
		float upper = 1.0f;
		const ofVec2f delta = end - start;
		for (int edgeIndex = 0; edgeIndex < 4; edgeIndex++)
		{
			const ofVec2f edgeStart = points[edgeIndex];
			const ofVec2f edgeEnd = points[(edgeIndex + 1) % 4];
			const ofVec2f edge = edgeEnd - edgeStart;
			const float orientation =
				(edge.x * (center.y - edgeStart.y) -
				 edge.y * (center.x - edgeStart.x)) >= 0.0f ? 1.0f : -1.0f;
			const float startSide = orientation *
				(edge.x * (start.y - edgeStart.y) -
				 edge.y * (start.x - edgeStart.x));
			const float deltaSide = orientation *
				(edge.x * delta.y - edge.y * delta.x);

			if (std::abs(deltaSide) <= 0.00001f)
			{
				if (startSide < 0.0f)
				{
					return;
				}
				continue;
			}

			const float crossing = -startSide / deltaSide;
			if (deltaSide > 0.0f)
			{
				lower = std::max(lower, crossing);
			}
			else
			{
				upper = std::min(upper, crossing);
			}
			if (lower > upper)
			{
				return;
			}
		}

		start += delta * lower;
		end = start + delta * (upper - lower);
		const ofVec2f screenStart(x + start.x * width,
			y + start.y * height);
		const ofVec2f screenEnd(x + end.x * width,
			y + end.y * height);
		ofDrawLine(screenStart, screenEnd);
	};

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofFill();
	ofSetColor(ofColor(COL_BG_DARK, 75));
	ofBeginShape();
	ofVertex(x + quad.topLeft.x * width, y + quad.topLeft.y * height);
	ofVertex(x + quad.topRight.x * width, y + quad.topRight.y * height);
	ofVertex(x + quad.bottomRight.x * width, y + quad.bottomRight.y * height);
	ofVertex(x + quad.bottomLeft.x * width, y + quad.bottomLeft.y * height);
	ofEndShape(true);

	ofNoFill();
	ofSetLineWidth(1.0f);
	for (int i = 1; i < 10; i++)
	{
		const float position = i / 10.0f;
		const ofVec2f horizontalStart = toScreen(ofVec2f(0.0f, position));
		const ofVec2f horizontalEnd = toScreen(ofVec2f(1.0f, position));
		const ofVec2f verticalStart = toScreen(ofVec2f(position, 0.0f));
		const ofVec2f verticalEnd = toScreen(ofVec2f(position, 1.0f));
		ofSetColor(i == 5 ? ofColor(COL_ACCENT_CYAN, 220) :
			ofColor(COL_TEXT_PRIMARY, 95));
		// Convert back to normalized preview coordinates for clipping. The
		// clipper keeps every bilinear grid segment inside the crop.
		drawClippedLine(ofVec2f((horizontalStart.x - x) / width,
			(horizontalStart.y - y) / height),
			ofVec2f((horizontalEnd.x - x) / width,
			(horizontalEnd.y - y) / height));
		drawClippedLine(ofVec2f((verticalStart.x - x) / width,
			(verticalStart.y - y) / height),
			ofVec2f((verticalEnd.x - x) / width,
			(verticalEnd.y - y) / height));
	}

	ofPopStyle();
}

void JPboxgroup::drawMappingHandles(const MappingQuad &quad,
	float x, float y, float width, float height, bool visible)
{
	if (!visible)
	{
		return;
	}

	const bool valid = isValidMappingQuad(quad);
	const ofVec2f points[4] = {
		quad.topLeft, quad.topRight, quad.bottomRight, quad.bottomLeft};

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofNoFill();
	ofSetLineWidth(2.0f);
	ofSetColor(valid ? COL_ACCENT_CYAN : COL_ACCENT_RED);
	for (int i = 0; i < 4; i++)
	{
		const ofVec2f &from = points[i];
		const ofVec2f &to = points[(i + 1) % 4];
		ofDrawLine(x + from.x * width, y + from.y * height,
			 x + to.x * width, y + to.y * height);
	}

	for (int i = 0; i < 4; i++)
	{
		const float screenX = x + points[i].x * width;
		const float screenY = y + points[i].y * height;
		const bool active = i == mappingDraggedCorner;
		ofSetColor(active ? COL_ACCENT_GOLD :
			(valid ? COL_ACCENT_CYAN : COL_ACCENT_RED));
		ofFill();
		ofDrawCircle(screenX, screenY, active ? 12.0f : 9.0f);
		ofSetColor(COL_BG_DARK);
		ofDrawCircle(screenX, screenY, active ? 5.0f : 3.5f);
	}
	ofPopStyle();
}

ofRectangle JPboxgroup::getMappingPanelPreviewRect() const
{
	const float padding = 10.0f;
	JPbox *box = const_cast<JPboxgroup *>(this)->getMappingEditBox();
	const float headerHeight = isAdvancedMappingShaderBox(box) ?
		60.0f : 30.0f;
	const float areaWidth = std::max(1.0f,
		mappingPanelW - padding * 2.0f);
	const float areaHeight = std::max(1.0f,
		mappingPanelH - headerHeight - padding * 2.0f);
	const ofRectangle local = getMappingPreviewRect(
		areaWidth, areaHeight);
	return ofRectangle(
		mappingPanelX + padding + local.x,
		mappingPanelY + headerHeight + padding + local.y,
		local.width, local.height);
}

void JPboxgroup::drawMappingPanel()
{
	if (!mappingEditActive)
	{
		return;
	}
	JPbox *box = getMappingEditBox();
	if (box == nullptr)
	{
		return;
	}

	if (isAdvancedMappingShaderBox(box))
	{
		drawAdvancedMappingPanel();
		return;
	}

	clampMappingPanelLayout();
	const float headerHeight = 30.0f;
	const float iconSize = 18.0f;
	const float padding = 10.0f;
	const float resizeHandleSize = 18.0f;
	const ofRectangle guidesBounds =
		getMappingPanelActionBounds(MAPPING_PANEL_GUIDES);
	const ofRectangle gridBounds =
		getMappingPanelActionBounds(MAPPING_PANEL_GRID);
	const ofRectangle renderGuidesBounds =
		getMappingPanelActionBounds(MAPPING_PANEL_RENDER_GUIDES);
	const ofRectangle closeBounds =
		getMappingPanelActionBounds(MAPPING_PANEL_CLOSE);
	const ofRectangle previewRect = getMappingPanelPreviewRect();

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(0, 105);
	ofDrawRectRounded(mappingPanelX + 3.0f, mappingPanelY + 4.0f,
		mappingPanelW, mappingPanelH, 6.0f);
	ofSetColor(COL_BG_TAB, 245);
	ofDrawRectRounded(mappingPanelX, mappingPanelY,
		mappingPanelW, mappingPanelH, 6.0f);
	ofSetColor(COL_BG_PANEL, 245);
	ofDrawRectRounded(mappingPanelX, mappingPanelY,
		mappingPanelW, headerHeight, 6.0f);
	ofNoFill();
	ofSetLineWidth(2.0f);
	ofSetColor(COL_ACCENT_CYAN, 225);
	ofDrawRectRounded(mappingPanelX, mappingPanelY,
		mappingPanelW, mappingPanelH, 6.0f);
	ofFill();

	string title = "MAP - " + box->name;
	const float maxTitleWidth = std::max(20.0f,
		guidesBounds.x - mappingPanelX - padding * 2.0f);
	while (!title.empty() &&
		jp_constants::p_font.stringWidth(title) > maxTitleWidth)
	{
		title.pop_back();
	}
	ofSetColor(COL_ACCENT_CYAN);
	jp_constants::p_font.drawString(
		title, mappingPanelX + padding, mappingPanelY + 21.0f);

	auto drawActionHover = [&](const ofRectangle &bounds, bool active) {
		if (bounds.inside(ofGetMouseX(), ofGetMouseY()))
		{
			ofSetColor(active ? ofColor(COL_ACCENT_CYAN, 75) :
				ofColor(COL_BG_HOVER, 230));
			ofDrawRectRounded(bounds, 3.0f);
		}
	};
	drawActionHover(guidesBounds, mappingGuidesVisible);
	drawActionHover(gridBounds, mappingGridVisible);
	drawActionHover(renderGuidesBounds, mappingRenderGuidesVisible);

	ofSetColor(mappingGuidesVisible ?
		COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	ofNoFill();
	ofSetLineWidth(1.4f);
	const float guideLeft = guidesBounds.getCenter().x - 6.0f;
	const float guideRight = guidesBounds.getCenter().x + 6.0f;
	const float guideTop = guidesBounds.getCenter().y - 5.0f;
	const float guideBottom = guidesBounds.getCenter().y + 5.0f;
	ofDrawLine(guideLeft, guideTop, guideLeft + 3.5f, guideTop);
	ofDrawLine(guideLeft, guideTop, guideLeft, guideTop + 3.5f);
	ofDrawLine(guideRight, guideTop, guideRight - 3.5f, guideTop);
	ofDrawLine(guideRight, guideTop, guideRight, guideTop + 3.5f);
	ofDrawLine(guideLeft, guideBottom, guideLeft + 3.5f, guideBottom);
	ofDrawLine(guideLeft, guideBottom, guideLeft, guideBottom - 3.5f);
	ofDrawLine(guideRight, guideBottom, guideRight - 3.5f, guideBottom);
	ofDrawLine(guideRight, guideBottom, guideRight, guideBottom - 3.5f);

	ofSetColor(mappingGridVisible ?
		COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	ofDrawRectRounded(gridBounds.getCenter().x - 6.0f,
		gridBounds.getCenter().y - 6.0f, 12.0f, 12.0f, 1.5f);
	ofDrawLine(gridBounds.getCenter().x,
		gridBounds.getCenter().y - 6.0f,
		gridBounds.getCenter().x,
		gridBounds.getCenter().y + 6.0f);
	ofDrawLine(gridBounds.getCenter().x - 6.0f,
		gridBounds.getCenter().y,
		gridBounds.getCenter().x + 6.0f,
		gridBounds.getCenter().y);

	ofSetColor(mappingRenderGuidesVisible ?
		COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	ofDrawRectRounded(renderGuidesBounds.getCenter().x - 6.0f,
		renderGuidesBounds.getCenter().y - 4.5f,
		12.0f, 9.0f, 1.5f);
	ofDrawLine(renderGuidesBounds.getCenter().x - 2.5f,
		renderGuidesBounds.getCenter().y + 6.0f,
		renderGuidesBounds.getCenter().x + 2.5f,
		renderGuidesBounds.getCenter().y + 6.0f);
	ofDrawLine(renderGuidesBounds.getCenter().x,
		renderGuidesBounds.getCenter().y + 4.5f,
		renderGuidesBounds.getCenter().x,
		renderGuidesBounds.getCenter().y + 6.0f);
	ofFill();

	const bool closeHovered = closeBounds.inside(
		ofGetMouseX(), ofGetMouseY());
	if (closeHovered)
	{
		ofSetColor(ofColor(COL_ACCENT_RED, 145));
		ofDrawRectRounded(closeBounds, 3.0f);
	}
	ofSetColor(closeHovered ? COL_ACCENT_RED : COL_TEXT_SECONDARY);
	ofSetLineWidth(1.5f);
	ofDrawLine(closeBounds.x + 5.0f, closeBounds.y + 5.0f,
		closeBounds.x + iconSize - 5.0f,
		closeBounds.y + iconSize - 5.0f);
	ofDrawLine(closeBounds.x + iconSize - 5.0f,
		closeBounds.y + 5.0f,
		closeBounds.x + 5.0f,
		closeBounds.y + iconSize - 5.0f);

	ofSetColor(COL_BG_DARK);
	ofDrawRectangle(previewRect);
	ofSetColor(255, 255);
	box->fbo.draw(previewRect.x, previewRect.y,
		previewRect.width, previewRect.height);
	const MappingQuad quad = getMappingQuad(box);
	drawMappingGrid(quad, previewRect.x, previewRect.y,
		previewRect.width, previewRect.height);
	drawMappingHandles(quad, previewRect.x, previewRect.y,
		previewRect.width, previewRect.height, mappingGuidesVisible);

	ofSetColor(COL_ACCENT_CYAN, 210);
	ofSetLineWidth(1.2f);
	ofDrawLine(
		mappingPanelX + mappingPanelW - resizeHandleSize,
		mappingPanelY + mappingPanelH - 4.0f,
		mappingPanelX + mappingPanelW - 4.0f,
		mappingPanelY + mappingPanelH - resizeHandleSize);
	ofDrawLine(
		mappingPanelX + mappingPanelW -
			resizeHandleSize * 0.65f,
		mappingPanelY + mappingPanelH - 4.0f,
		mappingPanelX + mappingPanelW - 4.0f,
		mappingPanelY + mappingPanelH -
			resizeHandleSize * 0.65f);
	jp_tooltip::draw("Toggle mapping borders and corners",
		guidesBounds.x, guidesBounds.y,
		guidesBounds.width, guidesBounds.height);
	jp_tooltip::draw("Toggle mapping calibration grid",
		gridBounds.x, gridBounds.y,
		gridBounds.width, gridBounds.height);
	jp_tooltip::draw(
		mappingRenderGuidesVisible ?
			"Hide mapping borders in render window" :
			"Show mapping borders in render window",
		renderGuidesBounds.x, renderGuidesBounds.y,
		renderGuidesBounds.width, renderGuidesBounds.height);
	jp_tooltip::draw("Close mapping editor",
		closeBounds.x, closeBounds.y,
		closeBounds.width, closeBounds.height);
	jp_tooltip::draw("Resize mapping editor",
		mappingPanelX + mappingPanelW - resizeHandleSize,
		mappingPanelY + mappingPanelH - resizeHandleSize,
		resizeHandleSize, resizeHandleSize);
	ofPopStyle();
}

void JPboxgroup::drawMappingOverlay(float _x, float _y,
	float _width, float _height)
{
	if (!mappingRenderGuidesVisible)
	{
		return;
	}
	JPbox *box = getMappingEditBox();
	if (box == nullptr)
	{
		return;
	}

	if (isAdvancedMappingShaderBox(box))
	{
		drawAdvancedMappingOverlay(_x, _y, _width, _height, true);
		return;
	}

	const MappingQuad quad = getMappingQuad(box);
	drawMappingHandles(quad, _x, _y,
		_width, _height, true);
}

void JPboxgroup::drawMappingOverlayForSource(bool followMainActive,
	const string &sourceBoxUid, float _x, float _y,
	float _width, float _height)
{
	if (!mappingEditActive || mappingTargetIndex < 0)
	{
		return;
	}

	const int topLevelIndex = mappingTargetGroupPath.empty() ?
		mappingTargetIndex : mappingTargetGroupPath.front();
	if (topLevelIndex < 0 || topLevelIndex >= (int)boxes.size())
	{
		return;
	}

	// Matches the mapping target's TOP-LEVEL ancestor against the bound source,
	// as before - just by identity now. A source nested inside a group does not
	// draw this overlay; it also could not be a source at all until now, so
	// nothing regresses.
	JPbox *sourceBox = findBoxByUid(sourceBoxUid);
	const bool sourceMatches = followMainActive ?
		(activerender != nullptr && *activerender == topLevelIndex) :
		(sourceBox != nullptr && sourceBox == boxes[topLevelIndex]);
	if (sourceMatches)
	{
		drawMappingOverlay(_x, _y, _width, _height);
	}
}

bool JPboxgroup::isMappingEditActive() const
{
	return mappingEditActive;
}

bool JPboxgroup::toggleMappingEdit()
{
	if (mappingEditActive)
	{
		endMappingEdit();
		return true;
	}

	JPbox *box = getInspectorBox();
	if (!isMappingShaderBox(box))
	{
		return false;
	}

	mappingTargetIndex = getCurrentViewSelectedIndex();
	mappingTargetGroupPath = activeGroupPath;
	mappingDraggedCorner = -1;
	mappingGuidesVisible = true;
	mappingGridVisible = false;
	mappingRenderGuidesVisible = false;
	mappingEditActive = true;
	advancedMappingViewZoom = 1.0f;
	advancedMappingViewCenter.set(0.5f, 0.5f);
	advancedMappingViewPanning = false;
	advancedMappingRightPanPending = false;
	advancedMappingSelectedMaskContour = -1;
	advancedMappingSelectedMaskNode = -1;
	advancedMappingSelectedMaskContours.clear();
	if (JPbox_shader *advanced = getAdvancedMappingEditBox())
	{
		if (auto *state = advanced->getAdvancedMappingState())
		{
			const auto &masks = state->layers[state->selectedLayer].masks;
			advancedMappingSelectedMaskContour = masks.empty() ? -1 : 0;
		}
	}
	clampMappingPanelLayout();
	return true;
}

bool JPboxgroup::toggleMappingGrid()
{
	if (!mappingEditActive || getMappingEditBox() == nullptr)
	{
		return false;
	}
	mappingGridVisible = !mappingGridVisible;
	return true;
}

bool JPboxgroup::toggleMappingGuides()
{
	if (!mappingEditActive || getMappingEditBox() == nullptr)
	{
		return false;
	}
	mappingGuidesVisible = !mappingGuidesVisible;
	return true;
}

bool JPboxgroup::toggleMappingRenderGuides()
{
	if (!mappingEditActive || getMappingEditBox() == nullptr)
	{
		return false;
	}
	mappingRenderGuidesVisible = !mappingRenderGuidesVisible;
	return true;
}

void JPboxgroup::endMappingEdit()
{
	mappingEditActive = false;
	mappingGuidesVisible = true;
	mappingGridVisible = false;
	mappingRenderGuidesVisible = false;
	mappingTargetIndex = -1;
	mappingTargetGroupPath.clear();
	mappingDraggedCorner = -1;
	mappingPanelDragging = false;
	mappingPanelResizing = false;
	mappingPanelPointerCaptured = false;
	advancedMappingDragKind = ADVANCED_MAPPING_DRAG_NONE;
	advancedMappingDragIndex = -1;
	advancedMappingSelectedMaskContour = -1;
	advancedMappingSelectedMaskNode = -1;
	advancedMappingSelectedMaskContours.clear();
	advancedMappingDragLayer = -1;
	advancedMappingDragContour = -1;
	advancedMappingDragContours.clear();
	advancedMappingDragSnapshot.masks.clear();
	advancedMappingViewPanning = false;
	advancedMappingRightPanPending = false;
	advancedMappingPendingDeleteContour = -1;
	advancedMappingPendingDeleteNode = -1;
}

void JPboxgroup::markMappingParameterChanged()
{
	markCueDraftDirty(cueSelectedIndex(), CUE_DIRTY_PARAMS);
}

bool JPboxgroup::updateMappingCorner(int corner, float x, float y,
	float width, float height)
{
	JPbox *box = getMappingEditBox();
	if (box == nullptr || width <= 0.0f || height <= 0.0f)
	{
		return false;
	}

	const MappingParameterIndices indices = getMappingParameterIndices(box);
	const int xIndices[4] = {indices.topLeftX, indices.topRightX,
		indices.bottomRightX, indices.bottomLeftX};
	const int yIndices[4] = {indices.topLeftY, indices.topRightY,
		indices.bottomRightY, indices.bottomLeftY};
	if (corner < 0 || corner >= 4 || xIndices[corner] < 0 || yIndices[corner] < 0)
	{
		return false;
	}

	const float nextX = ofClamp(x / width, 0.0f, 1.0f);
	const float nextY = ofClamp(y / height, 0.0f, 1.0f);
	if (std::abs(box->parameters.getFloatValue(xIndices[corner]) - nextX) < 0.0001f &&
		std::abs(box->parameters.getFloatValue(yIndices[corner]) - nextY) < 0.0001f)
	{
		return true;
	}

	const bool wasAnimated =
		box->parameters.getMovType(xIndices[corner]) != JPParameter::STANDART ||
		box->parameters.getMovType(yIndices[corner]) != JPParameter::STANDART;
	box->parameters.setFloatValue(nextX, xIndices[corner]);
	box->parameters.setFloatLerpValue(nextX, xIndices[corner]);
	box->parameters.setFloatValue(nextY, yIndices[corner]);
	box->parameters.setFloatLerpValue(nextY, yIndices[corner]);
	box->parameters.setmovetype(JPParameter::STANDART, xIndices[corner]);
	box->parameters.setmovetype(JPParameter::STANDART, yIndices[corner]);
	if (wasAnimated)
	{
		setControllers();
	}
	markMappingParameterChanged();
	return true;
}

void JPboxgroup::setupDefaultMappingPanelLayout()
{
	const float margin = 24.0f;
	const float headerHeight = 30.0f;
	const float minimumWidth = 320.0f;
	mappingPanelW = std::max(minimumWidth,
		std::min(620.0f, ofGetWidth() * 0.46f));
	mappingPanelH = headerHeight + 20.0f +
		(mappingPanelW - 20.0f) * 9.0f / 16.0f;
	mappingPanelX = margin;
	mappingPanelY = tabBarOffsetY + 48.0f;
	clampMappingPanelLayout();
}

void JPboxgroup::clampMappingPanelLayout()
{
	const float margin = 8.0f;
	const float topMargin = tabBarOffsetY + 40.0f;
	JPbox *box = getMappingEditBox();
	const float minimumWidth = isAdvancedMappingShaderBox(box) ?
		420.0f : 320.0f;
	const float minimumHeight = 220.0f;
	const float maximumWidth = std::max(
		minimumWidth, ofGetWidth() - margin * 2.0f);
	const float maximumHeight = std::max(
		minimumHeight, ofGetHeight() - topMargin - margin);
	mappingPanelW = ofClamp(
		mappingPanelW, minimumWidth, maximumWidth);
	mappingPanelH = ofClamp(
		mappingPanelH, minimumHeight, maximumHeight);
	mappingPanelX = ofClamp(mappingPanelX, margin,
		std::max(margin, ofGetWidth() - mappingPanelW - margin));
	mappingPanelY = ofClamp(mappingPanelY, topMargin,
		std::max(topMargin, ofGetHeight() - mappingPanelH - margin));
	if (isAdvancedMappingShaderBox(box)) clampAdvancedMappingView();
}

bool JPboxgroup::mouseOverMappingPanel() const
{
	return mappingEditActive &&
		ofGetMouseX() >= mappingPanelX &&
		ofGetMouseX() <= mappingPanelX + mappingPanelW &&
		ofGetMouseY() >= mappingPanelY &&
		ofGetMouseY() <= mappingPanelY + mappingPanelH;
}

ofRectangle JPboxgroup::getMappingPanelActionBounds(
	MappingPanelAction action) const
{
	const float padding = 10.0f;
	const float headerHeight = 30.0f;
	const float iconSize = 18.0f;
	const float iconGap = 5.0f;
	const int fromRight = MAPPING_PANEL_CLOSE - action;
	const float x = mappingPanelX + mappingPanelW -
		padding - iconSize - fromRight * (iconSize + iconGap);
	const float y = mappingPanelY +
		(headerHeight - iconSize) * 0.5f;
	return ofRectangle(x, y, iconSize, iconSize);
}

bool JPboxgroup::mouseOverMappingPanelHeader() const
{
	const float headerHeight = 30.0f;
	return mouseOverMappingPanel() &&
		ofGetMouseY() <= mappingPanelY + headerHeight;
}

bool JPboxgroup::mouseOverMappingPanelResizeHandle() const
{
	const float handleSize = 24.0f;
	return mappingEditActive &&
		ofGetMouseX() >= mappingPanelX + mappingPanelW - handleSize &&
		ofGetMouseX() <= mappingPanelX + mappingPanelW &&
		ofGetMouseY() >= mappingPanelY + mappingPanelH - handleSize &&
		ofGetMouseY() <= mappingPanelY + mappingPanelH;
}

bool JPboxgroup::mouseOverMappingPanelCloseIcon() const
{
	return mappingEditActive &&
		getMappingPanelActionBounds(MAPPING_PANEL_CLOSE).inside(
			ofGetMouseX(), ofGetMouseY());
}

bool JPboxgroup::update_mappingMousePressed(int mouseButton)
{
	if (!mappingEditActive || !mouseOverMappingPanel())
	{
		return false;
	}
	if (getAdvancedMappingEditBox() != nullptr)
	{
		return updateAdvancedMappingMousePressed(mouseButton);
	}
	if (mouseButton != OF_MOUSE_BUTTON_LEFT)
	{
		return true;
	}

	mappingPanelPointerCaptured = true;
	mappingDraggedCorner = -1;
	if (mouseOverMappingPanelCloseIcon())
	{
		endMappingEdit();
		return true;
	}
	if (getMappingPanelActionBounds(MAPPING_PANEL_GUIDES).inside(
		ofGetMouseX(), ofGetMouseY()))
	{
		toggleMappingGuides();
		return true;
	}
	if (getMappingPanelActionBounds(MAPPING_PANEL_GRID).inside(
		ofGetMouseX(), ofGetMouseY()))
	{
		toggleMappingGrid();
		return true;
	}
	if (getMappingPanelActionBounds(
		MAPPING_PANEL_RENDER_GUIDES).inside(
			ofGetMouseX(), ofGetMouseY()))
	{
		toggleMappingRenderGuides();
		return true;
	}
	if (mouseOverMappingPanelResizeHandle())
	{
		mappingPanelResizing = true;
		mappingPanelDragging = false;
		mappingPanelDragStartMouse.set(
			ofGetMouseX(), ofGetMouseY());
		mappingPanelResizeStartSize.set(
			mappingPanelW, mappingPanelH);
		return true;
	}
	if (mouseOverMappingPanelHeader())
	{
		mappingPanelDragging = true;
		mappingPanelResizing = false;
		mappingPanelDragStartMouse.set(
			ofGetMouseX(), ofGetMouseY());
		mappingPanelDragStartPos.set(
			mappingPanelX, mappingPanelY);
		return true;
	}

	const ofRectangle previewRect = getMappingPanelPreviewRect();
	if (mappingGuidesVisible &&
		previewRect.inside(ofGetMouseX(), ofGetMouseY()))
	{
		JPbox *box = getMappingEditBox();
		const MappingQuad quad = getMappingQuad(box);
		const ofVec2f points[4] = {
			quad.topLeft, quad.topRight,
			quad.bottomRight, quad.bottomLeft};
		float closestDistance = 18.0f;
		for (int i = 0; i < 4; i++)
		{
			const ofVec2f handle(
				previewRect.x + points[i].x * previewRect.width,
				previewRect.y + points[i].y * previewRect.height);
			const float distance = handle.distance(
				ofVec2f(ofGetMouseX(), ofGetMouseY()));
			if (distance <= closestDistance)
			{
				closestDistance = distance;
				mappingDraggedCorner = i;
			}
		}
	}
	return true;
}

bool JPboxgroup::update_mappingMouseDragged(int mouseButton)
{
	if (getAdvancedMappingEditBox() != nullptr)
	{
		return updateAdvancedMappingMouseDragged(mouseButton);
	}
	if (mouseButton != OF_MOUSE_BUTTON_LEFT ||
		!mappingPanelPointerCaptured)
	{
		return false;
	}

	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	const ofVec2f delta = mouse - mappingPanelDragStartMouse;
	if (mappingPanelDragging)
	{
		mappingPanelX = mappingPanelDragStartPos.x + delta.x;
		mappingPanelY = mappingPanelDragStartPos.y + delta.y;
		clampMappingPanelLayout();
		return true;
	}
	if (mappingPanelResizing)
	{
		mappingPanelW = mappingPanelResizeStartSize.x + delta.x;
		mappingPanelH = mappingPanelResizeStartSize.y + delta.y;
		clampMappingPanelLayout();
		return true;
	}
	if (mappingDraggedCorner >= 0)
	{
		const ofRectangle previewRect = getMappingPanelPreviewRect();
		updateMappingCorner(mappingDraggedCorner,
			mouse.x - previewRect.x, mouse.y - previewRect.y,
			previewRect.width, previewRect.height);
	}
	return true;
}

bool JPboxgroup::update_mappingMouseReleased(int mouseButton)
{
	if (getAdvancedMappingEditBox() != nullptr)
	{
		return updateAdvancedMappingMouseReleased(mouseButton);
	}
	if (mouseButton != OF_MOUSE_BUTTON_LEFT ||
		!mappingPanelPointerCaptured)
	{
		return false;
	}
	mappingDraggedCorner = -1;
	mappingPanelDragging = false;
	mappingPanelResizing = false;
	mappingPanelPointerCaptured = false;
	clampMappingPanelLayout();
	return true;
}

void JPboxgroup::drawNodeEditorBackground(float _width, float _height)
{
	JPbox *previewBox = getCuePreviewBox();
	if (cueFullscreenPreview && previewBox != nullptr)
	{
		ofPushStyle();
		ofEnableAlphaBlending();
		ofSetColor(255, 255);
		ofSetRectMode(OF_RECTMODE_CORNER);
		previewBox->fbo.draw(0, 0, _width, _height);
		ofPopStyle();
		return;
	}
	drawLiveOutput(0, 0, _width, _height);
}

void JPboxgroup::drawLiveOutput(float x, float y, float w, float h,
	const ofRectangle &srcNorm)
{
	if (boxes.empty() || activerender == nullptr || *activerender < 0 ||
		*activerender >= (int)boxes.size())
	{
		return;
	}

	// GUIPPER_RENDER_TRACE=1 reports what the active-render draw is actually
	// given, once a second.
	//
	// The node background is drawn at ofGetWidth()/ofGetHeight() with
	// OF_RECTMODE_CORNER, so on paper it always fills. When it does not, the
	// lie is in one of the values below - the size asked for, the viewport it
	// lands in, or the FBO behind it - and which one it is cannot be guessed
	// from a screenshot.
	if (std::getenv("GUIPPER_RENDER_TRACE"))
	{
		static uint64_t nextTrace = 0;
		const uint64_t now = ofGetElapsedTimeMillis();
		if (now >= nextTrace)
		{
			nextTrace = now + 1000;
			GLint vp[4] = {0, 0, 0, 0};
			glGetIntegerv(GL_VIEWPORT, vp);
			const ofRectangle current = ofGetCurrentViewport();
			ofFbo &traced = boxes[*activerender]->fbo;
			ofLogNotice("rendertrace")
				<< "frame=" << ofGetFrameNum()
				<< " asked=" << w << "x" << h
				<< " ofGet=" << ofGetWidth() << "x" << ofGetHeight()
				<< " glViewport=" << vp[0] << "," << vp[1] << " "
				<< vp[2] << "x" << vp[3]
				<< " ofViewport=" << current.x << "," << current.y << " "
				<< current.width << "x" << current.height
				<< " fbo=" << traced.getWidth() << "x" << traced.getHeight()
				<< " alloc=" << traced.isAllocated()
				<< " activerender=" << *activerender
				<< " boxes=" << (int)boxes.size()
				<< " transitionSrc=" << transition.isSourceAllocated()
				<< " lerp=" << transition.getLerpValue();
		}
	}
	const bool whole = srcNorm.x <= 0.0f && srcNorm.y <= 0.0f &&
		srcNorm.width >= 1.0f && srcNorm.height >= 1.0f;
	const bool transitioning = transition.getLerpValue() < 1.0f &&
		transition.isSourceAllocated();
	ofFbo &active = boxes[*activerender]->fbo;
	const float sourceW = transitioning ? transition.getSourceWidth() : active.getWidth();
	const float sourceH = transitioning ? transition.getSourceHeight() : active.getHeight();
	const ofRectangle subsection(srcNorm.x * sourceW, srcNorm.y * sourceH,
		std::max(1.0f, srcNorm.width * sourceW),
		std::max(1.0f, srcNorm.height * sourceH));

	ofPushStyle();
	ofEnableAlphaBlending();
	ofSetColor(255);
	ofSetRectMode(OF_RECTMODE_CORNER);
	if (transitioning)
	{
		if (whole) transition.draw(x, y, w, h);
		else transition.drawSubsection(x, y, w, h, subsection.x,
			subsection.y, subsection.width, subsection.height);
	}
	else if (active.isAllocated())
	{
		if (whole) active.draw(x, y, w, h);
		else active.getTexture().drawSubsection(x, y, w, h, subsection.x,
			subsection.y, subsection.width, subsection.height);
	}
	ofPopStyle();
}

float JPboxgroup::layoutInspectorInputRows(JPbox *box, float startY)
{
	inspectorInputRows.clear();
	inspectorInputsHeaderBounds.set(0, 0, 0, 0);
	if (box == nullptr)
	{
		return startY;
	}
	const int inputCount = box->fbohandlergroup.getSize();
	const bool showSingleInput =
		inputCount > 0 &&
		(isGroupViewActive() ||
		 box->getTipo() == JPbox::PRESETBOX);
	if (inputCount < 2 && !showSingleInput)
	{
		return startY;
	}

	const float panelLeft = inspectorBodyViewport.x;
	const float panelInset = inspectorLayout.contentPadding;
	const float headerHeight = inspectorLayout.minControlHeight;
	const float rowHeight = inspectorLayout.minControlHeight;
	const float arrowSize = 18.0f;
	const float exposeSize = 18.0f;
	const float unlinkSize = 18.0f;
	const float sectionGap = inspectorLayout.rowGap;
	inspectorInputsHeaderBounds.set(
		panelLeft + panelInset,
		startY,
		inspectorBodyViewport.width - panelInset * 2.0f,
		headerHeight);
	if (!inspectorInputsExpanded)
	{
		return inspectorInputsHeaderBounds.getBottom() + sectionGap;
	}

	float rowY = inspectorInputsHeaderBounds.getBottom() + 2.0f;

	for (int linkIndex = 0; linkIndex < box->fbohandlergroup.getSize(); linkIndex++)
	{
		InspectorInputRow row;
		row.linkIndex = linkIndex;
		row.bounds.set(panelLeft + panelInset + 2.0f, rowY,
			inspectorBodyViewport.width - (panelInset + 2.0f) * 2.0f,
			rowHeight);
		row.upButton.set(row.bounds.x + 3.0f,
			row.bounds.y + (rowHeight - arrowSize) / 2.0f,
			arrowSize, arrowSize);
		row.unlinkButton.set(row.bounds.getRight() - 3.0f - unlinkSize,
			row.bounds.y + (rowHeight - unlinkSize) / 2.0f,
			unlinkSize, unlinkSize);
		row.exposeButton.set(
			row.unlinkButton.x - 3.0f - exposeSize,
			row.bounds.y + (rowHeight - exposeSize) / 2.0f,
			exposeSize, exposeSize);
		inspectorInputRows.push_back(row);
		rowY += rowHeight + 2.0f;
	}

	return rowY + sectionGap;
}

void JPboxgroup::drawInspectorInputRows(JPbox *box)
{
	if (box == nullptr || inspectorInputsHeaderBounds.width <= 0.0f)
	{
		return;
	}

	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	const bool headerHovered = inspectorInputsHeaderBounds.inside(
		ofGetMouseX(), ofGetMouseY());
	if (headerHovered)
	{
		ofSetColor(ofColor(COL_BG_HOVER, 155));
		ofDrawRectRounded(inspectorInputsHeaderBounds, 3.0f);
	}

	const float headerCenterY = inspectorInputsHeaderBounds.getCenter().y;
	const float chevronX = inspectorInputsHeaderBounds.x + 11.0f;
	ofSetColor(headerHovered || inspectorInputsExpanded ?
		COL_ACCENT_CYAN : COL_TEXT_SECONDARY);
	ofSetLineWidth(1.5f);
	if (inspectorInputsExpanded)
	{
		ofDrawLine(chevronX - 4.0f, headerCenterY - 2.0f,
			chevronX, headerCenterY + 2.0f);
		ofDrawLine(chevronX, headerCenterY + 2.0f,
			chevronX + 4.0f, headerCenterY - 2.0f);
	}
	else
	{
		ofDrawLine(chevronX - 2.0f, headerCenterY - 4.0f,
			chevronX + 2.0f, headerCenterY);
		ofDrawLine(chevronX + 2.0f, headerCenterY,
			chevronX - 2.0f, headerCenterY + 4.0f);
	}
	ofSetLineWidth(1.0f);

	const float headerTextY = headerCenterY + 4.0f;
	ofSetColor(headerHovered ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
	jp_constants::p_font.drawString("INPUTS",
		inspectorInputsHeaderBounds.x + 25.0f, headerTextY);

	int linkedCount = 0;
	for (int linkIndex = 0;
		 linkIndex < box->fbohandlergroup.getSize();
		 linkIndex++)
	{
		if (box->fbohandlergroup.getisPointerSet(linkIndex))
		{
			linkedCount++;
		}
	}
	const string countLabel = ofToString(linkedCount) + "/" +
		ofToString(box->fbohandlergroup.getSize()) + " linked";
	ofSetColor(linkedCount > 0 ? COL_ACCENT_CYAN : COL_TEXT_MUTED);
	jp_constants::p_font.drawString(
		countLabel,
		inspectorInputsHeaderBounds.getRight() -
			jp_constants::p_font.stringWidth(countLabel) - 8.0f,
		headerTextY);
	ofSetColor(ofColor(COL_BORDER_MUTED, 115));
	ofDrawLine(
		inspectorInputsHeaderBounds.x + 2.0f,
		inspectorInputsHeaderBounds.getBottom(),
		inspectorInputsHeaderBounds.getRight() - 2.0f,
		inspectorInputsHeaderBounds.getBottom());
	drawInspectorClickBounds(inspectorInputsHeaderBounds);

	for (const InspectorInputRow &row : inspectorInputRows)
	{
		const bool rowHovered = row.bounds.inside(ofGetMouseX(), ofGetMouseY());
		const bool exposed =
			isInspectorTextureInputExposed(box, row.linkIndex);
		const bool previousExposed =
			row.linkIndex > 0 &&
			isInspectorTextureInputExposed(
				box, row.linkIndex - 1);
		const bool canMoveUp =
			row.linkIndex > 0 &&
			!exposed && !previousExposed;
		const bool canUnlink =
			!exposed &&
			box->fbohandlergroup.getisPointerSet(row.linkIndex);
		const bool showExpose = isGroupViewActive();
		const bool canExpose =
			showExpose &&
			(exposed ||
			 !box->fbohandlergroup.getisPointerSet(
				row.linkIndex));
		const bool arrowHovered = canMoveUp &&
			row.upButton.inside(ofGetMouseX(), ofGetMouseY());
		const bool exposeHovered = canExpose &&
			row.exposeButton.inside(
				ofGetMouseX(), ofGetMouseY());
		const bool unlinkHovered = canUnlink &&
			row.unlinkButton.inside(ofGetMouseX(), ofGetMouseY());
		if (rowHovered)
		{
			ofSetColor(ofColor(COL_BG_HOVER,
				(arrowHovered || exposeHovered ||
				 unlinkHovered) ? 205 : 115));
			ofDrawRectRounded(row.bounds, 3.0f);
		}

		if (arrowHovered)
		{
			ofSetColor(ofColor(COL_ACCENT_CYAN_DARK, 205));
			ofDrawRectRounded(row.upButton, 3.0f);
		}
		if (unlinkHovered)
		{
			ofSetColor(ofColor(COL_ACCENT_RED, 165));
			ofDrawRectRounded(row.unlinkButton, 3.0f);
		}
		if (exposeHovered || exposed)
		{
			ofSetColor(exposed ?
				ofColor(COL_ACCENT_CYAN_DARK, 220) :
				ofColor(COL_BG_HOVER, 220));
			ofDrawRectRounded(row.exposeButton, 3.0f);
		}

		const float arrowCx = row.upButton.getCenter().x;
		const float arrowCy = row.upButton.getCenter().y;
		ofSetColor(canMoveUp ?
			(arrowHovered ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY) :
			ofColor(COL_TEXT_MUTED, 65));
		ofSetLineWidth(1.5f);
		ofDrawLine(arrowCx, arrowCy + 5.0f, arrowCx, arrowCy - 4.0f);
		ofDrawLine(arrowCx, arrowCy - 4.0f, arrowCx - 4.0f, arrowCy);
		ofDrawLine(arrowCx, arrowCy - 4.0f, arrowCx + 4.0f, arrowCy);
		ofSetLineWidth(1.0f);

		const float sourceAreaWidth = std::min(165.0f, row.bounds.width * 0.42f);
		const float sourceRight =
			(showExpose ? row.exposeButton.x :
			 row.unlinkButton.x) - 7.0f;
		string sourceName = box->fbohandlergroup.getisPointerSet(row.linkIndex) ?
			box->fbohandlergroup.getFboName(row.linkIndex) : "Not connected";
		sourceName = fitInspectorLabel(sourceName, sourceAreaWidth);
		const float sourceX = sourceRight - jp_constants::p_font.stringWidth(sourceName);

		const float samplerX = row.upButton.getRight() + 7.0f;
		const float samplerMaxWidth = std::max(10.0f, sourceX - samplerX - 12.0f);
		string fullSamplerName =
			box->fbohandlergroup.getName(row.linkIndex);
		JPbox_preset *inputPreset =
			dynamic_cast<JPbox_preset *>(box);
		if (inputPreset != nullptr)
		{
			const string targetLabel =
				inputPreset->getExposedTextureInputTargetLabel(
					fullSamplerName);
			if (!targetLabel.empty())
			{
				fullSamplerName += " > " + targetLabel;
			}
		}
		const string samplerName = fitInspectorLabel(
			fullSamplerName, samplerMaxWidth);
		const float textY = row.bounds.y + row.bounds.height / 2.0f + 4.0f;
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::p_font.drawString(samplerName, samplerX, textY);
		ofSetColor(box->fbohandlergroup.getisPointerSet(row.linkIndex) ?
			COL_ACCENT_CYAN : COL_TEXT_MUTED);
		jp_constants::p_font.drawString(sourceName, sourceX, textY);

		ofSetColor(ofColor(COL_BORDER_MUTED, 75));
		ofDrawLine(row.bounds.x + 4.0f, row.bounds.getBottom(),
			row.bounds.getRight() - 4.0f, row.bounds.getBottom());

		if (canMoveUp)
		{
			jp_tooltip::draw(
				"Swap with " + box->fbohandlergroup.getName(row.linkIndex - 1),
				row.upButton.x, row.upButton.y,
				row.upButton.width, row.upButton.height);
		}

		if (showExpose)
		{
			const float exposeCx =
				row.exposeButton.getCenter().x;
			const float exposeCy =
				row.exposeButton.getCenter().y;
			ofSetColor(exposed ?
				COL_ACCENT_CYAN :
				(canExpose ? COL_TEXT_SECONDARY :
				 ofColor(COL_TEXT_MUTED, 50)));
			ofSetLineWidth(1.4f);
			ofDrawLine(exposeCx - 5.0f, exposeCy + 4.0f,
				exposeCx - 5.0f, exposeCy - 4.0f);
			ofDrawLine(exposeCx - 5.0f, exposeCy - 4.0f,
				exposeCx - 1.0f, exposeCy - 4.0f);
			ofDrawLine(exposeCx - 1.0f, exposeCy + 3.0f,
				exposeCx + 5.0f, exposeCy - 3.0f);
			ofDrawLine(exposeCx + 1.0f, exposeCy - 3.0f,
				exposeCx + 5.0f, exposeCy - 3.0f);
			ofDrawLine(exposeCx + 5.0f, exposeCy - 3.0f,
				exposeCx + 5.0f, exposeCy + 1.0f);
			ofSetLineWidth(1.0f);
			jp_tooltip::draw(
				exposed ? "Hide input from parent" :
					(canExpose ? "Expose input to parent" :
					 "Unlink texture before exposing"),
				row.exposeButton.x, row.exposeButton.y,
				row.exposeButton.width,
				row.exposeButton.height);
		}

		const float unlinkCx = row.unlinkButton.getCenter().x;
		const float unlinkCy = row.unlinkButton.getCenter().y;
		ofSetColor(canUnlink ?
			(unlinkHovered ? COL_ACCENT_RED : COL_TEXT_SECONDARY) :
			ofColor(COL_TEXT_MUTED, 45));
		ofSetLineWidth(1.5f);
		ofDrawLine(unlinkCx - 4.0f, unlinkCy - 4.0f,
			unlinkCx + 4.0f, unlinkCy + 4.0f);
		ofDrawLine(unlinkCx + 4.0f, unlinkCy - 4.0f,
			unlinkCx - 4.0f, unlinkCy + 4.0f);
		ofSetLineWidth(1.0f);
		if (canUnlink)
		{
			jp_tooltip::draw("Unlink texture",
				row.unlinkButton.x, row.unlinkButton.y,
				row.unlinkButton.width, row.unlinkButton.height);
		}
		drawInspectorClickBounds(row.upButton, canMoveUp);
		drawInspectorClickBounds(
			row.exposeButton, showExpose && canExpose);
		drawInspectorClickBounds(row.unlinkButton, canUnlink);
	}
	jp_tooltip::draw(
		inspectorInputsExpanded ? "Collapse inputs" : "Expand inputs",
		inspectorInputsHeaderBounds.x,
		inspectorInputsHeaderBounds.y,
		inspectorInputsHeaderBounds.width,
		inspectorInputsHeaderBounds.height);
	ofPopStyle();
}

bool JPboxgroup::moveInspectorInputUp(JPbox *box, int linkIndex)
{
	if (box == nullptr || linkIndex <= 0 ||
		linkIndex >= box->fbohandlergroup.getSize() ||
		isInspectorTextureInputExposed(box, linkIndex) ||
		isInspectorTextureInputExposed(box, linkIndex - 1) ||
		!box->fbohandlergroup.swapConnections(linkIndex, linkIndex - 1))
	{
		return false;
	}

	if (isCueDraftMode())
	{
		markCueDraftDirty(cueSelectedIndex(), CUE_DIRTY_LINKS);
		updateCueDraftGraph();
	}
	else
	{
		requestCueRebuild();
	}
	return true;
}

bool JPboxgroup::unlinkInspectorInput(JPbox *box, int linkIndex)
{
	if (box == nullptr || linkIndex < 0 ||
		linkIndex >= box->fbohandlergroup.getSize() ||
		isInspectorTextureInputExposed(box, linkIndex) ||
		!box->fbohandlergroup.getisPointerSet(linkIndex))
	{
		return false;
	}

	box->fbohandlergroup.deleteFboPointer(linkIndex);
	if (isCueDraftMode())
	{
		markCueDraftDirty(cueSelectedIndex(), CUE_DIRTY_LINKS);
		updateCueDraftGraph();
	}
	else
	{
		requestCueRebuild();
	}
	return true;
}

JPbox_preset *JPboxgroup::getInspectorInputOwnerPreset() const
{
	if (!isGroupViewActive())
	{
		return nullptr;
	}
	if (isCueDraftMode())
	{
		return getDraftPresetForCurrentView();
	}
	return getActivePreset();
}

bool JPboxgroup::isInspectorTextureInputExposed(
	JPbox *box, int linkIndex) const
{
	JPbox_preset *owner =
		getInspectorInputOwnerPreset();
	if (owner == nullptr || box == nullptr ||
		linkIndex < 0 ||
		linkIndex >= box->fbohandlergroup.getSize())
	{
		return false;
	}
	return owner->isTextureInputExposed(
		box->name,
		box->fbohandlergroup.getName(linkIndex));
}

bool JPboxgroup::toggleInspectorTextureInputExposure(
	JPbox *box, int linkIndex)
{
	JPbox_preset *owner =
		getInspectorInputOwnerPreset();
	if (owner == nullptr || box == nullptr ||
		linkIndex < 0 ||
		linkIndex >= box->fbohandlergroup.getSize())
	{
		return false;
	}
	const string samplerName =
		box->fbohandlergroup.getName(linkIndex);
	const bool exposed = owner->isTextureInputExposed(
		box->name, samplerName);
	bool changed = false;
	if (exposed)
	{
		changed = owner->removeExposedTextureInput(
			box->name, samplerName);
	}
	else if (!box->fbohandlergroup.getisPointerSet(linkIndex))
	{
		changed = owner->exposeTextureInput(
			box->name, samplerName);
	}
	if (!changed)
	{
		return false;
	}

	if (isCueDraftMode())
	{
		markCueDraftDirty(
			cueSelectedIndex(), CUE_DIRTY_LINKS);
		updateCueDraftGraph();
	}
	else
	{
		requestCueRebuild();
	}
	setControllers();
	return true;
}

bool JPboxgroup::handleInspectorInputClick(JPbox *box)
{
	if (!inspectorBodyContains(ofGetMouseX(), ofGetMouseY())) return false;
	if (box != nullptr &&
		inspectorInputsHeaderBounds.inside(ofGetMouseX(), ofGetMouseY()))
	{
		inspectorInputsExpanded = !inspectorInputsExpanded;
		setControllers();
		return true;
	}
	for (const InspectorInputRow &row : inspectorInputRows)
	{
		if (isGroupViewActive() &&
			row.exposeButton.inside(
				ofGetMouseX(), ofGetMouseY()))
		{
			if (toggleInspectorTextureInputExposure(
				box, row.linkIndex))
			{
				return true;
			}
			return true;
		}
		if (row.unlinkButton.inside(ofGetMouseX(), ofGetMouseY()) &&
			box != nullptr &&
			box->fbohandlergroup.getisPointerSet(row.linkIndex))
		{
			unlinkInspectorInput(box, row.linkIndex);
			return true;
		}
		if (row.linkIndex > 0 &&
			row.upButton.inside(ofGetMouseX(), ofGetMouseY()))
		{
			moveInspectorInputUp(box, row.linkIndex);
			return true;
		}
	}
	return false;
}

bool JPboxgroup::handleInspectorLockClick()
{
	if (!inspectorBodyContains(ofGetMouseX(), ofGetMouseY())) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	for (int i = 0; i < (int)parameterLockButtons.size() &&
		i < (int)controllers.size(); ++i)
	{
		if (!parameterLockButtons[i].inside(mouse) ||
			controllers[i] == nullptr || controllers[i]->parameters == nullptr)
			continue;
		controllers[i]->parameters->randomLocked =
			!controllers[i]->parameters->randomLocked;
		markCueDraftDirty(cueSelectedIndex());
		if (isCueDraftMode()) updateCueDraftGraph();
		return true;
	}
	return false;
}

bool JPboxgroup::handleInspectorRangeClick()
{
	if (!inspectorBodyContains(ofGetMouseX(), ofGetMouseY())) return false;
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	for (int i = 0; i < (int)parameterRangeButtons.size() &&
		i < (int)controllers.size(); ++i)
	{
		JPParameter *parameter = controllers[i] != nullptr ?
			controllers[i]->parameters : nullptr;
		if (parameterRangeButtons[i].inside(mouse) && parameter != nullptr &&
			parameter->variabletype == JPParameter::FLOAT)
		{
			parameter->setRangeEnabled(!parameter->rangeEnabled);
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode()) updateCueDraftGraph();
			setControllers();
			return true;
		}
	}
	for (JPcontroller *controller : controllers)
	{
		JPComplexSlider *slider = dynamic_cast<JPComplexSlider *>(controller);
		if (slider == nullptr) continue;
		const int handle = slider->rangeHandleAt(mouse.x, mouse.y);
		if (handle == 0) continue;
		rangeDragSlider = slider;
		rangeDragHandle = handle;
		slider->rangeHandleDragging = true;
		slider->slider_value.activable2 = false;
		slider->setRangeHandleFromMouse(handle, mouse.x);
		markCueDraftDirty(cueSelectedIndex());
		if (isCueDraftMode()) updateCueDraftGraph();
		return true;
	}
	return false;
}

bool JPboxgroup::handleInspectorAutomationClick()
{
	const ofVec2f mouse(ofGetMouseX(), ofGetMouseY());
	if (!inspectorBodyContains(mouse.x, mouse.y)) return false;
	auto boundsFor = [](const JPdragobject &control)
	{
		return ofRectangle(
			control.x - control.width / 2.0f,
			control.y - control.height / 2.0f,
			control.width, control.height);
	};
	for (JPcontroller *controller : controllers)
	{
		JPComplexSlider *slider =
			dynamic_cast<JPComplexSlider *>(controller);
		if (slider == nullptr || slider->parameters == nullptr)
		{
			continue;
		}

		JPParameter *parameter = slider->parameters;
		if (parameter->bpmEligible &&
			parameter->movtype == JPParameter::BPM &&
			slider->boton_bpm.activable2 &&
			boundsFor(slider->bpm_rate_button).inside(mouse))
		{
			parameter->cycleBpmRate();
			parameter->update();
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode())
			{
				updateCueDraftGraph();
			}
			return true;
		}
		if (parameter->bpmEligible &&
			parameter->movtype != JPParameter::STANDART &&
			slider->boton_bpm.activable2 &&
			boundsFor(slider->boton_bpm).inside(mouse))
		{
			parameter->setAutomationMode(JPParameter::BPM);
			parameter->needsUpdate = false;
			parameter->update();
			slider->boton_bpm.activable = false;
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode())
			{
				updateCueDraftGraph();
			}
			return true;
		}

		// AUDIO mode, mirroring the BPM pair above. Handle shaping controls
		// before the persistent source/division row and the mode button.
		if (parameter->audioEligible && parameter->movtype == JPParameter::AUDIO &&
			boundsFor(slider->audio_shape_button).inside(mouse))
		{
			parameter->audioShapingOpen = !parameter->audioShapingOpen;
			setControllers();
			return true;
		}
		if (parameter->audioEligible && parameter->movtype == JPParameter::AUDIO &&
			parameter->audioShapingOpen)
		{
			const int shapingControl = slider->audioShapingControlAt(
				mouse.x, mouse.y);
			if (shapingControl != JPComplexSlider::AUDIO_SHAPING_NONE)
			{
				audioShapingDragSlider = slider;
				audioShapingDragControl = shapingControl;
				slider->setAudioShapingControlFromMouse(
					shapingControl, mouse.x);
				parameter->update();
				markCueDraftDirty(cueSelectedIndex());
				if (isCueDraftMode()) updateCueDraftGraph();
				return true;
			}
			if (boundsFor(slider->audio_invert_button).inside(mouse))
			{
				parameter->audioInvert = !parameter->audioInvert;
				parameter->update();
				markCueDraftDirty(cueSelectedIndex());
				if (isCueDraftMode()) updateCueDraftGraph();
				return true;
			}
		}
		if (parameter->audioEligible &&
			parameter->movtype == JPParameter::AUDIO &&
			jp_audio::isRhythmSource(parameter->audioSource) &&
			boundsFor(slider->audio_div_button).inside(mouse))
		{
			parameter->cycleAudioDiv();
			parameter->update();
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode())
			{
				updateCueDraftGraph();
			}
			return true;
		}
		if (parameter->audioEligible &&
			parameter->movtype == JPParameter::AUDIO &&
			boundsFor(slider->audio_source_button).inside(mouse))
		{
			parameter->cycleAudioSource();
			parameter->update();
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode())
			{
				updateCueDraftGraph();
			}
			// No relayout needed: the chip slots are reserved, so the row does
			// not move when the source changes.
			return true;
		}
		if (parameter->audioEligible &&
			parameter->movtype != JPParameter::STANDART &&
			boundsFor(slider->boton_audio).inside(mouse))
		{
			if (parameter->movtype != JPParameter::AUDIO)
				parameter->audioBase = parameter->floatLerpValue;
			parameter->setAutomationMode(JPParameter::AUDIO);
			parameter->needsUpdate = false;
			parameter->update();
			slider->boton_audio.activable = false;
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode())
			{
				updateCueDraftGraph();
			}
			setControllers();
			return true;
		}

		// One compact button cycles the four basic automation patterns.
		if (parameter->movtype != JPParameter::STANDART &&
			boundsFor(slider->boton_idayvuelta).inside(mouse))
		{
			parameter->cycleAutomationPattern();
			parameter->update();
			slider->boton_idayvuelta.activable = false;
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode()) updateCueDraftGraph();
			return true;
		}
		if (!boundsFor(slider->boton_collapse).inside(mouse))
		{
			if (isCueDraftMode() &&
				parameter->movtype != JPParameter::STANDART)
			{
				const bool overAutomationControl =
					boundsFor(slider->slider_speed).inside(mouse) ||
					boundsFor(slider->handler1).inside(mouse) ||
					boundsFor(slider->handler2).inside(mouse) ||
					boundsFor(slider->boton_idayvuelta).inside(mouse) ||
					boundsFor(slider->boton_audio).inside(mouse) ||
					boundsFor(slider->audio_source_button).inside(mouse) ||
					boundsFor(slider->audio_div_button).inside(mouse);
				if (overAutomationControl)
				{
					markCueDraftDirty(cueSelectedIndex());
				}
			}
			continue;
		}

		parameter->toggleAutomation();
		parameter->needsUpdate = false;
		parameter->update();
		markCueDraftDirty(cueSelectedIndex());
		setControllers();
		return true;
	}
	return false;
}

// A row's geometry - its height, and where the mode chips sit - depends on the
// parameter's movtype. Rebuild when a mode changes so modifier rows and audio
// shaping controls always use geometry for their current state.
void JPboxgroup::rebuildControllersIfLayoutStale()
{
	if (JPMediaInspectable *media = dynamic_cast<JPMediaInspectable *>(getInspectorBox()))
	{
		const bool playable = media->mediaPlayable() || !media->mediaReady();
		if (mediaInspector.card.width > 0 &&
			playable != mediaInspectorPlayableBuilt)
		{
			setControllers();
			return;
		}
	}
	for (JPcontroller *controller : controllers)
	{
		JPComplexSlider *slider = dynamic_cast<JPComplexSlider *>(controller);
		if (slider == nullptr || slider->parameters == nullptr) continue;
		const bool sourceChanged =
			slider->parameters->movtype == JPParameter::AUDIO &&
			slider->builtForAudioSource != slider->parameters->audioSource;
		auto isBasicPattern = [](int mode)
		{
			return mode == JPParameter::OSC || mode == JPParameter::RANDOM ||
				mode == JPParameter::GODER || mode == JPParameter::GOIZQ;
		};
		const bool layoutChanged =
			slider->builtForMovtype != slider->parameters->movtype &&
			!(isBasicPattern(slider->builtForMovtype) &&
				isBasicPattern(slider->parameters->movtype));
		if (layoutChanged ||
			sourceChanged)
		{
			setControllers();
			return;
		}
	}
}

void JPboxgroup::draw_paramswindow()
{
	jp_pointer::Scope pointerScope(jp_pointer::kInspector);

	JPbox *inspectorBox = getInspectorBox();
	if (inspectorBox != nullptr)
	{
		rebuildControllersIfLayoutStale();
		// The inspector owns its surface. Preserve any upstream clip, but draw
		// the sticky panel/header unclipped before applying the body viewport.
		jp_gl::ScopedScissor inspectorClip;
		// Use an explicit full-viewport clip rather than disabling scissor. Some
		// OF render paths cache the enabled state and can otherwise reinstate a
		// stale body clip before the title glyphs are submitted.
		inspectorClip.setFullViewport();
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
		ofSetRectMode(OF_RECTMODE_CENTER);
		ofSetColor(COL_BG_PANEL);
		// constants_img::background.draw(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width, inspectorwindow_height);
		ofNoFill();
		ofSetColor(ofColor(COL_BORDER_MUTED, 185));
		ofSetLineWidth(1.0f);
		ofDrawRectangle(inspectorwindow_x, inspectorwindow_y, inspectorwindow_width - 1.0f, inspectorwindow_height - 1.0f);
		ofFill();
		ofSetRectMode(OF_RECTMODE_CORNER);

		string name = (cueTargetsCurrentView() && getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr) ? inspectorBox->name + " DRAFT" : inspectorBox->name;
		const float panelLeft =
			inspectorwindow_x - inspectorwindow_width / 2.0f;
		const float panelRight =
			inspectorwindow_x + inspectorwindow_width / 2.0f;
		const float headerDividerY = inspectorLayout.headerHeight - 8.0f;
		const bool hasRandomAction = std::any_of(
			controllers.begin(), controllers.end(), [](JPcontroller *controller)
			{
				return controller != nullptr && controller->parameters != nullptr;
			});
		const bool hasEditAction =
			inspectorBox->getTipo() == inspectorBox->SHADERBOX &&
			shaderEditor != nullptr && !inspectorBox->dir.empty();
		const bool hasMappingAction = isMappingShaderBox(inspectorBox);
		const bool hasCameraAction =
			inspectorBox->getTipo() == inspectorBox->CAMBOX ||
			inspectorBox->getTipo() == inspectorBox->KINECT2BOX;
		const float randomActionWidth = 44.0f;
		const float defaultActionWidth = 42.0f;
		const float saveDefaultActionWidth = 62.0f;
		const float mappingActionWidth = 48.0f;
		const float editActionWidth = 48.0f;
		const float cameraActionWidth = 48.0f;
		// Every box owns an FBO, so any of them can drive a live output.
		const bool hasToOutputAction = true;
		// Measured, not guessed. drawHeaderAction centres the label on the
		// button and never clips it, so a label wider than its box spills over
		// the control to its left and past the panel edge - which is exactly
		// what a hardcoded 72 did to "TO OUTPUT".
		const string toOutputLabel = "TO OUTPUT";
		const float toOutputActionWidth = std::max(72.0f,
			jp_constants::inspector_secondary_font.stringWidth(toOutputLabel)
				+ 16.0f);
		const float headerActionHeight = 26.0f;
		const float headerActionGap = 5.0f;
		// Count and width are SEPARATE sums - the count only feeds the
		// inter-button gaps. A button added to one and not the other shifts
		// every control in the row by a gap width.
		const int headerActionCount =
			(hasRandomAction ? 3 : 0) +
			(hasMappingAction ? 1 : 0) +
			(hasCameraAction ? 1 : 0) +
			(hasEditAction ? 1 : 0) +
			(hasToOutputAction ? 1 : 0);
		const float headerActionWidth =
			(hasRandomAction ? randomActionWidth : 0.0f) +
			(hasRandomAction ? defaultActionWidth + saveDefaultActionWidth : 0.0f) +
			(hasMappingAction ? mappingActionWidth : 0.0f) +
			(hasCameraAction ? cameraActionWidth : 0.0f) +
			(hasEditAction ? editActionWidth : 0.0f) +
			(hasToOutputAction ? toOutputActionWidth : 0.0f) +
			std::max(0, headerActionCount - 1) * headerActionGap;
		const float headerActionRight = panelRight - 12.0f;
		const float headerActionLeft =
			headerActionRight - headerActionWidth;
		const float titleVisualCenterY =
			inspectorLayout.headerHeight * 0.5f - 6.0f;
		const float titleBaselineY = titleVisualCenterY +
			jp_constants::inspector_title_font.stringHeight("Ag") * 0.5f;
		const float headerActionTop =
			titleVisualCenterY - headerActionHeight / 2.0f;

		inspectorrandom.width = 0.0f;
		inspectorrandom.height = 0.0f;
		inspectordefault.width = inspectordefault.height = 0.0f;
		inspectorsavedefault.width = inspectorsavedefault.height = 0.0f;
		mappingbutton.width = 0.0f;
		mappingbutton.height = 0.0f;
		editbutton.width = 0.0f;
		editbutton.height = 0.0f;
		camerarefreshbutton.width = 0.0f;
		camerarefreshbutton.height = 0.0f;
		// Zero size means hidden AND unhittable, which is how visibility is
		// expressed for every control in this row.
		tooutputbutton.width = 0.0f;
		tooutputbutton.height = 0.0f;

		float titleX = panelLeft + 16.0f;
		string title = name;
		const float titleRight = headerActionWidth > 0.0f ?
			headerActionLeft - 10.0f : panelRight - 12.0f;
		float maxTitleW = std::max(20.0f, titleRight - titleX);
		if (jp_constants::inspector_title_font.stringWidth(title) > maxTitleW)
		{
			while (title.size() > 1 &&
				jp_constants::inspector_title_font.stringWidth(title + "..") > maxTitleW)
			{
				title.pop_back();
			}
			title += "..";
		}
		// Header controls are measured here but painted once, after the clipped
		// body. Keeping geometry and painting separate prevents stale scissor
		// state and duplicated header styles from drifting apart.
		float nextActionX = headerActionLeft;
		auto placeHeaderAction = [&](JPBang &button, float actionWidth)
		{
			button.x = nextActionX + actionWidth * 0.5f;
			button.y = headerActionTop + headerActionHeight * 0.5f;
			button.width = actionWidth;
			button.height = headerActionHeight;
			nextActionX += actionWidth + headerActionGap;
		};
		if (hasRandomAction)
		{
			placeHeaderAction(inspectorrandom, randomActionWidth);
			placeHeaderAction(inspectordefault, defaultActionWidth);
			placeHeaderAction(inspectorsavedefault, saveDefaultActionWidth);
		}

		if (hasCameraAction)
		{
			placeHeaderAction(camerarefreshbutton, cameraActionWidth);
		}

		if (hasEditAction)
		{
			placeHeaderAction(editbutton, editActionWidth);
		}

		if (hasMappingAction)
		{
			placeHeaderAction(mappingbutton, mappingActionWidth);
		}

		if (hasToOutputAction)
		{
			placeHeaderAction(tooutputbutton, toOutputActionWidth);
		}

		inspectorClip.set(inspectorBodyViewport);
		const bool suppressBodyPointer = !inspectorBodyContains(
			ofGetMouseX(), ofGetMouseY());
		if (suppressBodyPointer)
			JPdragobject::setMouseOverride(ofVec2f(-10000.0f, -10000.0f));

		drawMediaInspector(dynamic_cast<JPMediaInspectable *>(inspectorBox));
		drawInspectorInputRows(inspectorBox);
		drawAdvancedMappingParameterHeaders(inspectorBox);

		if (auto *kinect = dynamic_cast<JPbox_kinect2 *>(inspectorBox))
		{
			const char *labels[] = {"COLOR", "DEPTH", "IR"};
			for (int i = 0; i < 3; ++i)
			{
				const bool selected = (int)kinect->getStream() == i;
				const bool hovered = kinectStreamButtons[i].inside(
					ofGetMouseX(), ofGetMouseY());
				ofSetColor(selected ? COL_ACCENT_CYAN_DIM :
					(hovered ? COL_BG_HOVER : COL_BG_INPUT));
				ofDrawRectRounded(kinectStreamButtons[i], 3.0f);
				ofNoFill();
				ofSetColor(selected ? COL_ACCENT_CYAN : COL_BORDER_MUTED);
				ofDrawRectRounded(kinectStreamButtons[i], 3.0f);
				ofFill();
				ofSetColor(selected ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
				jp_constants::p_font.drawString(labels[i],
					kinectStreamButtons[i].getCenter().x -
						jp_constants::p_font.stringWidth(labels[i]) / 2.0f,
					kinectStreamButtons[i].getCenter().y +
						jp_constants::p_font.stringHeight(labels[i]) / 2.0f - 2.0f);
			}
			ofSetColor(kinect->getCaptureStatus() == "STREAMING" ?
				COL_ACCENT_GREEN : COL_ACCENT_GOLD);
			jp_constants::p_font.drawString(kinect->getCaptureStatus(),
				panelLeft + 12.0f,
				kinectStreamButtons[0].getBottom() + 16.0f);
		}

		for (int i = 0; i < controllers.size(); i++)
		{
			if (controllers[i]->width <= 0.0f ||
				controllers[i]->height <= 0.0f)
			{
				continue;
			}
			const ofRectangle controllerBounds(
				controllers[i]->x - controllers[i]->width * 0.5f,
				controllers[i]->y - controllers[i]->height * 0.5f,
				controllers[i]->width, controllers[i]->height);
			if (!controllerBounds.intersects(inspectorBodyViewport)) continue;
			controllers[i]->draw();
			if (i < (int)parameterRangeButtons.size() &&
				parameterRangeButtons[i].width > 0.0f &&
				controllers[i]->parameters != nullptr)
			{
				const ofRectangle &button = parameterRangeButtons[i];
				const bool enabled = controllers[i]->parameters->rangeEnabled;
				const bool hovered = button.inside(ofGetMouseX(), ofGetMouseY());
				ofSetRectMode(OF_RECTMODE_CORNER);
				ofSetColor(enabled ? COL_ACCENT_CYAN_DIM :
					(hovered ? COL_BG_HOVER : COL_BG_INPUT));
				ofDrawRectRounded(button, 3.0f);
				ofNoFill();
				ofSetColor(enabled ? COL_TEXT_PRIMARY :
					(hovered ? COL_TEXT_SECONDARY : COL_BORDER_MUTED));
				ofDrawRectRounded(button, 3.0f);
				const float cx = button.getCenter().x;
				const float cy = button.getCenter().y;
				ofSetLineWidth(1.5f);
				ofDrawLine(cx - 6.0f, cy, cx + 6.0f, cy);
				ofDrawLine(cx - 6.0f, cy - 5.0f, cx - 6.0f, cy + 5.0f);
				ofDrawLine(cx + 6.0f, cy - 5.0f, cx + 6.0f, cy + 5.0f);
				ofFill();
				ofDrawCircle(cx - 6.0f, cy, 2.0f);
				ofDrawCircle(cx + 6.0f, cy, 2.0f);
				if (enabled)
				{
					ofSetColor(COL_ACCENT_CYAN);
					ofDrawCircle(cx, cy, 1.75f);
				}
				ofFill();
				ofSetLineWidth(1.0f);
				jp_tooltip::draw("custom range",
					button.x, button.y, button.width, button.height);
			}
			if (i < (int)parameterLockButtons.size() &&
				parameterLockButtons[i].width > 0.0f &&
				controllers[i]->parameters != nullptr)
			{
				const ofRectangle &button = parameterLockButtons[i];
				const bool locked = controllers[i]->parameters->randomLocked;
				const bool hovered = button.inside(ofGetMouseX(), ofGetMouseY());
				ofSetRectMode(OF_RECTMODE_CORNER);
				ofSetColor(locked ? COL_ACCENT_GOLD_DIM :
					(hovered ? COL_BG_HOVER : COL_BG_INPUT));
				ofDrawRectRounded(button, 3.0f);
				ofNoFill();
				ofSetColor(locked ? COL_ACCENT_GOLD : COL_BORDER_MUTED);
				ofDrawRectRounded(button, 3.0f);
				ofFill();
				const float cx = button.getCenter().x;
				const float cy = button.getCenter().y;
				ofNoFill();
				ofSetLineWidth(1.5f);
				ofDrawRectRounded(cx - 5.0f, cy - 1.0f,
					10.0f, 8.0f, 1.5f);
				const float shackleLeft = locked ? cx - 3.5f : cx - 1.0f;
				ofDrawLine(shackleLeft, cy - 1.0f,
					shackleLeft, cy - 5.0f);
				ofDrawLine(shackleLeft, cy - 5.0f,
					cx + 3.5f, cy - 5.0f);
				if (locked)
					ofDrawLine(cx + 3.5f, cy - 5.0f,
						cx + 3.5f, cy - 1.0f);
				ofFill();
				ofSetLineWidth(1.0f);
				jp_tooltip::draw(locked ?
					"Unlock for Random and Default" :
					"Lock from Random and Default",
					button.x, button.y, button.width, button.height);
			}
			if (dynamic_cast<JPComplexSlider *>(controllers[i]) == nullptr)
			{
				drawInspectorClickBounds(*controllers[i],
					controllers[i]->activable2);
			}
			// Draw expose button AFTER the controller (right side) when in group view
			if (i < (int)exposeButtons.size())
			{
				// Save current boolValue to detect changes after draw (activeFlag
				// can't be used because the button's internal draw clears it)
				bool prevBoolValue = exposeButtons[i]->boolValue;
				exposeButtons[i]->draw();
				// Sync button state back to preset's exposedParams on toggle
				if (exposeButtons[i]->boolValue != prevBoolValue)
				{
					JPbox_preset *preset = getActivePreset();
					if (preset != nullptr && groupInspectorIndex >= 0 &&
						groupInspectorIndex < (int)preset->exposedParams.size() &&
						i < (int)preset->exposedParams[groupInspectorIndex].size())
					{
						preset->exposedParams[groupInspectorIndex][i] = exposeButtons[i]->boolValue;
					}
				}
			}
		}
		// Colour swatches. Inside the clipped span on purpose, so they scroll
		// and clip with the rows instead of floating over the header.
		for (const InspectorColorSwatch &swatch : inspectorColorSwatches)
		{
			if (swatch.r == nullptr || swatch.g == nullptr || swatch.b == nullptr)
				continue;
			if (!swatch.bounds.intersects(inspectorBodyViewport)) continue;

			const ofFloatColor colour = JPParameter::swatchColor(
				swatch.r->floatValue, swatch.g->floatValue, swatch.b->floatValue);

			ofSetRectMode(OF_RECTMODE_CORNER);
			ofFill();
			ofSetColor(colour);
			ofDrawRectRounded(swatch.bounds, 3.0f);
			// Outline in the panel's own language, so a swatch that happens to
			// be near-black still reads as a control rather than a hole.
			ofNoFill();
			ofSetLineWidth(1.0f);
			ofSetColor(ofColor(COL_BORDER_DEFAULT, 200));
			ofDrawRectRounded(swatch.bounds, 3.0f);
			ofFill();
		}

		if (suppressBodyPointer) JPdragobject::clearMouseOverride();

		// Paint the sticky header last. Besides giving it the correct z-order,
		// this protects it from renderers that leave a stale FBO scissor active
		// before the inspector is entered.
		inspectorClip.setFullViewport();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(COL_BG_PANEL);
		ofDrawRectangle(inspectorHeaderBounds);
		ofSetColor(COL_TEXT_PRIMARY);
		jp_constants::inspector_title_font.drawString(
			title, titleX, titleBaselineY);
		ofSetColor(ofColor(COL_BORDER_MUTED, 120));
		ofDrawLine(titleX, headerDividerY, panelRight - 14.0f,
			headerDividerY);
		auto drawHeaderAction = [&](JPBang &button, const string &label,
			const ofColor &idleColor, const ofColor &hoverColor,
			const string &tooltip)
		{
			if (button.width <= 0.0f) return;
			const float left = button.x - button.width * 0.5f;
			const bool hovered = button.mouseOver();
			ofSetColor(hovered ? ofColor(COL_BG_HOVER, 245) :
				ofColor(COL_BG_INPUT, 235));
			ofDrawRectRounded(left, headerActionTop,
				button.width, headerActionHeight, 3.0f);
			ofNoFill();
			ofSetLineWidth(hovered ? 1.5f : 1.0f);
			ofSetColor(hovered ? ofColor(hoverColor, 210) :
				ofColor(COL_BORDER_MUTED, 175));
			ofDrawRectRounded(left, headerActionTop,
				button.width, headerActionHeight, 3.0f);
			ofFill();
			ofSetColor(hovered ? hoverColor : idleColor);
			jp_constants::inspector_secondary_font.drawString(label,
				button.x - jp_constants::inspector_secondary_font.stringWidth(label) * 0.5f,
				button.y + jp_constants::inspector_secondary_font.stringHeight(label) * 0.5f - 2.0f);
			if (!tooltip.empty())
				jp_tooltip::draw(tooltip, left,
					button.y - button.height * 0.5f,
					button.width, button.height);
			drawInspectorClickBounds(button);
		};
		drawHeaderAction(inspectorrandom, "RDM", COL_TEXT_SECONDARY,
			COL_ACCENT_CYAN, "Randomize parameters");
		drawHeaderAction(inspectordefault, "DEF", COL_TEXT_SECONDARY,
			COL_ACCENT_CYAN, "Restore unlocked parameter defaults");
		drawHeaderAction(inspectorsavedefault, "SET DEF", COL_ACCENT_GOLD_DIM,
			COL_ACCENT_GOLD, "Save current values as defaults");
		drawHeaderAction(camerarefreshbutton,
			inspectorBox->getTipo() == inspectorBox->KINECT2BOX ? "RETRY" : "SCAN",
			COL_TEXT_SECONDARY, COL_ACCENT_CYAN,
			inspectorBox->getTipo() == inspectorBox->KINECT2BOX ?
				"Reconnect the Kinect v2" :
				"Rescan cameras connected after startup");
		drawHeaderAction(editbutton, "EDIT", COL_ACCENT_GOLD_DIM,
			COL_ACCENT_GOLD, "Edit shader source");
		drawHeaderAction(mappingbutton, "MAP",
			mappingEditActive ? COL_ACCENT_CYAN : COL_TEXT_SECONDARY,
			COL_ACCENT_CYAN, "Edit mapping corners");
		// Same on/off treatment MAP uses: drawHeaderAction has no notion of a
		// toggled state, so a lit idle colour is what carries it.
		drawHeaderAction(tooutputbutton, toOutputLabel,
			inspectorBox->getOutputCandidate() ?
				COL_ACCENT_CYAN : COL_TEXT_SECONDARY,
			COL_ACCENT_CYAN,
			inspectorBox->getOutputCandidate() ?
				"Available as a live output source - click to remove" :
				"Make this box selectable as a live output source");

		if (inspectorMaxScrollY > 0.0f)
		{
			ofSetRectMode(OF_RECTMODE_CORNER);
			ofSetColor(ofColor(COL_BG_SCROLLBAR, 70));
			ofDrawRectRounded(inspectorScrollbarTrack, 2.0f);
			ofSetColor(inspectorScrollbarDragging ? COL_ACCENT_CYAN :
				ofColor(COL_TEXT_MUTED, 210));
			ofDrawRectRounded(inspectorScrollbarThumb, 2.0f);
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
			// During a cue, connections are staged on the draft clone. Draw the
			// draft's links so connections made while cueing are visible; the
			// node positions still come from the real boxes shown in the grid.
			JPbox *linkSrcK = boxes[k];
			if (cueTargetsCurrentView())
			{
				JPbox *dk = getCueDraftBoxForRealIndex(k);
				if (dk != nullptr) linkSrcK = dk;
			}
			for (int l = boxes[k]->fbohandlergroup.getSize() - 1; l >= 0; l--)
			{
				if (l < linkSrcK->fbohandlergroup.getSize() &&
					linkSrcK->fbohandlergroup.getFboName(l) ==
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
	auto profileStageStart = ProfileClock::now();
	update_paramswindow();
	if (profilingEnabled)
	{
		float unusedPeak = 0.0f;
		recordProfileValue(profileSnapshot.parametersMs, unusedPeak,
			elapsedProfileMs(profileStageStart));
	}
	if (mappingEditActive && getMappingEditBox() == nullptr)
	{
		endMappingEdit();
	}
	ofVec2f canvasMouse = screenToCanvas(ofVec2f(ofGetMouseX(), ofGetMouseY()));
	
	// Update sub-boxes when in group view mode
	if (isGroupViewActive())
	{
		profileStageStart = ProfileClock::now();
		JPbox_preset *preset = getActivePreset();
		// An enabled top-level preset recursively updates this same subtree in
		// the main graph pass below. The group editor only needs a separate
		// update when a paused/bypassed ancestor prevents that recursion.
		bool subtreeUpdatedByMainGraph = !activeGroupPath.empty();
		JPbox_preset *ancestor = nullptr;
		if (subtreeUpdatedByMainGraph)
		{
			const int rootIndex = activeGroupPath[0];
			if (rootIndex < 0 || rootIndex >= (int)boxes.size())
			{
				subtreeUpdatedByMainGraph = false;
			}
			else
			{
				ancestor = dynamic_cast<JPbox_preset *>(boxes[rootIndex]);
			}
		}
		for (size_t depth = 1;
			subtreeUpdatedByMainGraph && depth < activeGroupPath.size(); ++depth)
		{
			if (ancestor == nullptr || !ancestor->getonoff() ||
				ancestor->getBypass())
			{
				subtreeUpdatedByMainGraph = false;
				break;
			}
			const int childIndex = activeGroupPath[depth];
			if (childIndex < 0 || childIndex >= (int)ancestor->boxes.size())
			{
				subtreeUpdatedByMainGraph = false;
				break;
			}
			ancestor = dynamic_cast<JPbox_preset *>(ancestor->boxes[childIndex]);
		}
		if (subtreeUpdatedByMainGraph &&
			(ancestor == nullptr || !ancestor->getonoff() || ancestor->getBypass()))
		{
			subtreeUpdatedByMainGraph = false;
		}
		if (preset != nullptr)
		{
			for (int i = (int)preset->boxes.size() - 1; i >= 0; i--)
			{
				if (!subtreeUpdatedByMainGraph)
				{
					preset->boxes[i]->isactiverender = false;
					preset->boxes[i]->update();
				}
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
		if (profilingEnabled)
		{
			float unusedPeak = 0.0f;
			recordProfileValue(profileSnapshot.groupViewMs, unusedPeak,
				elapsedProfileMs(profileStageStart));
		}
	}
	else if (profilingEnabled)
	{
		float unusedPeak = 0.0f;
		recordProfileValue(profileSnapshot.groupViewMs, unusedPeak, 0.0f);
	}

	// Reset dragging state when mouse is not pressed
	if (!ofGetMousePressed())
	{
		draggedExposedBoxIndex = -1;
		draggedExposedParamIndex = -1;
	}

	float lerpAmount = 0.3;
	scheduleTopLevelRenders();
	profileStageStart = ProfileClock::now();
	for (int i = boxes.size() - 1; i >= 0; i--){
		const auto boxStart = profilingEnabled ? ProfileClock::now() :
			ProfileClock::time_point();
		boxes[i]->isactiverender = activerender != nullptr && i == *activerender;
		boxes[i]->update();
		if (profilingEnabled)
		{
			ProfileEntry *entry = nullptr;
			for (ProfileEntry &candidate : profileSnapshot.boxes)
			{
				if (candidate.name == boxes[i]->name)
				{
					entry = &candidate;
					break;
				}
			}
			if (entry == nullptr)
			{
				profileSnapshot.boxes.push_back(ProfileEntry());
				entry = &profileSnapshot.boxes.back();
				entry->name = boxes[i]->name;
			}
			recordProfileValue(entry->averageMs, entry->peakMs,
				elapsedProfileMs(boxStart));
		}
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
	if (profilingEnabled)
	{
		float unusedPeak = 0.0f;
		recordProfileValue(profileSnapshot.mainGraphMs, unusedPeak,
			elapsedProfileMs(profileStageStart));
	}
	processPendingCueRebuild();
	processPendingCueApply();
	profileStageStart = ProfileClock::now();
	if (isCueDraftMode())
	{
		updateCueDraftGraph();
	}
	if (profilingEnabled)
	{
		float unusedPeak = 0.0f;
		recordProfileValue(profileSnapshot.cueDraftMs, unusedPeak,
			elapsedProfileMs(profileStageStart));
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
				markCueDraftDirty(cueSelectedIndex());
				setControllers();

				inspectorBox->parameters.parameters[k]->needsUpdate = false;
			}
		}
	}

	if (!boxes.empty())
	{
		profileStageStart = ProfileClock::now();
		transition.update(); //ACTUALIZO EL TRANSITION
		updateParameterMorph();
		if (profilingEnabled)
		{
			float unusedPeak = 0.0f;
			recordProfileValue(profileSnapshot.transitionMs, unusedPeak,
				elapsedProfileMs(profileStageStart));
		}
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
	jp_pointer::Scope pointerScope(jp_pointer::kInspector);

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
		if (controllerselected < 0 || controllerselected == i)
		{
			controllers[i]->activable2 = true;
		}
		else
		{
			JPbox *box = getInspectorBox();
			int paramCount = (box != nullptr) ? box->parameters.getSize() : controllers.size();
			if (i < paramCount)
			{
				// Skip BOOL controllers (JPToogle) - they manage their own activeFlag in draw(), not in update()
				// Clearing activable2 here would prevent the toggle from ever being clicked
				if (box != nullptr && box->parameters.getType(i) != box->parameters.BOOL)
				{
					controllers[i]->activable2 = false;
				}
			}
			// Exposed controllers (i >= paramCount): keep activable2 true so they can be independently interacted with
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
	clampMappingPanelLayout();
	setControllers();
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
	if(mousebutton==OF_MOUSE_BUTTON_LEFT && (mediaTimelineDragging || mediaRangeDragging!=0))
	{
		auto *target=dynamic_cast<JPMediaInspectable *>(getInspectorBox());
		if(target)
		{
			JPMediaState&s=target->mediaState();
			if(mediaRangeDragging==5)s.fitMode=(JPMediaFitMode)ofClamp((int)((screenMouse.x-mediaInspector.fit.x)/mediaInspector.fit.width*5.0f),0,4);
			else if(mediaRangeDragging==3)s.rate=ofMap(screenMouse.x,mediaInspector.speed.x,mediaInspector.speed.getRight(),.25f,4.0f,true);
			else if(mediaRangeDragging==4)s.volume=ofMap(screenMouse.x,mediaInspector.volume.x,mediaInspector.volume.getRight(),0,1,true);
			else if(mediaInspector.timeline.width>0)
			{
				const float n=ofMap(screenMouse.x,mediaInspector.timeline.x,
					mediaInspector.timeline.getRight(),0,1,true);
				if(mediaRangeDragging==1)
				{
					s.rangeIn=n;
					if(s.rangeIn>s.rangeOut)s.rangeOut=s.rangeIn;
				}
				else if(mediaRangeDragging==2)
				{
					s.rangeOut=n;
					if(s.rangeOut<s.rangeIn)s.rangeIn=s.rangeOut;
				}
				else target->mediaSeek(ofClamp(n,s.rangeIn,s.rangeOut));
			}
			markCueDraftDirty(cueSelectedIndex());
		}
		return;
	}
	if (mousebutton == OF_MOUSE_BUTTON_LEFT &&
		rangeDragSlider != nullptr && rangeDragHandle != 0)
	{
		if (rangeDragSlider->setRangeHandleFromMouse(
			rangeDragHandle, screenMouse.x))
		{
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode()) updateCueDraftGraph();
		}
		return;
	}
	if (mousebutton == OF_MOUSE_BUTTON_LEFT && inspectorScrollbarDragging)
	{
		const float travel = inspectorScrollbarTrack.height -
			inspectorScrollbarThumb.height;
		if (travel > 0.0f && inspectorMaxScrollY > 0.0f)
		{
			const float thumbY = ofClamp(
				screenMouse.y - inspectorScrollbarDragOffset,
				inspectorScrollbarTrack.y,
				inspectorScrollbarTrack.getBottom() -
					inspectorScrollbarThumb.height);
			inspectorScrollY = (thumbY - inspectorScrollbarTrack.y) /
				travel * inspectorMaxScrollY;
			setControllers();
		}
		return;
	}
	if (mousebutton == OF_MOUSE_BUTTON_LEFT &&
		audioShapingDragSlider != nullptr &&
		audioShapingDragControl != JPComplexSlider::AUDIO_SHAPING_NONE)
	{
		if (audioShapingDragSlider->setAudioShapingControlFromMouse(
			audioShapingDragControl, screenMouse.x))
		{
			audioShapingDragSlider->parameters->update();
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode()) updateCueDraftGraph();
		}
		return;
	}
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

	// viewportPanning first, so a space-armed LEFT drag pans. It is only ever
	// set by the arm site and is cleared whenever the mouse is not pressed, so
	// it means exactly "this drag is a pan".
	if (viewportPanning ||
		mousebutton == OF_MOUSE_BUTTON_MIDDLE ||
		mousebutton == OF_MOUSE_BUTTON_RIGHT)
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
	JPbox_preset *inputOwnerPreset = nullptr;
	if (isGroupViewActive())
	{
		inputOwnerPreset = isCueDraftMode() ?
			getDraftPresetForCurrentView() :
			getActivePreset();
	}
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
					// A box cannot feed its own inlet. JPFbohandlerGroup
					// refuses this anyway, but stopping the gesture here keeps
					// the drop from looking accepted and avoids a pointless
					// cue rebuild - and in cue draft mode the draft and the
					// real box own different FBOs, so the funnel guard cannot
					// recognise the pair by pointer.
					if (i == k) continue;
					if (inputOwnerPreset != nullptr &&
						inputOwnerPreset
							->isExposedTextureInputTarget(
								activeBoxes[k]->name,
								activeBoxes[k]
									->fbohandlergroup
									.getName(l)))
					{
						continue;
					}
					if (activeBoxes[k]->fbohandlergroup.getFboName(l) == activeBoxes[i]->name)
					{
						continue;
					}
					if (isCueDraftMode() && cueTargetsCurrentView())
					{
						commitCueDraftLink(k, l, i);
					}
					else
					{
						activeBoxes[k]->fbohandlergroup.setFboPointer(&activeBoxes[i]->fbo,
																		&activeBoxes[i]->name, l);
						requestCueRebuild();
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
				auto *complex=dynamic_cast<JPComplexSlider *>(controllers[i]);

				// Check if this is a normal controller or an exposed one
				if (i < inspectorBox->parameters.getSize() &&
					inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT && inspectorBox->parameters.getMovType(i) == 0)
				{
					// This is because if movtype is 0, it doesn't update to avoid OSC overwrite
					// Manual update for movtype 0 sliders
					if(complex==nullptr)continue;
					controllers[i]->value=complex->valueFromMouse(ofGetMouseX());

					inspectorBox->parameters.setFloatValue(controllers[i]->value, i);
					inspectorBox->parameters.setFloatLerpValue(controllers[i]->value, i);
					markCueDraftDirty(cueSelectedIndex());
				}
				// Exposed controllers (i >= inspectorBox->parameters.getSize()): update via their own parameter pointer
				else if (i >= inspectorBox->parameters.getSize())
				{
					// Exposed controller: update the value via its internal parameter pointer
					JPParameter *parameter = controllers[i]->parameters;
					if(complex==nullptr)continue;
					float newVal=complex->valueFromMouse(ofGetMouseX());
					controllers[i]->value = newVal;
					controllers[i]->parameters->floatValue = newVal;
					controllers[i]->parameters->floatLerpValue = newVal;
				}
				// Exposed controllers: value update is handled by JPSlider::draw() via the internal parameter pointer
			}
		}
		// Si invertimos el for en este crashea. habra que cambiarlo en otro lugar tambien?
		if (!slideragarrado && inspectorBox != nullptr &&
			inspectorBodyContains(ofGetMouseX(), ofGetMouseY()))
		{
			for (int i = 0; i < controllers.size(); i++)
			{
				auto *complex=dynamic_cast<JPComplexSlider *>(controllers[i]);
				if (complex!=nullptr && complex->valueSliderContains(
					ofGetMouseX(),ofGetMouseY()) &&
					mousebutton == OF_MOUSE_BUTTON_LEFT)
				{
					// Normal controllers: check type from inspectorBox; exposed controllers handled by their own mouseOver in update_paramswindow
					if (i < inspectorBox->parameters.getSize() &&
						inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
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

bool JPboxgroup::handleMediaInspectorClick()
{
	auto *target=dynamic_cast<JPMediaInspectable *>(getInspectorBox());
	if(target==nullptr || !mediaInspector.card.inside(ofGetMouseX(),ofGetMouseY()))return false;
	JPMediaState&s=target->mediaState(); const ofVec2f m(ofGetMouseX(),ofGetMouseY());
	if(!target->mediaReady() && !mediaInspector.fit.inside(m))return true;
	// Click-away releases the IN/OUT time field. It used to persist until ESC
	// or Enter, which was invisible while nothing routed keys to it - now that
	// something does, a focus left set swallows every keystroke.
	if(!mediaInspector.inField.inside(m) && !mediaInspector.outField.inside(m))
	{
		mediaTimeFieldFocus=0;
		mediaTimeFieldBuffer.clear();
	}
	bool changed=true;
	if(mediaInspector.fit.inside(m)){mediaRangeDragging=5;s.fitMode=(JPMediaFitMode)ofClamp((int)((m.x-mediaInspector.fit.x)/mediaInspector.fit.width*5.0f),0,4);}
	else if(mediaInspector.restart.inside(m))target->mediaRestart();
	else if(mediaInspector.previous.inside(m))target->mediaStep(-1);
	else if(mediaInspector.play.inside(m))s.playing=!s.playing;
	else if(mediaInspector.next.inside(m))target->mediaStep(1);
	else if(mediaInspector.direction.inside(m))s.reverse=!s.reverse;
	else if(mediaInspector.loop.inside(m))s.loopMode=(JPMediaLoopMode)(((int)s.loopMode+1)%3);
	else if(mediaInspector.speed.inside(m)){mediaRangeDragging=3;s.rate=ofMap(m.x,mediaInspector.speed.x,mediaInspector.speed.getRight(),.25f,4.0f,true);}
	else if(mediaInspector.mute.inside(m)&&target->mediaHasAudio())s.muted=!s.muted;
	else if(mediaInspector.volume.inside(m)&&target->mediaHasAudio()){mediaRangeDragging=4;s.volume=ofMap(m.x,mediaInspector.volume.x,mediaInspector.volume.getRight(),0,1,true);}
	else if(mediaInspector.inButton.inside(m))jp_media::captureRangeIn(s);
	else if(mediaInspector.outButton.inside(m))jp_media::captureRangeOut(s);
	else if(mediaInspector.inField.inside(m)||mediaInspector.outField.inside(m))
	{
		mediaTimeFieldFocus=mediaInspector.inField.inside(m)?1:2;
		mediaTimeFieldBuffer=mediaTimeLabel((mediaTimeFieldFocus==1?s.rangeIn:s.rangeOut)*target->mediaDurationSeconds());
		mediaTimeFieldReplaceOnType=true;
	}
	else if(mediaInspector.timeline.inside(m))
	{
		const float n=ofMap(m.x,mediaInspector.timeline.x,mediaInspector.timeline.getRight(),0,1,true);
		const float tolerance=7.0f/mediaInspector.timeline.width;
		const bool nearIn=std::abs(n-s.rangeIn)<=tolerance;
		const bool nearOut=std::abs(n-s.rangeOut)<=tolerance;
		if(nearIn&&nearOut)mediaRangeDragging=n<s.rangeIn?1:2;
		else if(nearIn)mediaRangeDragging=1;
		else if(nearOut)mediaRangeDragging=2;
		else {mediaTimelineDragging=true;target->mediaSeek(ofClamp(n,s.rangeIn,s.rangeOut));}
	}
	else changed=false;
	if(changed){jp_media::normalize(s);markCueDraftDirty(cueSelectedIndex());if(isCueDraftMode())updateCueDraftGraph();}
	return true;
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

	JPbox *inputInspectorBox = getInspectorBox();
	if (mouseButton == OF_MOUSE_BUTTON_LEFT && inputInspectorBox != nullptr &&
		handleMediaInspectorClick()) return;
	if (mouseButton == OF_MOUSE_BUTTON_LEFT && inputInspectorBox != nullptr &&
		handleInspectorScrollbarPressed(ofGetMouseX(), ofGetMouseY()))
	{
		return;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		inputInspectorBox != nullptr && mouseOverGui() &&
		handleAdvancedMappingParameterHeaderClick(inputInspectorBox))
	{
		return;
	}

	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		inputInspectorBox != nullptr && mouseOverGui() &&
		handleInspectorInputClick(inputInspectorBox))
	{
		return;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		inputInspectorBox != nullptr && mouseOverGui() &&
		handleInspectorRangeClick())
	{
		return;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		inputInspectorBox != nullptr && mouseOverGui() &&
		handleInspectorLockClick())
	{
		return;
	}
	if (mouseButton == OF_MOUSE_BUTTON_LEFT &&
		inputInspectorBox != nullptr && mouseOverGui() &&
		handleInspectorAutomationClick())
	{
		return;
	}

	// In group view mode: handle click on sub-box, deselect on empty space, and handle outlet dragging
	if (isGroupViewActive() && mouseButton == OF_MOUSE_BUTTON_LEFT)
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr)
		{
			JPdragobject::setMouseOverride(canvasMouse);
			bool hitOutlet = false;
			bool hitBox = false;
			int clickedIndex = -1;

			// First check outlets (like MAIN view does at line 1581)
			for (int i = (int)preset->boxes.size() - 1; i >= 0; i--)
			{
				if (preset->boxes[i]->mouseOverOutlet())
				{
					hitOutlet = true;
					clearSelection();
					preset->boxes[i]->outletActiveFlag = true;
					ouletagarrado = true;
					shaderboxagarrado = false;
					outlet_cualestaagarrado = i;
					cualestaagarrado = -1;
					break;
				}
			}

			// Then check box body (non-outlet area)
			if (!hitOutlet)
			{
				for (int i = 0; i < (int)preset->boxes.size(); i++)
				{
					if (preset->boxes[i]->mouseOver() && !preset->boxes[i]->mouseOverOutlet())
					{
						hitBox = true;
						clickedIndex = i;
						break;
					}
				}
			}

			JPdragobject::clearMouseOverride();

			if (hitOutlet)
			{
				return;
			}

			if (hitBox && clickedIndex >= 0)
			{
				// Match main view behavior: ctrl toggles this one box, and
				// otherwise clicking a box that is not part of the selection
				// replaces it.
				if (selectionToggleModifier())
				{
					toggleBoxSelection(clickedIndex);
				}
				else if (!isBoxSelected(clickedIndex))
				{
					clearSelection();
				}
				// Single click: select for inspector (blue box, like openguinumber in main view)
				groupInspectorIndex = clickedIndex;
				groupPreviewBoxIndex = -1;
				setControllers();
				// Double-click: activate the box in the graph currently on screen.
				if (isDoubleClick)
				{
					requestSetActiveRenderForCurrentView(clickedIndex);
				}
				return; // Don't process main boxes
			}
			else if (!hitBox && !mouseOverGui())
			{
				groupInspectorIndex = -1;
				groupPreviewBoxIndex = -1;
				setControllers();
				// Same rule as the main view; the two must not drift.
				vector<int> keep;
				if (selectionAddModifier()) keep = selectedBoxIndices;
				clearSelection();
				selectionBase = keep;
				selectedBoxIndices = keep;
				draw_SelectionRect = true;
				lastMouseClick = canvasMouse;
				selectionEnd = lastMouseClick;
			}
		}
	}

	bool arafue = false; // POR SI NO TOCO NINGUN ELEMENTO;

	if (mouseButton == OF_MOUSE_BUTTON_RIGHT && isCueDraftMode() && cueTargetsCurrentView() && !mouseOverGui())
	{
		JPdragobject::setMouseOverride(canvasMouse);
		// Hit-test the graph currently on screen (== the cue's target graph).
		vector<JPbox *> &tboxes = getCueTargetBoxes();
		for (int i = (int)tboxes.size() - 1; i >= 0; i--)
		{
			if (tboxes[i]->mouseOver() && (isCueDraftDirty(i) || isCueAddedRealIndex(i)))
			{
				JPdragobject::clearMouseOverride();
				revertCueDraftBox(i);
				viewportPanning = false;
				return;
			}
		}
		JPdragobject::clearMouseOverride();
	}

	// Space + drag pans, as in every design program. Space is read HERE, when the
	// drag is armed, and never again for the rest of the gesture: the drag path
	// below keys off viewportPanning instead. Re-reading the key mid-drag would
	// let a marquee that had already started turn into a pan, leaving
	// draw_SelectionRect armed but no longer updated - so releasing would apply
	// a stale selection.
	//
	// Reaching the same viewportPanning flag as the middle/right drag is what
	// makes this small: box grabbing is already suppressed while it is set, and
	// it is already cleared on release.
	if ((mouseButton == OF_MOUSE_BUTTON_MIDDLE ||
		mouseButton == OF_MOUSE_BUTTON_RIGHT ||
		isSpacePanHeld()) && !mouseOverGui())
	{
		viewportPanning = true;
		return;
	}

	// SI EL MOUSE ESTA DENTRO DEL INSPECTOR WINDOW PAPA.
	JPbox *inspectorBox = getInspectorBox();
	// The canvas must yield to any overlay, not only to the inspector. This was
	// `mouseOverGui() && inspectorBox != nullptr`, so with the cue panel or the
	// mapping panel open and NO box selected the condition was false and the
	// else-branch hit-tested the boxes straight through the panel.
	if (mouseOverGui() && inspectorBox == nullptr)
	{
		arafue = true;
	}
	else if (mouseOverGui() && inspectorBox != nullptr)
	{
		arafue = true;
		if (mappingbutton.mouseGrab() && isMappingShaderBox(inspectorBox))
		{
			toggleMappingEdit();
			return;
		}
		if (inspectorsetactive.mouseGrab())
		{
			requestSetActiveRenderForCurrentView(getCurrentViewSelectedIndex());
		}
		if (inspectorreload.mouseGrab())
		{
			// reloadActiveshader();
		}
		if (inspectorrandom.mouseGrab())
		{
			bool randomized = false;
			for (JPParameter *parameter : getInspectorActionParameters())
			{
				if (parameter->variabletype == JPParameter::FLOAT &&
					!parameter->randomLocked)
				{
					parameter->floatValue = ofRandom(parameter->effectiveMin(),
						parameter->effectiveMax());
					parameter->floatLerpValue = parameter->floatValue;
					randomized = true;
				}
			}
			if (randomized)
			{
				markCueDraftDirty(cueSelectedIndex());
				if (isCueDraftMode())
				{
					updateCueDraftGraph();
				}
			}
		}
		if (inspectordefault.mouseGrab())
		{
			for (JPParameter *parameter : getInspectorActionParameters())
				parameter->restoreDefaultValue();
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode()) updateCueDraftGraph();
			setControllers();
		}
		if (inspectorsavedefault.mouseGrab())
		{
			for (JPParameter *parameter : getInspectorActionParameters())
				parameter->captureDefaultValue();
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode()) updateCueDraftGraph();
		}
		if (tooutputbutton.mouseGrab())
		{
			// inspectorBox is the cue DRAFT during a cue and the preset child
			// in group view, which is what makes a group child markable at all.
			// The draft carries the live box's uid, so the binding stays put
			// and the flag rides the normal draft->live commit.
			inspectorBox->setOutputCandidate(
				!inspectorBox->getOutputCandidate());
			markCueDraftDirty(cueSelectedIndex());
			if (isCueDraftMode()) updateCueDraftGraph();
			return;
		}
		if (camerarefreshbutton.mouseGrab() &&
			inspectorBox->getTipo() == inspectorBox->CAMBOX)
		{
			JPbox_cam::rescanCameraDevices();
			return;
		}
		if (camerarefreshbutton.mouseGrab() &&
			inspectorBox->getTipo() == inspectorBox->KINECT2BOX)
		{
			if (auto *kinect = dynamic_cast<JPbox_kinect2 *>(inspectorBox))
				kinect->requestReconnect();
			return;
		}
		if (auto *kinect = dynamic_cast<JPbox_kinect2 *>(inspectorBox))
		{
			for (int i = 0; i < 3; ++i)
			{
				if (kinectStreamButtons[i].inside(ofGetMouseX(), ofGetMouseY()))
				{
					kinect->setStream((JPbox_kinect2::Stream)i);
					markCueDraftDirty(cueSelectedIndex());
					return;
				}
			}
		}
		if (editbutton.mouseGrab() && shaderEditor != nullptr)
		{
			string shaderDir = inspectorBox->dir;
			string shaderName = inspectorBox->name;
			if (!shaderDir.empty()) {
				shaderEditor->openShader(shaderDir, shaderName, openguinumber);
			}
		}
		// Checkeo todos los controles.
		bool isovercontrol = false;
		// ESTE PARECE QUE NO HACE QUE CRASHEE COMO EL RESTO DE LOS CONTROLADORES
		int index = 0; // INDEX PARA RECORRER LOS BOTONES :
		int activeone = -1;
		for (int i = 0; i < controllers.size(); i++)
		{
			float mx = ofGetMouseX();
			float my = ofGetMouseY();
			auto *complex=dynamic_cast<JPComplexSlider *>(controllers[i]);
			const bool fallbackControlHit=complex==nullptr &&
				mx>=controllers[i]->x-controllers[i]->width*0.5f &&
				mx<=controllers[i]->x+controllers[i]->width*0.5f &&
				my>=controllers[i]->y-controllers[i]->height*0.5f &&
				my<=controllers[i]->y+controllers[i]->height*0.5f;
			bool overVisualSlider = inspectorBodyContains(mx, my) &&
				((complex!=nullptr && complex->valueSliderContains(mx,my)) ||
				 fallbackControlHit);
			if (overVisualSlider)
			{
				// cout << "MOUSE OVER " << controllers[i]->name << endl;
				isovercontrol = true;
				// Exposed controllers have i >= inspectorBox->parameters.getSize()
				// and are handled by their own JPComplexSlider::update()
				if (i < inspectorBox->parameters.getSize())
				{
					if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
					{
						controllers[i]->activeFlag = true;
						activeone = i;
					}
					else if (inspectorBox->parameters.getType(i) == inspectorBox->parameters.BOOL)
					{
						inspectorBox->parameters.setBoolValue(controllers[i]->boolValue, i);
						markCueDraftDirty(cueSelectedIndex());
					}
				}
			}
			// Only count normal FLOAT controllers for the index
			if (i < inspectorBox->parameters.getSize() &&
				inspectorBox->parameters.getType(i) == inspectorBox->parameters.FLOAT)
			{
				index++;
			}
		}
		// PONGO EN FALSE TODOS LOS QUE NO TENGO ACTIVOS:
		for (int i = 0; i < controllers.size(); i++)
		{
			if (i != activeone || i >= inspectorBox->parameters.getSize())
			{
				controllers[i]->activeFlag = false;
			}
			else
			{
				// Esto es porque si esta en movtype 0 no actualiza para que no pise con el OSC , entonces hay que actualizarlo manualmente
				// Esta es la actualizaci�n manual. Acordate que si el movtype esta en 0 el valor NO SE ACTUALIZA.
				auto *complex=dynamic_cast<JPComplexSlider *>(controllers[i]);
				if(complex==nullptr)continue;
				controllers[i]->value=complex->valueFromMouse(ofGetMouseX());
				inspectorBox->parameters.setFloatValue(controllers[i]->value, i);
				inspectorBox->parameters.setFloatLerpValue(controllers[i]->value, i);
				markCueDraftDirty(cueSelectedIndex());
			}
		}
		if (mouseButton == 2 && isDoubleClick)
		{
			cout << "DOBLE CLICK " << endl;
			for (JPParameter *parameter : getInspectorActionParameters())
			{
				if (parameter->variabletype == JPParameter::FLOAT &&
					!parameter->randomLocked)
				{
					const float rdm = ofRandom(parameter->effectiveMin(),
						parameter->effectiveMax());
					parameter->floatLerpValue = rdm;
					parameter->floatValue = rdm;
					markCueDraftDirty(cueSelectedIndex());

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
				if (selectionToggleModifier())
				{
					// Ctrl click adds or removes this one box and leaves the
					// rest of the selection alone.
					toggleBoxSelection(i);
				}
				else if (!isBoxSelected(i))
				{
					clearSelection();
				}
				openguinumber = i;
				boxes[i]->activeFlag = true;
				if (!mouseOverGui())
				{
					setControllers();
				}
		if (isDoubleClick)
		{
			//*activerender = i;
			requestSetActiveRenderForCurrentView(i);
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
			// Shift keeps what was already selected and the marquee adds to it.
			// Without the modifier the drag replaces, which is the old
			// behaviour and still the default.
			const bool addToSelection = selectionAddModifier();
			vector<int> keep;
			if (addToSelection) keep = selectedBoxIndices;
			clearSelection();
			selectionBase = keep;
			// Clear activeFlag on all boxes when deselecting
			for (int i = 0; i < (int)boxes.size(); i++)
			{
				boxes[i]->activeFlag = addToSelection && isBoxSelected(i);
			}
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
	if (mouseButton == OF_MOUSE_BUTTON_LEFT)
	{
		mediaTimelineDragging = false;
		mediaRangeDragging = 0;
		if (rangeDragSlider != nullptr)
		{
			rangeDragSlider->rangeHandleDragging = false;
			rangeDragSlider->slider_value.activable2 = true;
		}
		rangeDragSlider = nullptr;
		rangeDragHandle = 0;
		audioShapingDragSlider = nullptr;
		audioShapingDragControl = JPComplexSlider::AUDIO_SHAPING_NONE;
		inspectorScrollbarDragging = false;
	}
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
	if (mappingEditActive &&
		updateAdvancedMappingMouseScrolled(x, y, scrollY))
	{
		return true;
	}
	if (scrollY != 0.0f && getInspectorBox() != nullptr &&
		inspectorBodyContains(x, y))
	{
		if (inspectorMaxScrollY > 0.0f)
		{
			const float previous = inspectorScrollY;
			inspectorScrollY = ofClamp(inspectorScrollY - scrollY * 36.0f,
				0.0f, inspectorMaxScrollY);
			if (std::abs(previous - inspectorScrollY) > 0.01f)
				setControllers();
		}
		return true;
	}
	if (scrollY == 0 || mouseOverGui())
	{
		return false;
	}

	float zoomFactor = scrollY > 0 ? 1.1f : 1.0f / 1.1f;
	float oldZoom = viewportZoom;
	zoomViewport(ofVec2f(x, y), zoomFactor);
	return viewportZoom != oldZoom;
}

bool JPboxgroup::inspectorBodyContains(float x, float y) const
{
	return inspectorBodyViewport.width > 0.0f &&
		inspectorBodyViewport.inside(x, y);
}

bool JPboxgroup::handleInspectorScrollbarPressed(float x, float y)
{
	if (inspectorMaxScrollY <= 0.0f ||
		!inspectorScrollbarTrack.inside(x, y)) return false;
	if (inspectorScrollbarThumb.inside(x, y))
	{
		inspectorScrollbarDragOffset = y - inspectorScrollbarThumb.y;
	}
	else
	{
		inspectorScrollbarDragOffset = inspectorScrollbarThumb.height * 0.5f;
		const float travel = inspectorScrollbarTrack.height -
			inspectorScrollbarThumb.height;
		if (travel > 0.0f)
		{
			const float thumbY = ofClamp(y - inspectorScrollbarDragOffset,
				inspectorScrollbarTrack.y,
				inspectorScrollbarTrack.getBottom() - inspectorScrollbarThumb.height);
			inspectorScrollY = (thumbY - inspectorScrollbarTrack.y) /
				travel * inspectorMaxScrollY;
			setControllers();
		}
	}
	inspectorScrollbarDragging = true;
	return true;
}

void JPboxgroup::setInspectorScrollNormalized(float normalized)
{
	inspectorScrollY = ofClamp(normalized, 0.0f, 1.0f) * inspectorMaxScrollY;
	setControllers();
}
void JPboxgroup::armParameterMorph(JPbox *outgoing, JPbox *incoming)
{
	clearParameterMorph();
	morphOutgoing = nullptr;
	morphIncoming = nullptr;
	if (outgoing == nullptr || incoming == nullptr || outgoing == incoming)
		return;

	// Name-only matching. copyParametersByNameOrIndex is the existing precedent
	// but falls back to POSITION when a name is absent, which is right for cue
	// drafts - same shader, same array - and wrong here: two different shaders
	// would have unrelated uniforms paired by array index. A parameter with no
	// counterpart simply does not morph; the image crossfade still covers it.
	bool matchedAny = false;
	for (int i = 0; i < incoming->parameters.getSize(); ++i)
	{
		if (incoming->parameters.getType(i) != JPParameter::FLOAT) continue;
		const string name = incoming->parameters.getName(i);
		const int outIndex = outgoing->parameters.indexOfName(name);
		if (outIndex < 0) continue;
		if (outgoing->parameters.getType(outIndex) != JPParameter::FLOAT)
			continue;

		JPParameter *inParam = incoming->parameters.getJParameter(i);
		JPParameter *outParam = outgoing->parameters.getJParameter(outIndex);
		if (inParam == nullptr || outParam == nullptr) continue;

		// Each side aims at the other's CURRENT emitted value, captured now so
		// a parameter that is also being animated does not chase a moving
		// target for the length of the fade.
		const float incomingValue = inParam->floatValue;
		const float outgoingValue = outParam->floatValue;
		// Incoming starts wearing the outgoing look and returns to its own;
		// outgoing leaves wearing the incoming one. Amounts are set per frame.
		inParam->setMorph(outgoingValue, 1.0f);
		outParam->setMorph(incomingValue, 0.0f);
		matchedAny = true;
	}
	if (!matchedAny) return;
	morphOutgoing = outgoing;
	morphIncoming = incoming;
}

void JPboxgroup::clearParameterMorph()
{
	// Returning the amount to 0 is the whole restore: nothing permanent was
	// ever written, so both boxes emit their own values again from the next
	// tick. Guarded against a box that has since been deleted.
	auto clearOn = [this](JPbox *box)
	{
		if (box == nullptr) return;
		if (std::find(boxes.begin(), boxes.end(), box) == boxes.end()) return;
		for (int i = 0; i < box->parameters.getSize(); ++i)
		{
			JPParameter *parameter = box->parameters.getJParameter(i);
			if (parameter != nullptr) parameter->clearMorph();
		}
	};
	clearOn(morphOutgoing);
	clearOn(morphIncoming);
	morphOutgoing = nullptr;
	morphIncoming = nullptr;
}

void JPboxgroup::updateParameterMorph()
{
	if (morphOutgoing == nullptr && morphIncoming == nullptr) return;
	if (transition.getLerpValue() >= 1.0f)
	{
		clearParameterMorph();
		return;
	}
	// The same eased progress the crossfade uses, so pixels and parameters
	// stay in step rather than drifting apart mid-fade.
	const float t = transition.getLerpValue();
	const float eased = t * t * (3.0f - 2.0f * t);
	auto setAmount = [this](JPbox *box, float amount)
	{
		if (box == nullptr) return;
		if (std::find(boxes.begin(), boxes.end(), box) == boxes.end()) return;
		for (int i = 0; i < box->parameters.getSize(); ++i)
		{
			JPParameter *parameter = box->parameters.getJParameter(i);
			if (parameter != nullptr && parameter->isMorphing())
				parameter->morphAmount = amount;
		}
	};
	// Outgoing pulls toward the incoming look as it fades; incoming lets go of
	// it as it arrives.
	setAmount(morphOutgoing, eased);
	setAmount(morphIncoming, 1.0f - eased);
}

void JPboxgroup::setTransitionDurationMs(float _ms)
{
	transition.setDurationMs(_ms);
}

float JPboxgroup::getTransitionDurationMs() const
{
	return transition.getDurationMs();
}

void JPboxgroup::setTransitionType(int _type)
{
	transition.setType(_type);
}

int JPboxgroup::getTransitionType() const
{
	return transition.getType();
}

void JPboxgroup::updateTransition(int _idx) {

//	cout << "UPDATE TRANSITION " << endl;
	if (boxes.size() >= 1) {
		_idx = ofClamp(_idx, 0, int(boxes.size()) - 1);
		bool activeRenderChanged = _idx != *activerender;
		JPbox *outgoingBox = boxes[*activerender];
		if (&boxes[*activerender]->fbo != 0) {
			transition.setFboPointer1(&boxes[*activerender]->fbo);
		}
		*activerender = _idx;

		if (&boxes[*activerender]->fbo != 0) {
			transition.setFboPointer2(&boxes[*activerender]->fbo);
		}
		transition.setLerpValue(0);
		if (activeRenderChanged)
			armParameterMorph(outgoingBox, boxes[*activerender]);
		else
			clearParameterMorph();
		if (activeRenderChanged)
		{
			requestCueRebuild();
		}
	}
}

int JPboxgroup::getCurrentViewBoxCount() const
{
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		return preset != nullptr ? (int)preset->boxes.size() : 0;
	}
	return (int)boxes.size();
}

int JPboxgroup::getCurrentViewSelectedIndex() const
{
	return isGroupViewActive() ? groupInspectorIndex : openguinumber;
}

int JPboxgroup::getCurrentViewActiveRenderIndex() const
{
	if (isGroupViewActive())
	{
		JPbox_preset *draftPreset = getDraftPresetForCurrentView();
		if (draftPreset != nullptr)
		{
			return draftPreset->activeRender;
		}
		JPbox_preset *preset = getActivePreset();
		return preset != nullptr ? preset->activeRender : -1;
	}
	if (isCueDraftMode() && cueState.stagedActiveRenderIndex >= 0)
	{
		return cueState.stagedActiveRenderIndex;
	}
	return activerender != nullptr ? *activerender : -1;
}

bool JPboxgroup::selectOpenBoxForCurrentView(int index)
{
	if (index < 0 || index >= getCurrentViewBoxCount())
	{
		return false;
	}
	if (isGroupViewActive())
	{
		groupInspectorIndex = index;
	}
	else
	{
		openguinumber = index;
	}
	setControllers();
	return true;
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

void JPboxgroup::setPresetActiveOnlyBox(JPbox_preset *preset, int index)
{
	if (preset == nullptr)
	{
		return;
	}
	for (int i = (int)preset->boxes.size() - 1; i >= 0; i--)
	{
		if (preset->boxes[i] != nullptr)
		{
			preset->boxes[i]->setonoff(i == index);
		}
	}
}

bool JPboxgroup::requestSetActiveRenderForCurrentView(int index, bool activeOnly)
{
	if (!isGroupViewActive())
	{
		return requestSetActiveRender(index, activeOnly);
	}

	JPbox_preset *preset = getActivePreset();
	if (preset == nullptr || index < 0 || index >= (int)preset->boxes.size() ||
		preset->boxes[index] == nullptr)
	{
		return false;
	}

	if (isCueDraftMode())
	{
		JPbox_preset *draftPreset = getDraftPresetForCurrentView();
		if (draftPreset == nullptr || index >= (int)draftPreset->boxes.size() ||
			draftPreset->boxes[index] == nullptr)
		{
			return false;
		}
		draftPreset->activeRender = index;
		if (activeOnly)
		{
			setPresetActiveOnlyBox(draftPreset, index);
		}
		if (!activeGroupPath.empty())
		{
			markCueDraftDirty(activeGroupPath[0], CUE_DIRTY_PRESET_ACTIVE);
		}
		updateCueDraftGraph();
		return true;
	}

	preset->activeRender = index;
	if (activeOnly)
	{
		setPresetActiveOnlyBox(preset, index);
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
		data.appendChild("uid").set(boxes[i]->uid);
		data.appendChild("tooutput").set(boxes[i]->getOutputCandidate());
		data.appendChild("onoff").set(boxes[i]->getonoff());
		data.appendChild("bypass").set(boxes[i]->getBypass());
		boxes[i]->saveCustomState(data);
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
					saveParameterUserState(param,
						boxes[i]->parameters.getJParameter(k));
				}
				else
				{
					// string name = boxes[i]->parameters.getName(k);
					auto param = parameters.appendChild("param");
					// param.set(boxes[i]->parameters.getFloatValue(k));
					param.appendChild("name").set(boxes[i]->parameters.getName(k));
					param.appendChild("min").set(boxes[i]->parameters.getRangeMin(k));
					param.appendChild("max").set(boxes[i]->parameters.getRangeMax(k));
					param.appendChild("value").set(boxes[i]->parameters.getFloatValue(k));
					param.appendChild("movtype").set(boxes[i]->parameters.getMovType(k));
					param.appendChild("lastmovtype").set(boxes[i]->parameters.getLastMovType(k));
					saveParameterUserState(param,
						boxes[i]->parameters.getJParameter(k));
					param.appendChild("speed").set(boxes[i]->parameters.getSpeed(k));
					param.appendChild("bpmrate").set(boxes[i]->parameters.getBpmRate(k));
					param.appendChild("audiosource").set(boxes[i]->parameters.getAudioSource(k));
					param.appendChild("audiodiv").set(boxes[i]->parameters.getAudioDiv(k));
					param.appendChild("audiobase").set(boxes[i]->parameters.getAudioBase(k));
					param.appendChild("audioamount").set(boxes[i]->parameters.getAudioAmount(k));
					param.appendChild("audioinvert").set(boxes[i]->parameters.getAudioInvert(k));
					param.appendChild("audiothreshold").set(boxes[i]->parameters.getAudioThreshold(k));
					param.appendChild("audiocurve").set(boxes[i]->parameters.getAudioCurve(k));
					param.appendChild("audioattackms").set(boxes[i]->parameters.getAudioAttackMs(k));
					param.appendChild("audioreleasems").set(boxes[i]->parameters.getAudioReleaseMs(k));
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

	ofFilePath::createEnclosingDirectory(outputPath);
	xml.save(outputPath);

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
				preset->save();
			}
		}
	}
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
	// Kept in lockstep with `boxes`, so the link pass below can pair a box
	// with the node it came from even when some nodes produce no box.
	vector<ofXml> loadedBoxNodes;

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

		JPbox *bx = nullptr;
		if (directory.getValue().find(".frag") != std::string::npos)
		{
			bx = new JPbox_shader();
		}
		else if (jp_media::isImage(directory.getValue()))
		{
			bx = new JPbox_image();
		}
		else if (jp_media::isVideo(directory.getValue()))
		{
			bx = new JPbox_video();
		}
		else if (directory.getValue().find("kinect2") != std::string::npos)
		{
			bx = new JPbox_kinect2();
		}
		else if (directory.getValue().find("pointercloud") != std::string::npos)
		{
			bx = new JPbox_pointercloud();
		}
		// BEFORE the plain "cam" test: "camdepth" contains "cam", so the looser
		// check would swallow it and rebuild a CAMARITA on load - the box would
		// create correctly and then come back as the wrong type after a save.
		else if (directory.getValue().find("camdepth") != std::string::npos)
		{
			bx = new JPbox_camdepth();
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

		bx->setup(jp_normalizePath(directory.getValue()), nombre.getValue());
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

		int positionalIndex = 0;
		auto parameters = box.getChild("parameters").getChildren();
		int parameterLoadLimit = bx->parameters.getSize();
		// cout << "PARAMETER SIZE SB " << sb->parameters.getSize() << endl;

		for (auto &param : parameters) 
		{
			// By name, falling back to position. Purely positional loading
			// froze the parameter arrays: reordering, or inserting anywhere but
			// the end, shifted every value in every saved composition.
			const int destinationIndex = bx->parameters.resolveLoadIndex(
				param.getChild("name").getValue(), positionalIndex);
			++positionalIndex;
			// continue, not break: a later <param> may still match by name even
			// when this one resolves nowhere.
			if (destinationIndex < 0 || destinationIndex >= parameterLoadLimit)
				continue;

			if (bx->parameters.getType(destinationIndex) == bx->parameters.FLOAT)
			{
				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setRangeMin(param.getChild("min").getFloatValue(), destinationIndex);
				bx->parameters.setRangeMax(param.getChild("max").getFloatValue(), destinationIndex);
				bx->parameters.setFloatLerpValue(param.getChild("value").getFloatValue(), destinationIndex);
				bx->parameters.setFloatValue(param.getChild("value").getFloatValue(), destinationIndex);
				bx->parameters.setmovetype(param.getChild("movtype").getIntValue(), destinationIndex);
				auto lastMoveType = param.getChild("lastmovtype");
				if (lastMoveType)
				{
					bx->parameters.setlastmovetype(
						lastMoveType.getIntValue(), destinationIndex);
				}
				bx->parameters.setSpeed(param.getChild("speed").getFloatValue(), destinationIndex);
				auto bpmRate = param.getChild("bpmrate");
				if (bpmRate)
				{
					bx->parameters.setBpmRate(bpmRate.getIntValue(), destinationIndex);
				}
				auto audioSource = param.getChild("audiosource");
				if (audioSource)
				{
					bx->parameters.setAudioSource(audioSource.getIntValue(), destinationIndex);
				}
				auto audioDiv = param.getChild("audiodiv");
				if (audioDiv)
				{
					bx->parameters.setAudioDiv(audioDiv.getIntValue(), destinationIndex);
				}
				auto loadAudioFloat = [&](const char *key, auto setter)
				{
					auto node = param.getChild(key);
					if (node) (bx->parameters.*setter)(node.getFloatValue(), destinationIndex);
				};
				loadAudioFloat("audiobase", &JPParameterGroup::setAudioBase);
				loadAudioFloat("audioamount", &JPParameterGroup::setAudioAmount);
				auto audioInvert = param.getChild("audioinvert");
				if (audioInvert) bx->parameters.setAudioInvert(audioInvert.getBoolValue(), destinationIndex);
				loadAudioFloat("audiothreshold", &JPParameterGroup::setAudioThreshold);
				loadAudioFloat("audiocurve", &JPParameterGroup::setAudioCurve);
				loadAudioFloat("audioattackms", &JPParameterGroup::setAudioAttackMs);
				loadAudioFloat("audioreleasems", &JPParameterGroup::setAudioReleaseMs);
			}
			else if (bx->parameters.getType(destinationIndex) == bx->parameters.BOOL)
			{
				bx->parameters.setName(param.getChild("name").getValue());
				bx->parameters.setBoolValue(param.getChild("value").getBoolValue(), destinationIndex);
			}
			loadParameterUserState(param,
				bx->parameters.getJParameter(destinationIndex));
		}
		bx->loadCustomState(box);

	
#ifdef SPOUT
		if (bx->getTipo() == 4) {
			cout << "RELOAD CAJA DE SPOUT " << endl;
			bx->reload();
		}
#endif

		boxes.push_back(bx);
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

	//CLAUSULA DE SEGURIDAD : 
	*activerender = int(ofClamp(xml.getChild("activerender").getIntValue(),0,boxes.size()-1));
	cout << "TERMINA LINKS DE LOS FBO " << endl;

	updateTransition(*activerender);




	//}
	// activerender_loader.getIntValue();
	// activerender = activerender_loader.getIntValue();

	// Hand-edited XML, or the same group .xml placed twice, can deliver
	// duplicate identities. Shallowest box keeps the uid; the rest are re-minted.
	repairBoxUids();
}
vector<JPParameter *> JPboxgroup::getInspectorActionParameters() const
{
	vector<JPParameter *> result;
	for (JPcontroller *controller : controllers)
	{
		JPParameter *parameter = controller != nullptr ?
			controller->parameters : nullptr;
		if (parameter != nullptr &&
			std::find(result.begin(), result.end(), parameter) == result.end())
		{
			result.push_back(parameter);
		}
	}
	return result;
}

void JPboxgroup::setControllers(){
	audioShapingDragSlider = nullptr;
	audioShapingDragControl = JPComplexSlider::AUDIO_SHAPING_NONE;
	rangeDragSlider = nullptr;
	rangeDragHandle = 0;

	for (int i = 0; i < controllers.size(); i++)
	{
		delete controllers[i];
		controllers[i] = nullptr;
	}
	controllers.clear();
	parameterLockButtons.clear();
	parameterRangeButtons.clear();
	inspectorColorSwatches.clear();

	// Clean up expose buttons
	for (int i = 0; i < exposeButtons.size(); i++)
	{
		delete exposeButtons[i];
		exposeButtons[i] = nullptr;
	}
	exposeButtons.clear();
	inspectorInputRows.clear();
	advancedMappingParameterHeaders.clear();
	inspectorInputsHeaderBounds.set(0, 0, 0, 0);
	mediaInspector.clear();
	mediaInspectorPlayableBuilt = false;

	JPbox *inspectorBox = getInspectorBox();
	if (inspectorBox == nullptr)
	{
		inspectorScrollOwner = nullptr;
		inspectorScrollY = inspectorMaxScrollY = inspectorContentHeight = 0.0f;
		inspectorHeaderBounds.set(0, 0, 0, 0);
		inspectorBodyViewport.set(0, 0, 0, 0);
		return;
	}
	if (inspectorScrollOwner != inspectorBox)
	{
		inspectorScrollOwner = inspectorBox;
		inspectorScrollY = 0.0f;
	}
	inspectorwindow_width = inspectorLayout.panelWidth;
	inspectorwindow_x = ofGetWidth() - inspectorwindow_width * 0.5f;
	// Measure against the largest body the window can display. Once content is
	// known below, the actual panel contracts to that measured height.
	inspectorwindow_height = ofGetHeight();
	inspectorwindow_y = inspectorwindow_height * 0.5f;
	const float panelLeft = inspectorwindow_x - inspectorwindow_width * 0.5f;
	inspectorHeaderBounds.set(panelLeft, 0.0f,
		inspectorwindow_width, inspectorLayout.headerHeight);
	inspectorBodyViewport.set(
		panelLeft + inspectorLayout.outerInset,
		inspectorLayout.headerHeight,
		inspectorwindow_width - inspectorLayout.outerInset * 2.0f,
		std::max(0.0f, ofGetHeight() - inspectorLayout.headerHeight -
			inspectorLayout.outerInset));

	JPbox_shader *advancedShader = dynamic_cast<JPbox_shader *>(inspectorBox);
	JPbox_shader::AdvancedMappingState *advancedState =
		advancedShader != nullptr && advancedShader->isAdvancedMappingShader() ?
		advancedShader->getAdvancedMappingState() : nullptr;
	int lastAdvancedLayer = -1;

	float slider_width = inspectorwindow_width * 3 / 4;
	float slider_height = inspectorLayout.minControlHeight;
	// Keep parameter controls clear of the per-row lock button. Group view
	// reserves an additional column for its existing expose button.
	const float rowActionReserve = isGroupViewActive() ? 90.0f : 60.0f;
	const float controllerWidth = std::max(1.0f,
		inspectorBodyViewport.width - rowActionReserve);
	const float controllerX = inspectorBodyViewport.x + controllerWidth * 0.5f;
	const float standardControllerHeight = inspectorwindow_sepy;
	const float controllerRowGap = inspectorLayout.rowGap;
	auto controllerHeightFor = [&](JPParameter *parameter) {
		return JPComplexSlider::requiredHeight(parameter,
			standardControllerHeight);
	};
	auto controllerYFor = [&](float cursorY, JPParameter *parameter) {
		return cursorY + controllerHeightFor(parameter) * 0.5f;
	};
	float layoutCursor = inspectorBodyViewport.y +
		inspectorLayout.contentPadding - inspectorScrollY;
	layoutMediaInspector(dynamic_cast<JPMediaInspectable *>(inspectorBox),
		layoutCursor);
	layoutCursor = layoutInspectorInputRows(inspectorBox, layoutCursor);
	if (inspectorBox->getTipo() == inspectorBox->KINECT2BOX)
	{
		const float left = inspectorBodyViewport.x + inspectorLayout.contentPadding;
		const float gap = 4.0f;
		const float width = (inspectorwindow_width - 20.0f - gap * 2.0f) / 3.0f;
		for (int i = 0; i < 3; ++i)
		{
			kinectStreamButtons[i].set(left + i * (width + gap),
				layoutCursor, width, 26.0f);
		}
		layoutCursor += 54.0f + controllerRowGap;
	}
	else
	{
		for (ofRectangle &button : kinectStreamButtons) button.set(0, 0, 0, 0);
	}
	/*inspectorwindow_height += inspectorwindow_setactivesize;
	inspectorwindow_height += inspectorwindow_sepy * 0.5;
	*/
	auto *mediaBox = dynamic_cast<JPMediaInspectable *>(inspectorBox);
	auto isMediaOwnedParameter = [&](const string &parameterName) {
		return mediaBox != nullptr &&
			(parameterName == "strech" || parameterName == "speed" ||
			 parameterName == "position" || parameterName == "play");
	};
	// "scale ratio" is laid out first, above scalex.
	//
	// The array is NOT reordered to match - it cannot be, because
	// JPboxgroup::load fills parameters positionally and "scale ratio" is
	// appended last precisely so old savefiles keep working. Display order and
	// array order therefore differ here on purpose.
	//
	// That difference is why getBindableParameterOrder exists and must apply
	// the SAME sequence: when the two disagreed, a MIDI bind on the first row
	// drove the second parameter. Change one, change both.
	// Any box carrying a transform parameter lays its rows out in the canonical
	// order, not the array order. Media boxes always take this path because
	// their transport card owns four parameters that must be held out.
	const bool usesCanonicalOrder = mediaBox != nullptr ||
		[&] {
			for (int k = 0; k < inspectorBox->parameters.getSize(); ++k)
				if (jp_media::isRankedTransformParameter(
						inspectorBox->parameters.getName(k)))
					return true;
			return false;
		}();
	vector<float> mediaParameterTop(inspectorBox->parameters.getSize(), -1.0f);
	if (usesCanonicalOrder)
	{
		float mediaCursor = layoutCursor;
		auto place = [&](int k) {
			mediaParameterTop[k] = mediaCursor;
			mediaCursor += controllerHeightFor(
				inspectorBox->parameters.parameters[k]) + controllerRowGap;
		};
		auto visible = [&](int k) {
			return !isMediaOwnedParameter(inspectorBox->parameters.getName(k));
		};
		// Ranked first, in rank order; then everything else as declared.
		for (int rank = 0; rank < jp_media::kUnrankedTransformParameter; ++rank)
			for (int k = 0; k < inspectorBox->parameters.getSize(); ++k)
				if (visible(k) && jp_media::transformParameterRank(
						inspectorBox->parameters.getName(k)) == rank)
					place(k);
		for (int k = 0; k < inspectorBox->parameters.getSize(); ++k)
			if (visible(k) && !jp_media::isRankedTransformParameter(
					inspectorBox->parameters.getName(k)))
				place(k);
		layoutCursor = mediaCursor;
	}

	for (int k = 0; k < inspectorBox->parameters.getSize(); k++)
	{
		const string parameterName = inspectorBox->parameters.getName(k);
		const bool mediaOwned = isMediaOwnedParameter(parameterName);
		const int advancedLayer =
			getAdvancedMappingParameterLayer(parameterName);
		if (advancedState != nullptr && advancedLayer >= 0 &&
			advancedLayer != lastAdvancedLayer)
		{
			InspectorParameterGroupHeader header;
			header.layerIndex = advancedLayer;
			header.bounds.set(
				inspectorBodyViewport.x + inspectorLayout.contentPadding,
				layoutCursor,
				inspectorBodyViewport.width - inspectorLayout.contentPadding * 2.0f,
				inspectorLayout.minControlHeight);
			advancedMappingParameterHeaders.push_back(header);
			layoutCursor += inspectorLayout.minControlHeight + controllerRowGap;
			lastAdvancedLayer = advancedLayer;
		}
		const bool parameterHidden = mediaOwned || (advancedState != nullptr &&
			advancedLayer >= 0 &&
			!advancedState->layers[advancedLayer].inspectorExpanded);

		if (inspectorBox->parameters.getType(k) == inspectorBox->parameters.FLOAT)
		{

			if (parameterHidden)
			{
				JPComplexSlider *sl = new JPComplexSlider();
				sl->setup(controllerX, -10000.0f,
					controllerWidth, standardControllerHeight,
					inspectorBox->parameters.parameters[k]);
				sl->name = advancedLayer >= 0 && parameterName.size() > 7 ?
					parameterName.substr(7) : parameterName;
				sl->width = 0.0f;
				sl->height = 0.0f;
				controllers.push_back(sl);
				continue;
			}

			float complexsliderheight = controllerHeightFor(
				inspectorBox->parameters.parameters[k]);
			// boxes[openguinumber]->parameters.setFloatValue(0.0, k);

			// boxes[openguinumber]->parameters.parameters[k]->floatValue = 0.5;

			// boxes[openguinumber]->parameters.parameters[k]->floatLerpValue = 0.5;

			// JPParameter* as = boxes[openguinumber]->parameters.parameters[k];
			JPComplexSlider *sl = new JPComplexSlider();
			// usesCanonicalOrder, not mediaBox: the camera, Spout, NDI and
			// transform-style shader boxes are ordered the same way. Testing
			// the box type here made the panel draw array order while the bind
			// slots used canonical order - exactly the disagreement these two
			// are kept in step to avoid.
			const float parameterTop = usesCanonicalOrder ?
				mediaParameterTop[k] : layoutCursor;
			sl->setup(controllerX,
					  controllerYFor(parameterTop,
						  inspectorBox->parameters.parameters[k]),
					  controllerWidth, complexsliderheight,
					  inspectorBox->parameters.parameters[k]);
			if (advancedLayer >= 0)
				sl->name = parameterName.substr(7);

			controllers.push_back(sl);

			// Skipped whenever the canonical pass above already positioned
			// every row and left layoutCursor past them. This tested
			// mediaBox and so double-counted the camera and shader boxes,
			// measuring the panel at roughly twice its content.
			if (!usesCanonicalOrder)
				layoutCursor += complexsliderheight + controllerRowGap;

			// A colour triple gets a swatch in its own slot, directly under the
			// last of its three channels. Its own slot because a row has no
			// horizontal slack left: the card ends 7px short of the range
			// button, which is 5px from the lock button, which is 4px from the
			// panel edge.
			//
			// Placed when the THIRD channel of a group lands, so the swatch
			// follows whichever channel comes last in the file rather than
			// assuming r,g,b order.
			JPParameter *justPlaced = inspectorBox->parameters.parameters[k];
			if (!usesCanonicalOrder && justPlaced != nullptr &&
				justPlaced->colorChannel != JPParameter::COLOR_NONE)
			{
				InspectorColorSwatch swatch;
				for (int c = 0; c < inspectorBox->parameters.getSize(); ++c)
				{
					JPParameter *other = inspectorBox->parameters.parameters[c];
					if (other == nullptr) continue;
					if (other->colorGroup != justPlaced->colorGroup) continue;
					if (other->colorChannel == JPParameter::COLOR_R) swatch.r = other;
					else if (other->colorChannel == JPParameter::COLOR_G) swatch.g = other;
					else if (other->colorChannel == JPParameter::COLOR_B) swatch.b = other;
				}
				// Only once the group is complete, and only when THIS parameter
				// is the last of the three to have been laid out - otherwise a
				// swatch would appear under each channel.
				const bool complete = swatch.r != nullptr && swatch.g != nullptr &&
					swatch.b != nullptr;
				bool isLastOfGroup = complete;
				for (int c = k + 1; c < inspectorBox->parameters.getSize() &&
					isLastOfGroup; ++c)
				{
					JPParameter *other = inspectorBox->parameters.parameters[c];
					if (other != nullptr &&
						other->colorChannel != JPParameter::COLOR_NONE &&
						other->colorGroup == justPlaced->colorGroup)
					{
						isLastOfGroup = false;
					}
				}
				if (isLastOfGroup)
				{
					// Deliberately thinner than a parameter row. It is a strip
					// to read, not a control to hit: nothing clicks it, and at
					// full row height it competed with the sliders it belongs to.
					// Full row WIDTH though - the colour is easier to judge
					// across a wide band than a small square.
					const float swatchHeight = 12.0f;
					swatch.bounds.set(controllerX - controllerWidth * 0.5f,
						layoutCursor, controllerWidth, swatchHeight);
					inspectorColorSwatches.push_back(swatch);
					// Advance by its own height, not a row's, or it leaves a gap
					// the size of the difference. inspectorContentHeight and the
					// scroll range are both derived from this cursor.
					layoutCursor += swatchHeight + controllerRowGap;
				}
			}
		}
		else if (inspectorBox->parameters.getType(k) == inspectorBox->parameters.BOOL)
		{
			float complexsliderheight = standardControllerHeight;
			JPToogle *toogle = new JPToogle();
			toogle->setParametersPointer(inspectorBox->parameters.getJParameter(k));
			toogle->setFontPointer(jp_constants::inspector_body_font);
			// usesCanonicalOrder, not mediaBox: the camera, Spout, NDI and
			// transform-style shader boxes are ordered the same way. Testing
			// the box type here made the panel draw array order while the bind
			// slots used canonical order - exactly the disagreement these two
			// are kept in step to avoid.
			const float parameterTop = usesCanonicalOrder ?
				mediaParameterTop[k] : layoutCursor;
			toogle->setup(controllerX,
						  parameterTop + complexsliderheight * 0.5f,
						  std::min(slider_width, controllerWidth), slider_height,
						  inspectorBox->parameters.getName(k), inspectorBox->parameters.getBoolValue(k));
			if (parameterHidden)
			{
				toogle->y = -10000.0f;
				toogle->width = 0.0f;
				toogle->height = 0.0f;
			}
			controllers.push_back(toogle);
			if (!parameterHidden && !usesCanonicalOrder)
				layoutCursor += complexsliderheight + controllerRowGap;
		}
		}

		// Clear exposed controller mapping
		exposedControllerMapping.clear();

		// Recursive lambda: walk through preset children and collect exposed params
		std::function<void(JPbox_preset *, const string &)> collectExposedParams;
		collectExposedParams = [&](JPbox_preset *preset, const string &namePrefix)
		{
			if (preset == nullptr) return;

			for (int bi = 0; bi < (int)preset->boxes.size() && bi < (int)preset->exposedParams.size(); bi++)
			{
				if (preset->boxes[bi] == nullptr) continue;

				// First, recursively collect from nested presets (grandchildren)
				if (preset->boxes[bi]->getTipo() == JPbox::PRESETBOX)
				{
					JPbox_preset *childPreset = dynamic_cast<JPbox_preset *>(preset->boxes[bi]);
					if (childPreset != nullptr)
					{
						string childPrefix = namePrefix.empty()
							? preset->boxes[bi]->name
							: namePrefix + "." + preset->boxes[bi]->name;
						collectExposedParams(childPreset, childPrefix);
					}
				}

				// Then collect this preset's own exposed params for this child
				bool hasExposedInThisChild = false;
				for (int ei = 0; ei < (int)preset->exposedParams[bi].size(); ei++)
				{
					if (preset->exposedParams[bi][ei])
					{
						cout << "  collectExposedParams: found exposed param bi=" << bi << " ei=" << ei
							 << " in preset \"" << preset->name << "\" child \""
							 << preset->boxes[bi]->name << "\"" << endl;
						if (!hasExposedInThisChild && bi > 0)
						{
							layoutCursor += controllerRowGap;
						}
						hasExposedInThisChild = true;

						if (ei < preset->boxes[bi]->parameters.getSize() &&
							preset->boxes[bi]->parameters.getType(ei) == preset->boxes[bi]->parameters.FLOAT)
						{
							float complexsliderheight = controllerHeightFor(
								preset->boxes[bi]->parameters.parameters[ei]);

							JPComplexSlider *sl = new JPComplexSlider();
							sl->setup(controllerX,
									  controllerYFor(layoutCursor,
										  preset->boxes[bi]->parameters.parameters[ei]),
									  controllerWidth, complexsliderheight,
									  preset->boxes[bi]->parameters.parameters[ei]);

							// Prepend full path to the slider name
							string fullName = namePrefix.empty()
								? preset->boxes[bi]->name + "." + sl->name
								: namePrefix + "." + preset->boxes[bi]->name + "." + sl->name;
							sl->name = fullName;

							controllers.push_back(sl);
							exposedControllerMapping.push_back({bi, ei});

							layoutCursor += complexsliderheight + controllerRowGap;
						}
					}
				}
			}
		};

		// MAIN view: add exposed sliders from the clicked preset's DIRECT children only (one level)
		// Exposed params should bubble up exactly ONE level, not recursively through all nested presets
		if (!isGroupViewActive() && openguinumber >= 0 && openguinumber < (int)boxes.size() &&
			boxes[openguinumber]->getTipo() == JPbox::PRESETBOX)
		{
			// Bind exposed sliders to the DRAFT preset when a cue targets this view
			// (inspectorBox is the draft clone then), so exposed-param edits stage in
			// the cue instead of writing straight to the live preset's sub-boxes.
			JPbox_preset *rootPreset = dynamic_cast<JPbox_preset *>(inspectorBox);
			if (rootPreset != nullptr)
			{
				for (int bi = 0; bi < (int)rootPreset->boxes.size() && bi < (int)rootPreset->exposedParams.size(); bi++)
				{
					if (rootPreset->boxes[bi] == nullptr) continue;

					bool hasExposedInThisChild = false;
					for (int ei = 0; ei < (int)rootPreset->exposedParams[bi].size(); ei++)
					{
						if (rootPreset->exposedParams[bi][ei])
						{
							if (!hasExposedInThisChild && bi > 0)
							{
								layoutCursor += controllerRowGap;
							}
							hasExposedInThisChild = true;

							if (ei < rootPreset->boxes[bi]->parameters.getSize() &&
								rootPreset->boxes[bi]->parameters.getType(ei) == rootPreset->boxes[bi]->parameters.FLOAT)
							{
								float complexsliderheight = controllerHeightFor(
									rootPreset->boxes[bi]->parameters.parameters[ei]);

								JPComplexSlider *sl = new JPComplexSlider();
								sl->setup(controllerX,
										  controllerYFor(layoutCursor,
											  rootPreset->boxes[bi]->parameters.parameters[ei]),
										  controllerWidth, complexsliderheight,
										  rootPreset->boxes[bi]->parameters.parameters[ei]);

								// Prepend child name to the slider label
								string fullName = rootPreset->boxes[bi]->name + "." + sl->name;
								sl->name = fullName;

								controllers.push_back(sl);
								exposedControllerMapping.push_back({bi, ei});

								layoutCursor += complexsliderheight + controllerRowGap;
							}
							// Propagated expose: the exposed param comes from a grandchild (child's child)
							// Use exposedParamOriginalIndices[bi][ei] to find the original parameter
							else if (ei >= rootPreset->boxes[bi]->parameters.getSize() &&
									 rootPreset->boxes[bi]->getTipo() == JPbox::PRESETBOX &&
									 bi < (int)rootPreset->exposedParamOriginalIndices.size() &&
									 ei < (int)rootPreset->exposedParamOriginalIndices[bi].size())
							{
								int ci = rootPreset->exposedParamOriginalIndices[bi][ei].first;
								int pi = rootPreset->exposedParamOriginalIndices[bi][ei].second;
								JPbox_preset *childPreset = dynamic_cast<JPbox_preset *>(rootPreset->boxes[bi]);
								if (childPreset != nullptr && ci >= 0 && ci < (int)childPreset->boxes.size() &&
									pi >= 0 && pi < childPreset->boxes[ci]->parameters.getSize() &&
									childPreset->boxes[ci]->parameters.getType(pi) == childPreset->boxes[ci]->parameters.FLOAT)
								{
									float complexsliderheight = controllerHeightFor(
										childPreset->boxes[ci]->parameters.parameters[pi]);

									JPComplexSlider *sl = new JPComplexSlider();
									sl->setup(controllerX,
											  controllerYFor(layoutCursor,
													  childPreset->boxes[ci]->parameters.parameters[pi]),
												  controllerWidth, complexsliderheight,
												  childPreset->boxes[ci]->parameters.parameters[pi]);

									string fullName = rootPreset->boxes[bi]->name + "."
										+ childPreset->boxes[ci]->name + "." + sl->name;
									sl->name = fullName;

									controllers.push_back(sl);
									exposedControllerMapping.push_back({bi, ei});

									layoutCursor += complexsliderheight + controllerRowGap;
								}
							}
						}
					}
				}
			}
		}

		// GROUP view: when the selected child box is a PRESETBOX, also show its children's exposed params
		if (isGroupViewActive() && groupInspectorIndex >= 0)
		{
			JPbox_preset *activePreset = getActivePreset();
			cout << "GROUPVIEW: activePreset=" << (activePreset ? activePreset->name : "NULL")
				 << " gIdx=" << groupInspectorIndex;
			if (activePreset != nullptr) cout << " boxCount=" << (int)activePreset->boxes.size();
			cout << endl;
			if (activePreset != nullptr && groupInspectorIndex < (int)activePreset->boxes.size())
			{
				JPbox *selectedBox = activePreset->boxes[groupInspectorIndex];
				cout << "GROUPVIEW: selectedBox=" << (selectedBox ? selectedBox->name : "NULL")
					 << " tipo=" << (selectedBox ? ofToString(selectedBox->getTipo()) : "N/A")
					 << " PRESETBOX=" << JPbox::PRESETBOX << endl;
				if (selectedBox != nullptr && selectedBox->getTipo() == JPbox::PRESETBOX)
				{
					// Use the DRAFT clone (inspectorBox) when a cue targets this view
					// so nested exposed-param edits stage instead of hitting live.
					JPbox_preset *childPreset = dynamic_cast<JPbox_preset *>(inspectorBox);
					cout << "GROUPVIEW: childPreset=" << (childPreset ? childPreset->name : "NULL")
						 << " children=" << (childPreset ? (int)childPreset->boxes.size() : -1)
						 << " exposedSize=" << (childPreset ? (int)childPreset->exposedParams.size() : -1) << endl;
					if (childPreset != nullptr)
					{
						// Check exposedParams contents
						for (int ci = 0; ci < (int)childPreset->exposedParams.size() && ci < (int)childPreset->boxes.size(); ci++)
						{
							for (int pi = 0; pi < (int)childPreset->exposedParams[ci].size(); pi++)
							{
								if (childPreset->exposedParams[ci][pi])
								{
									cout << "GROUPVIEW: FOUND exposed: child[" << ci << "]=" << childPreset->boxes[ci]->name
										 << " param[" << pi << "]" << endl;
								}
							}
						}
						collectExposedParams(childPreset, childPreset->name);
					}
				}
				}
				}

				// Create expose buttons for group view (one per controller)
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr && groupInspectorIndex >= 0 &&
			groupInspectorIndex < (int)preset->exposedParams.size())
		{
			// Resize exposedParams to match total controllers (own params + propagated)
			JPbox *childBox = preset->boxes[groupInspectorIndex];
			int childOwnParams = (childBox != nullptr) ? childBox->parameters.getSize() : 0;
			if ((int)preset->exposedParams[groupInspectorIndex].size() < (int)controllers.size())
			{
				preset->exposedParams[groupInspectorIndex].resize(controllers.size(), false);
			}
			// Resize exposedParamOriginalIndices to match
			if ((int)preset->exposedParamOriginalIndices.size() <= groupInspectorIndex)
			{
				preset->exposedParamOriginalIndices.resize(groupInspectorIndex + 1);
			}
			if ((int)preset->exposedParamOriginalIndices[groupInspectorIndex].size() < (int)controllers.size())
			{
				preset->exposedParamOriginalIndices[groupInspectorIndex].resize(controllers.size(), {-1, -1});
			}

			float btnSize = 24;
			float btnRightMargin = 4;
			for (int k = 0; k < (int)controllers.size(); k++)
			{
				// Position button at the far right edge of the inspector panel
				float btnX = inspectorwindow_x + inspectorwindow_width / 2 - btnSize / 2 - btnRightMargin;
				float btnY = controllers[k]->y;
				if (JPComplexSlider *slider =
					dynamic_cast<JPComplexSlider *>(controllers[k]))
					btnY = slider->primaryRowY;

				JPExposeButton *btn = new JPExposeButton();
				btn->setup(btnX, btnY, btnSize);
				// Sync initial state with stored exposed params
				if (k < (int)preset->exposedParams[groupInspectorIndex].size())
				{
					btn->boolValue = preset->exposedParams[groupInspectorIndex][k];
				}
				// For propagated controllers (beyond child's own params), store original indices
				if (k >= childOwnParams && k < (int)exposedControllerMapping.size())
				{
					int ci = exposedControllerMapping[k].first;
					int pi = exposedControllerMapping[k].second;
					preset->exposedParamOriginalIndices[groupInspectorIndex][k] = {ci, pi};
				}
				exposeButtons.push_back(btn);
			}
		}
	}

	const float lockSize = 22.0f;
	const float lockRightInset = isGroupViewActive() ? 33.0f : 4.0f;
	for (JPcontroller *controller : controllers)
	{
		if (controller == nullptr || controller->parameters == nullptr ||
			controller->width <= 0.0f)
		{
			parameterLockButtons.emplace_back(0, 0, 0, 0);
			parameterRangeButtons.emplace_back(0, 0, 0, 0);
			continue;
		}
		float actionCenterY = controller->y;
		if (JPComplexSlider *slider =
			dynamic_cast<JPComplexSlider *>(controller))
			actionCenterY = slider->primaryRowY;
		parameterLockButtons.emplace_back(
			inspectorBodyViewport.getRight() - lockRightInset - lockSize,
			actionCenterY - lockSize * 0.5f,
			lockSize, lockSize);
		if (controller->parameters->variabletype == JPParameter::FLOAT)
		{
			const ofRectangle &lock = parameterLockButtons.back();
			parameterRangeButtons.emplace_back(
				lock.x - lockSize - 5.0f, lock.y, lockSize, lockSize);
		}
		else
		{
			parameterRangeButtons.emplace_back(0, 0, 0, 0);
		}
	}

	const float trailingGap = controllers.empty() ? 0.0f : controllerRowGap;
	inspectorContentHeight = std::max(0.0f,
		layoutCursor - trailingGap + inspectorScrollY - inspectorBodyViewport.y +
		inspectorLayout.contentPadding);
	const float availableBodyHeight = std::max(0.0f,
		ofGetHeight() - inspectorLayout.headerHeight -
			inspectorLayout.outerInset);
	inspectorBodyViewport.height = std::min(
		inspectorContentHeight, availableBodyHeight);
	inspectorwindow_height = inspectorLayout.headerHeight +
		inspectorBodyViewport.height + inspectorLayout.outerInset;
	inspectorwindow_y = inspectorwindow_height * 0.5f;
	inspectorMaxScrollY = std::max(0.0f,
		inspectorContentHeight - inspectorBodyViewport.height);
	const float previousScroll = inspectorScrollY;
	inspectorScrollY = ofClamp(inspectorScrollY, 0.0f, inspectorMaxScrollY);
	if (std::abs(previousScroll - inspectorScrollY) > 0.01f &&
		!inspectorRelayoutForClamp)
	{
		inspectorRelayoutForClamp = true;
		setControllers();
		inspectorRelayoutForClamp = false;
		return;
	}
	inspectorScrollbarTrack.set(
		inspectorBodyViewport.getRight() - 4.0f,
		inspectorBodyViewport.y + 2.0f, 3.0f,
		std::max(0.0f, inspectorBodyViewport.height - 4.0f));
	if (inspectorMaxScrollY > 0.0f)
	{
		const float thumbHeight = std::max(30.0f,
			inspectorScrollbarTrack.height * inspectorBodyViewport.height /
				inspectorContentHeight);
		const float travel = inspectorScrollbarTrack.height - thumbHeight;
		inspectorScrollbarThumb.set(inspectorScrollbarTrack.x - 1.0f,
			inspectorScrollbarTrack.y + travel *
				(inspectorScrollY / inspectorMaxScrollY),
			5.0f, thumbHeight);
	}
	else
	{
		inspectorScrollbarThumb.set(0, 0, 0, 0);
	}

	cout << "setControllers done: controllers=" << (int)controllers.size()
		 << " exposeButtons=" << (int)exposeButtons.size()
		 << " groupActive=" << isGroupViewActive()
		 << " gIdx=" << groupInspectorIndex
		 << " openNum=" << openguinumber << endl;
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
		// The numeric index belongs to the graph currently on screen.
		if (_val >= 0 && _val < getCurrentViewBoxCount()) {
			requestSetActiveRenderForCurrentView(floor(_val));
		}
	}

	if (_dir == "/nextshader") {
		int count = getCurrentViewBoxCount();
		if (count == 0) {
			return;
		}

		int base = getCurrentViewSelectedIndex();
		if (base < 0 || base >= count) {
			base = getCurrentViewActiveRenderIndex();
		}
		int val = base + 1;
		if (val > count - 1) {
			val = 0;
		}
		selectOpenBoxForCurrentView(val);
	}

	if (_dir == "/prevshader") {
		int count = getCurrentViewBoxCount();
		if (count == 0) {
			return;
		}

		int base = getCurrentViewSelectedIndex();
		if (base < 0 || base >= count) {
			base = getCurrentViewActiveRenderIndex();
		}
		int val = base - 1;
		if (val < 0) {
			val = count - 1;
		}
		selectOpenBoxForCurrentView(val);
	}

	if (_dir == "/nextshader_gallerymode") {
		int count = getCurrentViewBoxCount();
		if (count == 0) {
			return;
		}

		int val = getCurrentViewSelectedIndex();
		if (val < 0 || val >= count) {
			val = getCurrentViewActiveRenderIndex();
		}
		val++;
		if (val > count - 1) {
			val = 0;
		}
		selectOpenBoxForCurrentView(val);
		requestSetActiveRenderForCurrentView(val, true);
	}

	if (_dir == "/prevshader_gallerymode") {
		int count = getCurrentViewBoxCount();
		if (count == 0) {
			return;
		}

		int val = getCurrentViewSelectedIndex();
		if (val < 0 || val >= count) {
			val = getCurrentViewActiveRenderIndex();
		}
		val--;
		if (val < 0) {
			val = count - 1;
		}
		selectOpenBoxForCurrentView(val);
		requestSetActiveRenderForCurrentView(val, true);
	}

	if (_dir == "/setactiveshader") {
		int index = getCurrentViewSelectedIndex();
		if (index >= 0 && index < getCurrentViewBoxCount()) {
			requestSetActiveRenderForCurrentView(index);
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
							controllers[k]->value = targetBox->parameters.getFloatValue(k);
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
			markCueDraftDirty(cueSelectedIndex());
			controllers[Intindex]->value = inspectorBox->parameters.getFloatValue(Intindex);
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
ofVec2f JPboxgroup::getMasterCanvasSize() const
{
	if (transition.isSourceAllocated())
		return ofVec2f(transition.getSourceWidth(),
			transition.getSourceHeight());
	return ofVec2f(0.0f, 0.0f);
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

JPbox *JPboxgroup::findTopLevelBoxByName(const string &boxName) const
{
	return findBoxByName(boxName);
}

namespace
{
	// Depth-first walk of a box list and every group nested inside it.
	//
	// The name-based lookups above are deliberately left alone: they are
	// top-level-only and other callers depend on that. Anything reached through
	// a uid instead walks the whole tree, which is what makes a box inside a
	// group addressable at all.
	void walkBoxTree(const vector<JPbox *> &list,
		const std::function<void(JPbox *, const string &)> &visit,
		const string &pathPrefix)
	{
		for (JPbox *box : list)
		{
			if (box == nullptr) continue;
			visit(box, pathPrefix);
			JPbox_preset *group = dynamic_cast<JPbox_preset *>(box);
			if (group != nullptr)
			{
				walkBoxTree(group->boxes, visit,
					pathPrefix.empty() ? box->name :
						pathPrefix + " / " + box->name);
			}
		}
	}
}

JPbox *JPboxgroup::findBoxByUid(const string &boxUid) const
{
	if (boxUid.empty()) return nullptr;
	JPbox *found = nullptr;
	walkBoxTree(boxes, [&](JPbox *box, const string &)
	{
		if (found == nullptr && box->uid == boxUid) found = box;
	}, "");
	return found;
}

bool JPboxgroup::hasBoxUid(const string &boxUid) const
{
	return findBoxByUid(boxUid) != nullptr;
}

ofVec2f JPboxgroup::getBoxFboSizeByUid(const string &boxUid) const
{
	JPbox *box = findBoxByUid(boxUid);
	if (box == nullptr || !box->fbo.isAllocated())
		return ofVec2f(0.0f, 0.0f);
	return ofVec2f(box->fbo.getWidth(), box->fbo.getHeight());
}

vector<JPboxgroup::OutputCandidate> JPboxgroup::getOutputCandidates() const
{
	vector<OutputCandidate> candidates;
	walkBoxTree(boxes, [&](JPbox *box, const string &pathPrefix)
	{
		if (!box->getOutputCandidate()) return;
		OutputCandidate candidate;
		candidate.uid = box->uid;
		// Path-qualified, because two groups may each hold a box called
		// "mask" and a bare name could not tell them apart.
		candidate.label = pathPrefix.empty() ? box->name :
			pathPrefix + " / " + box->name;
		candidates.push_back(candidate);
	}, "");
	return candidates;
}

void JPboxgroup::repairBoxUids()
{
	// Breadth-by-depth: every box at depth 0, then depth 1, and so on. First
	// occurrence keeps its uid; later duplicates and empties are re-minted.
	//
	// The ORDER is the load-bearing part. Under a depth-first walk a nested
	// child can claim a uid before a shallower box is even visited, so the
	// shallower one - the more likely live-output target, and the one the user
	// can see without opening a group - would be the one silently renumbered.
	// Shallowest wins instead.
	//
	// This fires for hand-edited XML and for the same group .xml being included
	// twice in one composition, which paste reaches trivially: pasting a group
	// box copies only its <directory>, so the children are re-read from the
	// shared file carrying the originals' uids.
	std::set<string> seen;
	vector<JPbox *> current = boxes;
	while (!current.empty())
	{
		vector<JPbox *> next;
		for (JPbox *box : current)
		{
			if (box == nullptr) continue;
			if (box->uid.empty() || !seen.insert(box->uid).second)
			{
				box->uid = jp_boxuid::mint();
				seen.insert(box->uid);
			}
			JPbox_preset *group = dynamic_cast<JPbox_preset *>(box);
			if (group != nullptr)
				next.insert(next.end(), group->boxes.begin(),
					group->boxes.end());
		}
		current.swap(next);
	}
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
	// The caller (setCueBoxByIndex / toggleCueBoxByIndex) sets the intended target
	// graph. clearCue() below resets targetPreset to nullptr, so capture and restore
	// it — otherwise a group-box cue would wrongly build its draft from the main graph.
	JPbox_preset *intendedTarget = cueState.targetPreset;
	if (index < 0 || index >= getCueTargetBoxSize())
	{
		clearCue();
		return false;
	}

	bool wasInspectorTarget = isCueDraftMode() && cueSelectedIndex() == cueState.sourceIndex;
	clearCue();
	cueState.targetPreset = intendedTarget;
	if (wasInspectorTarget && cueSelectedIndex() >= 0 && cueSelectedIndex() < getCueTargetBoxSize())
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
		copyFboStraight(getCueTargetBoxAt(getCueTargetActiveRender())->fbo,
			cueApplySnapshotFbo);
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
	bool wasInspectorTarget = isCueDraftMode() && cueSelectedIndex() == cueState.sourceIndex;
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
	if (wasInspectorTarget && cueSelectedIndex() >= 0)
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
	// Global cue: always target the main-graph tree.
	cueState.targetPreset = nullptr;
	return setCueByIndex(index);
}

bool JPboxgroup::setCueBoxByName(string boxName)
{
	cueState.targetPreset = nullptr;
	return setCueByIndex(findCueTargetBoxIndexByName(boxName));
}

bool JPboxgroup::toggleCueBoxByIndex(int index)
{
	cout << "toggleCueBoxByIndex(" << index << ") called" << endl;
	// The cue is a GLOBAL staging session over the whole main-graph tree, so it
	// always targets the main graph (nullptr). Edits inside groups stage into the
	// corresponding draft-tree sub-box (see getDraftBoxForCurrentInspector).
	cueState.targetPreset = nullptr;
	return toggleCueByIndex(index);
}

int JPboxgroup::getCueEntryIndexForCurrentView() const
{
	// The cue is global (main-graph tree). Return a MAIN-graph source index: inside
	// a group that is the main box containing the group; in the main graph the
	// selected box, else the active render.
	if (isGroupViewActive())
	{
		if (!activeGroupPath.empty())
		{
			return activeGroupPath[0];
		}
		return activerender != nullptr ? *activerender : -1;
	}
	if (openguinumber >= 0 && openguinumber < (int)boxes.size())
	{
		return openguinumber;
	}
	return activerender != nullptr ? *activerender : -1;
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

bool JPboxgroup::cueTargetsCurrentView() const
{
	if (!hasCue())
	{
		return false;
	}
	return isGroupViewActive() ? (cueState.targetPreset == getActivePreset())
							   : (cueState.targetPreset == nullptr);
}

int JPboxgroup::cueSelectedIndex() const
{
	// The cue is a GLOBAL staging session over the whole main-graph tree. This
	// returns the TOP-LEVEL main-graph box index for the current context: inside a
	// group that is the main box that contains the group (activeGroupPath[0]), so
	// dirty-marking and draft lookups stay at the main-graph level. The exact box
	// being edited (a sub-box deep in the tree) is resolved by
	// getDraftBoxForCurrentInspector().
	if (isGroupViewActive())
	{
		return activeGroupPath.empty() ? -1 : activeGroupPath[0];
	}
	return openguinumber;
}
JPbox *JPboxgroup::getDraftBoxForCurrentInspector()
{
	// Resolve the draft-tree box that the inspector is editing. The draft graph
	// clones the MAIN boxes (and, for presets, their internal sub-boxes via
	// copyPresetInternalState), so we navigate that draft tree by activeGroupPath
	// and finally the selected sub-box.
	if (!isCueDraftMode())
	{
		return nullptr;
	}
	if (!isGroupViewActive())
	{
		return getCueDraftBoxForRealIndex(openguinumber);
	}
	if (activeGroupPath.empty())
	{
		return nullptr;
	}
	JPbox_preset *dp = dynamic_cast<JPbox_preset *>(getCueDraftBoxForRealIndex(activeGroupPath[0]));
	if (dp == nullptr)
	{
		return nullptr;
	}
	for (size_t depth = 1; depth < activeGroupPath.size(); depth++)
	{
		int idx = activeGroupPath[depth];
		if (idx < 0 || idx >= (int)dp->boxes.size() || dp->boxes[idx] == nullptr)
		{
			return nullptr;
		}
		dp = dynamic_cast<JPbox_preset *>(dp->boxes[idx]);
		if (dp == nullptr)
		{
			return nullptr;
		}
	}
	if (groupInspectorIndex < 0 || groupInspectorIndex >= (int)dp->boxes.size())
	{
		return nullptr;
	}
	return dp->boxes[groupInspectorIndex];
}

int JPboxgroup::findCueTargetBoxIndexByName(const string &boxName) const
{
	const vector<JPbox *> &target = (cueState.targetPreset != nullptr) ? cueState.targetPreset->boxes : boxes;
	for (int i = 0; i < (int)target.size(); i++)
	{
		if (target[i] != nullptr && target[i]->name == boxName)
		{
			return i;
		}
	}
	return -1;
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

	struct PresetExposedInputSnapshot
	{
		vector<string> presetPath;
		vector<JPbox_preset::ExposedTextureInput> inputs;
	};
	std::function<void(
		JPbox_preset *,
		const vector<string> &,
		vector<PresetExposedInputSnapshot> &)>
		snapshotExposedInputs =
		[&snapshotExposedInputs](
			JPbox_preset *preset,
			const vector<string> &path,
			vector<PresetExposedInputSnapshot> &result) {
			if (preset == nullptr)
			{
				return;
			}
			PresetExposedInputSnapshot item;
			item.presetPath = path;
			item.inputs = preset->exposedTextureInputs;
			result.push_back(item);
			for (JPbox *box : preset->boxes)
			{
				if (box != nullptr &&
					box->getTipo() == JPbox::PRESETBOX)
				{
					vector<string> childPath = path;
					childPath.push_back(box->name);
					snapshotExposedInputs(
						dynamic_cast<JPbox_preset *>(box),
						childPath, result);
				}
			}
		};
	auto restoreExposedInputs =
		[](JPbox_preset *root,
		   const vector<PresetExposedInputSnapshot> &items) {
			for (const PresetExposedInputSnapshot &item :
				items)
			{
				JPbox_preset *preset = root;
				for (const string &name : item.presetPath)
				{
					JPbox_preset *next = nullptr;
					if (preset != nullptr)
					{
						for (JPbox *box : preset->boxes)
						{
							if (box != nullptr &&
								box->name == name &&
								box->getTipo() ==
									JPbox::PRESETBOX)
							{
								next = dynamic_cast<
									JPbox_preset *>(box);
								break;
							}
						}
					}
					preset = next;
				}
				if (preset != nullptr)
				{
					preset->setExposedTextureInputs(
						item.inputs);
				}
			}
		};

	struct DraftSnapshot
	{
		int realIndex = -1;
		string name;
		JPParameterGroup parameters;
		bool onoff = true;
		bool bypass = false;
		vector<string> linkNames;
		vector<bool> linkSet;
		vector<int> presetActiveRenders;
		vector<PresetLinkAssignment> presetLinks;
		vector<PresetExposedInputSnapshot>
			presetExposedInputs;
		unsigned int dirtyFlags = CUE_DIRTY_NONE;
		bool hasMedia = false;
		JPMediaState media;
	};

	vector<DraftSnapshot> snapshots;
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex < 0 || realIndex >= getCueTargetBoxSize() ||
			i < 0 || i >= cueState.draftBoxes.size() ||
			getCueTargetBoxAt(realIndex) == nullptr || cueState.draftBoxes[i] == nullptr)
		{
			continue;
		}
		DraftSnapshot snapshot;
		snapshot.realIndex = realIndex;
		snapshot.name = getCueTargetBoxAt(realIndex)->name;
		snapshot.parameters = cueState.draftBoxes[i]->parameters;
		snapshot.onoff = cueState.draftBoxes[i]->getonoff();
		snapshot.bypass = cueState.draftBoxes[i]->getBypass();
		if (auto *media = dynamic_cast<JPMediaInspectable *>(cueState.draftBoxes[i]))
		{
			snapshot.hasMedia = true;
			snapshot.media = media->mediaState();
		}
		if (cueState.draftBoxes[i]->getTipo() == JPbox::PRESETBOX)
		{
			JPbox_preset *draftPreset =
				dynamic_cast<JPbox_preset *>(cueState.draftBoxes[i]);
			snapshotPresetActiveRenders(
				draftPreset, snapshot.presetActiveRenders);
			snapshotPresetLinks(
				draftPreset, snapshot.presetLinks);
			snapshotExposedInputs(
				draftPreset, {},
				snapshot.presetExposedInputs);
		}
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
	int keepStagedActiveRenderIndex = cueState.stagedActiveRenderIndex;
	bool keepFullscreenPreview = cueFullscreenPreview;
	CueMonitorMode keepMonitorMode = cueMonitorMode;

	if (sourceIndex < 0 || sourceIndex >= getCueTargetBoxSize() ||
		getCueTargetBoxAt(sourceIndex) == nullptr)
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
	if (keepStagedActiveRenderIndex >= 0 && keepStagedActiveRenderIndex < getCueTargetBoxSize())
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
		if (realIndex < 0 || realIndex >= getCueTargetBoxSize() ||
			getCueTargetBoxAt(realIndex) == nullptr ||
			getCueTargetBoxAt(realIndex)->name != snapshots[i].name)
		{
			realIndex = findCueTargetBoxIndexByName(snapshots[i].name);
		}

		JPbox *draftBox = getCueDraftBoxForRealIndex(realIndex);
		if (draftBox == nullptr)
		{
			continue;
		}

		copyParametersByNameOrIndex(draftBox->parameters, snapshots[i].parameters);
		if (snapshots[i].hasMedia)
			if (auto *media = dynamic_cast<JPMediaInspectable *>(draftBox))
				media->mediaState() = snapshots[i].media;
		draftBox->setonoff(snapshots[i].onoff);
		draftBox->setBypass(snapshots[i].bypass);
		if (!snapshots[i].presetActiveRenders.empty() &&
			draftBox->getTipo() == JPbox::PRESETBOX)
		{
			int valueIndex = 0;
			restorePresetActiveRenders(dynamic_cast<JPbox_preset *>(draftBox),
								 snapshots[i].presetActiveRenders, valueIndex);
		}
		if (!snapshots[i].presetExposedInputs.empty() &&
			draftBox->getTipo() == JPbox::PRESETBOX)
		{
			restoreExposedInputs(
				dynamic_cast<JPbox_preset *>(draftBox),
				snapshots[i].presetExposedInputs);
		}
		if (!snapshots[i].presetLinks.empty() &&
			draftBox->getTipo() == JPbox::PRESETBOX)
		{
			restorePresetLinks(
				dynamic_cast<JPbox_preset *>(draftBox),
				snapshots[i].presetLinks);
		}
		for (int linkIndex = 0; linkIndex < snapshots[i].linkSet.size() &&
								   linkIndex < draftBox->fbohandlergroup.getSize(); linkIndex++)
		{
			if (!snapshots[i].linkSet[linkIndex])
			{
				draftBox->fbohandlergroup.deleteFboPointer(linkIndex);
				continue;
			}
			int linkedRealIndex = findCueTargetBoxIndexByName(snapshots[i].linkNames[linkIndex]);
			JPbox *linkedDraft = getCueDraftBoxForRealIndex(linkedRealIndex);
			if (linkedDraft != nullptr)
			{
				draftBox->fbohandlergroup.setFboPointer(&linkedDraft->fbo, &linkedDraft->name, linkIndex);
			}
			else if (linkedRealIndex >= 0 && linkedRealIndex < getCueTargetBoxSize() && getCueTargetBoxAt(linkedRealIndex) != nullptr)
			{
				JPbox *linkedReal = getCueTargetBoxAt(linkedRealIndex);
				draftBox->fbohandlergroup.setFboPointer(&linkedReal->fbo, &linkedReal->name, linkIndex);
			}
		}
		markCueDraftDirty(realIndex, snapshots[i].dirtyFlags);
	}
	for (int i = 0; i < cueState.cueAddedRealIndices.size(); i++)
	{
		markCueDraftDirty(cueState.cueAddedRealIndices[i], CUE_DIRTY_ADDED);
	}

	// Rebuild the inspector controllers if the current selection maps to a draft
	// (selection index is preserved across the rebuild for both main and group).
	if (getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr)
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
	bool draftWasInspectorTarget = getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr;
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
		copyFboStraight(getCueTargetBoxAt(targetActiveRender)->fbo,
			cueApplySnapshotFbo);
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
		if ((flags & (CUE_DIRTY_PARAMS | CUE_DIRTY_LINKS |
					  CUE_DIRTY_ADDED | CUE_DIRTY_PRESET_ACTIVE |
					  CUE_DIRTY_BYPASS_PAUSE)) != 0)
		{
			copyParametersByNameOrIndex(targetBoxes[realIndex]->parameters, cueState.draftBoxes[draftIndex]->parameters);
			targetBoxes[realIndex]->copyCustomStateFrom(
				cueState.draftBoxes[draftIndex]);
			// For a preset/group, the editable state (incl. exposed params) lives
			// in its internal sub-boxes; commit those staged edits back to the live
			// preset too.
			if (targetBoxes[realIndex]->getTipo() == JPbox::PRESETBOX &&
				cueState.draftBoxes[draftIndex]->getTipo() == JPbox::PRESETBOX)
			{
				copyPresetInternalState(dynamic_cast<JPbox_preset *>(targetBoxes[realIndex]),
										dynamic_cast<JPbox_preset *>(cueState.draftBoxes[draftIndex]));
			}
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
			if (cueState.targetPreset != nullptr)
			{
				// Delete from the target preset's own box vector (deleteBoxAtIndex
				// only handles the main graph).
				vector<JPbox *> &pb = cueState.targetPreset->boxes;
				if (realIndex < (int)pb.size() && pb[realIndex] != nullptr)
				{
					string deletedName = pb[realIndex]->name;
					cueState.targetPreset
						->removeExposedTextureInputsForBox(
							deletedName);
					for (int k = 0; k < (int)pb.size(); k++)
					{
						if (k == realIndex || pb[k] == nullptr) continue;
						for (int l = 0; l < pb[k]->fbohandlergroup.getSize(); l++)
							if (pb[k]->fbohandlergroup.getFboName(l) == deletedName)
								pb[k]->fbohandlergroup.deleteFboPointer(l);
					}
					pb[realIndex]->clear();
					delete pb[realIndex];
					pb.erase(pb.begin() + realIndex);
					if (realIndex < (int)cueState.targetPreset
							->exposedParams.size())
					{
						cueState.targetPreset->exposedParams.erase(
							cueState.targetPreset
								->exposedParams.begin() +
							realIndex);
					}
					if (realIndex < (int)cueState.targetPreset
							->exposedParamOriginalIndices.size())
					{
						cueState.targetPreset
							->exposedParamOriginalIndices.erase(
								cueState.targetPreset
									->exposedParamOriginalIndices
									.begin() + realIndex);
					}
					if (cueState.targetPreset->activeRender > realIndex) cueState.targetPreset->activeRender--;
				}
			}
			else
			{
				deleteBoxAtIndex(realIndex);
			}
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
	// Group-internal boxes added during the cue are already in their presets;
	// keep them (just drop the staging tracking) since we are committing.
	cueAddedGroupBoxes.clear();
	updateRealBoxesForCueApply();
	if (stagedActiveIndex >= 0 && stagedActiveIndex < targetSize)
	{
		getCueTargetActiveRender() = stagedActiveIndex;
		// The shared crossfader drives MAIN only. Group presets own their local
		// child crossfade, so do not hijack MAIN with a sub-box FBO.
		if (cueState.targetPreset == nullptr)
		{
			transition.setFboPointer1(&cueApplySnapshotFbo);
			transition.setFboPointer2(&targetBoxes[stagedActiveIndex]->fbo);
			transition.setLerpValue(0);
		}
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
	if (draftWasInspectorTarget && getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr)
	{
		setControllers();
	}
	return true;
}

JPbox *JPboxgroup::getInspectorBox()
{
	// While a (global) cue is active, edit the corresponding DRAFT-tree box so all
	// changes stage in the cue instead of the live graph — anywhere in the tree,
	// main graph or inside any group.
	if (isCueDraftMode())
	{
		JPbox *draftBox = getDraftBoxForCurrentInspector();
		if (draftBox != nullptr)
		{
			cueState.draftInspectorRealIndex = cueSelectedIndex();
			return draftBox;
		}
	}

	// Group view: real sub-box from the active preset (uses groupInspectorIndex).
	if (isGroupViewActive() && groupInspectorIndex >= 0)
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr && groupInspectorIndex < (int)preset->boxes.size())
		{
			return preset->boxes[groupInspectorIndex];
		}
	}
	// Main view: real box.
	if (openguinumber >= 0 && openguinumber < boxes.size())
	{
		return boxes[openguinumber];
	}
	return nullptr;
}

JPbox *JPboxgroup::getCuePreviewBox()
{
	if (cueMonitorMode == CUE_MONITOR_SELECTED_BOX &&
		cueSelectedIndex() >= 0 && cueSelectedIndex() < getCueTargetBoxSize())
	{
		JPbox *draftBox = getCueDraftBoxForRealIndex(cueSelectedIndex());
		if (draftBox != nullptr)
		{
			return draftBox;
		}
		return getCueTargetBoxAt(cueSelectedIndex());
	}
	if (isCueDraftMode())
	{
		// Show the STAGED draft output so all pending changes are visible in the
		// CUE window before Apply, while the live output stays untouched.
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

JPbox *JPboxgroup::getCueDraftBoxForCurrentViewIndex(int index) const
{
	if (!isCueDraftMode() || index < 0)
	{
		return nullptr;
	}
	if (!isGroupViewActive())
	{
		return getCueDraftBoxForRealIndex(index);
	}
	JPbox_preset *draftPreset = getDraftPresetForCurrentView();
	if (draftPreset == nullptr || index >= (int)draftPreset->boxes.size())
	{
		return nullptr;
	}
	return draftPreset->boxes[index];
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

	bool draftWasInspectorTarget = isCueDraftMode() && cueSelectedIndex() == cueState.sourceIndex;
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
			if (draftWasInspectorTarget)
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
	// Rebuild the inspector so its sliders bind to the DRAFT box's parameters
	// (group-aware: cueSelectedIndex() is groupInspectorIndex in group view). Without
	// this, editing in a group cue would keep hitting the live box's parameters.
	if (getCueDraftBoxForRealIndex(cueSelectedIndex()) != nullptr)
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
	// A preset draft is reloaded from disk by createBoxForDirectory/setup, so seed
	// its internal sub-box state from the LIVE preset. Without this the draft (and
	// its exposed-param sliders) would show stale on-disk values, and the CUE
	// preview would not match the live composite.
	if (type == source->PRESETBOX)
	{
		JPbox_preset *draftPreset =
			dynamic_cast<JPbox_preset *>(draft);
		JPbox_preset *sourcePreset =
			dynamic_cast<JPbox_preset *>(source);
		if (!synchronizeCuePresetStructure(
				draftPreset, sourcePreset))
		{
			draft->clear();
			delete draft;
			return nullptr;
		}
		copyPresetInternalState(draftPreset, sourcePreset);
	}
	draft->name = source->name;
	// The draft is a staging view of the SAME box, not a new one: the inspector
	// hands it out in place of the live box for the duration of the cue. Give it
	// a separate identity and anything asking "which box is this output bound
	// to" would see two identities alternate as the cue opens and closes.
	//
	// Note this is assigned HERE, by the caller. copyEditableBoxState and
	// copyPresetInternalState must never copy uid - doing so would write the
	// draft's identity onto the live box when the cue is applied.
	draft->uid = source->uid;
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
	destination->copyCustomStateFrom(source);
}

void JPboxgroup::copyBoxLinksByName(
	JPbox *destination,
	JPbox *source,
	const vector<JPbox *> &destinationSiblings)
{
	if (destination == nullptr || source == nullptr)
	{
		return;
	}
	for (int destinationIndex = 0;
		 destinationIndex < destination->fbohandlergroup.getSize();
		 destinationIndex++)
	{
		const string samplerName =
			destination->fbohandlergroup.getName(destinationIndex);
		const int sourceIndex =
			source->fbohandlergroup.findIndexByName(samplerName);
		if (sourceIndex < 0 ||
			!source->fbohandlergroup.getisPointerSet(sourceIndex))
		{
			destination->fbohandlergroup.deleteFboPointer(destinationIndex);
			continue;
		}

		const string linkedName =
			source->fbohandlergroup.getFboName(sourceIndex);
		JPbox *linkedDestination = nullptr;
		for (JPbox *candidate : destinationSiblings)
		{
			if (candidate != nullptr && candidate->name == linkedName)
			{
				linkedDestination = candidate;
				break;
			}
		}
		if (linkedDestination != nullptr)
		{
			destination->fbohandlergroup.setFboPointer(
				&linkedDestination->fbo,
				&linkedDestination->name,
				destinationIndex);
		}
		else
		{
			destination->fbohandlergroup.deleteFboPointer(destinationIndex);
		}
	}
}

void JPboxgroup::snapshotPresetLinks(
	JPbox_preset *preset,
	vector<PresetLinkAssignment> &assignments,
	const vector<string> &presetPath) const
{
	if (preset == nullptr)
	{
		return;
	}
	for (JPbox *box : preset->boxes)
	{
		if (box == nullptr)
		{
			continue;
		}
		for (int linkIndex = 0;
			 linkIndex < box->fbohandlergroup.getSize();
			 linkIndex++)
		{
			if (preset->isExposedTextureInputTarget(
					box->name,
					box->fbohandlergroup.getName(linkIndex)))
			{
				continue;
			}
			PresetLinkAssignment assignment;
			assignment.presetPath = presetPath;
			assignment.boxName = box->name;
			assignment.samplerName =
				box->fbohandlergroup.getName(linkIndex);
			assignment.connected =
				box->fbohandlergroup.getisPointerSet(linkIndex);
			if (assignment.connected)
			{
				assignment.sourceName =
					box->fbohandlergroup.getFboName(linkIndex);
			}
			assignments.push_back(assignment);
		}

		if (box->getTipo() == JPbox::PRESETBOX)
		{
			vector<string> childPath = presetPath;
			childPath.push_back(box->name);
			snapshotPresetLinks(
				dynamic_cast<JPbox_preset *>(box),
				assignments,
				childPath);
		}
	}
}

void JPboxgroup::restorePresetLinks(
	JPbox_preset *preset,
	const vector<PresetLinkAssignment> &assignments)
{
	if (preset == nullptr)
	{
		return;
	}

	for (const PresetLinkAssignment &assignment : assignments)
	{
		JPbox_preset *parentPreset = preset;
		for (const string &presetName : assignment.presetPath)
		{
			JPbox_preset *nextPreset = nullptr;
			for (JPbox *candidate : parentPreset->boxes)
			{
				if (candidate != nullptr &&
					candidate->name == presetName &&
					candidate->getTipo() == JPbox::PRESETBOX)
				{
					nextPreset = dynamic_cast<JPbox_preset *>(candidate);
					break;
				}
			}
			parentPreset = nextPreset;
			if (parentPreset == nullptr)
			{
				break;
			}
		}
		if (parentPreset == nullptr)
		{
			continue;
		}

		JPbox *targetBox = nullptr;
		JPbox *sourceBox = nullptr;
		for (JPbox *candidate : parentPreset->boxes)
		{
			if (candidate == nullptr)
			{
				continue;
			}
			if (candidate->name == assignment.boxName)
			{
				targetBox = candidate;
			}
			if (assignment.connected &&
				candidate->name == assignment.sourceName)
			{
				sourceBox = candidate;
			}
		}
		if (targetBox == nullptr)
		{
			continue;
		}

		const int linkIndex =
			targetBox->fbohandlergroup.findIndexByName(
				assignment.samplerName);
		if (linkIndex < 0)
		{
			continue;
		}
		if (assignment.connected && sourceBox != nullptr)
		{
			targetBox->fbohandlergroup.setFboPointer(
				&sourceBox->fbo, &sourceBox->name, linkIndex);
		}
		else
		{
			targetBox->fbohandlergroup.deleteFboPointer(linkIndex);
		}
	}
}

JPbox_preset *JPboxgroup::getDraftPresetForCurrentView() const
{
	if (!isCueDraftMode() || activeGroupPath.empty())
	{
		return nullptr;
	}

	JPbox_preset *draftPreset = dynamic_cast<JPbox_preset *>(
		getCueDraftBoxForRealIndex(activeGroupPath[0]));
	if (draftPreset == nullptr)
	{
		return nullptr;
	}

	for (size_t depth = 1; depth < activeGroupPath.size(); depth++)
	{
		int index = activeGroupPath[depth];
		if (index < 0 || index >= (int)draftPreset->boxes.size() ||
			draftPreset->boxes[index] == nullptr)
		{
			return nullptr;
		}
		draftPreset = dynamic_cast<JPbox_preset *>(draftPreset->boxes[index]);
		if (draftPreset == nullptr)
		{
			return nullptr;
		}
	}
	return draftPreset;
}

void JPboxgroup::copyPresetInternalState(JPbox_preset *destination, JPbox_preset *source)
{
	if (destination == nullptr || source == nullptr)
	{
		return;
	}
	destination->activeRender = source->boxes.empty() ? 0 :
		ofClamp(source->activeRender, 0, (int)source->boxes.size() - 1);
	destination->exposedParams = source->exposedParams;
	destination->exposedParamOriginalIndices =
		source->exposedParamOriginalIndices;
	destination->setExposedTextureInputs(
		source->exposedTextureInputs);
	int n = std::min((int)destination->boxes.size(), (int)source->boxes.size());
	for (int i = 0; i < n; i++)
	{
		if (destination->boxes[i] == nullptr || source->boxes[i] == nullptr)
		{
			continue;
		}
		destination->boxes[i]->parameters = source->boxes[i]->parameters; // deep copy
		destination->boxes[i]->setonoff(source->boxes[i]->getonoff());
		destination->boxes[i]->setBypass(source->boxes[i]->getBypass());
		destination->boxes[i]->copyCustomStateFrom(
			source->boxes[i]);
		copyBoxLinksByName(
			destination->boxes[i],
			source->boxes[i],
			destination->boxes);
		if (destination->boxes[i]->getTipo() == JPbox::PRESETBOX &&
			source->boxes[i]->getTipo() == JPbox::PRESETBOX)
		{
			copyPresetInternalState(dynamic_cast<JPbox_preset *>(destination->boxes[i]),
									dynamic_cast<JPbox_preset *>(source->boxes[i]));
		}
	}
	destination->syncExposedTextureInputs();
}

bool JPboxgroup::synchronizeCuePresetStructure(
	JPbox_preset *destination,
	JPbox_preset *source)
{
	if (destination == nullptr || source == nullptr)
	{
		return false;
	}

	bool structureMatches =
		destination->boxes.size() == source->boxes.size();
	if (structureMatches)
	{
		for (int i = 0; i < (int)source->boxes.size(); i++)
		{
			JPbox *destinationBox = destination->boxes[i];
			JPbox *sourceBox = source->boxes[i];
			if (destinationBox == nullptr || sourceBox == nullptr ||
				destinationBox->name != sourceBox->name ||
				destinationBox->dir != sourceBox->dir ||
				destinationBox->getTipo() != sourceBox->getTipo())
			{
				structureMatches = false;
				break;
			}
		}
	}

	if (!structureMatches)
	{
		destination->clear();
		for (JPbox *sourceBox : source->boxes)
		{
			if (sourceBox == nullptr)
			{
				continue;
			}
			string cloneName = sourceBox->name;
			JPbox *clone =
				createBoxForDirectory(
					sourceBox->dir, cloneName);
			if (clone == nullptr)
			{
				destination->clear();
				return false;
			}
			clone->setup(sourceBox->dir, cloneName);
			clone->name = sourceBox->name;
			// Mirrors the live child, so it carries the same identity - see the
			// note in the draft clone above.
			clone->uid = sourceBox->uid;
			copyEditableBoxState(clone, sourceBox);
			destination->boxes.push_back(clone);
		}
		destination->resizeExposedParams(
			(int)destination->boxes.size());
	}

	for (int i = 0;
		i < (int)source->boxes.size() &&
		i < (int)destination->boxes.size();
		i++)
	{
		JPbox *destinationBox = destination->boxes[i];
		JPbox *sourceBox = source->boxes[i];
		if (destinationBox == nullptr || sourceBox == nullptr)
		{
			continue;
		}
		if (sourceBox->getTipo() == JPbox::PRESETBOX)
		{
			if (destinationBox->getTipo() != JPbox::PRESETBOX ||
				!synchronizeCuePresetStructure(
					dynamic_cast<JPbox_preset *>(
						destinationBox),
					dynamic_cast<JPbox_preset *>(
						sourceBox)))
			{
				return false;
			}
		}
	}
	return true;
}

void JPboxgroup::snapshotPresetActiveRenders(JPbox_preset *source, vector<int> &values) const
{
	if (source == nullptr)
	{
		return;
	}
	values.push_back(source->activeRender);
	for (JPbox *box : source->boxes)
	{
		if (box != nullptr && box->getTipo() == JPbox::PRESETBOX)
		{
			snapshotPresetActiveRenders(dynamic_cast<JPbox_preset *>(box), values);
		}
	}
}

void JPboxgroup::restorePresetActiveRenders(JPbox_preset *destination, const vector<int> &values, int &valueIndex) const
{
	if (destination == nullptr || valueIndex >= (int)values.size())
	{
		return;
	}
	destination->activeRender = destination->boxes.empty() ? 0 :
		ofClamp(values[valueIndex], 0, (int)destination->boxes.size() - 1);
	valueIndex++;
	for (JPbox *box : destination->boxes)
	{
		if (box != nullptr && box->getTipo() == JPbox::PRESETBOX)
		{
			restorePresetActiveRenders(dynamic_cast<JPbox_preset *>(box), values, valueIndex);
		}
	}
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
	// The CUE OUTPUT preview renders cueState.draftOutputBox. Point it at the
	// DRAFT clone of the staged active render (not the live box), and refresh it
	// every time the staged index changes, so the preview reflects staged edits.
	JPbox *draftOut = getCueDraftBoxForRealIndex(index);
	cueState.draftOutputBox = draftOut != nullptr ? draftOut : getCueTargetBoxAt(index);
	cueState.draftOutputRealIndex = index;
	markCueDraftDirty(cueState.sourceIndex, CUE_DIRTY_STAGED_ACTIVE);
	return true;
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
	bool presetActive = false;
	for (int i = 0; i < cueState.draftDirtyFlags.size(); i++)
	{
		unsigned int flags = cueState.draftDirtyFlags[i];
		if (flags & CUE_DIRTY_PARAMS) params++;
		if (flags & CUE_DIRTY_BYPASS_PAUSE) bypassPause++;
		if (flags & CUE_DIRTY_LINKS) links++;
		if (flags & CUE_DIRTY_ADDED) added++;
		if (flags & CUE_DIRTY_DELETED) deleted++;
		if (flags & CUE_DIRTY_STAGED_ACTIVE) stagedActive = true;
		if (flags & CUE_DIRTY_PRESET_ACTIVE) presetActive = true;
	}
	vector<string> parts;
	if (params > 0) parts.push_back("params " + ofToString(params));
	if (bypassPause > 0) parts.push_back("pause " + ofToString(bypassPause));
	if (links > 0) parts.push_back("links " + ofToString(links));
	if (added > 0) parts.push_back("new " + ofToString(added));
	if (deleted > 0) parts.push_back("delete " + ofToString(deleted));
	if (stagedActive) parts.push_back("active");
	if (presetActive) parts.push_back("group active");
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
	// Operate on the cue's target graph (main or the active preset in group view).
	vector<JPbox *> &tboxes = getCueTargetBoxes();
	int &tActiveRender = getCueTargetActiveRender();
	int *selPtr = isGroupViewActive() ? &groupInspectorIndex : &openguinumber;

	if (!isCueDraftMode() || index < 0 || index >= (int)tboxes.size())
	{
		return false;
	}
	if (isCueAddedRealIndex(index))
	{
		string deletedName = tboxes[index]->name;
		for (int k = (int)tboxes.size() - 1; k >= 0; k--)
		{
			if (k == index || tboxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < tboxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (tboxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					tboxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		tboxes[index]->clear();
		delete tboxes[index];
		tboxes[index] = nullptr;
		tboxes.erase(tboxes.begin() + index);
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
		if (*selPtr == index)
		{
			*selPtr = -1;
			for (int c = 0; c < controllers.size(); c++)
			{
				delete controllers[c];
				controllers[c] = nullptr;
			}
			controllers.clear();
		}
		else if (*selPtr > index)
		{
			(*selPtr)--;
		}
		if (tActiveRender > index)
		{
			tActiveRender--;
		}
		tActiveRender = tboxes.empty() ? 0 : ofClamp(tActiveRender, 0, int(tboxes.size()) - 1);
		requestCueRebuild();
		return true;
	}
	int draftIndex = findCueDraftCloneIndexForRealIndex(index);
	if (draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
		draftIndex >= cueState.draftBaselineParameters.size() ||
		tboxes[index] == nullptr || cueState.draftBoxes[draftIndex] == nullptr)
	{
		return false;
	}
	cueState.draftBoxes[draftIndex]->parameters = cueState.draftBaselineParameters[draftIndex];
	cueState.draftBoxes[draftIndex]->setonoff(cueState.draftBaselineOnOff[draftIndex]);
	cueState.draftBoxes[draftIndex]->setBypass(cueState.draftBaselineBypass[draftIndex]);
	cueState.draftBoxes[draftIndex]->copyCustomStateFrom(
		tboxes[index]);
	if (cueState.draftBoxes[draftIndex]->getTipo() == JPbox::PRESETBOX &&
		tboxes[index]->getTipo() == JPbox::PRESETBOX)
	{
		copyPresetInternalState(
			dynamic_cast<JPbox_preset *>(cueState.draftBoxes[draftIndex]),
			dynamic_cast<JPbox_preset *>(tboxes[index]));
	}
	removeCueDraftDirty(index);
	if (cueSelectedIndex() == index)
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
	// Operate on the cue's target graph (main boxes, or the target preset's
	// boxes in group view) and keep the matching selection index consistent.
	vector<JPbox *> &tboxes = getCueTargetBoxes();
	int &tActiveRender = getCueTargetActiveRender();
	int *selPtr = isGroupViewActive() ? &groupInspectorIndex : &openguinumber;

	vector<int> added = cueState.cueAddedRealIndices;
	std::sort(added.begin(), added.end(), std::greater<int>());
	added.erase(std::unique(added.begin(), added.end()), added.end());
	cueState.cueAddedRealIndices.clear();
	for (int i = 0; i < added.size(); i++)
	{
		int index = added[i];
		if (index < 0 || index >= (int)tboxes.size() || tboxes[index] == nullptr)
		{
			continue;
		}
		string deletedName = tboxes[index]->name;
		for (int k = (int)tboxes.size() - 1; k >= 0; k--)
		{
			if (k == index || tboxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < tboxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (tboxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					tboxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		tboxes[index]->clear();
		delete tboxes[index];
		tboxes[index] = nullptr;
		tboxes.erase(tboxes.begin() + index);
		if (*selPtr == index)
		{
			*selPtr = -1;
			for (int c = 0; c < controllers.size(); c++)
			{
				delete controllers[c];
				controllers[c] = nullptr;
			}
			controllers.clear();
		}
		else if (*selPtr > index)
		{
			(*selPtr)--;
		}
		if (tActiveRender > index)
		{
			tActiveRender--;
		}
	}
	if (!tboxes.empty())
	{
		tActiveRender = ofClamp(tActiveRender, 0, int(tboxes.size()) - 1);
	}
	else
	{
		tActiveRender = 0;
	}
}
void JPboxgroup::removeCueAddedGroupBoxes()
{
	if (cueAddedGroupBoxes.empty())
	{
		return;
	}
	for (auto &entry : cueAddedGroupBoxes)
	{
		JPbox_preset *preset = entry.first;
		JPbox *box = entry.second;
		if (preset == nullptr || box == nullptr)
		{
			continue;
		}
		int idx = -1;
		for (int i = 0; i < (int)preset->boxes.size(); i++)
		{
			if (preset->boxes[i] == box)
			{
				idx = i;
				break;
			}
		}
		if (idx < 0)
		{
			continue; // already removed
		}
		// Drop any sibling links referencing this box.
		string deletedName = box->name;
		for (int k = 0; k < (int)preset->boxes.size(); k++)
		{
			if (k == idx || preset->boxes[k] == nullptr)
			{
				continue;
			}
			for (int l = 0; l < preset->boxes[k]->fbohandlergroup.getSize(); l++)
			{
				if (preset->boxes[k]->fbohandlergroup.getFboName(l) == deletedName)
				{
					preset->boxes[k]->fbohandlergroup.deleteFboPointer(l);
				}
			}
		}
		box->clear();
		delete box;
		preset->boxes.erase(preset->boxes.begin() + idx);
		if (preset->activeRender > idx)
		{
			preset->activeRender--;
		}
		preset->activeRender = preset->boxes.empty() ? 0 : ofClamp(preset->activeRender, 0, (int)preset->boxes.size() - 1);
		// Fix the group-view selection if it pointed at/after the removed box.
		if (isGroupViewActive() && getActivePreset() == preset)
		{
			if (groupInspectorIndex == idx)
			{
				groupInspectorIndex = -1;
			}
			else if (groupInspectorIndex > idx)
			{
				groupInspectorIndex--;
			}
		}
	}
	cueAddedGroupBoxes.clear();
}

bool JPboxgroup::commitCueDraftLink(int targetRealIndex, int linkIndex, int sourceRealIndex)
{
	vector<JPbox *> &target = getCueTargetBoxes();
	if (!isCueDraftMode() ||
		targetRealIndex < 0 || targetRealIndex >= (int)target.size() ||
		sourceRealIndex < 0 || sourceRealIndex >= (int)target.size())
	{
		return false;
	}
	JPbox *targetDraft = getCueDraftBoxForRealIndex(targetRealIndex);
	JPbox *sourceDraft = getCueDraftBoxForRealIndex(sourceRealIndex);
	if (targetDraft == nullptr ||
		linkIndex < 0 ||
		linkIndex >= targetDraft->fbohandlergroup.getSize() ||
		target[sourceRealIndex] == nullptr)
	{
		return false;
	}
	if (sourceDraft != nullptr)
	{
		targetDraft->fbohandlergroup.setFboPointer(&sourceDraft->fbo, &sourceDraft->name, linkIndex);
	}
	else
	{
		targetDraft->fbohandlergroup.setFboPointer(&target[sourceRealIndex]->fbo, &target[sourceRealIndex]->name, linkIndex);
	}
	markCueDraftDirty(targetRealIndex, CUE_DIRTY_LINKS);
	updateCueDraftGraph();
	return true;
}

void JPboxgroup::copyCueDraftLinksToReal(int realIndex)
{
	vector<JPbox *> &target = getCueTargetBoxes();
	int draftIndex = findCueDraftCloneIndexForRealIndex(realIndex);
	if (draftIndex < 0 || draftIndex >= cueState.draftBoxes.size() ||
		realIndex < 0 || realIndex >= (int)target.size() ||
		cueState.draftBoxes[draftIndex] == nullptr ||
		target[realIndex] == nullptr)
	{
		return;
	}
	JPbox *draftBox = cueState.draftBoxes[draftIndex];
	int maxLinks = std::min(draftBox->fbohandlergroup.getSize(), target[realIndex]->fbohandlergroup.getSize());
	for (int linkIndex = 0; linkIndex < maxLinks; linkIndex++)
	{
		if (!draftBox->fbohandlergroup.getisPointerSet(linkIndex))
		{
			target[realIndex]->fbohandlergroup.deleteFboPointer(linkIndex);
			continue;
		}
		string linkedName = draftBox->fbohandlergroup.getFboName(linkIndex);
		int linkedRealIndex = findCueTargetBoxIndexByName(linkedName);
		if (linkedRealIndex >= 0 && linkedRealIndex < (int)target.size() && target[linkedRealIndex] != nullptr)
		{
			target[realIndex]->fbohandlergroup.setFboPointer(&target[linkedRealIndex]->fbo,
															&target[linkedRealIndex]->name,
															linkIndex);
		}
		else
		{
			target[realIndex]->fbohandlergroup.deleteFboPointer(linkIndex);
		}
	}
}

void JPboxgroup::rewireCueDraftGraph()
{
	vector<JPbox *> &target = getCueTargetBoxes();
	for (int draftIndex = 0; draftIndex < cueState.draftBoxes.size(); draftIndex++)
	{
		int realIndex = cueState.draftRealIndices[draftIndex];
		if (realIndex < 0 || realIndex >= (int)target.size() || cueState.draftBoxes[draftIndex] == nullptr)
		{
			continue;
		}
		if ((getCueDraftDirtyFlags(realIndex) & CUE_DIRTY_LINKS) != 0)
		{
			continue;
		}
		for (int linkIndex = 0; linkIndex < cueState.draftBoxes[draftIndex]->fbohandlergroup.getSize() &&
			 linkIndex < target[realIndex]->fbohandlergroup.getSize(); linkIndex++)
		{
			if (!target[realIndex]->fbohandlergroup.getisPointerSet(linkIndex))
			{
				continue;
			}
			string linkedName = target[realIndex]->fbohandlergroup.getFboName(linkIndex);
			int linkedRealIndex = findCueTargetBoxIndexByName(linkedName);
			int linkedDraftIndex = findCueDraftCloneIndexForRealIndex(linkedRealIndex);
			if (linkedDraftIndex >= 0 && linkedDraftIndex < cueState.draftBoxes.size())
			{
				cueState.draftBoxes[draftIndex]->fbohandlergroup.setFboPointer(&cueState.draftBoxes[linkedDraftIndex]->fbo,
																			   &cueState.draftBoxes[linkedDraftIndex]->name,
																			   linkIndex);
			}
			else if (linkedRealIndex >= 0 && linkedRealIndex < (int)target.size())
			{
				cueState.draftBoxes[draftIndex]->fbohandlergroup.setFboPointer(&target[linkedRealIndex]->fbo,
																			   &target[linkedRealIndex]->name,
																			   linkIndex);
			}
		}
	}
}

void JPboxgroup::updateCueDraftGraph()
{
	vector<JPbox *> &target = getCueTargetBoxes();
	vector<bool> required(cueState.draftBoxes.size(), false);
	std::function<void(int)> markDraftDependencies = [&](int index)
	{
		if (index < 0 || index >= (int)cueState.draftBoxes.size() ||
			required[index] || cueState.draftBoxes[index] == nullptr)
		{
			return;
		}
		required[index] = true;
		JPbox *consumer = cueState.draftBoxes[index];
		for (int inlet = 0; inlet < consumer->fbohandlergroup.getSize(); ++inlet)
		{
			if (!consumer->fbohandlergroup.getisPointerSet(inlet)) continue;
			ofFbo *input = consumer->fbohandlergroup.getFboPointerReference(inlet);
			for (int source = 0; source < (int)cueState.draftBoxes.size(); ++source)
			{
				JPbox *candidate = cueState.draftBoxes[source];
				if (candidate != nullptr && &candidate->fbo == input)
				{
					markDraftDependencies(source);
					break;
				}
			}
		}
	};
	auto markDraftBox = [&](JPbox *box)
	{
		for (int i = 0; box != nullptr && i < (int)cueState.draftBoxes.size(); ++i)
		{
			if (cueState.draftBoxes[i] == box)
			{
				markDraftDependencies(i);
				break;
			}
		}
	};
	markDraftBox(cueState.draftOutputBox);
	markDraftBox(getCuePreviewBox());
	for (int i = 0; i < (int)cueState.draftBoxes.size(); ++i)
	{
		if (cueState.draftBoxes[i] != nullptr)
		{
			cueState.draftBoxes[i]->setRenderThisFrame(required[i]);
		}
	}
	for (int i = 0; i < cueState.draftBoxes.size(); i++)
	{
		if (cueState.draftBoxes[i] == nullptr)
		{
			continue;
		}
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex >= 0 && realIndex < (int)target.size() && target[realIndex] != nullptr)
		{
			int type = target[realIndex]->getTipo();
			if (type == target[realIndex]->PRESETBOX)
			{
				if (isCueDraftDirty(realIndex))
				{
					// Staged edits: re-render the draft preset, re-rendering its
					// shader sub-boxes but mirroring the live output for source
					// boxes (so cameras/etc. are not re-opened and it does not
					// break to an empty "one group").
					renderPresetDraftMirroringLive(dynamic_cast<JPbox_preset *>(cueState.draftBoxes[i]),
												   dynamic_cast<JPbox_preset *>(target[realIndex]));
				}
				else
				{
					// Clean passthrough: mirror the live composite.
					copyFboStraight(target[realIndex]->fbo,
						cueState.draftBoxes[i]->fbo);
				}
				continue;
			}
			if (type != target[realIndex]->SHADERBOX &&
				type != target[realIndex]->FRAMEDIFFERENCEBOX)
			{
				// Media/source box: mirror the live output.
				cueState.draftBoxes[i]->update();
				copyFboStraight(target[realIndex]->fbo,
					cueState.draftBoxes[i]->fbo);
				continue;
			}
		}
		// Shader / frame-difference: re-render from the draft graph so staged edits preview.
		if (cueState.draftBoxes[i] != nullptr)
		{
			cueState.draftBoxes[i]->update();
		}
	}
}
void JPboxgroup::renderPresetDraftMirroringLive(JPbox_preset *draftPreset, JPbox_preset *livePreset)
{
	if (draftPreset == nullptr || livePreset == nullptr)
	{
		return;
	}
	// This renderer intentionally bypasses JPbox_preset::update(), so resolve
	// staged public inlets here before any child shader samples its inputs.
	draftPreset->pruneInvalidExposedTextureInputs();
	draftPreset->syncExposedTextureInputs();
	int n = std::min((int)draftPreset->boxes.size(), (int)livePreset->boxes.size());
	// Render internal boxes back-to-front (dependency order, matching JPbox_preset::updateFBO).
	for (int i = n - 1; i >= 0; i--)
	{
		JPbox *d = draftPreset->boxes[i];
		JPbox *l = livePreset->boxes[i];
		if (d == nullptr || l == nullptr)
		{
			continue;
		}
		int t = l->getTipo();
		if (t == JPbox::SHADERBOX || t == JPbox::FRAMEDIFFERENCEBOX)
		{
			// Re-render with staged params, reading its (draft internal) inputs.
			d->update();
		}
		else if (t == JPbox::PRESETBOX)
		{
			renderPresetDraftMirroringLive(dynamic_cast<JPbox_preset *>(d),
										   dynamic_cast<JPbox_preset *>(l));
		}
		else if (d->fbo.isAllocated() && l->fbo.isAllocated())
		{
			// Source/media box (camera/video/spout/ndi/image): mirror the live
			// output instead of re-opening the device.
			copyFboStraight(l->fbo, d->fbo);
		}
	}
	// Composite through the same local crossfade used by live presets so a
	// staged group activation animates in the CUE preview as well.
	draftPreset->renderActiveRender();
}

void JPboxgroup::updateRealBoxesForCueApply()
{
	vector<JPbox *> &target = getCueTargetBoxes();
	for (int i = 0; i < cueState.draftRealIndices.size(); i++)
	{
		int realIndex = cueState.draftRealIndices[i];
		if (realIndex >= 0 && realIndex < (int)target.size() && target[realIndex] != nullptr)
		{
			target[realIndex]->update();
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
			destination.setRangeMin(source.getRangeMin(srcIndex), dstIndex);
			destination.setRangeMax(source.getRangeMax(srcIndex), dstIndex);
			destination.setRangeEnabled(
				source.getJParameter(srcIndex)->rangeEnabled, dstIndex);
			destination.setSpeed(source.getSpeed(srcIndex), dstIndex);
			destination.setBpmRate(source.getBpmRate(srcIndex), dstIndex);
			destination.setAudioSource(source.getAudioSource(srcIndex), dstIndex);
			destination.setAudioDiv(source.getAudioDiv(srcIndex), dstIndex);
			destination.setAudioBase(source.getAudioBase(srcIndex), dstIndex);
			destination.setAudioAmount(source.getAudioAmount(srcIndex), dstIndex);
			destination.setAudioInvert(source.getAudioInvert(srcIndex), dstIndex);
			destination.setAudioThreshold(source.getAudioThreshold(srcIndex), dstIndex);
			destination.setAudioCurve(source.getAudioCurve(srcIndex), dstIndex);
			destination.setAudioAttackMs(source.getAudioAttackMs(srcIndex), dstIndex);
			destination.setAudioReleaseMs(source.getAudioReleaseMs(srcIndex), dstIndex);
			destination.setmovetype(source.getMovType(srcIndex), dstIndex);
			destination.setlastmovetype(
				source.getLastMovType(srcIndex), dstIndex);
		}
		else if (source.getType(srcIndex) == source.BOOL)
		{
			destination.setBoolValue(source.getBoolValue(srcIndex), dstIndex);
		}
		JPParameter *destinationParameter = destination.getJParameter(dstIndex);
		JPParameter *sourceParameter = source.getJParameter(srcIndex);
		destinationParameter->randomLocked = sourceParameter->randomLocked;
		destinationParameter->defaultFloatValue =
			sourceParameter->defaultFloatValue;
		destinationParameter->defaultBoolValue =
			sourceParameter->defaultBoolValue;
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
void JPboxgroup::setMappingPanelLayout(float x, float y, float w, float h)
{
	mappingPanelX = x;
	mappingPanelY = y;
	mappingPanelW = w;
	mappingPanelH = h;
	clampMappingPanelLayout();
}
void JPboxgroup::getMappingPanelLayout(
	float &x, float &y, float &w, float &h) const
{
	x = mappingPanelX;
	y = mappingPanelY;
	w = mappingPanelW;
	h = mappingPanelH;
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
int JPboxgroup::getOpenParameterCount() const
{
	// Bind SLOTS, not controllers. A media box carries a zero-size placeholder
	// controller for each parameter its transport card owns, and those are not
	// bindable, so counting controllers would offer four dead rows.
	return (int)getBindableParameterOrder(
		const_cast<JPboxgroup *>(this)->getInspectorBox()).size();
}
vector<int> JPboxgroup::getBindableParameterOrder(JPbox *box) const
{
	vector<int> order;
	if (box == nullptr) return order;
	const int count = box->parameters.getSize();
	const bool isMedia = dynamic_cast<JPMediaInspectable *>(box) != nullptr;
	// The four the transport card drives. Not drawn as sliders, so a slot on
	// them would be an invisible bind target that only fights the card. Media
	// boxes only - the camera/NDI/Spout `strech` is an ordinary slider.
	auto mediaOwned = [&](int k) {
		if (!isMedia) return false;
		const string &name = box->parameters.getName(k);
		return name == "strech" || name == "speed" ||
			name == "position" || name == "play";
	};
	bool hasRanked = false;
	for (int k = 0; k < count; ++k)
		if (jp_media::isRankedTransformParameter(box->parameters.getName(k)))
			hasRanked = true;
	// MUST match the row order in setControllers. The transform parameters are
	// drawn in canonical order but stored in whatever order the box or shader
	// declares, so binding to the raw array index would point a knob at a
	// different row than the one it sits next to.
	if (hasRanked || isMedia)
	{
		for (int rank = 0; rank < jp_media::kUnrankedTransformParameter; ++rank)
			for (int k = 0; k < count; ++k)
				if (!mediaOwned(k) && jp_media::transformParameterRank(
						box->parameters.getName(k)) == rank)
					order.push_back(k);
		for (int k = 0; k < count; ++k)
			if (!mediaOwned(k) && !jp_media::isRankedTransformParameter(
					box->parameters.getName(k)))
				order.push_back(k);
		return order;
	}
	for (int k = 0; k < count; ++k)
		if (!mediaOwned(k)) order.push_back(k);
	return order;
}

int JPboxgroup::resolveBindableParameterIndex(JPbox *box, int slot) const
{
	const vector<int> order = getBindableParameterOrder(box);
	if (slot < 0 || slot >= (int)order.size()) return -1;
	return order[slot];
}

JPParameter *JPboxgroup::getOpenParameterAtIndex(
	int parameterIndex) const
{
	// parameterIndex is a bind SLOT, not an array index - see
	// getBindableParameterOrder. The MIDI panel labels its rows from this, so
	// the resolution has to happen here or the panel would show one name and
	// drive another.
	// getInspectorBox() is not const-qualified; this reads it without mutating.
	const int resolved = resolveBindableParameterIndex(
		const_cast<JPboxgroup *>(this)->getInspectorBox(), parameterIndex);
	if (resolved < 0 ||
		resolved >= (int)controllers.size() ||
		controllers[resolved] == nullptr)
	{
		return nullptr;
	}
	return controllers[resolved]->parameters;
}
// Writing a MIDI 0..1 value onto one parameter, plus the inspector row that
// mirrors it when there is one.
//
// floatValue lives in the parameter's OWN [min,max] domain: the drag path
// ofMaps the mouse into min..max (JPboxgroup.cpp:3288, :3622) and the slider
// draws ofMap(floatValue, min, max, 0, width) (jp_slider.cpp:106). MIDI stored
// the raw 0..1 instead, so any parameter whose range was not 0..1 moved its
// slider at the wrong rate - rotatecolor p0 has min 0.645, so the bottom two
// thirds of the knob's travel sat pinned at the left end of the bar.
static bool applyMidiParameterValue(JPParameter *parameter,
	JPcontroller *controller, float value)
{
	if (parameter == nullptr)
	{
		return false;
	}
	const float normalized = ofClamp(value, 0.0f, 1.0f);
	if (parameter->variabletype == JPParameter::FLOAT)
	{
		if (parameter->movtype != JPParameter::STANDART)
		{
			// Automation speed is its own 0..1 control and is NOT in [min,max].
			parameter->speed = normalized;
			JPComplexSlider *slider =
				dynamic_cast<JPComplexSlider *>(controller);
			if (slider != nullptr)
			{
				slider->speed = normalized;
				slider->slider_speed.value = normalized;
			}
			return true;
		}
		const float mapped = ofMap(normalized, 0.0f, 1.0f,
			parameter->effectiveMin(), parameter->effectiveMax(), true);
		parameter->floatValue = mapped;
		parameter->floatLerpValue = mapped;
		if (controller != nullptr)
		{
			controller->value = mapped;
		}
		return true;
	}
	if (parameter->variabletype == JPParameter::BOOL)
	{
		const bool boolValue = normalized > 0.5f;
		parameter->boolValue = boolValue;
		if (controller != nullptr)
		{
			controller->boolValue = boolValue;
			controller->activeFlag = false;
		}
		return true;
	}
	return false;
}

bool JPboxgroup::setOpenBoxParameterAtIndex(int parameterIndex, float value)
{
	if (parameterIndex < 0 ||
		parameterIndex >= maxBindableParameters)
	{
		return false;
	}
	// No box open means nothing to drive. `controllers` is only rebuilt by
	// setControllers(), so without this a MIDI knob kept moving the parameters
	// of the last box whose inspector had been up.
	if (getInspectorBox() == nullptr)
	{
		return false;
	}
	JPParameter *parameter =
		getOpenParameterAtIndex(parameterIndex);
	if (parameter == nullptr)
	{
		return false;
	}
	// The slot has to be resolved again for the controller: `controllers` is
	// indexed by array position, while parameterIndex is a bind slot, and for a
	// media box those differ. Indexing controllers with the raw slot updated the
	// wrong row's cached value even when the right parameter changed.
	const int resolved =
		resolveBindableParameterIndex(getInspectorBox(), parameterIndex);
	if (resolved < 0 || resolved >= (int)controllers.size())
	{
		return false;
	}
	if (!applyMidiParameterValue(parameter, controllers[resolved], value))
	{
		return false;
	}
	markCueDraftDirty(cueSelectedIndex());
	return true;
}

bool JPboxgroup::setBoxParameterAtIndex(string boxName, int parameterIndex,
	float value)
{
	// The fallback for "no inspector open": drive the box the binding names.
	// Main graph only - a box nested inside a group is not reachable by name
	// here, and a group itself has no parameters of its own to drive.
	if (parameterIndex < 0 || parameterIndex >= maxBindableParameters)
	{
		return false;
	}
	const int index = findBoxIndexByName(boxName);
	JPbox *box = getEditableBoxForRealIndex(index);
	if (box == nullptr || parameterIndex >= box->parameters.getSize())
	{
		return false;
	}
	// If that box is the one on screen, go through the open path so the
	// slider's cached copy stays in step instead of being overwritten later.
	if (getInspectorBox() == box && parameterIndex < (int)controllers.size())
	{
		return setOpenBoxParameterAtIndex(parameterIndex, value);
	}
	// Slot -> array index, same as the open path. Resolved from the box itself
	// so a binding behaves identically whether or not its inspector is up.
	const int resolved = resolveBindableParameterIndex(box, parameterIndex);
	if (resolved < 0)
	{
		return false;
	}
	if (!applyMidiParameterValue(
			box->parameters.getJParameter(resolved), nullptr, value))
	{
		return false;
	}
	markCueDraftDirty(index);
	return true;
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
void JPboxgroup::setExternalGuiHitTest(std::function<bool(float, float)> fn)
{
	externalGuiHitTest = std::move(fn);
}

void JPboxgroup::setExternalTextCaptureTest(std::function<bool()> fn)
{
	externalTextCaptureTest = std::move(fn);
}


bool JPboxgroup::spacePanAllowed() const
{
	// Space is a pan gesture on the canvas but a printable character in a field,
	// so the gesture has to stand down whenever anything is capturing text.
	// JPboxgroup only knows its own two fields; the rest live in ofApp and are
	// reported through the hook.
	if (wantsKeyCapture()) return false;
	if (externalTextCaptureTest && externalTextCaptureTest()) return false;
	return true;
}

bool JPboxgroup::isSpacePanHeld() const
{
	// openFrameworks already tracks every held key, not just the modifiers -
	// ofEvents inserts e.key on press and erases it on release - so this needs
	// no key state of its own. ofApp::keyReleased is an empty body and the
	// project has no held-key concept at all, which is what the naive version
	// would have had to invent.
	return ofGetKeyPressed(' ') && spacePanAllowed();
}

void JPboxgroup::closeInspector()
{
	openguinumber = -1;
	groupInspectorIndex = -1;
	setControllers();
}

ofRectangle JPboxgroup::getInspectorBounds() const
{
	// Drawn with RECTMODE_CENTER, so the stored values are a centre point.
	if (inspectorwindow_height <= 0.0f) return ofRectangle();
	return ofRectangle(inspectorwindow_x - inspectorwindow_width / 2.0f,
		inspectorwindow_y - inspectorwindow_height / 2.0f,
		inspectorwindow_width, inspectorwindow_height);
}

ofRectangle JPboxgroup::getCuePanelBounds()
{
	// Same predicate JPboxgroup::draw() uses to decide whether to paint it.
	if (getCuePreviewBox() == nullptr) return ofRectangle();
	return ofRectangle(cuePanelX, cuePanelY, cuePanelW, cuePanelH);
}

ofRectangle JPboxgroup::getMappingPanelBounds() const
{
	if (!mappingEditActive) return ofRectangle();
	return ofRectangle(mappingPanelX, mappingPanelY,
		mappingPanelW, mappingPanelH);
}

bool JPboxgroup::mouseOverGui()
{
	const float mx = ofGetMouseX();
	const float my = ofGetMouseY();
	// Surfaces this class owns...
	if (getMappingPanelBounds().inside(mx, my)) return true;
	if (getInspectorBounds().inside(mx, my)) return true;
	if (getCuePanelBounds().inside(mx, my)) return true;
	// ...plus everything stacked above the canvas that it does not own. This
	// used to know about exactly two rects, so the canvas happily panned and
	// zoomed underneath the MIDI panel and the save modal.
	if (externalGuiHitTest && externalGuiHitTest(mx, my)) return true;
	return false;
}
void JPboxgroup::addBox(string directory, float _x, float _y)
{
	// When in group view, add any box type to the active preset's sub-boxes
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset != nullptr)
		{
			string nombre = makeUniqueBoxName(makeNameFromDirectory(directory), preset->boxes);
			JPbox *bx = createBoxForDirectory(directory, nombre);
			if (bx == nullptr)
			{
				return;
			}
			bx->setup(directory, nombre);
			bx->setonoff(true);
			bx->setPos(_x, _y);
			preset->boxes.push_back(bx);
			// Resize exposedParams to match the new box count
			preset->resizeExposedParams((int)preset->boxes.size());
			// Auto-select the new box so controllers and expose buttons appear immediately
			groupInspectorIndex = (int)preset->boxes.size() - 1;
			groupPreviewBoxIndex = -1;
			// Adding a box is a direct, permanent operation and is kept whether the
			// cue is applied or cancelled (adds are not part of cue staging).
			setControllers();
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

	// Adding a box is a direct, permanent operation (not part of cue staging): it
	// is kept whether the cue is applied or closed. Rebuild the draft so the new
	// box is included and editable, but do NOT register it as cue-added (which
	// would remove it on cancel).
	if (isCueDraftMode())
	{
		openguinumber = int(boxes.size()) - 1;
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
	endMappingEdit();
	clearSelection();
	clearCue();
	// Before the boxes go: clearParameterMorph only touches boxes still in the
	// vector, so releasing it afterwards would leave a box that survives the
	// clear emitting a blended value forever.
	clearParameterMorph();
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
	// The group view is an index PATH into the vector that was just emptied.
	// Leaving it set meant that loading a session while inside a group left
	// getActivePreset indexing past the end - reachable today through the OSC
	// load command, not just from the UI.
	activeGroupPath.clear();
}

void JPboxgroup::clearSelection()
{
	selectedBoxIndices.clear();
	selectionBase.clear();
	draw_SelectionRect = false;
}

bool JPboxgroup::selectionAddModifier() const
{
	return ofGetKeyPressed(OF_KEY_SHIFT);
}

bool JPboxgroup::selectionToggleModifier() const
{
	return ofGetKeyPressed(OF_KEY_CONTROL);
}

void JPboxgroup::toggleBoxSelection(int index)
{
	if (index < 0) return;
	auto it = std::find(selectedBoxIndices.begin(), selectedBoxIndices.end(),
		index);
	if (it != selectedBoxIndices.end())
	{
		selectedBoxIndices.erase(it);
	}
	else
	{
		selectedBoxIndices.push_back(index);
	}
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

void JPboxgroup::mergeSelection(vector<int> &target, const vector<int> &base,
								const vector<int> &marqueeHits)
{
	target = base;
	for (int index : marqueeHits)
	{
		// The base may already hold it. A box listed twice is moved twice by the
		// multi-drag, so it drifts away from the rest at double speed.
		if (std::find(target.begin(), target.end(), index) == target.end())
		{
			target.push_back(index);
		}
	}
}

void JPboxgroup::updateBoxSelection()
{
	// Built from selectionBase, NOT from empty. This runs on every frame of the
	// drag and re-derives the rectangle from scratch, so anything a shift-drag
	// meant to keep would be gone the moment the mouse moved.
	vector<JPbox *> &activeBoxes = isGroupViewActive() ? getActivePreset()->boxes : boxes;
	vector<int> hits;
	for (int i = 0; i < (int)activeBoxes.size(); i++)
	{
		if (boxIntersectsSelection(activeBoxes[i])) hits.push_back(i);
	}
	mergeSelection(selectedBoxIndices, selectionBase, hits);
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

	// Grouping restructures the real graph, so close a cue before resolving
	// pointers into the graph that will be modified.
	if (hasCue())
	{
		clearCue();
	}

	vector<JPbox *> *currentBoxes = getCurrentViewBoxes();
	int *currentActiveRender =
		getCurrentViewActiveRenderPointer();
	JPbox_preset *parentPreset =
		isGroupViewActive() ? getActivePreset() : nullptr;
	if (currentBoxes == nullptr || currentActiveRender == nullptr)
	{
		return;
	}

	vector<int> selectedIndices = selectedBoxIndices;
	std::sort(selectedIndices.begin(), selectedIndices.end());
	selectedIndices.erase(
		std::unique(selectedIndices.begin(), selectedIndices.end()),
		selectedIndices.end());
	selectedIndices.erase(
		std::remove_if(
			selectedIndices.begin(), selectedIndices.end(),
			[currentBoxes](int index) {
				return index < 0 ||
					index >= (int)currentBoxes->size() ||
					(*currentBoxes)[index] == nullptr;
			}),
		selectedIndices.end());
	if (selectedIndices.size() < 2)
	{
		return;
	}

	const string groupName =
		makeNextGroupName(*currentBoxes);
	const int previousActiveRender = *currentActiveRender;
	int groupedActiveRender = 0;
	for (int i = 0; i < (int)selectedIndices.size(); i++)
	{
		if (selectedIndices[i] == previousActiveRender)
		{
			groupedActiveRender = i;
			break;
		}
	}

	float avgX = 0.0f;
	float avgY = 0.0f;
	vector<string> selectedNames;
	selectedNames.reserve(selectedIndices.size());
	for (int index : selectedIndices)
	{
		JPbox *box = (*currentBoxes)[index];
		avgX += box->x;
		avgY += box->y;
		selectedNames.push_back(box->name);
		if (box->getTipo() == JPbox::PRESETBOX)
		{
			JPbox_preset *selectedPreset =
				dynamic_cast<JPbox_preset *>(box);
			if (selectedPreset != nullptr)
			{
				selectedPreset->save();
			}
		}
	}
	avgX /= (float)selectedIndices.size();
	avgY /= (float)selectedIndices.size();

	struct IncomingGroupTextureLink
	{
		string targetBoxName;
		string targetSamplerName;
		string sourceName;
		string parentPublicName;
		string newPublicName;
	};
	vector<IncomingGroupTextureLink> incomingTextureLinks;
	for (int selectedIndex : selectedIndices)
	{
		JPbox *targetBox = (*currentBoxes)[selectedIndex];
		for (int samplerIndex = 0;
			samplerIndex <
				targetBox->fbohandlergroup.getSize();
			samplerIndex++)
		{
			const string samplerName =
				targetBox->fbohandlergroup.getName(
					samplerIndex);
			IncomingGroupTextureLink incoming;
			incoming.targetBoxName = targetBox->name;
			incoming.targetSamplerName = samplerName;

			if (parentPreset != nullptr)
			{
				for (const auto &parentInput :
					parentPreset->exposedTextureInputs)
				{
					if (parentInput.targetBoxName ==
							targetBox->name &&
						parentInput.targetSamplerName ==
							samplerName)
					{
						incoming.parentPublicName =
							parentInput.publicName;
						break;
					}
				}
			}

			if (!incoming.parentPublicName.empty())
			{
				incomingTextureLinks.push_back(incoming);
				continue;
			}
			if (!targetBox->fbohandlergroup
					.getisPointerSet(samplerIndex))
			{
				continue;
			}

			const string sourceName =
				targetBox->fbohandlergroup.getFboName(
					samplerIndex);
			for (int sourceIndex = 0;
				sourceIndex < (int)currentBoxes->size();
				sourceIndex++)
			{
				if ((*currentBoxes)[sourceIndex] != nullptr &&
					(*currentBoxes)[sourceIndex]->name ==
						sourceName &&
					std::find(selectedIndices.begin(),
						selectedIndices.end(), sourceIndex) ==
						selectedIndices.end())
				{
					incoming.sourceName = sourceName;
					incomingTextureLinks.push_back(incoming);
					break;
				}
			}
		}
	}

	// Build XML in the EXACT format that JPbox_preset::setup() expects
	// (same format as JPboxgroup::save() but only for selected boxes)
	ofXml xml;
	xml.appendChild("activerender").set(groupedActiveRender);

	for (int newIndex = 0;
		newIndex < (int)selectedIndices.size();
		newIndex++)
	{
		const int sourceIndex = selectedIndices[newIndex];
		JPbox *box = (*currentBoxes)[sourceIndex];
		auto data = xml.appendChild("box");
		data.appendChild("nombre").set(box->name);
		data.appendChild("x").set(box->x);
		data.appendChild("y").set(box->y);
		data.appendChild("directory").set(box->dir);
		// Identity rides through grouping: the children below are deleted and
		// rebuilt by JPbox_preset::setup from this XML, so without <uid> here
		// grouping would silently break every output bound to a child.
		data.appendChild("uid").set(box->uid);
		data.appendChild("tooutput").set(box->getOutputCandidate());
		data.appendChild("onoff").set(box->getonoff());
		data.appendChild("bypass").set(box->getBypass());
		box->saveCustomState(data);

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
					param.appendChild("min").set(box->parameters.getRangeMin(k));
					param.appendChild("max").set(box->parameters.getRangeMax(k));
					param.appendChild("value").set(box->parameters.getFloatValue(k));
					param.appendChild("movtype").set(box->parameters.getMovType(k));
					param.appendChild("lastmovtype").set(box->parameters.getLastMovType(k));
					param.appendChild("speed").set(box->parameters.getSpeed(k));
					param.appendChild("bpmrate").set(box->parameters.getBpmRate(k));
					param.appendChild("audiosource").set(box->parameters.getAudioSource(k));
					param.appendChild("audiodiv").set(box->parameters.getAudioDiv(k));
					param.appendChild("audiobase").set(box->parameters.getAudioBase(k));
					param.appendChild("audioamount").set(box->parameters.getAudioAmount(k));
					param.appendChild("audioinvert").set(box->parameters.getAudioInvert(k));
					param.appendChild("audiothreshold").set(box->parameters.getAudioThreshold(k));
					param.appendChild("audiocurve").set(box->parameters.getAudioCurve(k));
					param.appendChild("audioattackms").set(box->parameters.getAudioAttackMs(k));
					param.appendChild("audioreleasems").set(box->parameters.getAudioReleaseMs(k));
				}
				saveParameterUserState(param, box->parameters.getJParameter(k));
			}
		}

		// Preserve only links whose source is moving into this group.
		if (box->fbohandlergroup.getPointerSetsSize() > 0)
		{
			auto fboslinks = data.appendChild("fboslinks");
			for (int k = 0; k < box->fbohandlergroup.getSize(); k++)
			{
				if (box->fbohandlergroup.getisPointerSet(k))
				{
					const string sourceName =
						box->fbohandlergroup.getFboName(k);
					if (std::find(selectedNames.begin(),
						selectedNames.end(), sourceName) !=
						selectedNames.end())
					{
						fboslinks.appendChild(
							box->fbohandlergroup.getName(k))
							.set(sourceName);
					}
				}
			}
		}
	}

	// Exposure choices move with their boxes into the generated preset.
	if (parentPreset != nullptr)
	{
		auto exposedNode = xml.appendChild("exposedParams");
		for (int newIndex = 0;
			newIndex < (int)selectedIndices.size();
			newIndex++)
		{
			const int sourceIndex = selectedIndices[newIndex];
			if (sourceIndex < 0 ||
				sourceIndex >=
					(int)parentPreset->exposedParams.size())
			{
				continue;
			}
			for (int parameterIndex = 0;
				parameterIndex <
					(int)parentPreset
						->exposedParams[sourceIndex].size();
				parameterIndex++)
			{
				if (!parentPreset
						->exposedParams[sourceIndex][parameterIndex])
				{
					continue;
				}
				auto boxNode =
					exposedNode.appendChild("box");
				boxNode.set(newIndex);
				boxNode.appendChild("param")
					.set(parameterIndex);
				if (sourceIndex <
						(int)parentPreset
							->exposedParamOriginalIndices.size() &&
					parameterIndex <
						(int)parentPreset
							->exposedParamOriginalIndices
								[sourceIndex].size())
				{
					const pair<int, int> original =
						parentPreset
							->exposedParamOriginalIndices
								[sourceIndex][parameterIndex];
					if (original.first >= 0 &&
						original.second >= 0)
					{
						boxNode.appendChild("origBox")
							.set(original.first);
						boxNode.appendChild("origParam")
							.set(original.second);
					}
				}
			}
		}
	}

	// Save to temp XML file in data/groups/
	string outputDir = "data/groups/";
	string timestamp = ofGetTimestampString();
	string outputPath = outputDir + "group_" + timestamp + ".xml";
	ofFilePath::createEnclosingDirectory(outputPath);
	if (!xml.save(outputPath))
	{
		ofLogError("JPboxgroup")
			<< "Unable to save generated group to "
			<< outputPath;
		return;
	}
	cout << "groupSelectedBoxes: saved to " << outputPath << endl;

	string setupName = groupName;
	JPbox *newBox =
		createBoxForDirectory(outputPath, setupName);
	if (newBox == nullptr)
	{
		ofLogError("JPboxgroup")
			<< "Unable to create generated group "
			<< outputPath;
		return;
	}
	newBox->setup(outputPath, groupName);
	newBox->setonoff(true);
	newBox->setPos(avgX, avgY);
	JPbox_preset *newPreset =
		dynamic_cast<JPbox_preset *>(newBox);
	if (newPreset == nullptr)
	{
		newBox->clear();
		delete newBox;
		ofLogError("JPboxgroup")
			<< "Generated group is not a preset: "
			<< outputPath;
		return;
	}
	for (IncomingGroupTextureLink &incoming :
		incomingTextureLinks)
	{
		if (!newPreset->exposeTextureInput(
				incoming.targetBoxName,
				incoming.targetSamplerName,
				&incoming.newPublicName))
		{
			ofLogWarning("JPboxgroup")
				<< "Unable to preserve incoming texture for "
				<< incoming.targetBoxName << "."
				<< incoming.targetSamplerName;
		}
	}

	// Disconnect consumers that remain outside the new group.
	for (int boxIndex = 0;
		boxIndex < (int)currentBoxes->size();
		boxIndex++)
	{
		if (std::find(selectedIndices.begin(),
			selectedIndices.end(), boxIndex) !=
			selectedIndices.end())
		{
			continue;
		}
		JPbox *consumer = (*currentBoxes)[boxIndex];
		if (consumer == nullptr)
		{
			continue;
		}
		for (int linkIndex = 0;
			linkIndex < consumer->fbohandlergroup.getSize();
			linkIndex++)
		{
			const string sourceName =
				consumer->fbohandlergroup
					.getFboName(linkIndex);
			if (std::find(selectedNames.begin(),
				selectedNames.end(), sourceName) !=
				selectedNames.end())
			{
				consumer->fbohandlergroup
					.deleteFboPointer(linkIndex);
			}
		}
	}

	vector<int> descendingIndices = selectedIndices;
	std::sort(descendingIndices.begin(),
		descendingIndices.end(), std::greater<int>());
	for (int index : descendingIndices)
	{
		JPbox *box = (*currentBoxes)[index];
		box->clear();
		delete box;
		currentBoxes->erase(currentBoxes->begin() + index);
		if (parentPreset != nullptr)
		{
			if (index <
				(int)parentPreset->exposedParams.size())
			{
				parentPreset->exposedParams.erase(
					parentPreset->exposedParams.begin() +
					index);
			}
			if (index <
				(int)parentPreset
					->exposedParamOriginalIndices.size())
			{
				parentPreset
					->exposedParamOriginalIndices.erase(
						parentPreset
							->exposedParamOriginalIndices
							.begin() + index);
			}
		}
	}

	currentBoxes->push_back(newBox);

	const int newIndex = (int)currentBoxes->size() - 1;
	for (const IncomingGroupTextureLink &incoming :
		incomingTextureLinks)
	{
		if (incoming.newPublicName.empty())
		{
			continue;
		}
		if (!incoming.sourceName.empty())
		{
			JPbox *sourceBox = nullptr;
			for (JPbox *candidate : *currentBoxes)
			{
				if (candidate != nullptr &&
					candidate != newBox &&
					candidate->name == incoming.sourceName)
				{
					sourceBox = candidate;
					break;
				}
			}
			const int publicIndex =
				newPreset->fbohandlergroup.findIndexByName(
					incoming.newPublicName);
			if (sourceBox != nullptr && publicIndex >= 0)
			{
				newPreset->fbohandlergroup.setFboPointer(
					&sourceBox->fbo, &sourceBox->name,
					publicIndex);
			}
		}
		if (parentPreset != nullptr &&
			!incoming.parentPublicName.empty())
		{
			parentPreset->retargetExposedTextureInput(
				incoming.parentPublicName,
				groupName, incoming.newPublicName);
		}
	}
	if (parentPreset != nullptr)
	{
		parentPreset->syncExposedTextureInputs();
	}
	newPreset->syncExposedTextureInputs();
	if (!incomingTextureLinks.empty())
	{
		newPreset->save();
	}
	*currentActiveRender = newIndex;
	clearSelection();
	shaderboxagarrado = false;
	ouletagarrado = false;
	cualestaagarrado = -1;
	outlet_cualestaagarrado = -1;
	groupPreviewBoxIndex = -1;

	if (parentPreset != nullptr)
	{
		parentPreset->exposedParams.resize(
			currentBoxes->size());
		parentPreset->exposedParamOriginalIndices.resize(
			currentBoxes->size());
		parentPreset->exposedParams[newIndex].assign(
			newBox->parameters.getSize(), false);
		parentPreset
			->exposedParamOriginalIndices[newIndex].assign(
				newBox->parameters.getSize(), {-1, -1});
		parentPreset->activeRenderTransitionRunning = false;
		parentPreset->activeRenderTransitionInitialized = false;
		parentPreset->activeRenderTransitionTarget = -1;
		parentPreset->lastCompositedActiveRender = newIndex;
		groupInspectorIndex = newIndex;
	}
	else
	{
		openguinumber = newIndex;
		transition.setFboPointer1(&newBox->fbo);
		transition.setFboPointer2(&newBox->fbo);
		transition.setLerpValue(0);
	}

	setControllers();
	ensureTabStateSize();
	requestCueRebuild();
	cout << "groupSelectedBoxes: created " << groupName
		 << " in " << (parentPreset != nullptr ?
			 parentPreset->name : "MAIN")
		 << ", boxes=" << currentBoxes->size()
		 << " activeRender=" << *currentActiveRender
		 << endl;
}

void JPboxgroup::deleteSelectedShader()
{
	// In group view mode, delete the selected sub-boxes from the preset
	if (isGroupViewActive())
	{
		JPbox_preset *preset = getActivePreset();
		if (preset == nullptr) return;

		// If a cue targets this group, STAGE the deletion into the draft instead of
		// hard-deleting the live sub-box (which would invalidate the cue). Mirrors
		// the main graph; the real removal happens on Apply.
		if (cueTargetsCurrentView() && isCueDraftMode())
		{
			vector<int> toDelete;
			if (!selectedBoxIndices.empty()) toDelete = selectedBoxIndices;
			else if (groupInspectorIndex >= 0) toDelete.push_back(groupInspectorIndex);
			for (int idx : toDelete)
			{
				if (idx < 0 || idx >= (int)preset->boxes.size()) continue;
				JPbox *draftBox = getCueDraftBoxForRealIndex(idx);
				if (draftBox != nullptr) { draftBox->setonoff(false); draftBox->setBypass(true); }
				markCueDraftDirty(idx, CUE_DIRTY_DELETED);
			}
			updateCueDraftGraph();
			clearSelection();
			return;
		}

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
				preset->removeExposedTextureInputsForBox(
					deletedName);

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
				if (idx < (int)preset->exposedParams.size())
				{
					preset->exposedParams.erase(
						preset->exposedParams.begin() + idx);
				}
				if (idx < (int)preset
						->exposedParamOriginalIndices.size())
				{
					preset->exposedParamOriginalIndices.erase(
						preset
							->exposedParamOriginalIndices.begin() +
						idx);
				}
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
			preset->removeExposedTextureInputsForBox(
				deletedName);

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
			if (idx < (int)preset->exposedParams.size())
			{
				preset->exposedParams.erase(
					preset->exposedParams.begin() + idx);
			}
			if (idx < (int)preset
					->exposedParamOriginalIndices.size())
			{
				preset->exposedParamOriginalIndices.erase(
					preset
						->exposedParamOriginalIndices.begin() +
					idx);
			}

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
		// NO <uid> here, deliberately. Paste re-reads this XML, and a pasted
		// duplicate that inherited the original's identity would silently steal
		// any live output bound to it. The constructor's mint stands instead.
		data.appendChild("tooutput").set(box->getOutputCandidate());
		data.appendChild("onoff").set(box->getonoff());
		data.appendChild("bypass").set(box->getBypass());
		box->saveCustomState(data);

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
					param.appendChild("min").set(box->parameters.getRangeMin(k));
					param.appendChild("max").set(box->parameters.getRangeMax(k));
					param.appendChild("value").set(box->parameters.getFloatValue(k));
					param.appendChild("movtype").set(box->parameters.getMovType(k));
					param.appendChild("lastmovtype").set(box->parameters.getLastMovType(k));
					param.appendChild("speed").set(box->parameters.getSpeed(k));
					param.appendChild("bpmrate").set(box->parameters.getBpmRate(k));
					param.appendChild("audiosource").set(box->parameters.getAudioSource(k));
					param.appendChild("audiodiv").set(box->parameters.getAudioDiv(k));
					param.appendChild("audiobase").set(box->parameters.getAudioBase(k));
					param.appendChild("audioamount").set(box->parameters.getAudioAmount(k));
					param.appendChild("audioinvert").set(box->parameters.getAudioInvert(k));
					param.appendChild("audiothreshold").set(box->parameters.getAudioThreshold(k));
					param.appendChild("audiocurve").set(box->parameters.getAudioCurve(k));
					param.appendChild("audioattackms").set(box->parameters.getAudioAttackMs(k));
					param.appendChild("audioreleasems").set(box->parameters.getAudioReleaseMs(k));
				}
				saveParameterUserState(param, box->parameters.getJParameter(k));
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
		auto toOutput = box.getChild("tooutput");

		if (!nombre || !directory) continue;

		string dir = jp_normalizePath(directory.getValue());

		// Reuse the same addBox pattern but without requiring an xml file on disk
		string nombreFinal;
		if (targetPreset != nullptr)
		{
			nombreFinal = makeUniqueBoxName(makeNameFromDirectory(dir), targetPreset->boxes);
		}
		else
		{
			nombreFinal = makeUniqueBoxName(makeNameFromDirectory(dir));
		}
		JPbox *bx = createBoxForDirectory(dir, nombreFinal);
		if (bx == nullptr) continue;

		if (targetPreset != nullptr)
		{
			bx->setup(dir, nombreFinal);
			bx->setonoff(true);
			bx->setPos(pastePos.x + pasteIndex * 30.0f, pastePos.y + pasteIndex * 30.0f);
			targetPreset->boxes.push_back(bx);
			targetPreset->resizeExposedParams((int)targetPreset->boxes.size());
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
		// The flag is a property of the box and copies like any other. The
		// IDENTITY does not: no <uid> was written to the clipboard, so bx keeps
		// the one its constructor minted and the original keeps its outputs.
		if (toOutput) bx->setOutputCandidate(toOutput.getBoolValue());

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
						bx->parameters.setRangeMin(param.getChild("min").getFloatValue(), paramIndex);
						bx->parameters.setRangeMax(param.getChild("max").getFloatValue(), paramIndex);
						bx->parameters.setFloatLerpValue(param.getChild("value").getFloatValue(), paramIndex);
						bx->parameters.setFloatValue(param.getChild("value").getFloatValue(), paramIndex);
						bx->parameters.setmovetype(param.getChild("movtype").getIntValue(), paramIndex);
						auto lastMoveType = param.getChild("lastmovtype");
						if (lastMoveType)
						{
							bx->parameters.setlastmovetype(
								lastMoveType.getIntValue(), paramIndex);
						}
						bx->parameters.setSpeed(param.getChild("speed").getFloatValue(), paramIndex);
						auto bpmRate = param.getChild("bpmrate");
						if (bpmRate)
						{
							bx->parameters.setBpmRate(bpmRate.getIntValue(), paramIndex);
						}
						auto audioSource = param.getChild("audiosource");
						if (audioSource)
						{
							bx->parameters.setAudioSource(audioSource.getIntValue(), paramIndex);
						}
						auto audioDiv = param.getChild("audiodiv");
						if (audioDiv)
						{
							bx->parameters.setAudioDiv(audioDiv.getIntValue(), paramIndex);
						}
						auto loadAudioFloat = [&](const char *key, auto setter)
						{
							auto node = param.getChild(key);
							if (node) (bx->parameters.*setter)(node.getFloatValue(), paramIndex);
						};
						loadAudioFloat("audiobase", &JPParameterGroup::setAudioBase);
						loadAudioFloat("audioamount", &JPParameterGroup::setAudioAmount);
						auto audioInvert = param.getChild("audioinvert");
						if (audioInvert) bx->parameters.setAudioInvert(audioInvert.getBoolValue(), paramIndex);
						loadAudioFloat("audiothreshold", &JPParameterGroup::setAudioThreshold);
						loadAudioFloat("audiocurve", &JPParameterGroup::setAudioCurve);
						loadAudioFloat("audioattackms", &JPParameterGroup::setAudioAttackMs);
						loadAudioFloat("audioreleasems", &JPParameterGroup::setAudioReleaseMs);
					}
					else if (bx->parameters.getType(paramIndex) == bx->parameters.BOOL)
					{
						bx->parameters.setName(param.getChild("name").getValue());
						bx->parameters.setBoolValue(param.getChild("value").getBoolValue(), paramIndex);
					}
					loadParameterUserState(param,
						bx->parameters.getJParameter(paramIndex));
					paramIndex++;
				}
			}
		}
		bx->loadCustomState(box);

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
		for (auto &fbolink : fboslinks)
		{
			string linkedName = fbolink.getValue();
			int linkIndex = newBox->fbohandlergroup.findIndexByName(
				fbolink.getName());
			if (linkIndex < 0)
			{
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
		}
		pasteIndex++;
	}

	// Pasting a GROUP box copies only its <directory>, so its children are
	// re-read from the shared group file and arrive carrying the originals'
	// identities. The clipboard deliberately omits <uid> for the top-level
	// boxes, but it cannot reach inside a group file - this is what covers it.
	repairBoxUids();

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
// CONTEXTUAL TAB SYSTEM - Breadcrumb Navigation
// ============================================================
// Tabs are organized as: MAIN | breadcrumb[1] | ... | breadcrumb[N] | child[0] | child[1] | ...
// - Tab 0: MAIN (always)
// - Tabs 1..activeGroupPath.size(): clickable breadcrumb to go back to that level
// - Tabs beyond: direct child presets of the current view
// This ensures you can ALWAYS navigate backwards.
// ============================================================

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

void JPboxgroup::drawTabs()
{
	vector<int> childIndices = getDirectChildPresetIndices();
	int pathLen = (int)activeGroupPath.size();

	// Total tabs = MAIN(1) + breadcrumb(pathLen) + children(childIndices.size())
	int totalTabs = 1 + pathLen + (int)childIndices.size();
	if (totalTabs <= 1)
	{
		return; // Only MAIN, no presets at all
	}

	// activeTab logic:
	// In MAIN view, activeTab = 0 (MAIN tab)
	// In group view, activeTab = pathLen (the last breadcrumb tab)
	int activeTabIndex = isGroupViewActive() ? pathLen : 0;

	const float tabMinWidth = 90;
	const float pad = 8;
	const float gap = 2;
	const float tabH = 28;
	float x = pad;
	const float y = tabBarOffsetY + pad;

	// Draw tab bar background
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(COL_BG_TAB, 210);
	ofDrawRectRounded(x, y, ofGetWidth() - pad * 2, tabH + gap * 2, 4);
	ofPopStyle();

	// Draw each tab
	int tabCounter = 0;

	// TAB 0: MAIN (always)
	{
		string name = "MAIN";
		float textW = jp_constants::p_font.stringWidth(name);
		float tabW = std::max(tabMinWidth, textW + 24);
		bool isActive = (activeTabIndex == tabCounter);

		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		// Active = soft raised fill + green border & underline; inactive = flat dark.
		ofSetColor(isActive ? ofColor(COL_BG_HOVER, 240) : ofColor(COL_TAB_INACTIVE_BG, 220));
		ofDrawRectRounded(x, y, tabW, tabH, 4);
		ofNoFill(); ofSetLineWidth(1);
		ofSetColor(isActive ? ofColor(COL_ACCENT_GREEN, 230) : ofColor(COL_TAB_INACTIVE_BRD, 200));
		ofDrawRectRounded(x, y, tabW, tabH, 4); ofFill();
		if (isActive) { ofSetColor(COL_ACCENT_GREEN_BR); ofDrawRectangle(x + 6, y + tabH - 4, tabW - 12, 2); }
		ofSetColor(isActive ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
		jp_constants::p_font.drawString(name, x + (tabW - textW) * 0.5f, y + tabH * 0.5f + 5);
		ofPopStyle();

		x += tabW + gap;
		tabCounter++;
	}

	// Breadcrumb tabs (level 1..pathLen) - each is clickable to go back
	if (isGroupViewActive())
	{
		for (int level = 1; level <= pathLen; level++)
		{
			string name = getBreadcrumbNameAtLevel(activeGroupPath, boxes, level);
			float textW = jp_constants::p_font.stringWidth(name);
			float tabW = std::max(tabMinWidth, textW + 24);
			bool isActive = (activeTabIndex == tabCounter);

			ofPushStyle();
			ofSetRectMode(OF_RECTMODE_CORNER);
			if (isActive)
			{
				// Current level - highlighted green
				ofSetColor(COL_ACCENT_GREEN, 235);
			}
			else if (level < pathLen)
			{
				// Parent level - clickable to go back
				ofSetColor(COL_BG_HOVER, 225);
			}
			else
			{
				// Should not happen, but just in case
				ofSetColor(COL_TAB_INACTIVE_BG, 225);
			}
			ofDrawRectRounded(x, y, tabW, tabH, 3);
			ofNoFill(); ofSetLineWidth(1);
			ofSetColor(isActive ? ofColor(COL_ACCENT_GREEN_BR, 255) : ofColor(COL_BORDER_MUTED, 200));
			ofDrawRectRounded(x, y, tabW, tabH, 3); ofFill();

			// Small ">" separator before breadcrumb levels (except first)
			if (level > 1)
			{
				ofSetColor(COL_TEXT_DIM);
				jp_constants::p_font.drawString(">", x - gap - 10, y + tabH * 0.5f + 5);
			}

			ofSetColor(isActive ? COL_TEXT_PRIMARY : (level < pathLen ? COL_TEXT_SECONDARY : COL_TEXT_DIM));

			if (tabRenaming && tabCounter == tabRenameTabIndex)
			{
				// Draw text input field overlay on this breadcrumb tab
				float inputPad = 4;
				ofSetRectMode(OF_RECTMODE_CORNER);
				ofSetColor(240, 240, 245);
				ofDrawRectRounded(x + inputPad, y + inputPad, tabW - inputPad * 2, tabH - inputPad * 2, 2);
				ofSetColor(COL_TEXT_DARK);
				float txtW = jp_constants::p_font.stringWidth(tabRenameBuffer.empty() ? " " : tabRenameBuffer);
				float txtX = x + (tabW - txtW) * 0.5f;
				float txtY = y + tabH * 0.5f + 5;
				float maxTextX = x + tabW - inputPad - 4;
				if (txtX + txtW > maxTextX) txtX = maxTextX - txtW;
				if (txtX < x + inputPad + 2) txtX = x + inputPad + 2;
				jp_constants::p_font.drawString(tabRenameBuffer, txtX, txtY);
				if ((ofGetFrameNum() / 20) % 2 == 0)
				{
					int cc = std::max(0, std::min(tabRenameCursor, (int)tabRenameBuffer.size()));
					float caretX = txtX + jp_constants::p_font.stringWidth(tabRenameBuffer.substr(0, cc));
					ofSetColor(COL_TEXT_DARK);
					ofDrawRectRounded(caretX, y + inputPad + 3, 2, tabH - inputPad * 2 - 6, 1);
				}
			}
			else
			{
				jp_constants::p_font.drawString(name, x + (tabW - textW) * 0.5f, y + tabH * 0.5f + 5);
			}
			ofPopStyle();
			jp_tooltip::draw(level < pathLen ? "Return to this parent group" : "Current group", x, y, tabW, tabH);

			x += tabW + gap;
			tabCounter++;
		}
	}

	// Child preset tabs (direct children)
	for (int ti = 0; ti < (int)childIndices.size(); ti++)
	{
		int idx = childIndices[ti];
		string tabName = "?";

		const vector<JPbox *> *boxList = &boxes;
		if (isGroupViewActive())
		{
			JPbox_preset *preset = getActivePreset();
			if (preset != nullptr) boxList = &preset->boxes;
		}
		if (idx >= 0 && idx < (int)boxList->size() && (*boxList)[idx] != nullptr)
			tabName = (*boxList)[idx]->name;
		if (tabName.empty()) tabName = "Group " + ofToString(ti);

		float textW = jp_constants::p_font.stringWidth(tabName);
		float tabW = std::max(tabMinWidth, textW + 24);
		bool isActive = (activeTabIndex == tabCounter);

		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		// Active = soft raised fill + green border & underline; inactive = flat dark.
		ofSetColor(isActive ? ofColor(COL_BG_HOVER, 240) : ofColor(COL_TAB_INACTIVE_BG, 220));
		ofDrawRectRounded(x, y, tabW, tabH, 4);
		ofNoFill(); ofSetLineWidth(1);
		ofSetColor(isActive ? ofColor(COL_ACCENT_GREEN, 230) : ofColor(COL_TAB_INACTIVE_BRD, 200));
		ofDrawRectRounded(x, y, tabW, tabH, 4); ofFill();
		if (isActive) { ofSetColor(COL_ACCENT_GREEN_BR); ofDrawRectangle(x + 6, y + tabH - 4, tabW - 12, 2); }

		if (tabRenaming && tabCounter == tabRenameTabIndex)
		{
			// Draw text input field overlay on this tab
			float inputPad = 4;
			ofSetRectMode(OF_RECTMODE_CORNER);
			ofSetColor(240, 240, 245);
			ofDrawRectRounded(x + inputPad, y + inputPad, tabW - inputPad * 2, tabH - inputPad * 2, 2);
			ofSetColor(COL_TEXT_DARK);
			string displayText = tabRenameBuffer;
			if (displayText.empty()) displayText = " ";
			float textWidth = jp_constants::p_font.stringWidth(displayText);
			float textX = x + (tabW - textWidth) * 0.5f;
			float textY = y + tabH * 0.5f + 5;
			// Clamp textX so cursor is always visible
			float maxTextX = x + tabW - inputPad - 4;
			if (textX + textWidth > maxTextX) textX = maxTextX - textWidth;
			if (textX < x + inputPad + 2) textX = x + inputPad + 2;
			jp_constants::p_font.drawString(tabRenameBuffer, textX, textY);
			// Blinking insertion caret at cursor
			if ((ofGetFrameNum() / 20) % 2 == 0)
			{
				int cc = std::max(0, std::min(tabRenameCursor, (int)tabRenameBuffer.size()));
				float caretX = textX + jp_constants::p_font.stringWidth(tabRenameBuffer.substr(0, cc));
				ofSetColor(COL_TEXT_DARK);
				ofDrawRectRounded(caretX, y + inputPad + 3, 2, tabH - inputPad * 2 - 6, 1);
			}
		}
		else
		{
			ofSetColor(isActive ? COL_TEXT_PRIMARY : COL_TEXT_SECONDARY);
			jp_constants::p_font.drawString(tabName, x + (tabW - textW) * 0.5f, y + tabH * 0.5f + 5);
		}
		ofPopStyle();

		x += tabW + gap;
		tabCounter++;
	}
}

int JPboxgroup::getTabAtScreenPos(int screenX, int screenY) const
{
	vector<int> childIndices = getDirectChildPresetIndices();
	int pathLen = (int)activeGroupPath.size();
	int totalTabs = 1 + pathLen + (int)childIndices.size();

	const float tabHeight = 28;
	const float tabMinWidth = 90;
	const float pad = 8;
	const float gap = 2;

	// tabBarOffsetY was added twice, so the strip claimed clicks ~48px below
	// where it is actually drawn - a dead band across the top of the canvas.
	if (totalTabs <= 1 || screenY < tabBarOffsetY ||
		screenY > tabBarOffsetY + tabHeight + pad * 2)
	{
		return -1;
	}

	float x = pad;

	// Iterate tabs in same order as drawTabs() and find hit
	int tabIndex = 0;

	// MAIN
	{
		float tabW = std::max(tabMinWidth, jp_constants::p_font.stringWidth("MAIN") + 24);
		if (screenX >= x && screenX <= x + tabW) return tabIndex;
		x += tabW + gap;
		tabIndex++;
	}

	// Breadcrumb
	if (isGroupViewActive())
	{
		for (int level = 1; level <= pathLen; level++)
		{
			string name = getBreadcrumbNameAtLevel(activeGroupPath, boxes, level);
			float tabW = std::max(tabMinWidth, jp_constants::p_font.stringWidth(name) + 24);
			// Account for ">" separator space
			if (level > 1) x += 12;
			if (screenX >= x && screenX <= x + tabW) return tabIndex;
			x += tabW + gap;
			tabIndex++;
		}
	}

	// Children
	for (int ti = 0; ti < (int)childIndices.size(); ti++)
	{
		int idx = childIndices[ti];
		string tabName = "?";
		const vector<JPbox *> *boxList = &boxes;
		if (isGroupViewActive())
		{
			JPbox_preset *preset = getActivePreset();
			if (preset != nullptr) boxList = &preset->boxes;
		}
		if (idx >= 0 && idx < (int)boxList->size() && (*boxList)[idx] != nullptr)
			tabName = (*boxList)[idx]->name;
		if (tabName.empty()) tabName = "Group " + ofToString(ti);

		float tabW = std::max(tabMinWidth, jp_constants::p_font.stringWidth(tabName) + 24);
		if (screenX >= x && screenX <= x + tabW) return tabIndex;
		x += tabW + gap;
		tabIndex++;
	}

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
	int tabIndex = getTabAtScreenPos(ofGetMouseX(), ofGetMouseY());
	if (tabIndex < 0)
	{
		return false;
	}

	int pathLen = (int)activeGroupPath.size();
	vector<int> childIndices = getDirectChildPresetIndices();

	// Tab index breakdown:
	// 0 = MAIN
	// 1..pathLen = breadcrumb levels (clickable to go back)
	// pathLen+1.. = child presets (clickable to go forward)

	if (tabIndex == 0)
	{
		// MAIN tab
		if (!isGroupViewActive())
		{
			// Already in MAIN - reset zoom
			viewportZoom = 1.0f;
			viewportPan = ofVec2f(0, 0);
			ensureTabStateSize();
			if (0 < (int)tabZooms.size())
			{
				tabZooms[0] = viewportZoom;
				tabPans[0] = viewportPan;
			}
		}
		else
		{
			navigateToBreadcrumbLevel(0);
		}
		return true;
	}

	if (tabIndex >= 1 && tabIndex <= pathLen)
	{
		// Handle double-click on the last breadcrumb tab (current level) -> inline rename
		if (isDoubleClick && tabIndex == pathLen && pathLen > 0)
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

		// Clicked a breadcrumb level - navigate back
		if (tabIndex == pathLen && isGroupViewActive())
		{
			// Clicked the current level - reset zoom
			viewportZoom = 1.0f;
			viewportPan = ofVec2f(0, 0);
			ensureTabStateSize();
			if (tabIndex < (int)tabZooms.size())
			{
				tabZooms[tabIndex] = viewportZoom;
				tabPans[tabIndex] = viewportPan;
			}
		}
		else
		{
			// Navigate back to that breadcrumb level
			navigateToBreadcrumbLevel(tabIndex);
		}
		return true;
	}

	// Child preset tab
	int childIndex = tabIndex - pathLen - 1;
	if (childIndex >= 0 && childIndex < (int)childIndices.size())
	{
		// Handle double-click on child -> inline rename
		if (isDoubleClick)
		{
			int realIdx = childIndices[childIndex];
			const vector<JPbox *> *boxList = &boxes;
			if (isGroupViewActive())
			{
				JPbox_preset *preset = getActivePreset();
				if (preset != nullptr) boxList = &preset->boxes;
			}
			if (realIdx >= 0 && realIdx < (int)boxList->size() && (*boxList)[realIdx] != nullptr)
			{
				// Start inline rename
				tabRenaming = true;
				tabRenameTabIndex = tabIndex;
				tabRenameBuffer = (*boxList)[realIdx]->name;
				tabRenameCursor = tabRenameBuffer.size();
			}
			return true;
		}

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

bool JPboxgroup::wantsKeyCapture() const
{
	return tabRenaming || mediaTimeFieldFocus != 0;
}

void JPboxgroup::keyPressed(int key)
{
	if (mediaTimeFieldFocus != 0)
	{
		auto *target=dynamic_cast<JPMediaInspectable *>(getInspectorBox());
		if(target==nullptr){mediaTimeFieldFocus=0;return;}
		JPMediaState&s=target->mediaState();
		if(key==OF_KEY_ESC){mediaTimeFieldFocus=0;mediaTimeFieldBuffer.clear();return;}
		if(key==OF_KEY_UP||key==OF_KEY_DOWN)
		{
			float &v=mediaTimeFieldFocus==1?s.rangeIn:s.rangeOut;
			v=target->mediaSteppedPosition(v,key==OF_KEY_UP?1:-1);
			v=ofClamp(v,0.0f,1.0f);
			if(mediaTimeFieldFocus==1&&v>s.rangeOut)s.rangeOut=v;
			else if(mediaTimeFieldFocus==2&&v<s.rangeIn)s.rangeIn=v;
			mediaTimeFieldBuffer=mediaTimeLabel(v*target->mediaDurationSeconds());
			mediaTimeFieldReplaceOnType=false;return;
		}
		if(key==OF_KEY_RETURN||key=='\r')
		{
			float n=0; if(parseMediaTime(mediaTimeFieldBuffer,target->mediaDurationSeconds(),target->mediaFrameCount(),n))
			{
				n=ofClamp(n,0.0f,1.0f);
				if(mediaTimeFieldFocus==1){s.rangeIn=n;if(n>s.rangeOut)s.rangeOut=n;}
				else{s.rangeOut=n;if(n<s.rangeIn)s.rangeIn=n;}
				markCueDraftDirty(cueSelectedIndex());
			}
			mediaTimeFieldFocus=0;return;
		}
		if(key==OF_KEY_BACKSPACE)
		{
			if(mediaTimeFieldReplaceOnType)mediaTimeFieldBuffer.clear();
			else if(!mediaTimeFieldBuffer.empty())mediaTimeFieldBuffer.pop_back();
			mediaTimeFieldReplaceOnType=false;
		}
		else if((key>='0'&&key<='9')||key==':'||key=='.'||key=='f'||key=='F')
		{
			if(mediaTimeFieldReplaceOnType)mediaTimeFieldBuffer.clear();
			mediaTimeFieldReplaceOnType=false;
			mediaTimeFieldBuffer.push_back((char)key);
		}
		return;
	}
	if (!tabRenaming)
		return;

	// Cancel on Escape
	if (key == OF_KEY_ESC)
	{
		cancelTabRename();
		return;
	}

	// Commit on Enter
	if (key == OF_KEY_RETURN || key == '\r')
	{
		commitTabRename();
		return;
	}

	// Cursor navigation + edit at cursor (LEFT/RIGHT/HOME/END/BACKSPACE/DEL/insert).
	if (tabRenameBuffer.size() < 64 || key < 32)
	{
		jp_textfield::handleKey(tabRenameBuffer, tabRenameCursor, key);
	}
}

bool JPboxgroup::handleInspectorRangeShortcut(int key)
{
	if ((key != OF_KEY_DOWN && key != OF_KEY_UP) ||
		!inspectorBodyContains(ofGetMouseX(), ofGetMouseY()))
	{
		return false;
	}
	if (getInspectorBox() == nullptr) return false;

	// Use the controller's parameter pointer so exposed parameters in preset
	// and group inspectors receive the same shortcut behavior as own params.
	for (JPcontroller *controller : controllers)
	{
		JPComplexSlider *slider =
			dynamic_cast<JPComplexSlider *>(controller);
		if (slider == nullptr || slider->parameters == nullptr ||
			slider->parameters->variabletype != JPParameter::FLOAT ||
			!slider->parameters->rangeEnabled ||
			!ofRectangle(slider->x - slider->width * 0.5f,
				slider->y - slider->height * 0.5f,
				slider->width, slider->height).intersects(inspectorBodyViewport) ||
			!slider->slider_value.mouseOver())
		{
			continue;
		}

		JPParameter *parameter = slider->parameters;
		if (key == OF_KEY_DOWN)
		{
			parameter->captureRangeStart();
		}
		else
		{
			parameter->captureRangeEnd();
		}
		markCueDraftDirty(cueSelectedIndex());
		if (isCueDraftMode()) updateCueDraftGraph();
		setControllers();
		return true;
	}
	return false;
}

// Extracted from keyPressed so clicking away can commit the rename too - the
// only exits used to be ENTER and ESC, so a click elsewhere left the field
// open and swallowing every keystroke.
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
