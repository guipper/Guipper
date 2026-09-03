#include "jp_box_image.h"
#include "jp_box_cam.h"
#include <chrono>

JPbox_image::JPbox_image() {}
JPbox_image::~JPbox_image() {}
void JPbox_image::reload()
{
	isGifSource = jp_media::isGif(dir);
	imageLoadAttempts = 0;
	if (isGifSource) startGifLoad();
	else startImageLoad();
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
	else startImageLoad();

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
	else updateImage();

	updateFBO();
}
void JPbox_image::updateFBO()
{

	if (onoff.boolValue)
	{
		// Legacy parameter sync stays outside the gate: it is state, not
		// pixels, and the inspector reads it back every frame.
		const bool legacyStretch=parameters.getBoolValue(4);
		// First frame: adopt what is there instead of comparing against a
		// default nobody set. loadCustomState has already decided fitMode - from
		// <media>, or from this very `strech` value for a pre-media file - and
		// comparing a loaded `false` against the constructor's `true` used to
		// read as "stretch was just unticked" and rewrote that fit to Custom.
		// The symptom was every image coming back deformed after a reload.
		if(!legacyShadowsPrimed){legacyShadowsPrimed=true;lastLegacyStretch=legacyStretch;}
		if(legacyStretch!=lastLegacyStretch)media.fitMode=legacyStretch?JPMediaFitMode::Stretch:JPMediaFitMode::Custom;
		parameters.setBoolValue(media.fitMode==JPMediaFitMode::Stretch,4);
		lastLegacyStretch=parameters.getBoolValue(4);
		const float scaleRatio = parameters.getFloatValue(5);

		// The GIF's pixels reach the GPU HERE, on a frame we are actually going
		// to composite - not in updateGif, which runs even while the render is
		// skipped so the clock keeps advancing. The transfer is the whole frame
		// at full resolution (16 MB a frame for the larger GIFs in a real
		// composition), and paying it for a box the scheduler has throttled to
		// one frame in four, or that is off screen entirely, buys nothing.
		//
		// Catching up uploads ONE frame, the current one, never the backlog.
		if (isGifSource && shouldRenderThisFrame() && gif &&
			gifPendingFrame >= 0 && gifPendingFrame != gifFrame &&
			gifPendingFrame < (int)gif->frames.size())
		{
			gifTexture.loadData(gif->frames[gifPendingFrame]);
			gifFrame = gifPendingFrame;
		}

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

void JPbox_image::startImageLoad()
{
	img.clear();
	loadStatus = "Loading image";
	imageLoadAttempts++;
	lasttime_autoreload = ofGetElapsedTimeMillis();
	imageFuture = jp_quick_image::requestImage(dir);
}

// Adopts the decoded pixels on the frame the worker finishes, and nothing else.
// The upload (setFromPixels) is the only part that has to be here: it is GL.
void JPbox_image::updateImage()
{
	if (imageFuture.valid() &&
		imageFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		std::shared_ptr<const ofPixels> pixels = imageFuture.get();
		imageFuture = {};
		if (pixels && pixels->isAllocated())
		{
			img.setFromPixels(*pixels);
			loadStatus = "Ready";
			invalidateRender();
		}
		else
		{
			loadStatus = "Image decode failed";
		}
		lasttime_autoreload = ofGetElapsedTimeMillis();
		return;
	}
	if (imageFuture.valid() || img.isAllocated()) return;
	// Not loaded and nothing in flight: the file may still be being copied in.
	// Retry a few times, then stop - this used to hammer the disk forever.
	if (imageLoadAttempts >= 4) return;
	if (ofGetElapsedTimeMillis() - lasttime_autoreload <= duration_autoreload) return;
	startImageLoad();
}

void JPbox_image::startGifLoad()
{
	gif.reset(); gifTexture.clear(); gifFrame = -1; gifPendingFrame = -1;
	loadStatus = "Loading GIF";
	gifFuture = jp_quick_image::requestGif(dir);
}

void JPbox_image::updateGif()
{
	if (!gif && gifFuture.valid() && gifFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		gif = gifFuture.get(); loadStatus = gif ? "Ready" : "GIF decode failed";
		// Release the future: it holds a second strong reference to the frames,
		// and the cache is now weak, so leaving it here would pin ~hundreds of
		// MB for the life of the box even after it is deleted.
		gifFuture = {};
		// Hand ownership over to this box: until the cache demotes its own copy
		// to a weak reference, deleting the box would free nothing.
		jp_quick_image::compactGifCache();
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
	// Only NOTE the frame; updateFBO uploads it if and when it renders. The
	// signature still has to move or the render would be skipped and the GIF
	// would sit on one frame.
	if(frame!=gifPendingFrame){gifPendingFrame=frame;invalidateRender();}
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
void JPbox_image::mediaSeek(float n){media.position=ofClamp(n,0,1);gifFrame=-1;gifPendingFrame=-1;invalidateRender();}
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
	imageFuture = {};
	gif.reset();
	gifFuture = {};
	gifTexture.clear();
	gifFrame = -1;
	gifPendingFrame = -1;
	cout << "CORRE CLEAR SHADERBOX " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
