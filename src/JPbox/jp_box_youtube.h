#pragma once
#include "jp_box_video.h"
#include "../JPutils/jp_youtube.h"

class JPbox_youtube : public JPbox_video {
    jp_youtube::Download resolver, download;
    std::string sourceUrl, openedFile, playbackError;
    bool retained = false, preferCache = false, usingCachedFile = false;
    bool streamHasAudio = true, resumePending = false;
    bool resolveApplied = true, cacheApplied = true;
    float pendingSourcePosition = -1;
    uint64_t loadingSince = 0, lastFrameAt = 0;
    void openSource(const std::string &source, bool local);
    void closeVideo();
public:
    std::string urlText;
    bool urlFocused = false;
    explicit JPbox_youtube(jp_youtube::Runner runner = jp_youtube::runProcess);
    ~JPbox_youtube() override;
    using JPbox_video::setup;
    void setup(string directory, string label) override;
    void update() override;
    void clear() override;
    void setCaptureActive(bool active) override;
    void playStreaming();
    void downloadToCache();
    void prepare() { downloadToCache(); } // Legacy test/API entry point.
    void cancelPreparation();
    void cancelConnection();
    void suspend();
    bool preparing() const;
    bool connecting() const;
    bool cacheAvailable() const;
    bool isUsingCache() const { return usingCachedFile && !openedFile.empty() && jp_youtube::canonicalUrl(urlText) == sourceUrl; }
    bool hasPreparedSource() const { return !openedFile.empty() || preparing() || connecting() || resumePending; }
    std::string statusLabel() const;
    bool mediaHasAudio() const override { return usingCachedFile || streamHasAudio; }
    bool mediaReady() const override;
    std::string mediaStatus() const override;
    void saveCustomState(ofXml &boxNode) const override;
    void loadCustomState(const ofXml &boxNode) override;
    void copyCustomStateFrom(const JPbox *source) override;
};
