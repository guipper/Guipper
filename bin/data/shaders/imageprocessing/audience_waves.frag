#pragma include "../common.frag"
// Audience controls: normalized 0..1. amount=0 preserves the original image.
uniform sampler2D textura1;
uniform float amount=0.35;
uniform float waves=0.35;
uniform float speed=0.25;
uniform float center_x=0.5;
uniform float center_y=0.5;
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 vec2 aspect=vec2(resolution.x/resolution.y,1.0);
 vec2 d=(uv-vec2(center_x,center_y))*aspect;
 float r=length(d);
 vec2 direction=d/max(r,0.0001);
 float wave=sin(r*mix(8.0,65.0,waves)-time*speed*6.0);
 vec2 warped=uv+direction/aspect*wave*0.045*amount*smoothstep(0.0,0.08,r);
 fragColor=mix(texture(textura1,uv),texture(textura1,clamp(warped,0.0,1.0)),amount);
}
