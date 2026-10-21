#pragma once
namespace jp_analogcolor {
inline constexpr const char *vertex=R"GLSL(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
void main(){gl_Position=modelViewProjectionMatrix*position;}
)GLSL";
inline constexpr const char *fragment=R"GLSL(#version 150
uniform sampler2D source;
uniform vec2 resolution,sourceSize;
uniform int paletteMode,colorCount;
uniform float dithering,grain,bleed,contrastAmount,saturationAmount,mixAmount,frameSeed,pixelSize;
out vec4 fragColor;
vec3 palette(int index){
    float t=float(index)/float(colorCount-1);
    if(paletteMode==1)return mix(vec3(.075,.045,.025),vec3(1.,.91,.72),t);
    if(paletteMode==2)return vec3(t);
    vec3 inks[16]=vec3[16](vec3(.025,.025,.04),vec3(.12,.10,.20),vec3(.22,.12,.19),vec3(.13,.25,.28),
        vec3(.38,.20,.27),vec3(.24,.38,.34),vec3(.48,.32,.25),vec3(.30,.40,.54),
        vec3(.66,.35,.28),vec3(.51,.48,.37),vec3(.43,.60,.55),vec3(.76,.53,.42),
        vec3(.65,.67,.55),vec3(.88,.72,.49),vec3(.85,.82,.71),vec3(.98,.95,.85));
    return inks[int(floor(t*15.+.5))];
}
float hash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7))+frameSeed)*43758.5453);}
void main(){
    vec2 uv=gl_FragCoord.xy/resolution;
    vec4 original=texture(source,uv);
    if(mixAmount<=0.){fragColor=original;return;}
    float size=max(1.,floor(pixelSize+.5));
    vec2 cell=floor(gl_FragCoord.xy/size);
    vec2 start=cell*size;
    uv=(start+min(start+vec2(size),resolution))*.5/resolution;
    vec4 sampled=texture(source,uv);
    vec2 dx=vec2(bleed/max(sourceSize.x,1.),0);
    vec4 left=texture(source,clamp(uv-dx,vec2(0),vec2(1)));
    vec4 right=texture(source,clamp(uv+dx,vec2(0),vec2(1)));
    // Alpha-weighted sampling prevents invisible RGB bleeding into the edge.
    vec3 c=(2.*sampled.rgb*sampled.a+left.rgb*left.a+right.rgb*right.a)/max(2.*sampled.a+left.a+right.a,.00001);
    c=mix(vec3(dot(c,vec3(.2126,.7152,.0722))),c,saturationAmount);
    c=(c-.5)*contrastAmount+.5;
    int bayer[16]=int[16](0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5);
    ivec2 p=ivec2(mod(cell,4.));
    float threshold=(float(bayer[p.y*4+p.x])+.5)/16.-.5;
    c+=vec3((threshold*dithering+(hash(cell+vec2(.5))-.5)*grain)/sqrt(float(colorCount)));
    c=clamp(c,0.,1.);
    vec3 reduced=palette(0);float best=1e10;
    for(int i=0;i<16;i++){if(i>=colorCount)break;vec3 candidate=palette(i);vec3 d=c-candidate;
        float error=dot(d*d,vec3(.299,.587,.114));if(error<best){best=error;reduced=candidate;}}
    fragColor=mix(original,vec4(reduced,sampled.a),mixAmount);
}
)GLSL";
}
