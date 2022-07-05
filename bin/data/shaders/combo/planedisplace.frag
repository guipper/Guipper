#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float image_scale;
uniform float displacement;
uniform float detail;
uniform float dist;
uniform float light_dir_x;
uniform float light_dir_y;
uniform float light_dir_z;
uniform float ambient;
uniform float ambient_occlusion;
uniform float diffuse;
uniform float specular;
uniform float spec_exp;
uniform float color_mix;
uniform float rotation_x;
uniform float rotation_y;

const float maxdist=20.;
float det=.005*detail;
#define dirlight normalize(vec3(light_dir_x-.5,light_dir_y-.5,light_dir_z-.5))
uniform sampler2DRect textura;


vec4 de(vec3 p) {
  p.xz*=rotate2d(rotation_y*3.1416);
  p.yz*=rotate2d(rotation_x*3.1416);
  vec2 uv =(p.xy+.5/image_scale)*image_scale;
  vec3 col = texture2DRect(textura, uv*resolution).rgb;
  float disp=sqrt(length(col));
  if (displacement-.5<0.) disp=2.-length(col);
  float d = abs(p.z)-disp*abs(displacement-.5)*.5;
  d=max(d,length(max(vec2(0.),abs(p.xy)-.5/image_scale)));
  return vec4(col,d*.5);
}

float ao(vec3 p, vec3 n) {
  float st=.1, oc=0., totdist=0.;
  for (float i=1.; i<7.; i++) {
    totdist=i*st;
    float d=de(p-totdist*n).w;
    oc+=max(0.,(totdist-d)/totdist);
  }
  return clamp(1.-oc*0.2*ambient_occlusion,0.,1.);
}


vec3 normal(vec3 p) {
  vec3 e = vec3(0.,det*2.,0.);
  return normalize(vec3(de(p-e.yxx).w,de(p-e.xyx).w,de(p-e.xxy).w)-de(p).w);
}

vec3 light(vec3 p, vec3 dir, vec3 n, vec3 col) {
  vec3 ldir=normalize(dirlight);
  float ao=ao(p,n);
  float amb=ambient*ao*.5;
  float diff=max(0.,dot(ldir,-n))*diffuse*1.5;
  vec3 ref=reflect(dir,-n);
  float spec=pow(max(0.,dot(ldir,ref)),100.*spec_exp)*specular;
  return col*(amb+diff)+spec;
}

vec3 march(vec3 from, vec3 dir) {
  vec3 p, col=vec3(0.), backcol=vec3(0.);
  float totdist=0.;
  vec4 d;
  for (int i=0; i<80; i++) {
    p=from+dir*totdist;
    d=de(p);
    totdist+=d.w;
    if (d.w<det || totdist>maxdist) {
      break;
    }
  }
  if (d.w<.05) {
    p-=det*dir*2.;
    col=d.rgb;
    vec3 n=normal(p);
    col=mix(vec3(1.),col,color_mix);
    col=light(p,dir,n,col);
  }
  return col;
}

void main()
{
  vec2 uv = fragCoord.xy/resolution-.5;
  uv.x*=resolution.x/resolution.y;
  vec3 dir = normalize(vec3(uv,1.));
  vec3 from = vec3(0.,0.,-dist*5.);
  vec3 col=march(from, dir);
  fragColor = vec4(col,1.);
}
