#version 330
uniform sampler2DRect positions;
uniform vec2 targetSize,scaleRange;
uniform int surface,shape;
in vec4 position;
out vec3 tint;
flat out int roundPoint;
vec3 palette(float u){
 vec3 colors[5]=vec3[5](vec3(0),vec3(.62109375,.06640625,.10546875),vec3(.69140625,.0859375,.13671875),vec3(.16015625,.171875,.21484375),vec3(.796875));
 float t=clamp(u*5.-.5,0.,4.);int i=min(3,int(floor(t)));return mix(colors[i],colors[i+1],t-float(i));
}
void main(){
 vec4 p=texture(positions,position.xy);float d=1.8-p.z;
 float f=1./tan(radians(22.5));float aspect=targetSize.x/targetSize.y;
 gl_Position=vec4(p.x*f,-(p.y-.14)*f*aspect,d-.2,d);
 if(d<=.1||abs(p.x)>100.)gl_Position=vec4(2,2,2,1);
 gl_PointSize=1.;tint=palette(1.-p.w);roundPoint=0;
 if(surface!=0){
  float seed=fract(sin(dot(position.xy,vec2(127.1,311.7)))*43758.5453);
  float scale=mix(scaleRange.x,scaleRange.y,seed);
  gl_PointSize=shape==2?1.:clamp(scale*targetSize.x*f/max(.1,d),1.,64.);
  tint=vec3(1);roundPoint=shape==0?1:0;
 }
}
