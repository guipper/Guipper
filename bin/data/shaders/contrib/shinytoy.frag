#pragma include "../common.frag"

uniform sampler2D iChannel0;
// Shiny Toy Car by eiffie
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// V2 Sedan Version: simplified car, added quadcop, bushs and the speed, lighting fix. 

// Just putting back some old shaders.
// I removed this while being poisoned with heavy metals by the illuminati!
// True story. Who knew the world is actually run by psychopathic pedophiles?
// If it happens to you: cilantro and get out of 5 eyes controlled countries.
const int Rays=3, RaySteps=48, maxBounces=6;//up the Rays value for less fuzz
const float fov = 4.5,blurAmount = 0.005,maxDepth=11.0,HitDistance=0.001;
const vec3 sunColor=vec3(1.0,0.9,0.8),sunDir=vec3(0.35,0.5,-0.35),skyColor=vec3(0.13,0.14,0.15);
const vec2 ve=vec2(0.0001,0.0);
int obj=0;
float tim;
mat2 rmx;
#define tex iChannel0
#define time iTime*0.5
#define size iResolution

struct material {vec3 color;float difExp,spec,specExp;};

float smin(float a,float b,float k){return -log(exp(-k*a)+exp(-k*b))/k;}//from iq
#define TAO 6.283
void Rotate(inout vec2 v, float angle) {v*=mat2(cos(angle),sin(angle),-sin(angle),cos(angle));}
void Kaleido(inout vec2 v,float power){Rotate(v,floor(0.5+atan(v.x,-v.y)*power/TAO)*TAO/power);}

float DE(in vec3 p0){//carcar
 p0.z+=tim;
 vec3 p=p0+vec3(0.0,1.24,0.0);
 float r=length(p.yz);
 float d= length(max(vec3(abs(p.x)-0.35,r-1.92,-p.y+1.4),0.0))-0.05;
 d=max(d,p.z-1.0);
 p=p0+vec3(0.0,-0.22,0.39);
 p.xz=abs(p.xz)-vec2(0.5300,0.9600);p.x=abs(p.x);
 r=length(p.yz);
 d=smin(d,length(max(vec3(p.x-0.08,r-0.25,-p.y-0.08),0.0))-0.04,8.0);
 d=max(d,-max(p.x-0.165,r-0.24));
 float d2=length(vec2(max(p.x-0.13,0.0),r-0.2))-0.02;
 float d3=length(vec2(max(p.x-0.11,0.0),r-0.18))-0.02;
 if(p0.z<0.0)p.yz=p.yz*rmx;
 else p.yz=rmx*p.yz;
 Kaleido(p.yz,9.0);
 d3=min(d3,length(max(abs(p.xyz)-vec3(0.07,0.0,0.17),0.0))-0.01);
 d=min(min(p0.y,d),min(d2,d3));
 if(obj<0){
  if(d==d2)obj=1;
  else if(d==d3)obj=2;
  else if(d==p0.y)obj=3;
  else obj=0;
 }
 return d;
}

material getMaterial( in vec3 p0, inout vec3 nor )
{//return material properties
 vec3 dif=vec3(0.0);
 if(obj==0){//body
  if(abs(p0.y-0.6)>0.1 || abs(p0.x)>0.43 || p0.z+tim<-0.9900)dif=vec3(0.9,0.9,0.4);
  return material(dif,pow(2.0,10.0),1.0,pow(2.0,14.0));
 }else if(obj==1){//tire
  return material(dif,4.0,0.75,32.0);
 }else if(obj==2){//rim
  return material(vec3(0.8),pow(2.0,16.0),1.0,2048.0);
 }else {//ground
  p0.x+=(sin(p0.z*0.1)+sin(p0.z*0.13))*0.5;
  if(abs(abs(p0.x-1.0)-2.5)<0.05 || (abs(p0.x-1.0)<0.05 && fract(p0.z*0.25)<0.25))dif=vec3(1.0);
  else if(abs(p0.x-1.0)<3.25-texture(tex,p0.xz*0.5).r*0.2)dif=vec3(0.25);
  else dif=vec3(0.6,0.5,0.3);
  vec3 col=min(10.0,abs(p0.x))*0.01*texture(tex,p0.xz*0.05).rgb;
  nor=normalize(nor+col);
  dif+=col;
  return material(dif,3.0,0.5,1024.0);
 }
}

float DEQCop(vec2 z){
 vec2 p=abs(z)-vec2(2.0);
 p*=rmx;
 float d=max(abs(p.x)-1.5,abs(p.y)-0.1);
 p=abs(z*mat2(0.707,-0.707,0.707,0.707));
 d=min(d,length(p)-1.0);
 d=min(d,max(p.x-3.0,p.y-0.1));
 d=min(d,max(p.x-0.1,p.y-3.0));
 return step(0.0,d)*0.75+0.25;
}

vec3 getBackground( in vec3 ro, vec3 rd, vec3 qcop  ){
 vec2 pt=vec2(rd.x+rd.z*0.6,rd.y*2.0)/iResolution.xy*64.0;
 if(rd.y<texture(tex,pt).r*0.02)return vec3(0.05,0.1,0.025)+max(0.0,rd.y)*vec3(8.0,4.0,0.0);
 vec3 clouds=texture(tex,pt*0.1).rgb*0.05+texture(tex,pt*0.3).rgb*0.025;
 float t=1.0;
 if(ro!=qcop){
  t=(qcop.y-ro.y)/rd.y;
  if(t>0.0){
   pt=ro.xz+rd.xz*t-qcop.xz;
   t=DEQCop(pt);
  }else t=1.0;
 }

 return t*(clouds+skyColor+rd*0.05+sunColor*(pow(max(0.0,dot(rd,sunDir)),2.0)*0.5+pow(max(0.0,dot(rd,sunDir)),80.0)));
}

float BBox(vec3 p, vec3 rd, vec3 bs)
{
 vec3 t0=(-bs-p)/rd,t1=(bs-p)/rd;
 vec3 n=min(t0,t1),f=max(t0,t1);
 float tmin=max(n.x,max(n.y,n.z)),tmax=min(f.x,min(f.y,f.z));
 if(tmin<=tmax) return tmin;
 return maxDepth;
}

//random seed and generator
vec2 randv2;
vec2 rand2(){// implementation derived from one found at: lumina.sourceforge.net/Tutorials/Noise.html
 randv2+=vec2(1.0,1.0);
 return vec2(fract(sin(dot(randv2.xy ,vec2(12.9898,78.233))) * 4375.5453),
  fract(cos(dot(randv2.xy ,vec2(4.898,7.23))) * 2342.631));
}
 
vec3 powDir(vec3 nor, vec3  dir, float power) //modified from syntopia's code
{//creates a biased random sample without penetrating the surface (approx Schlick's)
 float ddn=max(0.01,abs(dot(dir,nor)));
 vec2 r=rand2()*vec2(TAO,1.0);
 vec3 nr=(ddn<0.99)?nor:((abs(nor.x)<0.5)?vec3(1.0,0.0,0.0):vec3(0.0,1.0,0.0));
 vec3 sdir=normalize(cross(dir,nr));
 r.y=pow(r.y,1.0/power);
 vec3 ro= normalize(sqrt(1.0-r.y*r.y)*(cos(r.x)*sdir + sin(r.x)*cross(dir,sdir)*ddn) + r.y*dir);
 return (dot(ro,nor)<0.0)?reflect(ro,nor):ro;
}

vec3 scene(vec3 ro, vec3 rd) 
{// find color of scene
 vec3 fcol=vec3(1.333),qcop=ro;
 float d,t=min(ro.y/-rd.y,BBox(ro-vec3(0.0,0.22,-tim-0.33),rd,vec3(0.8,0.51,1.75)));//bounding
 int iHitCount=0;
 for(int i=0; i<RaySteps; i++ ){// march loop
  if(t>=maxDepth)continue;
  t+=d=DE(ro+t*rd);//march
  if(abs(d)<HitDistance*t){//hit
   obj=-1;//turn on material mapping
   t+=d=DE(ro+t*rd);//move closer while coloring
   ro+=rd*t;// advance ray position to hit point
   vec3 nor = normalize(vec3(-DE(ro-ve.xyy)+DE(ro+ve.xyy),
    -DE(ro-ve.yxy)+DE(ro+ve.yxy),
    -DE(ro-ve.yyx)+DE(ro+ve.yyx)));// get the surface normal
   material m=getMaterial( ro, nor );//and material
   vec3 refl=reflect(rd,nor);//setting up for a new ray direction and defaulting to a reflection
   rd=powDir(nor,refl,m.difExp);//redirect the ray
   m.color+=mix(vec3(-0.2,0.0,0.2),vec3(0.2,0.0,-0.2),0.25+0.75*dot(rd,nor));
   //the next line calcs the amount of energy left in the ray based on how it bounced (diffuse vs specular) 
   fcol*=mix(m.color,vec3(1.0),min(pow(max(0.0,dot(rd,refl)),m.specExp)*m.spec,1.0));
   t=max(d*5.0,HitDistance);//hopefully pushs thru the surface
   if(iHitCount++>maxBounces || dot(fcol,fcol)<0.01)t=maxDepth;
  }
 }
 if(rd.y<0.0){//one more ground hit for good luck
  obj=3;
  t=ro.y/-rd.y;//calc the intersection
  ro+=rd*t;// advance ray position to hit point
  vec3 nor = vec3(0.0,1.0,0.0);
  material m=getMaterial( ro, nor );
  vec3 refl=reflect(rd,nor);//setting up for a new ray direction and defaulting to a reflection
  rd=powDir(nor,refl,m.difExp);//redirect the ray
  m.color+=mix(vec3(-0.2,0.0,0.2),vec3(0.2,0.0,-0.2),0.25+0.75*dot(rd,nor));
  fcol*=mix(m.color,vec3(1.0),min(pow(max(0.0,dot(rd,refl)),m.specExp)*m.spec,1.0)); 
 }
 return fcol*getBackground(ro,rd,qcop);//light the scene
} 

mat3 lookat(vec3 fw,vec3 up){
 fw=normalize(fw);vec3 rt=normalize(cross(fw,normalize(up)));return mat3(rt,cross(rt,fw),fw);
}

void main() {
	
	vec2 invertfc = vec2(gl_FragCoord.xy.x,1.-gl_FragCoord.xy.y);
	
 randv2=fract(cos((gl_FragCoord.xy.xy+gl_FragCoord.xy.yx*vec2(100.0,100.0))+vec2(time)*10.0)*1000.0);
 vec3 clr=vec3(0.0);
 for(int iRay=0;iRay<Rays;iRay++){
  float tim3=time+0.02*float(iRay)/float(Rays);
  tim=tim3*tim3;//14.4;
  float tim2=tim3*0.15;
  float ct=cos(tim*1.25),st=sin(tim*1.25);
  vec3 ro=vec3(cos(tim2)*vec2(sin(tim2*6.4),cos(tim2*6.4)),sin(tim2))*(6.0+3.0*sin(tim2*3.0));
  ro.y=ro.y*0.2+2.5;
  float focusDistance=max(length(ro)-0.1,0.01);
  mat3 rotCam=lookat(-ro+texture(tex,vec2(tim2,tim2*1.3)).rgb*0.06+vec3(cos(tim2*0.75),sin(tim2*0.4),sin(tim2*0.6))*0.5,vec3(0.0,1.0+cos(tim2*25.0)*0.125,0.125*sin(tim2*25.0)));
  ro.z-=tim;
  rmx=mat2(ct,st,-st,ct);
  vec2 pxl=(-size.xy+2.0*(gl_FragCoord.xy.xy+rand2()))/size.y;
  vec3 er = normalize( vec3( pxl.xy, fov ) );
  vec3 go = blurAmount*focusDistance*vec3( -1.0 + 2.0*rand2(), 0.0 );
  vec3 gd = normalize( er*focusDistance - go );gd.z=0.0;
  clr+=scene(ro+rotCam*go,normalize(rotCam*(er+gd)));
 }
 clr/=vec3(Rays);
 fragColor = vec4(sqrt(clr)*1.4-0.25,1.0);
}