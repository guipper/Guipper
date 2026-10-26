#pragma include "../common.frag"
// Bind amount to Body -> Mano izq. sobre ojo. Zero preserves RGBA exactly.
uniform sampler2D textura1;
uniform float amount=0.0;
uniform float brightness=0.5;
void main(){
 vec4 original=texture(textura1,gl_FragCoord.xy/resolution);
 float luma=dot(original.rgb,vec3(.2126,.7152,.0722));
 vec3 celeste=vec3(.28,.78,1.0)*luma*mix(.5,1.5,brightness);
 fragColor=vec4(mix(original.rgb,celeste,amount),original.a);
}
