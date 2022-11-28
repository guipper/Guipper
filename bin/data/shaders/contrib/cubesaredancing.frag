#pragma include "../common.frag"
// Code by Flopine

// Thanks to wsmind, leon, XT95, lsdlive, lamogui, 
// Coyhot, Alkama,YX, NuSan, slerpy and wwrighter for teaching me

// Thanks LJ for giving me the spark :3

// Thanks to the Cookie Collective, which build a cozy and safe environment for me 
// and other to sprout :)  https://twitter.com/CookieDemoparty

uniform float speed ;
uniform sampler2D iChannel0 ;
//#define PI acos(-1.)
#define TAU 6.283581
#define ITER 80.

#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
#define crep(p,c,l) p=p-c*clamp(round(p/c),-l,l)

#define dt(sp,off) fract((iTime*speed+off)*sp)
#define bouncy(sp,off) sqrt(sin(dt(sp,off)*PI))

struct obj
{
  float d;
  vec3 cs; 
  vec3 cl;
};

obj minobj (obj a, obj b)
{
  if (a.d<b.d) return a;
  else return b;
}

float stmin(float a, float b, float k, float n)
{
  float st = k/n;
  float u = b-k;
  return min(min(a,b),0.5*(u+a+abs(mod(u-a+st,2.*st)-st)));
}

void mo (inout vec2 p, vec2 d)
{
  p = abs(p)-d;
  if(p.y>p.x) p = p.yx;
}

float box (vec3 p, vec3 c)
{
  vec3 q = abs(p)-c;
  return min(0.,max(q.x,max(q.y,q.z)))+length(max(q,0.));
}

float sc (vec3 p, float d)
{
  p=abs(p);
  p=max(p,p.yzx);
  return min(p.x,min(p.y,p.z))-d;
}

obj prim1 (vec3 p)
{
  p.x = abs(p.x)-3.;
  float per = 0.9;
  float id = round(p.y/per);
  p.xz *= rot(sin(dt(0.8,id*1.2)*TAU));
  crep(p.y, per,4.);
  mo(p.xz,vec2(0.3));
  p.x += bouncy(2.,0.)*0.8;
  float pd = box(p,vec3(1.5,0.2,0.2));
  return obj(pd,vec3(0.5,0.,0.),vec3(1.,0.5,0.9));
}

obj prim2 (vec3 p)
{
  p.y = abs(p.y)-6.;
  p.z = abs(p.z)-4.;
  mo(p.xz, vec2(1.));
  vec3 pp = p;
  mo(p.yz, vec2(0.5));
  p.y -= 0.5;
  float p2d = max(-sc(p,0.7),box(p,vec3(1.)));
  p = pp;
  p2d = min(p2d, max(box(p,vec3(bouncy(2.,0.))*4.),sc(p,0.2)));
  return obj(p2d, vec3(0.2),vec3(1.));
}

obj prim3 (vec3 p)
{
  p.z = abs(p.z)-9.;
  float per = 0.8;
  vec2 id = round(p.xy/per)-.5;
  float height = 1.*bouncy(2.,sin(length(id*0.05)));
  float p3d = box(p,vec3(2.,2.,0.2));
  crep(p.xy,per,2.);
  p3d = stmin(p3d,box(p+vec3(0.,0.,height*0.9),vec3(0.15,.15,height)),0.2,3.);
  return obj (p3d, vec3(0.1,0.7,0.),vec3(1.,0.9,0.));
}

obj prim4 (vec3 p)
{
  p.y = abs(p.y)-5.;
  mo(p.xz, vec2(1.));
  float scale = 1.5;
  p *= scale;
  float per = 2.*(bouncy(0.5,0.));
  crep(p.xz,per,2.);
  float p4d = max(box(p,vec3(0.9)),sc(p,0.25));
  return obj (p4d/scale, vec3(0.1,0.2,0.4),vec3(0.1,0.8,0.9));
}

float squared (vec3 p,float s)
{
  mo(p.zy,vec2(s));
  return box(p,vec3(0.2,10.,0.2));
}

obj prim5 (vec3 p)
{
  p.x = abs(p.x)-8.;
  float id = round(p.z/7.);
  crep(p.z,7.,2.);
  float scarce = 3.;
  float p5d=1e10;
  for(int i=0;i<4; i++)
  {
    p.x += bouncy(1.,id*0.9)*0.6;
    p5d = min(p5d,squared(p,scarce));
    p.yz *= rot(PI/4.);
    scarce -= 1.;    
  }
  return obj(p5d,vec3(0.5,0.2,0.1),vec3(1.,0.9,0.1));
}

obj SDF (vec3 p)
{
  p.yz *= rot(-atan(1./sqrt(2.)));
  p.xz *= rot(PI/4.);

  obj scene = prim1(p);
  scene = minobj(scene,prim2(p));
  scene = minobj(scene,prim3(p));
  scene = minobj(scene,prim4(p));
  scene = minobj(scene, prim5(p));
  return scene;
}

vec3 getnorm (vec3 p)
{
  vec2 eps = vec2(0.001,0.);
  return normalize(SDF(p).d-vec3(SDF(p-eps.xyy).d,SDF(p-eps.yxy).d,SDF(p-eps.yyx).d));
}


void main()
{
 vec2 uv = (2.*fragCoord.xy-iResolution.xy)/ iResolution.y;

vec3 ro = vec3(uv*5.,-30.),rd = vec3(0.,0.,1.),
  p = ro,
  col = vec3(smoothstep(0.3,0.65,texture(iChannel0, vec2(length(uv*0.5),0.025)).xxx)),
  l = normalize(vec3(1.,1.4,-2.));

  col = vec3(1.0);
  
  obj O; bool hit = false;

  for (float i=0.; i<ITER;i++)
  {
   O = SDF(p);
   if (O.d<0.001)
   {hit = true; break;}
   p += O.d*rd;
  }

  if (hit)
  {
    vec3 n = getnorm(p);
    float light = max(dot(n,l),0.);
    col = mix(O.cs,O.cl, light);
  }
    fragColor = vec4(sqrt(col),1.);
}