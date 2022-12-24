#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float extrude_sph;
uniform float freq_sph;

const float maxdist=10.;
const float det=.005;
const vec3 dirlight = vec3(1.,-2.,-1.);
float obj_id;


float sphere(vec3 p, float radio) {
  return length(p)-radio;
}


float de(vec3 p) {
  vec3 psph=p;
  psph.xz*=rotate2d(time);
  psph+=vec3(1.5,0.5,0.);
  float sph=sphere(psph,1.)+length(sin(psph*freq_sph*10.))*mapr(extrude_sph,-.5,.5);
  sph*=.5;
  float pla=-p.y+1.-length(cos(p*5))*.2;
  float d=min(sph,pla);
  obj_id=step(sph,d)+step(pla,d)*2.;
  return d;
}

float ao(vec3 p, vec3 n) {
  float st=.05, oc=0., totdist=0.;
  for (float i=1.; i<7.; i++) {
    totdist=i*st;
    float d=de(p-totdist*n);
    oc+=max(0.,(totdist-d)/totdist);
  }
  return clamp(1.-oc*.13,0.,1.);
}

float shadow(vec3 p) {
  vec3 ldir=normalize(dirlight);
  float totdist=0., sh=1.;
  for(int i=0;i<30;i++) {
    p+=totdist*ldir;
    float d=de(p);
    totdist+=d;
    sh=min(sh,3.*d/totdist);
  }
  return clamp(sh,0.3,1.);
}

vec3 normal(vec3 p) {
  vec3 e = vec3(0.,det*2.,0.);
  return normalize(vec3(de(p-e.yxx),de(p-e.xyx),de(p-e.xxy))-de(p));
}

float is_id(float id) {
  return 1.-step(.1,abs(id-obj_id));
}


vec3 color() {
  vec3 col=vec3(0.);
  col+=vec3(1.,0.,0.)*is_id(1.);
  col+=vec3(0.,0.,1.)*is_id(2.);
  return col;
}

vec3 light(vec3 p, vec3 dir, vec3 n, vec3 col) {
  vec3 ldir=normalize(dirlight);
  float sh=shadow(p);
  float ao=ao(p,n);
  float amb=.2*ao;
  float diff=max(0.,dot(ldir,-n))*.7*sh;
  vec3 ref=reflect(dir,-n);
  float spec=pow(max(0.,dot(ldir,ref)),50.)*.7*sh;
  return col*(amb+diff)+spec;
}

vec4 march(vec3 from, vec3 dir) {
  vec3 p, col=vec3(0.), backcol=vec3(0.);
  float totdist=0.,d;
  for (int i=0; i<80; i++) {
    p=from+dir*totdist;
    d=de(p);
    totdist+=d;
    if (d<det || totdist>maxdist) {
      break;
    }
  }
  if (d<.1) {
    p-=det*dir*2.;
    vec3 obj_col=color();
    vec3 n=normal(p);
    col=light(p,dir,n,obj_col);
  } else {
    totdist=maxdist;
    p=from+dir*maxdist;
  }
  backcol=vec3(.8,.9,1.)*(1.-.7*smoothstep(0.,10.,-p.y-.5));
  float depth = 1.-(maxdist-totdist)/maxdist;
  col=mix(col,backcol,pow(depth,1.5));
  return vec4(col,depth);
}

mat3 lookat(vec3 dir,vec3 up){
    dir=normalize(dir);vec3 rt=normalize(cross(dir,normalize(up)));
    return mat3(rt,cross(rt,dir),dir);
}

void main()
{
  vec2 uv = gl_FragCoord.xy/resolution-.5;
  uv.x*=resolution.x/resolution.y;
  vec3 dir = normalize(vec3(uv,1.));
  vec3 from = vec3(0.,0.,-5.);
  //vec3 target = vec3(-mouse.x+.5,mouse.y-.5,0.)*5.;
  //dir=lookat(target-from,vec3(0.,1.,0.))*dir;
  vec4 col=march(from, dir);
  fragColor = col;
}
