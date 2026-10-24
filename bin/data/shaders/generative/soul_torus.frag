#pragma include "../common.frag"
// Reflective 3D torus and central sphere. All numeric controls use 0..1.
uniform float zoom=0.5;
uniform float thickness=0.45;
uniform float center_size=0.45;
uniform float tilt_x=0.5;
uniform float tilt_y=0.52;
uniform float rotation=0.5;
uniform float speed=0.0;
uniform float deform=0.04;
uniform float light_angle=0.5;
uniform float brightness=0.5;
uniform float rim=0.45;
uniform float hue=0.6;
uniform float color_amount=0.0;
uniform bool transparent=false;

mat2 soulTurn(float a){float c=cos(a),s=sin(a);return mat2(c,-s,s,c);}
vec3 soulLocal(vec3 p){
 p.xy=soulTurn((rotation-.5)*6.2831853)*p.xy;
 p.xz=soulTurn((tilt_y-.5)*6.2831853+time*speed*1.2)*p.xz;
 p.yz=soulTurn((tilt_x-.5)*6.2831853)*p.yz;
 return p;
}
vec2 soulScene(vec3 p){
 vec3 q=soulLocal(p);
 float tube=mix(.08,.36,thickness);
 float a=atan(q.y,q.x);
 float waviness=sin(a*3.0+time*speed)*deform*.055;
 float torus=length(vec2(length(q.xy)-.88,q.z))-tube-waviness;
 float ball=length(p)-center_size*(.88-tube)*.82;
 return center_size>.001&&ball<torus?vec2(ball,1.0):vec2(torus,0.0);
}
vec3 soulNormal(vec3 p){
 const vec2 e=vec2(.001,0);
 return normalize(vec3(soulScene(p+e.xyy).x-soulScene(p-e.xyy).x,
 soulScene(p+e.yxy).x-soulScene(p-e.yxy).x,
 soulScene(p+e.yyx).x-soulScene(p-e.yyx).x));
}
vec3 soulEnvironment(vec3 ray){
 ray.xz=soulTurn((light_angle-.5)*6.2831853)*ray.xz;
 ray.x=-ray.x; // Align the large softbox to the reference in Guipper texture space.
 // Localized softboxes on the front hemisphere, not infinitely long
 // strips (which painted bright closed loops on the previous material).
 float front=smoothstep(-.1,.4,ray.z);
 vec2 key=ray.xy-vec2(.42,.55);
 key=soulTurn(-.45)*key;
 float upperRight=exp(-pow(key.x/.13,2.0)-pow(key.y/.34,2.0))*front;
 vec2 fill=ray.xy-vec2(-.62,.38);
 fill=soulTurn(.55)*fill;
 float upperLeft=exp(-pow(fill.x/.055,2.0)-pow(fill.y/.24,2.0))*front;
 float lower=exp(-pow((ray.x+.32)/.38,2.0)-pow((ray.y+.65)/.09,2.0))*front;
 return vec3(.00015+.18*upperRight+.055*upperLeft+.022*lower);
}
void main(){
 vec2 uv=(gl_FragCoord.xy*2.0-resolution)/resolution.y;
 uv/=mix(.6,1.8,zoom);
 vec3 ro=vec3(0,0,4.6),rd=normalize(vec3(uv,-1.8));
 float travel=0.0,closest=1.0;vec2 hit=vec2(0);bool found=false;
 // Conservative steps accommodate the subtle surface deformation.
 for(int i=0;i<112;i++){
  hit=soulScene(ro+rd*travel);
  closest=min(closest,abs(hit.x)/max(travel,.1));
  if(hit.x<.001){found=true;break;}
  travel+=max(hit.x*.75,.0004);
  if(travel>8.0)break;
 }
 vec3 col=vec3(0);float alpha=transparent?0.0:1.0;
 if(found){
  vec3 p=ro+rd*travel,n=soulNormal(p),view=-rd;
  float facing=clamp(dot(n,view),0.0,1.0);
  float fresnel=pow(1.0-facing,3.0);
  vec3 reflected=soulEnvironment(reflect(rd,n));
  float edge=pow(1.0-facing,3.0)*rim*.18;
  float diffuse=max(dot(n,normalize(vec3(.6,.8,1.1))),0.0);
  col=reflected*(.65+.35*fresnel)+vec3(edge+.0005*diffuse);
  vec3 tint=.5+.5*cos(6.2831853*(hue+vec3(0,.333333,.666667)));
  col*=mix(vec3(1),.15+1.5*tint,color_amount);
  col*=mix(.4,3.0,brightness);
  col=pow(clamp(col,0.0,1.0),vec3(1.0/2.2));
  alpha=1.0;
 }
 // Narrow screen-space halo outside both silhouettes. Its alpha is
 // premultiplication-safe: transparent mode carries the same visible glow.
 if(!found){
  float halo=exp(-closest*300.0)*rim*.055*mix(.4,3.0,brightness);
  col=vec3(pow(clamp(halo,0.0,1.0),1.0/2.2));
  if(transparent){alpha=clamp(max(col.r,max(col.g,col.b)),0.0,1.0);col=alpha>0.0001?col/alpha:vec3(0);}
 }
 fragColor=vec4(col,alpha);
}
