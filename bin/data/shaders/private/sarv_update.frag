#version 330
uniform sampler2DRect previousState,emitter;
uniform vec2 stateSize;
uniform int initialize;
uniform float time,duration,variance;
out vec4 fragColor;
float hash(vec3 p){return fract(sin(dot(p,vec3(127.1,311.7,74.7)))*43758.5453);}
// Coherent vector noise with the reference periods, amplitudes and harmonic gain.
// Hash/interpolation differ from TouchDesigner's internal simplex implementation.
float noise(vec3 p){vec3 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);
 return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1)),f.x),f.y),f.z)*2.-1.;}
vec3 field(vec3 p){return vec3(noise(p),noise(p+vec3(19,47,7)),noise(p+vec3(83,11,29)));}
vec3 harmonics(vec3 p){return (field(p)+.7*field(p*2.)+.49*field(p*4.))/2.19;}
void main(){
 vec2 uv=gl_FragCoord.xy;
 vec3 source=texture(emitter,uv/stateSize*vec2(512,424)).xyz;
 vec4 p=texture(previousState,uv);
 if(initialize!=0){fragColor=vec4(source,hash(vec3(uv,7668)));return;}
 float age=p.w+max(0.,1.+variance*noise(vec3(uv*.01,time*.001)))/(60.*max(.05,duration));
 if(age>=1.){fragColor=vec4(source,0.);return;}
 vec3 n=harmonics(p.xyz/3.+vec3(0,0,time*.2))*.008;
 n+=harmonics(p.xyz/.7+vec3(1555,1555,1555+time*.6))*.003;
 fragColor=vec4(p.xyz*vec3(1.007,1.009,1.)+vec3(0,.006,-.01)+n,age);
}
