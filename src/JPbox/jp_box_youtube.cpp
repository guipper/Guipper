#include "jp_box_youtube.h"
#include "../JPutils/jp_app_paths.h"
#include "../JPgui/jp_gl_state.h"

JPbox_youtube::JPbox_youtube(jp_youtube::Runner runner)
    : resolver(jp::AppPaths::current().cache / "youtube", runner),
      download(jp::AppPaths::current().cache / "youtube", std::move(runner)) {}
JPbox_youtube::~JPbox_youtube() {
    // A streaming decoder may still read its temporary HLS playlist.
    movie.close(); resolver.cancel(); download.cancel();
}
void JPbox_youtube::setup(string directory, string label) {
    setupVideo("youtubeVideo", label, false); tipo = YOUTUBEBOX;
    media.fitMode = JPMediaFitMode::Fit; media.muted = true;
}
void JPbox_youtube::closeVideo() {
    movie.close(); videoTexture.clear(); openedFile.clear();
    playbackStarted = false; sourceDuration = 0; sourceFrames = 0;
    nextMetadataQueryMs = 0; loadingSince = 0; lastFrameAt = 0; lastAppliedVolume = -1;
    lastAppliedSpeed = 0; pendingSpeed = 0; legacyShadowsPrimed = false;
    invalidateRender();
    if (fbo.isAllocated()) {jp_gl::ScopedNoScissor clip; fbo.begin(); ofClear(0,0,0,0); fbo.end();}
}
void JPbox_youtube::openSource(const std::string &source, bool local) {
    if (source.empty() || retained) return;
    closeVideo(); playbackError.clear(); openedFile = source; usingCachedFile = local;
    loadingSince = lastFrameAt = ofGetElapsedTimeMillis();
    movie.setPixelFormat(OF_PIXELS_RGBA); movie.loadAsync(openedFile); movie.setVolume(0);
    // Keep the playhead when switching to the completed local copy.
    pendingSourcePosition = media.position > 0 ? media.position : -1;
    mediaSeekPending = false;
}
void JPbox_youtube::playStreaming() {
    if (retained) return;
    const auto canonical = jp_youtube::canonicalUrl(urlText);
    if (canonical.empty()) {playbackError = "Pega un enlace de un video de YouTube"; return;}
    if (canonical != sourceUrl) {download.cancel(); cacheApplied = true; media.position = 0;}
    closeVideo(); resolver.cancel(); playbackError.clear();
    if (!preparing()) cacheApplied = true;
    sourceUrl = urlText = canonical; preferCache = false; resumePending = false;
    resolveApplied = false; streamHasAudio = true; setonoff(true); resolver.resolve(sourceUrl);
}
void JPbox_youtube::downloadToCache() {
    if (retained) return;
    const auto canonical = jp_youtube::canonicalUrl(urlText);
    if (canonical.empty()) {playbackError = "Pega un enlace de un video de YouTube"; return;}
    const bool force = !playbackError.empty() && usingCachedFile && canonical == sourceUrl;
    if (canonical != sourceUrl) {
        closeVideo(); resolver.cancel(); resolveApplied = true; media.position = 0;
    }
    playbackError.clear(); sourceUrl = urlText = canonical; resumePending = false;
    if (openedFile.empty() && !connecting()) preferCache = true;
    cacheApplied = false; setonoff(true); download.start(sourceUrl, force);
}
void JPbox_youtube::cancelPreparation() { download.cancel(); cacheApplied = true; }
void JPbox_youtube::cancelConnection() {
    closeVideo(); resolver.cancel(); resolveApplied = true; resumePending = false;
}
bool JPbox_youtube::preparing() const {return download.snapshot().state == jp_youtube::State::Preparing;}
bool JPbox_youtube::connecting() const {return resolver.snapshot().state == jp_youtube::State::Preparing;}
bool JPbox_youtube::cacheAvailable() const {return !download.cached(urlText).empty();}
void JPbox_youtube::suspend() {
    if (openedFile.empty() && !connecting() && !preparing()) return;
    if (!openedFile.empty() || connecting()) resumePending = true;
    closeVideo(); resolver.cancel(); download.cancel(); resolveApplied = cacheApplied = true;
}
void JPbox_youtube::update() {
    if (retained) return;
    if (!onoff.boolValue) {
        if (!openedFile.empty() || preparing() || connecting()) suspend();
        JPbox_video::update(); return;
    }
    if (resumePending) {
        resumePending = false;
        if (preferCache) openSource(download.cached(sourceUrl).string(), true);
        else {
            const auto edit = urlText; urlText = sourceUrl;
            playStreaming(); urlText = edit;
        }
    }
    const auto stream = resolver.snapshot();
    if (!resolveApplied && stream.state == jp_youtube::State::Ready) {
        resolveApplied = true; streamHasAudio = stream.hasAudio;
        openSource(stream.streamUrl.empty() ? stream.file.string() : stream.streamUrl, false);
    }
    const auto cache = download.snapshot();
    if (!cacheApplied && cache.state == jp_youtube::State::Ready) {
        cacheApplied = true; closeVideo(); resolver.cancel(); resolveApplied = true;
        preferCache = true; openSource(cache.file.string(), true);
    }
    JPbox_video::update();
    if (pendingSourcePosition >= 0) {
        media.position = pendingSourcePosition;
        if (mediaReady() && sourceDuration > 0) {
            movie.setPosition(pendingSourcePosition);
            pendingSourcePosition = -1;
            invalidateRender();
        }
    }
    const auto now = ofGetElapsedTimeMillis();
    if (movie.isFrameNew()) lastFrameAt = now;
    if (loadingSince && !mediaReady() && now - loadingSince > 30000) {
        closeVideo(); resolver.cancel();
        playbackError = "No se pudo abrir el video. Reintenta streaming o descarga en cache.";
    } else if (!usingCachedFile && mediaReady() && media.playing && now - lastFrameAt > 30000) {
        closeVideo(); resolver.cancel();
        playbackError = "Conexion interrumpida. Reconecta o descarga en cache.";
    }
}
bool JPbox_youtube::mediaReady() const {return JPbox_video::mediaReady() && movie.getTexture().isAllocated();}
std::string JPbox_youtube::mediaStatus() const {return mediaReady() ? "Ready" : connecting() ? "Connecting" : preparing() ? "Downloading" : "No video loaded";}
std::string JPbox_youtube::statusLabel() const {
    if (!playbackError.empty()) return playbackError;
    const auto cache = download.snapshot();
    const auto stream = resolver.snapshot();
    if (preparing()) return mediaReady() ? "Streaming activo. Descargando en cache..." : "Descargando en cache...";
    if (connecting()) return "Conectando streaming...";
    if (!cacheApplied && cache.state == jp_youtube::State::Error) return cache.message;
    if (!resolveApplied && stream.state == jp_youtube::State::Error) return stream.message;
    if (mediaReady()) {
        if (usingCachedFile) return "Reproduciendo desde cache (sin internet)";
        if (media.playing && ofGetElapsedTimeMillis() - lastFrameAt > 3000) return "Esperando datos de internet. Puedes descargar en cache.";
        return streamHasAudio ? "Reproduciendo por streaming" : "Streaming solo video. Descarga en cache para incluir audio.";
    }
    if (!openedFile.empty()) return usingCachedFile ? "Abriendo cache..." : "Cargando streaming...";
    if (preferCache && !sourceUrl.empty()) return "Sin copia local. Pulsa Descargar en cache o reproduce por streaming.";
    return "Pega un enlace y pulsa Reproducir streaming";
}
void JPbox_youtube::setCaptureActive(bool active) {
    retained = !active; urlFocused = false;
    if (!active) suspend();
    else resumePending = !sourceUrl.empty();
}
void JPbox_youtube::clear() {closeVideo(); resolver.cancel(); download.cancel(); urlFocused = false; JPbox_video::clear();}
void JPbox_youtube::saveCustomState(ofXml &node) const {
    JPbox_video::saveCustomState(node);
    auto settings = node.appendChild("youtube");
    settings.appendChild("url").set(sourceUrl.empty() ? jp_youtube::canonicalUrl(urlText) : sourceUrl);
    settings.appendChild("cache").set(preferCache);
}
void JPbox_youtube::loadCustomState(const ofXml &node) {
    JPbox_video::loadCustomState(node);
    const auto settings = node.getChild("youtube");
    sourceUrl = urlText = jp_youtube::canonicalUrl(settings.getChild("url").getValue());
    // Compositions from the initial cache-only version keep their local mode.
    preferCache = !settings.getChild("cache") || settings.getChild("cache").getBoolValue();
    if (preferCache) openSource(download.cached(sourceUrl).string(), true);
    else resumePending = !sourceUrl.empty();
}
void JPbox_youtube::copyCustomStateFrom(const JPbox *source) {
    JPbox_video::copyCustomStateFrom(source);
    if (auto youtube = dynamic_cast<const JPbox_youtube *>(source)) {
        sourceUrl = youtube->sourceUrl; urlText = youtube->urlText; preferCache = youtube->preferCache;
        if (preferCache) openSource(download.cached(sourceUrl).string(), true);
        else resumePending = !sourceUrl.empty();
    }
}
