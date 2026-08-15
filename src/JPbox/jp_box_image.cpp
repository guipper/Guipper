#include "jp_box_image.h"
#include "jp_box_cam.h"
#include <FreeImage.h>
#include <chrono>
#include <mutex>
#include <unordered_map>

struct JPbox_image::GifData
{
	std::vector<ofPixels> frames;
	std::vector<double> ends;
	double duration = 0.0;
};

namespace
{
	int gifMetadataInt(FIBITMAP *bitmap, const char *key, int fallback)
	{
		FITAG *tag = nullptr;
		if (!FreeImage_GetMetadata(FIMD_ANIMATION, bitmap, key, &tag) || tag == nullptr)
			return fallback;
		const void *value = FreeImage_GetTagValue(tag);
		if (value == nullptr) return fallback;
		switch (FreeImage_GetTagType(tag))
		{
		case FIDT_BYTE: return *(const BYTE *)value;
		case FIDT_SHORT: return *(const WORD *)value;
		case FIDT_LONG: return (int)*(const DWORD *)value;
		default: return fallback;
		}
	}
}

JPbox_image::JPbox_image() {}
JPbox_image::~JPbox_image() {}
void JPbox_image::reload()
{
	isGifSource = jp_media::isGif(dir);
	if (isGifSource) startGifLoad();
	else img.loadImage(dir);
	// New pixels, and possibly new dimensions.
	invalidateRender();
}

void JPbox_image::invalidateRender()
{
	++sourceGeneration;
	lastRenderSignature.valid = false;
}
void JPbox_image::setup(string _dir, string _nombre)
{
	// JPbox::setup(_font);
	JPbox::setup(_dir, _nombre);
	img.clear();
	isGifSource = jp_media::isGif(_dir);
	if (isGifSource) startGifLoad();
	else img.loadImage(_dir);

	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addBoolValue(true, "strech");
	parameters.addFloatValue(1.0, "scale ratio");
	if (JPParameter *ratio = parameters.getJParameter(parameters.getSize()-1))
	{
		ratio->nativeMin = ratio->min = 0.1f;
		ratio->nativeMax = ratio->max = 4.0f;
		ratio->defaultFloatValue = 1.0f;
	}
	media.muted = true;

	tipo = IMAGEBOX;

	if (img.isAllocated() || jp_media::isGif(_dir)){
		cout << "CARGO BIEN LA IMAGEN" << endl;
	}
	else{
		cout << "CARGO COMO EL ORTO LA IMAGEN" << endl;
	}
	lasttime_autoreload = ofGetElapsedTimeMillis();
	duration_autoreload = 2000;

	// fbo.allocate(jp_constants::renderWidth, jp_constants::renderHeight);
}
void JPbox_image::update()
{
	JPbox::update();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetColor(255, 255);

	// Esto es para que si no recargo, recargue bien carajo.
	//
	// This half always runs: GIF timing has to keep advancing even when the
	// render is skipped, or an off-screen GIF would freeze and then jump when
	// it comes back into view.
	if (isGifSource)
	{
		if (onoff.boolValue) updateGif();
		else gifLastUpdate = ofGetElapsedTimef();
	}
	else if (!img.isAllocated() && ofGetElapsedTimeMillis() - lasttime_autoreload > duration_autoreload)
	{
		img.loadImage(dir);
		lasttime_autoreload = ofGetElapsedTimeMillis();
		if (img.isAllocated()) invalidateRender();
		cout << "RECARGA LA IMAGEN YA QUE LA CARGO COMO EL ORTO" << endl;
	}

	updateFBO();
}
void JPbox_image::updateFBO()
{

	if (onoff.boolValue)
	{
		// Legacy parameter sync stays outside the gate: it is state, not
		// pixels, and the inspector reads it back every frame.
		const bool legacyStretch=parameters.getBoolValue(4);
		if(legacyStretch!=lastLegacyStretch)media.fitMode=legacyStretch?JPMediaFitMode::Stretch:JPMediaFitMode::Custom;
		parameters.setBoolValue(media.fitMode==JPMediaFitMode::Stretch,4);
		lastLegacyStretch=parameters.getBoolValue(4);
		const float scaleRatio = parameters.getFloatValue(5);

		const float sourceW = gifTexture.isAllocated() ? gifTexture.getWidth() :
			(img.isAllocated() ? img.getWidth() : 0.0f);
		const float sourceH = gifTexture.isAllocated() ? gifTexture.getHeight() :
			(img.isAllocated() ? img.getHeight() : 0.0f);

		JPMediaRenderSignature signature;
		signature.valid = true;
		signature.fitMode = (int)media.fitMode;
		signature.scaleX = parameters.getFloatValue(0);
		signature.scaleY = parameters.getFloatValue(1);
		signature.offsetX = parameters.getFloatValue(2);
		signature.offsetY = parameters.getFloatValue(3);
		signature.scaleRatio = scaleRatio;
		signature.targetW = jp_constants::renderWidth;
		signature.targetH = jp_constants::renderHeight;
		signature.sourceW = sourceW;
		signature.sourceH = sourceH;
		signature.sourceGeneration = sourceGeneration;

		// A still image composited with an unchanged transform produces the
		// same pixels it produced last frame, so the pass buys nothing. The
		// FBO already holds that result and nothing else writes to it.
		const bool unchanged = fbo.isAllocated() &&
			signature.matches(lastRenderSignature);
		// Off the dependency path, the scheduler drops us to the same staggered
		// preview rate it already applies to shader boxes.
		if (unchanged || !shouldRenderThisFrame())
		{
			jp_box_media_stats::countSkipped();
			return;
		}
		lastRenderSignature = signature;
		jp_box_media_stats::countRendered();

		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		fbo.begin();
		ofClear(0, 0, 0, 0);
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		auto render = [&](float sw, float sh, auto draw)
		{
			draw(jp_media::transformedRect(sw, sh,
				jp_constants::renderWidth, jp_constants::renderHeight,
				media.fitMode, parameters.getFloatValue(0),
				parameters.getFloatValue(1), parameters.getFloatValue(2),
				parameters.getFloatValue(3), scaleRatio, 0.5f, 0.5f));
		};
		if (gifTexture.isAllocated())
			render(gifTexture.getWidth(), gifTexture.getHeight(),
				[&](const ofRectangle &r){ gifTexture.draw(r.x,r.y,r.width,r.height); });
		else if (img.isAllocated())
			render(img.getWidth(), img.getHeight(),
				[&](const ofRectangle &r){ img.draw(r.x,r.y,r.width,r.height); });
		ofEnableAlphaBlending();
		fbo.end();
	}
	else
	{
		JPbox::updateFBO();
	}
}

void JPbox_image::startGifLoad()
{
	gif.reset(); gifTexture.clear(); gifFrame = -1; loadStatus = "Loading GIF";
	string path = ofToDataPath(dir, true);
	try { path = std::filesystem::weakly_canonical(path).string(); }
	catch (...) {}
	using Result = std::shared_ptr<const GifData>;
	static std::mutex cacheMutex;
	static std::unordered_map<string, std::shared_future<Result>> cache;
	string key = path;
	try { key += ":" + std::to_string((long long)std::filesystem::last_write_time(path).time_since_epoch().count()); }
	catch (...) {}
	std::lock_guard<std::mutex> lock(cacheMutex);
	auto found = cache.find(key);
	if (found != cache.end()) { gifFuture = found->second; return; }
	gifFuture = std::async(std::launch::async, [path]() -> Result
	{
		auto result = std::make_shared<GifData>();
		FIMULTIBITMAP *multi = FreeImage_OpenMultiBitmap(FIF_GIF,
			path.c_str(), FALSE, TRUE, TRUE, GIF_LOAD256);
		if (multi == nullptr) return {};
		const int pages = FreeImage_GetPageCount(multi);
		ofPixels canvas, restore;
		int canvasW = 0, canvasH = 0;
		for (int i=0; i<pages; ++i)
		{
			FIBITMAP *page = FreeImage_LockPage(multi, i);
			if (!page) continue;
			FIBITMAP *rgba = FreeImage_ConvertTo32Bits(page);
			const int pw = FreeImage_GetWidth(rgba), ph = FreeImage_GetHeight(rgba);
			const int left = gifMetadataInt(page, "FrameLeft", 0);
			const int top = gifMetadataInt(page, "FrameTop", 0);
			canvasW = std::max(canvasW, left+pw); canvasH = std::max(canvasH, top+ph);
			if (!canvas.isAllocated()) { canvas.allocate(canvasW,canvasH,OF_PIXELS_RGBA); canvas.set(0); }
			else if (canvas.getWidth()<canvasW || canvas.getHeight()<canvasH)
			{
				ofPixels grown; grown.allocate(canvasW,canvasH,OF_PIXELS_RGBA); grown.set(0);
				canvas.pasteInto(grown,0,0); canvas.swap(grown);
			}
			const int disposal = gifMetadataInt(page, "DisposalMethod", 0);
			if (disposal == 3) restore = canvas;
			const BYTE *bits = FreeImage_GetBits(rgba); const int pitch = FreeImage_GetPitch(rgba);
			for (int y=0;y<ph;++y) for(int x=0;x<pw;++x)
			{
				const BYTE *src = bits + (ph-1-y)*pitch + x*4;
				ofColor c(src[FI_RGBA_RED],src[FI_RGBA_GREEN],src[FI_RGBA_BLUE],src[FI_RGBA_ALPHA]);
				if(c.a>0) canvas.setColor(left+x,top+y,c);
			}
			result->frames.push_back(canvas);
			const int ms = std::max(10, gifMetadataInt(page,"FrameTime",100));
			result->duration += ms/1000.0; result->ends.push_back(result->duration);
			if (disposal == 2)
				for(int y=0;y<ph;++y) for(int x=0;x<pw;++x) canvas.setColor(left+x,top+y,ofColor(0,0));
			else if (disposal == 3 && restore.isAllocated()) canvas = restore;
			FreeImage_Unload(rgba); FreeImage_UnlockPage(multi,page,FALSE);
		}
		FreeImage_CloseMultiBitmap(multi,0);
		return result->frames.empty() ? Result{} : result;
	}).share();
	cache[key] = gifFuture;
}

void JPbox_image::updateGif()
{
	if (!gif && gifFuture.valid() && gifFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		gif = gifFuture.get(); loadStatus = gif ? "Ready" : "GIF decode failed";
		gifLastUpdate = ofGetElapsedTimef();
	}
	if (!gif || gif->frames.empty()) return;
	const double now=ofGetElapsedTimef(), dt=std::max(0.0,now-gifLastUpdate); gifLastUpdate=now;
	jp_media::normalize(media);
	if (media.playing && gif->frames.size()>1 && media.rangeOut>media.rangeIn)
	{
		float next=media.position+(media.reverse?-1:1)*(float)(dt*media.rate/gif->duration);
		jp_media::applyBoundary(media,next);
		media.position=ofClamp(next,media.rangeIn,media.rangeOut);
	}
	const double t=media.position*gif->duration;
	int frame=(int)(std::upper_bound(gif->ends.begin(),gif->ends.end(),t)-gif->ends.begin());
	frame=ofClamp(frame,0,(int)gif->frames.size()-1);
	// New texture contents: the signature has to move or the render would be
	// skipped and the GIF would sit on one frame.
	if(frame!=gifFrame){gifTexture.loadData(gif->frames[frame]);gifFrame=frame;invalidateRender();}
}

bool JPbox_image::mediaPlayable() const { return gif && gif->frames.size()>1; }
bool JPbox_image::mediaReady() const { return isGifSource ? (bool)gif : img.isAllocated(); }
string JPbox_image::mediaStatus() const { return mediaReady()?"Ready":(loadStatus.empty()?"Loading image":loadStatus); }
double JPbox_image::mediaDurationSeconds() const { return gif?gif->duration:0.0; }
int JPbox_image::mediaFrameCount() const { return gif?(int)gif->frames.size():(img.isAllocated()?1:0); }
float JPbox_image::mediaSteppedPosition(float normalized, int frames) const
{
	if(!gif || gif->frames.size()<2 || gif->duration<=0.0)return normalized;
	const double t=ofClamp(normalized,0.0f,1.0f)*gif->duration;
	int current=(int)(std::upper_bound(gif->ends.begin(),gif->ends.end(),t)-gif->ends.begin());
	current=ofClamp(current,0,(int)gif->frames.size()-1);
	const int target=ofClamp(current+frames,0,(int)gif->frames.size()-1);
	const double start=target==0?0.0:gif->ends[target-1];
	return ofClamp((float)(start/gif->duration),0.0f,1.0f);
}
void JPbox_image::mediaSeek(float n){media.position=ofClamp(n,0,1);gifFrame=-1;invalidateRender();}
void JPbox_image::mediaStep(int frames){media.playing=false;mediaSeek(ofClamp(mediaSteppedPosition(media.position,frames),media.rangeIn,media.rangeOut));}
void JPbox_image::mediaRestart(){mediaSeek(media.reverse?media.rangeOut:media.rangeIn);}
void JPbox_image::saveCustomState(ofXml &boxNode) const { jp_media::save(boxNode,media); }
void JPbox_image::loadCustomState(const ofXml &boxNode){if(!jp_media::load(boxNode,media)){auto ps=boxNode.getChild("parameters").getChildren("param");for(auto&p:ps)if(p.getChild("name").getValue()=="strech")media.fitMode=p.getChild("value").getBoolValue()?JPMediaFitMode::Stretch:JPMediaFitMode::Custom;}}
// A cue-draft clone starts with an empty FBO, so it must paint once regardless
// of how closely its state matches the box it was cloned from.
void JPbox_image::copyCustomStateFrom(const JPbox *source){if(auto image=dynamic_cast<const JPbox_image*>(source)){media=image->media;mediaSeek(media.position);invalidateRender();}}
void JPbox_image::draw()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	// PARA QUE EL FBO FUNCIONE BIEN NECESITA OFRECTMODE CORNER CUANDO LEVANTA EL SHADER, AS� QUE LO PONEMOS ASI
	// shaderrender.fbo.draw(x- width/2, y-height/2, width, height);
	ofSetColor(255);
	JPbox::draw();
	// fbo.draw(x - width / 2, y - height / 2, width, height);
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
}
void JPbox_image::clear()
{
	JPbox::clear();
	img.clear();
	gif.reset();
	gifFuture = {};
	gifTexture.clear();
	gifFrame = -1;
	cout << "CORRE CLEAR SHADERBOX " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
