#pragma include "../common.frag"
// Audience controls: normalized 0..1. amount=0 preserves the original image.
uniform sampler2D textura1;
uniform float amount=0.35;
uniform float distance=0.35;
uniform float direction=0.5;
uniform float radial=0.25;
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 float angle=direction*6.2831853;
 vec2 aspect=vec2(resolution.y/resolution.x,1.0);
 vec2 axis=mix(vec2(cos(angle),sin(angle))*aspect,(uv-0.5)*2.0,radial);
 vec2 shift=axis*distance*amount*0.10;
 vec4 original=texture(textura1,uv);
 vec3 split=vec3(texture(textura1,clamp(uv+shift,0.0,1.0)).r,original.g,texture(textura1,clamp(uv-shift,0.0,1.0)).b);
 fragColor=vec4(mix(original.rgb,split,amount),original.a);
}
