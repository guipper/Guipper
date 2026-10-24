#pragma include "../common.frag"
// Audience controls: normalized 0..1. amount=0 preserves the original image.
uniform sampler2D textura1;
uniform float amount=0.35;
uniform float block_size=0.35;
uniform float color_steps=0.5;
uniform float grid=0.2;
void main(){
 vec2 uv=gl_FragCoord.xy/resolution;
 float size=mix(1.0,64.0,block_size*block_size);
 vec2 cell=floor(gl_FragCoord.xy/size);
 vec2 sampled=clamp((cell+0.5)*size/resolution,0.0,1.0);
 vec4 mosaic=texture(textura1,sampled);
 float levels=floor(mix(2.0,32.999,color_steps));
 mosaic.rgb=floor(mosaic.rgb*(levels-1.0)+0.5)/(levels-1.0);
 vec2 local=fract(gl_FragCoord.xy/size);
 float border=min(min(local.x,1.0-local.x),min(local.y,1.0-local.y))*size;
 mosaic.rgb*=mix(1.0,smoothstep(0.0,1.0,border),grid);
 fragColor=mix(texture(textura1,uv),mosaic,amount);
}
