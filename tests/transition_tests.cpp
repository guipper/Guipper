#include "../src/JPutils/jp_transition.h"
#include <cassert>
#include <iostream>
using namespace jp_transition;
int main() {
    Config c; Timeline t;
    static_assert(Random==18 && PaletteEcho==19 && SpectralGlitch==21,"stored effect IDs must remain stable");
    for(int effect:{PaletteEcho,PaletteMosh,SpectralGlitch}) {
        c.effect=effect;t.request(c,0,1);assert(t.effect()==effect);
        c.effect=Random;c.randomEnabled.fill(false);c.randomEnabled[effect]=true;
        t.request(c,0,1);assert(t.effect()==effect);
    }
    c=Config();
    t.request(c, 0, 123); t.tick(9); assert(t.phase()==Phase::Preparing);
    t.ready(9,120,0); t.tick(9); assert(t.progress()==0);
    t.tick(9.75); assert(t.progress()==.5f);
    t.tick(9.5); assert(t.progress()==.5f);
    t.tick(10.5); assert(t.phase()==Phase::Complete);
    c.start=Start::Bar; c.beats=true; c.duration=4;
    t.request(c,0,12); t.ready(.1,120,0); t.tick(1.999); assert(!t.started());
    t.tick(2.1); assert(t.progress()==0); t.tick(3.1); assert(t.progress()==.5f);
    t.request(c,0,12); t.tick(10); assert(t.phase()==Phase::Failed);
    t.cancel(); assert(t.phase()==Phase::Idle);
    c=Config(); c.effect=Feedback; t.request(c,0,12); assert(t.effect()==Mix && !t.reason().empty());
    t.request(c,0,12,{false,false,true}); assert(t.effect()==Feedback);
    c.effect=StagedMorph; t.request(c,0,12,{true,false,false}); assert(t.effect()==Morph);
    c.effect=Random; c.randomEnabled.fill(false); c.randomEnabled[Warp]=true; c.randomEnabled[Bayer]=true;
    t.request(c,0,12); int first=t.effect(); auto seed=t.seed();
    t.ready(0,120,0); t.tick(0); t.tick(.4); assert(seed==t.seed() && first==t.effect());
    t.request(c,1,12); assert(first!=t.effect());
    c.randomEnabled.fill(false); c.randomEnabled[Morph]=true;
    c.randomEnabled[StagedMorph]=true; c.randomEnabled[Warp]=true;
    int previous=-1;
    for(unsigned seedIndex=0;seedIndex<30;++seedIndex) {
        t.request(c,0,seedIndex,{true,false,false});
        assert(t.effect()!=previous && t.effect()!=StagedMorph); previous=t.effect();
    }
    c=Config(); t.request(c,0,1); t.ready(0,120,0); t.tick(0);
    for(int i=0;i<11;++i) assert(!t.sample(20,60));
    assert(t.sample(20,60) && t.scale()==.75f);
    for(int i=0;i<12;++i) t.sample(20,60);
    assert(t.scale()==.5f);
    for(int i=0;i<12;++i) t.sample(20,60);
    assert(t.capture());
    c.effect=BeatCut; t.request(c,0,1); t.ready(.1,120,0); t.tick(.49); assert(t.progress()==0);
    t.tick(.5); assert(t.progress()==1);
    c=Config(); c.start=Start::Beat; c.beats=true; c.duration=2;
    t.request(c,0,1); t.ready(.1,120,0);
    t.tick(.2,60,0); t.tick(.5,60,0); assert(!t.started());
    t.tick(1,60,0); assert(t.progress()==0);
    t.tick(2,180,0); assert(t.progress()==.5f); // BPM frozen only after presentation.
    assert(ease(0)==0 && ease(1)==1);
    assert(category("// @transition-category motion zoom", "zoom")==Category::Motion);
    assert(category("uniform float color;", "color")==Category::None);
    assert(morphProgress(.6f,Category::Motion)==1);
    assert(morphProgress(.2f,Category::Color)==0);
    assert(morphProgress(.4f,Category::Shape)==0);
    std::cout << "transition tests passed\n";
}
