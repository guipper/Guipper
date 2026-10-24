#pragma include "../common.frag"
// Audience controls: normalized 0..1. amount=0 preserves the original image.
uniform sampler2D textura1;
uniform float amount=0.35;
uniform float twist=0.6;
uniform float radius=0.55;
uniform float center_x=0.5;
uniform float center_y=0.5;
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 vec2 aspect=vec2(resolution.x/resolution.y,1.0);
 vec2 center=vec2(center_x,center_y),p=(uv-center)*aspect;
 float falloff=1.0-smoothstep(0.0,mix(0.12,1.5,radius),length(p));
 float angle=(twist*2.0-1.0)*6.2831853*amount*falloff*falloff;
 mat2 spin=mat2(cos(angle),-sin(angle),sin(angle),cos(angle));
 vec2 warped=spin*p/aspect+center;
 fragColor=mix(texture(textura1,uv),texture(textura1,clamp(warped,0.0,1.0)),amount);
}
