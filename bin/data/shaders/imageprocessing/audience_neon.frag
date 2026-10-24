#pragma include "../common.frag"
// Audience controls: normalized 0..1. amount=0 preserves the original image.
uniform sampler2D textura1;
uniform float amount=0.35;
uniform float sensitivity=0.5;
uniform float thickness=0.25;
uniform float hue=0.5;
uniform float background=0.6;
float audienceLuma(vec2 uv){return dot(texture(textura1,clamp(uv,0.0,1.0)).rgb,vec3(0.2126,0.7152,0.0722));}
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 vec2 stepUV=vec2(mix(1.0,6.0,thickness))/resolution;
 float dx=audienceLuma(uv+vec2(stepUV.x,0))-audienceLuma(uv-vec2(stepUV.x,0));
 float dy=audienceLuma(uv+vec2(0,stepUV.y))-audienceLuma(uv-vec2(0,stepUV.y));
 float edge=smoothstep(0.03,0.3,length(vec2(dx,dy))*mix(1.0,8.0,sensitivity));
 vec3 tint=0.5+0.5*cos(6.2831853*(hue+vec3(0.0,0.333333,0.666667)));
 vec4 original=texture(textura1,uv);
 vec3 neon=clamp(original.rgb*background+(0.25+0.75*tint)*edge,0.0,1.0);
 fragColor=vec4(mix(original.rgb,neon,amount),original.a);
}
