#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float sph_size;
uniform float sph_extrude;
uniform float sph_freq;
uniform float tile_size;
uniform float domo_size;
uniform sampler2D background;
uniform sampler2D sph_texture;
uniform float sph_texture_scale;
uniform sampler2D floor_texture;
uniform float floor_texture_scale;
uniform float ambient_light;
uniform float reflection;
uniform float rotationspeed;


const float maxdist=10.;
const float det=.003;
const vec3 dirlight = vec3(1.,-1.,-1.);
float obj_id;
vec3 psph;


float sphere(vec3 p, float radio) {
  return length(p)-radio;
}

vec3 tile(vec3 p, vec3 tile) {
  return abs(tile-mod(p,tile)*2.);
}

float de(vec3 p) {
  psph=p;
  psph.xz*=rotate2d(time*mapr(rotationspeed,-0.1,0.1));
  psph=tile(psph,vec3(vec2(tile_size),tile_size*1.5));
  float sph=sphere(psph,sph_size)+length(sin(psph*sph_freq*10.))*mapr(sph_extrude,-.5,.5);
  sph=max(sphere(p,domo_size*10.),sph);
  sph*=.4;
  float pla=-p.y;//-length(cos(p*5))*.2;
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
  float totdist=0.1, sh=1.;
  for(int i=1;i<20;i++) {
    p+=totdist*ldir;
    float d=de(p);
    totdist+=d;
    sh=min(sh,10.*d/totdist);
    if (sh<.001) break;
  }
  return clamp(sh,0.,1.);
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
  //col+=vec3(1.,0.,0.)*is_id(1.);

  vec2 uv_shp = fract(psph.xy)*resolution*sph_texture_scale*2.;
  col+=texture(sph_texture, uv_shp/resolution).rgb*is_id(1.);
  col+=vec3(0.,0.,1.)*is_id(2.);
  return col;
}

vec3 light(vec3 p, vec3 dir, vec3 n, vec3 col) {
  vec3 ldir=normalize(dirlight);
  float sh=shadow(p);
  float ao=ao(p,n);
  float amb=.5*ao*ambient_light;
  float diff=max(0.,dot(ldir,-n))*.7*sh;
  vec3 ref=reflect(dir,-n);
  float spec=pow(max(0.,dot(ldir,ref)),50.)*.7*sh;
  return col*(amb+diff)+spec;
}

vec3 march(vec3 from, vec3 dir) {
  vec3 p=from, col=vec3(0.), backcol=vec3(0.),refp=vec3(0.);
  float totdist=0.,d, id=0., ref=0.;
  for (int i=0; i<100; i++) {
    p+=dir*d;
    d=de(p);
    totdist+=d;
    if (d<det && is_id(2.)>.5) {
	        p-=dir*det*2.;
        	vec3 n=normal(p);
          dir=reflect(dir,n);
          ref=reflection;
          refp=p;
        }
    if ((d<det && is_id(2.)<.5) || totdist>maxdist) {
      break;
    }
  }
  if (d<det) {
    id=obj_id;
    p-=det*dir*2.;
    vec3 obj_col=color();
    vec3 n=normal(p);
    col=light(p,dir,n,obj_col);
  } else {
    totdist=maxdist;
    p=from+dir*maxdist;
    col=texture(background,abs(dir.xy-vec2(0.,.1))).rgb;
  }
  vec2 uvfloor=abs(resolution-mod(refp.xz*floor_texture_scale*resolution,resolution*2.));
  vec3 floor_col=texture(floor_texture,uvfloor/resolution).rgb;
  col=mix(col,floor_col,ref);
  return col;
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
  vec3 from = vec3(0.,-1.,-7.);
  vec3 target = vec3(-mouse.x+.5,mouse.y-.5,0.)*5.;
  dir=lookat(target-from,vec3(0.,1.,0.))*dir;
  vec3 col=march(from, dir);
  gl_FragColor = vec4(col,1.);
}
