#include "jp_box_video.h"

JPbox_video::JPbox_video() { }
JPbox_video::~JPbox_video() { }

void JPbox_video::setup(string _dir, string _nombre) {

	JPbox::setup(_dir, _nombre);

	//parameters.coutData();
	name = _nombre;
	dir = _dir;
	//img.loadImage(_dir);
	movie.setPixelFormat(OF_PIXELS_RGBA);
	movie.loadAsync(_dir);
	movie.setLoopState(OF_LOOP_NONE);
	movie.setVolume(0.0f);
	// NOT play() here - the load is asynchronous and cannot have finished by
	// this line. updateFBO starts playback on the first frame the movie is
	// genuinely loaded. See playbackStarted.
	playbackStarted = false;
	sourceDuration = 0.0;
	sourceFrames = 0;
	nextMetadataQueryMs = 0;

	parameters.addFloatValue(0.5, "scalex");
	parameters.addFloatValue(0.5, "scaley");
	parameters.addFloatValue(0.5, "offsetx");
	parameters.addFloatValue(0.5, "offsety");
	parameters.addBoolValue(true, "strech");
	parameters.addFloatValue(0.25, "speed");
	parameters.addFloatValue(0.0, "position");
	parameters.addBoolValue(true, "play");
	parameters.addFloatValue(1.0, "scale ratio");
	if (JPParameter *ratio = parameters.getJParameter(parameters.getSize()-1))
	{
		ratio->nativeMin = ratio->min = 0.1f;
		ratio->nativeMax = ratio->max = 4.0f;
		ratio->defaultFloatValue = 1.0f;
	}
	tipo = VIDEOBOX;
	auxpos = -1.0f;
}
void JPbox_video::update() {
	JPbox::update();
	updateFBO();
}
void JPbox_video::invalidateRender()
{
	++sourceGeneration;
	lastRenderSignature.valid = false;
}
void JPbox_video::requestSpeed(float target, bool immediate)
{
	// Never two seeks in one frame. A flushing seek that lands while the
	// previous one is still settling leaves the pipeline's segment in a state
	// where gst_segment_do_seek trips its format assertion and the whole
	// process wedges.
	if (seekedThisFrame) return;
	if (std::abs(target-lastAppliedSpeed) < 0.0005f) return;

	const uint64_t now = ofGetElapsedTimeMillis();
	// A direction change cannot wait: setPosition reads the sign of the speed
	// the backend currently holds to decide which way its segment runs.
	const bool signFlip = (target < 0.0f) != (lastAppliedSpeed < 0.0f);
	if (!immediate && !signFlip)
	{
		// Debounce: restart the timer whenever the requested value moves, so a
		// slider drag collapses into one seek once it settles rather than one
		// seek per frame.
		if (std::abs(target-pendingSpeed) >= 0.0005f)
		{
			pendingSpeed = target;
			pendingSpeedSinceMs = now;
			return;
		}
		if (now-pendingSpeedSinceMs < 120) return;
	}
	movie.setSpeed(target);
	lastAppliedSpeed = target;
	pendingSpeed = target;
	pendingSpeedSinceMs = now;
	seekedThisFrame = true;
}
void JPbox_video::updateFBO() {
	if (onoff.boolValue) {
		movie.update();
		seekedThisFrame = false;
		// GUIPPER_VIDEO_TRACE=1 reports what the video backend is actually
		// doing, once a second per box. The backend differs per platform -
		// GStreamer on Linux, AVFoundation on macOS, DirectShow on Windows -
		// and they disagree about isLoaded/isPaused/getTotalNumFrames timing,
		// so "the video does not work" is only diagnosable with these values
		// from the machine that shows the problem.
		static const bool trace = std::getenv("GUIPPER_VIDEO_TRACE") != nullptr;
		if (trace)
		{
			const uint64_t now = ofGetElapsedTimeMillis();
			if (now >= nextTraceMs)
			{
				nextTraceMs = now + 1000;
				ofLogNotice("videotrace")
					<< name
					<< " loaded=" << movie.isLoaded()
					<< " playing=" << (movie.isPlaying() ? 1 : 0)
					<< " paused=" << (movie.isPaused() ? 1 : 0)
					<< " started=" << playbackStarted
					<< " size=" << movie.getWidth() << "x" << movie.getHeight()
					<< " frames=" << movie.getTotalNumFrames()
					<< " dur=" << movie.getDuration()
					<< " pos=" << movie.getPosition()
					<< " done=" << (movie.getIsMovieDone() ? 1 : 0)
					<< " gen=" << sourceGeneration
					<< " mediaPlaying=" << (media.playing ? 1 : 0)
					<< " rate=" << media.rate
					<< " dir=" << dir;
			}
		}
		// Everything from here to the FBO block is state, not pixels: decoder
		// pumping, loop/boundary handling and the legacy parameter write-back
		// the inspector reads. It must run every frame even when the render is
		// skipped, or playback position freezes and the position slider stops
		// tracking.
		if (movie.isFrameNew()) invalidateRender();
		if (movie.isLoaded())
		{
			const uint64_t now=ofGetElapsedTimeMillis();
			if((sourceDuration<=0.0 || sourceFrames<=0) && now>=nextMetadataQueryMs)
			{
				sourceDuration=std::max(sourceDuration,(double)movie.getDuration());
				sourceFrames=std::max(sourceFrames,movie.getTotalNumFrames());
				nextMetadataQueryMs=now+250;
			}
			// Playback has to be STARTED once, here, rather than in setup().
			// setup() issues play() one line after loadAsync(), when the asset
			// cannot possibly be ready: a backend that queues the state change
			// until preroll honours it, one that requires a loaded asset drops
			// it on the floor. Nothing afterwards recovers, because the pause
			// management below only acts when isPaused() DISAGREES with the
			// target - and a player that was never started can report itself
			// as not paused, which agrees, so it sits on frame one forever.
			//
			// Gated on mediaReady(), NOT isLoaded(). Measured with
			// GUIPPER_VIDEO_TRACE: isLoaded() goes true a good two seconds
			// before the movie has any dimensions or duration - 720x480 arrives
			// long after loaded=1 - and starting inside that window repeats the
			// original mistake, just later. mediaReady() is the existing
			// "metadata has actually arrived" test the inspector already uses.
			//
			// The setPaused call below runs later in this same update, so a box
			// restored in a paused state never advances a frame.
			if(!playbackStarted && mediaReady()){playbackStarted=true;movie.play();}
			if(mediaSeekPending){movie.setPosition(media.position);mediaSeekPending=false;}
			const float legacySpeed=parameters.getFloatValue(5);
			const float legacyPosition=parameters.getFloatValue(6);
			const bool legacyPlay=parameters.getBoolValue(7);
			const bool legacyStretch=parameters.getBoolValue(4);
			if(std::abs(legacySpeed-lastLegacySpeed)>0.0001f)media.rate=ofClamp(legacySpeed*4.0f,0.25f,4.0f);
			if(std::abs(legacyPosition-lastLegacyPosition)>0.0001f)mediaSeek(legacyPosition);
			if(legacyPlay!=lastLegacyPlay)media.playing=legacyPlay;
			if(legacyStretch!=lastLegacyStretch)media.fitMode=legacyStretch?JPMediaFitMode::Stretch:JPMediaFitMode::Custom;
			jp_media::normalize(media);
			const bool zeroRange = std::abs(media.rangeOut-media.rangeIn) < 0.000001f;
			// Guarded because ofGstUtils::setVolume is a bare g_object_set on
			// the pipeline with no change check of its own - unlike setSpeed
			// below, which early-returns when the value has not moved.
			const float targetVolume =
				media.muted || !isactiverender ? 0.0f : media.volume;
			if (std::abs(targetVolume - lastAppliedVolume) > 0.0001f)
			{
				movie.setVolume(targetVolume);
				lastAppliedVolume = targetVolume;
			}
			const bool shouldPause=!media.playing || zeroRange;
			if(movie.isPaused()!=shouldPause)movie.setPaused(shouldPause);
			if (media.playing && !zeroRange)
			{
				// A negative GStreamer segment decodes backward continuously. Do not
				// emulate reverse with setPosition() here: each accurate seek flushes
				// the decoder and produces visible stalls on inter-frame codecs.
				//
				// Debounced: dragging the speed slider changes this value on
				// every frame, and one flushing seek per frame is what wedged
				// the pipeline.
				requestSpeed(media.reverse ? -media.rate : media.rate, false);
			}
			const float backendPosition=movie.getPosition();
			float p = backendPosition>=0.0f ?
				ofClamp(backendPosition,0.0f,1.0f):media.position;

			// The backend never reports the nominal end. It parks on the last
			// frame's timestamp and raises EOS - measured at 0.999977 on a 431
			// frame clip - so `p >= rangeOut` is false forever and the clip sits
			// on its final frame: ping-pong never turns around and loop never
			// wraps. (Reverse toward 0 was unaffected, because seeking to zero
			// is exact, which is why only the forward end was broken.)
			//
			// Snap to the boundary when the backend says it is done, with a
			// half-frame tolerance as a fallback for containers that do not
			// raise EOS. Half a frame cannot skip a frame that would otherwise
			// have been shown.
			if (!zeroRange && media.playing)
			{
				const float edge = sourceFrames > 1 ?
					0.5f/(float)sourceFrames : 0.0005f;
				// EOS means "end of the current playback segment", and which
				// end that is depends on direction: playing backward, the
				// segment ends at frame ZERO. The flag also latches - it is
				// only cleared when a new buffer arrives, never by a seek - so
				// after a reverse leg reaches the start of the file it is still
				// raised on the first frames of the following forward leg.
				//
				// Trusting it on its own therefore snapped the clip to the far
				// end and bounced it straight back, which reads as a freeze.
				// It only happened when IN sat near the start of the file,
				// because that is the only case where a reverse leg reaches
				// frame zero at all - with IN mid-file no EOS is ever raised
				// and the same code worked fine.
				//
				// Requiring the position to agree with the flag makes it
				// direction-safe: a stale EOS carried over from the other end
				// of the range can no longer fire the wrong boundary.
				const float midpoint =
					(media.rangeIn+media.rangeOut)*0.5f;
				const bool eos = movie.getIsMovieDone();
				const bool atEnd = p >= media.rangeOut-edge ||
					(eos && p > midpoint);
				const bool atStart = p <= media.rangeIn+edge ||
					(eos && p <= midpoint);
				if (!media.reverse && atEnd) p = media.rangeOut;
				else if (media.reverse && atStart) p = media.rangeIn;
			}

			if (zeroRange)
			{
				p=media.rangeIn;
				if(backendPosition<0.0f || std::abs(backendPosition-p)>0.000001f)
					movie.setPosition(p);
			}
			else
			{
				const bool wasReverse = media.reverse;
				if (jp_media::applyBoundary(media, p))
				{
					// A turnaround needs the sign applied before the position
					// seek: ofGstUtils::setPosition picks its seek direction
					// from the speed the backend currently holds, so seeking
					// first would run a forward segment for material we are
					// about to play backward.
					//
					// These are two seeks in one frame, which is exactly what
					// the throttle exists to prevent - but a boundary is a rare
					// event (once per loop, not once per frame), and the pair
					// is ordered rather than racing. The speed request is
					// marked immediate so the debounce cannot defer the sign
					// past the seek that depends on it.
					if (media.playing && media.reverse != wasReverse)
					{
						requestSpeed(media.reverse ? -media.rate : media.rate,
							true);
					}
					movie.setPosition(p);
				}
			}
			media.position = p;
		}
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);

		if (!movie.isLoaded() && auxpos != parameters.getFloatValue(6)) {
			auxpos = parameters.getFloatValue(6);
			movie.setPosition(parameters.getFloatValue(6));
		}
		parameters.setFloatValue(media.rate / 4.0f, 5);
		parameters.setFloatValue(media.position, 6);
		parameters.setBoolValue(media.playing, 7);
		parameters.setBoolValue(media.fitMode==JPMediaFitMode::Stretch,4);
		lastLegacySpeed=parameters.getFloatValue(5);
		lastLegacyPosition=parameters.getFloatValue(6);
		lastLegacyPlay=parameters.getBoolValue(7);
		lastLegacyStretch=parameters.getBoolValue(4);
		auxpos = media.position;

		JPMediaRenderSignature signature;
		signature.valid = true;
		signature.fitMode = (int)media.fitMode;
		signature.scaleX = parameters.getFloatValue(0);
		signature.scaleY = parameters.getFloatValue(1);
		signature.offsetX = parameters.getFloatValue(2);
		signature.offsetY = parameters.getFloatValue(3);
		signature.scaleRatio = parameters.getFloatValue(8);
		signature.targetW = jp_constants::renderWidth;
		signature.targetH = jp_constants::renderHeight;
		signature.sourceW = movie.getWidth();
		signature.sourceH = movie.getHeight();
		signature.sourceGeneration = sourceGeneration;

		// No new decoded frame and no transform change means the composite
		// would be identical to the one already in the FBO. A 30fps clip in a
		// 60fps app hits this on every second frame; a paused one hits it
		// always.
		const bool unchanged = fbo.isAllocated() &&
			signature.matches(lastRenderSignature);
		if (unchanged || !shouldRenderThisFrame())
		{
			jp_box_media_stats::countSkipped();
			return;
		}
		lastRenderSignature = signature;
		jp_box_media_stats::countRendered();

		fbo.begin();
		ofClear(0, 0, 0, 0);
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		ofRectangle r = jp_media::transformedRect(movie.getWidth(), movie.getHeight(),
			jp_constants::renderWidth, jp_constants::renderHeight, media.fitMode,
			parameters.getFloatValue(0), parameters.getFloatValue(1),
			parameters.getFloatValue(2), parameters.getFloatValue(3),
			parameters.getFloatValue(8), 0.5f, 1.0f);
		movie.draw(r.x, r.y, r.width, r.height);
		ofEnableAlphaBlending();
		fbo.end();
	} else {
		if (movie.isLoaded() && !movie.isPaused()) movie.setPaused(true);
		JPbox::updateFBO();
	}
}

void JPbox_video::mediaSeek(float normalized)
{
	media.position = ofClamp(normalized, 0.0f, 1.0f);
	// Deliberately NOT routed through the seek throttle. mediaSeekPending
	// carries no target of its own - it replays media.position - and updateFBO
	// overwrites media.position from the backend every frame, so deferring a
	// position seek loses where it was supposed to go.
	if (movie.isLoaded())
	{
		movie.setPosition(media.position);
		mediaSeekPending = false;
	}
	else mediaSeekPending = true;
}
bool JPbox_video::mediaReady() const
{
	return movie.isLoaded() && sourceDuration>0.0 && sourceFrames>0;
}
string JPbox_video::mediaStatus() const
{
	return !movie.isLoaded()?"Loading video":(mediaReady()?"Ready":"Loading metadata");
}
float JPbox_video::mediaSteppedPosition(float normalized, int frames) const
{
	const int count=sourceFrames;
	if(count<=1)return normalized;
	const int current=ofClamp((int)std::round(normalized*(count-1)),0,count-1);
	const int target=ofClamp(current+frames,0,count-1);
	return ofClamp(target/(float)(count-1),0.0f,1.0f);
}
void JPbox_video::mediaStep(int frames)
{
	media.playing = false;
	if (movie.isLoaded())
	{
		mediaSeek(ofClamp(mediaSteppedPosition(media.position,frames),media.rangeIn,media.rangeOut));
	}
}
void JPbox_video::mediaRestart()
{
	mediaSeek(media.reverse ? media.rangeOut : media.rangeIn);
}
void JPbox_video::saveCustomState(ofXml &boxNode) const { jp_media::save(boxNode, media); }
void JPbox_video::loadCustomState(const ofXml &boxNode)
{
	const bool legacyTransform = jp_media::transformVersion(boxNode) < 2;
	if (!jp_media::load(boxNode, media))
	{
		media.muted = true;
		auto params = boxNode.getChild("parameters").getChildren("param");
		for (auto &p : params)
		{
			const string n = p.getChild("name").getValue();
			if (n == "strech") media.fitMode = p.getChild("value").getBoolValue() ?
				JPMediaFitMode::Stretch : JPMediaFitMode::Custom;
			else if (n == "position") media.position = p.getChild("value").getFloatValue();
			else if (n == "play") media.playing = p.getChild("value").getBoolValue();
			else if (n == "speed")
			{
				const float old = p.getChild("value").getFloatValue()*4.0f;
				if (old <= 0.0f) media.playing = false;
				media.rate = std::max(0.25f, old);
			}
		}
		jp_media::normalize(media);
	}
	if (legacyTransform)
	{
		// Older video boxes used 1.0 as the neutral X scale. Normalize them to
		// the same 0.5 center as every other transform without changing pixels.
		if (JPParameter *scaleX=parameters.getJParameter(0))
		{
			scaleX->floatValue*=0.5f;scaleX->floatLerpValue*=0.5f;
			scaleX->min*=0.5f;scaleX->max*=0.5f;
			scaleX->defaultFloatValue*=0.5f;
			scaleX->audioBase*=0.5f;
		}
	}
	mediaSeek(media.position);
}
void JPbox_video::copyCustomStateFrom(const JPbox *source)
{
	if (auto video = dynamic_cast<const JPbox_video *>(source))
	{
		media = video->media;
		mediaSeek(media.position);
		// A clone's FBO starts empty, so it has to paint once even though its
		// state now matches the source exactly.
		invalidateRender();
	}
}
void JPbox_video::draw() {
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();
}
void JPbox_video::clear() {
	JPbox::clear();
	movie.close();
	movie.closeMovie();
	//movie.unbind();
	//movie.
	//movie.clear();
	cout << "CORRE CLEAR VIDEOBOX " << endl;
	fbo.clear();
	fbo.destroy();
	fbohandlergroup.clear();
}
