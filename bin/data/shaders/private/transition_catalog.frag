#version 330
uniform sampler2D textura1, textura2;
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
void main() {
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
    fragColor=mix(a,b,w);
}
