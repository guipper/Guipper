#version 330
uniform sampler2D textura1, textura2;
uniform sampler2D paletteTexture, historyTexture;
uniform float historyValid, historyStep;
uniform vec2 resolution, transitionCenter;
uniform float mixst, transitionSeed, edgeSoftness, plasmaScale, warpIntensity;
uniform int transitionEffect, transitionDirection;
out vec4 fragColor;
float hash(vec2 p) { return fract(sin(dot(p,vec2(127.1,311.7))+transitionSeed)*43758.5453); }
float noise(vec2 p) {
    vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
    return mix(mix(hash(i),hash(i+vec2(1,0)),f.x),mix(hash(i+vec2(0,1)),hash(i+vec2(1)),f.x),f.y);
}
float mask(float threshold, float p) {
    float s=max(.0001,edgeSoftness);
    return smoothstep(threshold-s,threshold+s,mix(-s,1.0+s,p));
}
float axis(vec2 uv) {
    if(transitionDirection==1) return 1.0-uv.x;
    if(transitionDirection==2) return uv.y;
    if(transitionDirection==3) return 1.0-uv.y;
    return uv.x;
}
vec2 shiftAxis(vec2 uv,float d) {
    if(transitionDirection==1) uv.x-=d;
    else if(transitionDirection==2) uv.y+=d;
    else if(transitionDirection==3) uv.y-=d;
    else uv.x+=d;
    return uv;
}
float luma(vec3 c) { return dot(c,vec3(.2126,.7152,.0722)); }
vec2 safeUV(vec2 uv) { return clamp(uv,vec2(0.0),vec2(1.0)); }
// Six GPU texels: shadows/midtones/highlights for A and B. Transparent
// pixels have no vote, and an empty tone bin falls back to the scene average.
vec4 analyzePalette() {
    float tone=floor(gl_FragCoord.x), scene=floor(gl_FragCoord.y);
    vec3 sum=vec3(0.0), average=vec3(0.0);
    float weights=0.0, coverage=0.0;
    for(int y=0;y<12;++y) for(int x=0;x<12;++x) {
        vec2 q=(vec2(x,y)+.5)/12.0;
        vec4 c=scene<.5?texture(textura1,q):texture(textura2,q);
        float weight=max(0.0,1.0-abs(luma(c.rgb)-(.15+tone*.35))/.35)*c.a;
        sum+=c.rgb*weight; weights+=weight;
        average+=c.rgb*c.a; coverage+=c.a;
    }
    return vec4(weights>.0001?sum/weights:average/max(coverage,.0001),coverage/144.0);
}
vec3 palette(float light,float scene) {
    float t=clamp((light-.15)/.35,0.0,2.0);
    vec3 low=texelFetch(paletteTexture,ivec2(int(floor(t)),int(scene)),0).rgb;
    vec3 high=texelFetch(paletteTexture,ivec2(min(2,int(floor(t))+1),int(scene)),0).rgb;
    return mix(low,high,fract(t));
}
void main() {
    if(transitionEffect==-1) { fragColor=analyzePalette(); return; }
    vec2 uv=gl_FragCoord.xy/resolution;
    float p=clamp(mixst,0.0,1.0);
    vec4 a=texture(textura1,uv), b=texture(textura2,uv);
    // Exact endpoints bypass every displaced lookup and threshold.
    if(p<=0.0) { fragColor=a; return; }
    if(p>=1.0) { fragColor=b; return; }
    float w=p;
    int e=transitionEffect;
    if(e==1) {
        vec2 q=uv-transitionCenter;
        float envelope=sin(p*3.14159265)*warpIntensity;
        float angle=envelope*length(q)*2.0;
        mat2 r=mat2(cos(angle),-sin(angle),sin(angle),cos(angle));
        // Same continuous flow on both sources, zero displacement at endpoints.
        vec2 flow=transitionCenter+r*q*(1.0-envelope*.3);
        a=texture(textura1,flow); b=texture(textura2,flow);
    } else if(e==2) {
        int x=int(mod(gl_FragCoord.x,4.0)),y=int(mod(gl_FragCoord.y,4.0));
        float m[16]=float[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
        w=step((m[x+4*y]+.5)/16.0,p);
    } else if(e==3 || e==4) { w=1.0;
    } else if(e==5) {
        vec2 q=uv*max(.25,plasmaScale);
        float n=(noise(q)+.5*noise(q*2.0)+.25*noise(q*4.0))/1.75;
        w=mask(n,p);
    } else if(e==6 || e==7) {
        vec2 aspect=vec2(resolution.x/resolution.y,1.0);
        float radius=length((uv-transitionCenter)*aspect);
        vec2 farthest=max(transitionCenter,1.0-transitionCenter)*aspect;
        float t=radius/max(length(farthest),.0001);
        w=mask(e==6?1.0-t:t,p);
    } else if(e==8) {
        float x=axis(uv);
        if(x<1.0-p) { a=texture(textura1,shiftAxis(uv,p)); w=0.0; }
        else { b=texture(textura2,shiftAxis(uv,p-1.0)); w=1.0; }
    } else if(e==9) { w=mask(axis(uv),p);
    } else if(e==10) { w=mask(hash(floor(uv*3.0)),p);
    } else if(e==11) {
        float side=uv.y<.5?1.0:-1.0;
        vec2 q=vec2(uv.x+side*p,uv.y);
        if(q.x>=0.0 && q.x<=1.0) { a=texture(textura1,q);w=0.0; }
        else { b=texture(textura2,vec2(q.x-side,uv.y));w=1.0; }
    } else if(e==12 || e==13) {
        vec2 q=uv-.5;
        float remain=1.0-p;
        if(abs(q.x)<.5*remain && abs(q.y)<.5*remain) {
            vec2 source=e==13?q/max(remain,.0001)+.5:uv+sign(q)*p*.5;
            a=texture(textura1,source);w=0.0;
        } else w=1.0;
    } else if(e==14) { w=step(hash(floor(gl_FragCoord.xy)),p); }
    else if(e>=19 && e<=21) {
        float envelope=sin(p*3.14159265);
        float strength=clamp(warpIntensity*2.0,0.0,1.0);
        vec3 pa=palette(luma(a.rgb),0.0), pb=palette(luma(b.rgb),1.0);
        // Similar colors arrive together, with luminance guiding the reveal.
        float affinity=1.0-clamp(length(a.rgb-pb)/1.732,0.0,1.0);
        float arrival=clamp(.55*luma(a.rgb)+.3*(1.0-affinity)+.15*noise(uv*6.0),0.0,1.0);
        w=mask(arrival,p);
        vec2 flow=vec2(0.0);
        float memory=.82;
        if(e==19) {
            vec2 q=uv-transitionCenter;
            vec2 gradient=vec2(
                luma(texture(textura1,safeUV(uv+vec2(.003,0))).rgb)-luma(a.rgb),
                luma(texture(textura1,safeUV(uv+vec2(0,.003))).rgb)-luma(a.rgb));
            flow=(vec2(-q.y,q.x)*.035+gradient*.12+(pb.rg-pa.rg)*.012)*strength*envelope;
            a=texture(textura1,safeUV(uv-flow));
            b=texture(textura2,safeUV(uv+flow*.6));
        } else if(e==20) {
            vec2 grid=vec2(48.0,27.0), cell=floor(uv*grid), center=(cell+.5)/grid;
            vec3 ca=texture(textura1,center).rgb, cb=texture(textura2,center).rgb;
            float gate=step(.35,hash(cell+floor(p*18.0)));
            // Content-driven block displacement, deliberately a codec-like
            // simulation rather than claiming decoded motion vectors.
            flow=(cb.rg-ca.rg+vec2(hash(cell),hash(cell+7.0))-.5)*.12*strength*envelope*gate;
            a=texture(textura1,safeUV(uv-flow));
            b=texture(textura2,safeUV(uv+flow*.25));
            a.rgb=mix(a.rgb,palette(luma(a.rgb),1.0),.65*envelope*strength);
            w=mask(clamp(arrival*.65+hash(cell)*.35,0.0,1.0),p);
            memory=.94;
        } else {
            float band=floor(uv.y*72.0), tick=floor(p*32.0);
            float tear=step(.70,hash(vec2(band,tick)));
            flow=vec2((hash(vec2(band,tick+19.0))-.5)*.18*tear,0.0)*strength*envelope;
            vec2 split=vec2(.018*strength*envelope*(.3+tear),0.0);
            vec2 qa=safeUV(uv-flow), qb=safeUV(uv+flow*.4);
            a=texture(textura1,qa);b=texture(textura2,qb);
            a.r=texture(textura1,safeUV(qa+split)).r;a.b=texture(textura1,safeUV(qa-split)).b;
            b.r=texture(textura2,safeUV(qb-split)).r;b.b=texture(textura2,safeUV(qb+split)).b;
            w=mask(clamp(arrival+.15*(hash(vec2(band,tick))-.5),0.0,1.0),p);
            memory=.68;
        }
        vec4 current=mix(a,b,w);
        vec3 bridge=mix(palette(luma(current.rgb),0.0),palette(luma(current.rgb),1.0),p);
        current.rgb=mix(current.rgb,bridge,.3*strength*envelope);
        vec4 echo=texture(historyTexture,safeUV(uv-flow));
        echo.rgb=mix(echo.rgb,bridge,.10*strength);
        float persistence=historyValid*envelope*strength*pow(memory,max(.25,historyStep));
        // Straight RGBA keeps transparent compositions transparent. Exact
        // endpoint returns above guarantee no residual trails after arrival.
        fragColor=mix(current,echo,persistence);
        return;
    }
    fragColor=mix(a,b,w);
}
