#pragma include "../common.frag"

uniform sampler2D tx;

uniform float steps_multiplier;
uniform float extrusion;
uniform float height_scale;
uniform float rotationXY;
uniform float rotationXZ;
uniform float rotationYZ;
uniform float cameraX;
uniform float cameraY;
uniform float cameraZ;
uniform float fov;
uniform float lightdirX;
uniform float lightdirY;
uniform float lightdirZ;
uniform float ambient;
uniform float diffuse;
uniform bool invert;
uniform float distortX;
uniform float distortY;
uniform float distortZ;
uniform float distort_scale;
uniform float fudge_factor;

float maxdist=50., det=.001;
vec3 objcol;

float hash(vec2 p)
{
	vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

mat2 rot(float a) {
    float s=sin(a), c=cos(a);
    return mat2(c,s,-s,c);
}

float box(vec3 p, vec3 c) {
    return length(max(vec3(0.),abs(p)-c));
}

float de(vec3 p) {
  p.xy*=rot(p.z*distortZ*5.);
  p.xz*=rot(-p.y*distortY);
  p.yz*=rot(p.x*distortX);
  float st=steps_multiplier*.3+.005;
  float extrude=extrusion*5.;
  p.z+=extrude*.5;
  p.z+=.5;
  //if (distort) p.yz*=rot(p.x*.5+iTime);
  vec3 p2=p;
  p2.x*=3./5.;
  if(!invert) objcol=texture2D(tx,p2.xy*.15*(1.+p.z*3.*(distort_scale-.5))+.5).rgb;
  else objcol=1.-texture2D(tx,p2.xy*.15*(1.+p.z*3.*(distort_scale-.5))+.5).rgb;

  //float l=invert?1.-length(objcol)*.5:length(objcol)*.3*smoothstep(.75,1.,dot(normalize(objcol),normalize(vec3(0.,.5,1.))));
  float l=length(objcol)*.5*height_scale*2.;
  float z=p.z;
  p.z=mod(p.z,st)-st*.5;
  float d=box(p, vec3(5.,3.,.01));
  d=max(d,abs(z)-extrude*.5);
  float h=hash(gl_FragCoord.xy)-.5;
  d+=smoothstep(1.-(z+extrude*.5)/extrude,0.,l)*.02;
  return d*fudge_factor;
  }

float shadow(vec3 p, vec3 ldir) {
    float td=.001,sh=1.,d=det;
    for (int i=0; i<80; i++) {
		    p-=ldir*d;
        d=de(p);
        td+=d;
		    sh=min(sh,50.*d/td);
		    if (td>maxdist) break;
    }
    return clamp(sh,0.,1.);
}

vec3 normal(vec3 p) {
    vec2 e=vec2(0.,det);
    return normalize(vec3(de(p+e.yxx),de(p+e.xyx),de(p+e.xxy))-de(p));
}

vec3 march(vec3 from, vec3 dir) {
    vec3 p, col=vec3(0.);
    float d, td=0.;
    for (int i=0; i<2000; i++) {
        p=from+dir*td;
        d=de(p);
        if (d<det || td>maxdist) break;
        td+=d;
    }
    if (d<det) {
        p-=dir*det;
        vec3 n=normal(p);
        vec3 ldir=normalize(vec3(lightdirX-.5,lightdirY-.5,lightdirZ-.5));
        //float sh=shadow(p, -ldir);
        col=objcol*max(ambient,max(0.,dot(ldir,n))*diffuse);
    }
    return col;
}

void main()
{
    vec2 uv = gl_FragCoord.xy/resolution.xy-.5;
    uv.x*=resolution.x/resolution.y;
    vec3 from=vec3(cameraX*4.-2.,cameraY*4.-2.,-cameraZ*10.);
     vec3 dir=normalize(vec3(uv,fov*3.));
    from.xz*=rot(rotationXZ*3.1416);
    dir.xz*=rot(rotationXZ*3.1416);
    from.yz*=rot(-rotationYZ*3.1416);
    dir.yz*=rot(-rotationYZ*3.1416);
    from.xy*=rot(rotationXY*3.1416);
    dir.xy*=rot(rotationXY*3.1416);
    vec3 col = march(from,dir);
    fragColor = vec4(col,1.0);
}