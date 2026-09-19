#pragma once
// Deterministic transition policy. No graphics, clock, filesystem or app globals.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <sstream>
#include <vector>

namespace jp_transition {
enum Effect { Mix = 0, Warp = 1, Bayer = 2, Cut, BeatCut, Plasma, RadialIn,
    RadialOut, Push, Wipe, Blocks, SplitPush, CenterPush, CenterSqueeze, Dots,
    Feedback, Morph, StagedMorph, Random, PaletteEcho, PaletteMosh, SpectralGlitch, EffectCount };
enum class Start { Immediate, Beat, Bar };
enum class Quality { Automatic, Full, Reduced, Capture };
enum class Phase { Idle, Preparing, Waiting, Running, Complete, Failed };
struct Descriptor { const char *en, *es, *family; };
inline constexpr std::array<Descriptor, EffectCount> catalog {{
    {"Crossfade", "Fundido", "Basic"}, {"Shared warp", "Deformación", "Organic"},
    {"Bayer dither", "Trama Bayer", "Pattern"}, {"Cut", "Corte", "Basic"},
    {"Beat cut", "Corte al pulso", "Basic"}, {"Plasma", "Plasma", "Organic"},
    {"Radial inward", "Radial hacia adentro", "Organic"}, {"Radial outward", "Radial hacia afuera", "Organic"},
    {"Push", "Empuje", "AVS"}, {"Wipe", "Barrido", "AVS"},
    {"Nine blocks", "Nueve bloques", "AVS"}, {"Split push", "Empuje dividido", "AVS"},
    {"Push to center", "Empuje al centro", "AVS"}, {"Squeeze to center", "Compresión al centro", "AVS"},
    {"Point dissolve", "Disolución por puntos", "AVS"}, {"Feedback continuity", "Feedback", "Organic"},
    {"Parameter morph", "Morph de parámetros", "Morph"}, {"Staged morph", "Morph por etapas", "Morph"},
    {"Random", "Aleatorio", "Random"},
    {"Chromatic echo", "Eco cromático", "Organic"},
    {"Palette datamosh", "Datamosh de paleta", "Organic"},
    {"Spectral glitch", "Glitch espectral", "Pattern"}
}};
struct Config {
    int effect = Mix;
    double duration = 1.5;
    bool beats = false;
    Start start = Start::Immediate;
    Quality quality = Quality::Automatic;
    int direction = 0;
    float centerX = .5f, centerY = .5f, softness = .12f, plasmaScale = 3.f, intensity = .25f;
    std::array<bool, EffectCount> favorites {};
    std::array<bool, EffectCount> randomEnabled {};
    Config() { randomEnabled.fill(true); randomEnabled[Random] = false; }
};
struct Capabilities { bool morph = false, staged = false, feedback = false; };
inline bool compatible(int effect, Capabilities caps) {
    return effect >= 0 && effect < EffectCount && effect != Random &&
        (effect != Morph || caps.morph) && (effect != StagedMorph || caps.morph) &&
        (effect != Feedback || caps.feedback);
}
inline float ease(float t) { t = std::clamp(t, 0.f, 1.f); return t*t*(3.f-2.f*t); }
enum class Category { None, Motion, Color, Shape };
inline Category category(const std::string &source, const std::string &parameter) {
    std::istringstream lines(source); std::string line;
    while(std::getline(lines,line)) {
        const auto at=line.find("// @transition-category ");
        if(at==std::string::npos) continue;
        std::istringstream words(line.substr(at+24)); std::string kind,name; words>>kind>>name;
        if(name!=parameter) continue;
        if(kind=="motion") return Category::Motion;
        if(kind=="color") return Category::Color;
        if(kind=="shape") return Category::Shape;
    }
    return Category::None;
}
inline float morphProgress(float p, Category category) {
    if(category==Category::Motion) return ease(p/.6f);
    if(category==Category::Color) return ease((p-.2f)/.6f);
    if(category==Category::Shape) return ease((p-.4f)/.6f);
    return ease(p);
}
class Timeline {
public:
    void request(Config config, double now, uint32_t seed, Capabilities caps = {}) {
        config.effect = std::clamp(config.effect, 0, int(EffectCount)-1);
        config.duration = std::isfinite(config.duration) ? std::max(0., config.duration) : 1.5;
        config_ = config; seed_ = seed; reason_.clear(); elapsed_ = 0.; progress_ = 0.;
        started_ = false; preparedAt_ = now; phase_ = Phase::Preparing; overBudget_ = 0;
        scale_ = config.quality == Quality::Reduced ? .75f : 1.f;
        capture_ = config.quality == Quality::Capture;
        std::mt19937 rng(seed);
        effect_ = config.effect;
        direction_ = std::clamp(config.direction, 0, 3);
        if (effect_ == Random) {
            std::vector<int> choices;
            for (int i = 0; i < EffectCount; ++i)
                if (config.randomEnabled[i] && compatible(i, caps) &&
                    (i != StagedMorph || caps.staged)) choices.push_back(i);
            if (choices.size() > 1) choices.erase(std::remove(choices.begin(), choices.end(), previous_), choices.end());
            effect_ = choices.empty() ? Mix : choices[rng() % choices.size()];
            direction_ = int(rng() % 4);
        }
        if (!compatible(effect_, caps)) {
            reason_ = effect_ == Feedback ? "Destination has no feedback capability" : "No compatible parameter schema";
            effect_ = Mix;
        }
        if (effect_ == StagedMorph && !caps.staged) { effect_ = Morph; reason_ = "No declared parameter categories; simultaneous morph"; }
        previous_ = effect_;
    }
    // Readiness is provided by the resource adapter, never inferred from pixels.
    void ready(double now, double bpm, double beatOrigin) {
        if (phase_ != Phase::Preparing) return;
        bpm_ = std::max(1., bpm); beatOrigin_ = beatOrigin;
        duration_ = config_.duration * (config_.beats ? 60. / bpm_ : 1.);
        if (effect_ == Cut || effect_ == BeatCut) duration_ = 0.;
        const Start sync = effect_ == BeatCut ? Start::Beat : config_.start;
        scheduled_ = now;
        if (sync != Start::Immediate) {
            const double period = 60. / std::max(1., bpm) * (sync == Start::Bar ? 4. : 1.);
            scheduled_ = beatOrigin + (std::floor((now - beatOrigin) / period) + 1.) * period;
        }
        phase_ = Phase::Waiting;
    }
    void tick(double now, double currentBpm = 0., double beatOrigin = 0.) {
        // While waiting, follow changes to the master clock. Once presented,
        // duration is frozen: later BPM edits belong to the next transition.
        if (phase_ == Phase::Waiting && currentBpm > 0. &&
            (currentBpm != bpm_ || beatOrigin != beatOrigin_)) {
            bpm_ = std::max(1., currentBpm); beatOrigin_ = beatOrigin;
            const Start sync = effect_ == BeatCut ? Start::Beat : config_.start;
            if (sync != Start::Immediate) {
                const double period = 60. / bpm_ * (sync == Start::Bar ? 4. : 1.);
                scheduled_ = beatOrigin_ + (std::floor((now - beatOrigin_) / period) + 1.) * period;
            }
        }
        if (phase_ == Phase::Preparing && now - preparedAt_ >= 10.) fail("Preparation timed out");
        if (phase_ == Phase::Waiting && now >= scheduled_) {
            duration_ = config_.duration * (config_.beats ? 60. / bpm_ : 1.);
            if (effect_ == Cut || effect_ == BeatCut) duration_ = 0.;
            phase_ = Phase::Running; started_ = true; presentedAt_ = now;
        }
        if (phase_ != Phase::Running) return;
        elapsed_ = std::max(elapsed_, std::max(0., now - presentedAt_));
        progress_ = duration_ <= 0. ? 1.f : float(std::min(1., elapsed_ / duration_));
        if (progress_ >= 1.f) phase_ = Phase::Complete;
    }
    // Adapter measures the complete render cost; returns true on a policy change.
    bool sample(double frameMs, double fps) {
        if (phase_ != Phase::Running || config_.quality != Quality::Automatic || capture_) return false;
        const double budget = 1000. / std::clamp(fps, 1., 60.);
        overBudget_ = frameMs > budget ? overBudget_ + 1 : 0;
        if (overBudget_ < 12) return false;
        overBudget_ = 0;
        if (scale_ > .75f) scale_ = .75f;
        else if (scale_ > .5f) scale_ = .5f;
        else capture_ = true;
        reason_ = capture_ ? "Render budget exceeded at 50%; outgoing captured" : "Render budget exceeded for 12 frames";
        return true;
    }
    void fail(std::string reason) { phase_ = Phase::Failed; reason_ = std::move(reason); }
    void cancel() { phase_ = Phase::Idle; progress_ = 1.f; started_ = false; }
    Phase phase() const { return phase_; }
    float progress() const { return progress_; }
    int effect() const { return effect_; }
    int direction() const { return direction_; }
    uint32_t seed() const { return seed_; }
    float scale() const { return scale_; }
    bool capture() const { return capture_; }
    bool started() const { return started_; }
    const std::string &reason() const { return reason_; }
    const Config &config() const { return config_; }
private:
    Config config_;
    Phase phase_ = Phase::Idle;
    double preparedAt_ = 0., scheduled_ = 0., presentedAt_ = 0., elapsed_ = 0., duration_ = 1.5;
    double bpm_ = 120., beatOrigin_ = 0.;
    float progress_ = 1.f, scale_ = 1.f;
    int effect_ = Mix, previous_ = -1, direction_ = 0, overBudget_ = 0;
    uint32_t seed_ = 0;
    bool capture_ = false, started_ = false;
    std::string reason_;
};
} // namespace jp_transition
