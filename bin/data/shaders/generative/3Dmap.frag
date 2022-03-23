#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float shape;
uniform float zoom;
uniform float texturemix;
uniform float texturesize;
uniform bool mirror;
uniform float grid;
uniform float gridsize;
uniform float glowmix;
uniform float ambient;
uniform float specular;
uniform sampler2D intext;
uniform float rotationxy;
uniform float rotationxz;
uniform float rotationyz;
uniform float deftext;
float det=.001, maxdist=50.;
vec3 pp;

float hash12(vec2 p)
{
    p*=1000.;
	vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

mat2 rot(float a)
{
    float s=sin(a),c=cos(a);
    return mat2(c,s,-s,c);
}

float sdSphere(vec3 p, float r)
{
    return length(p)-r;
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sdTorus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
}

float sdHexPrism( vec3 p, vec2 h )
{
  const vec3 k = vec3(-0.8660254, 0.5, 0.57735);
  p = abs(p);
  p.xy -= 2.0*min(dot(k.xy, p.xy), 0.0)*k.xy;
  vec2 d = vec2(
       length(p.xy-vec2(clamp(p.x,-k.z*h.x,k.z*h.x), h.x))*sign(p.y-h.x),
       p.z-h.y );
  return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

float sdOctahedron( vec3 p, float s)
{
  p = abs(p);
  float m = p.x+p.y+p.z-s;
  vec3 q;
       if( 3.0*p.x < m ) q = p.xyz;
  else if( 3.0*p.y < m ) q = p.yzx;
  else if( 3.0*p.z < m ) q = p.zxy;
  else return m*0.57735027;

  float k = clamp(0.5*(q.z-q.y+s),0.0,s);
  return length(vec3(q.x,q.y-s+k,q.z-k));
}

float sdPyramid( vec3 p, float h)
{
  float m2 = h*h + 0.25;

  p.xz = abs(p.xz);
  p.xz = (p.z>p.x) ? p.zx : p.xz;
  p.xz -= 0.5;

  vec3 q = vec3( p.z, h*p.y - 0.5*p.x, h*p.x + 0.5*p.y);

  float s = max(-q.x,0.0);
  float t = clamp( (q.y-0.5*p.z)/(m2+0.25), 0.0, 1.0 );

  float a = m2*(q.x+s)*(q.x+s) + q.y*q.y;
  float b = m2*(q.x+0.5*t)*(q.x+0.5*t) + (q.y-m2*t)*(q.y-m2*t);

  float d2 = min(q.y,-q.x*m2-q.y*0.5) > 0.0 ? 0.0 : min(a,b);

  return sqrt( (d2+q.z*q.z)/m2 ) * sign(max(q.z,-p.y));
}




vec3 tex(vec2 p)
{
    vec2 p2=fract(p*gridsize*5.1);
    if (mirror) p=abs(.5-fract(p*texturesize*2.));
    else p=abs(fract(p*texturesize*2.+.5));
    return texture(intext,p).rgb+smoothstep(.9,1.,max(p2.x,p2.y))*grid;
}
float de(vec3 p)
{
	
	//float ms2 = floor(1.0)+1.;
	p.z+=time;
    p.xy*=rot(rotationxy*2.);
    p.xz*=rot(rotationxz*2.);
    p.yz*=rot(rotationyz*2.);
    pp=p;
    float d;
	
	float ms = floor(10.0)+1.;
	p.x = mod(p.x, ms) - ms/2.;
    p.z = mod(p.z, ms) - ms/2.;
	p.y = mod(p.y, ms) - ms/2.;
	vec2 p2 = p.xy;
	//p2.x = 1.-p2.x;
	vec3 t = tex(p2);
	
	float t_e = (t.x +t.y + t.z)/3.;
    //float t_e =0.0;
	int i=int(floor(shape*5.));
    if (i==0) d=sdSphere(p,3.+t_e*deftext);
    if (i==1) d=sdBox(p,vec3(2.+t_e*deftext));
    if (i==2) d=sdTorus(p,vec2(2.,1.+t_e*deftext));
    if (i==3) d=sdHexPrism(p,vec2(2.,1.+t_e*deftext));
    if (i==4) d=sdOctahedron(p,3.+t_e*deftext);
    return d;
}

vec3 normal(vec3 p)
{
    vec2 e=vec2(0.,det);
    return normalize(vec3(de(p+e.yxx),de(p+e.xyx),de(p+e.xxy))-de(p));
}



vec3 glow(vec3 p)
{
        vec3 n=normal(p);
        n=abs(n);
        vec3 col=tex(pp.xy)*n.z;
        col+=tex(pp.xz)*n.y;
        col+=tex(pp.yz)*n.x;
        return col;
}


vec3 march(vec3 from, vec3 dir)
{
    float d,td=0.;
    vec3 p, col=vec3(0.);
    float h=1.-hash12(dir.xy)*.3;
    for (int i=0; i<100; i++)
    {
        p=from+td*dir;
        d=de(p);
        if (d<det || td>maxdist) break;
        td+=d*h;
      //  col+=glow(p)/(1.+d*d)*.1*glowmix;
    }
    if (d<det)
    {
        dir=normalize(dir+.5);
        vec3 n=normal(p);
        float l=max(0.,dot(-dir,n));
        vec3 ref=reflect(dir,n);
        float spe=pow(max(0.,dot(ref,n)),50.);
        n=abs(n);
        vec3 c=tex(pp.xy)*n.z;
        c+=tex(pp.xz)*n.y;
        c+=tex(pp.yz)*n.x;
        c=c*max(ambient,l)*texturemix+spe*specular;
        col+=c;
    }
    return col;
}

void main(void)
{
    vec2 uv=gl_FragCoord.xy/resolution-.5;
    uv.x*=resolution.x/resolution.y;
    vec3 from=vec3(0.,0.,-5.-zoom*15.);
    vec3 dir=normalize(vec3(uv,1.));
    // from.xz*=rot(time);
    // dir.xz*=rot(time);
    // from.yz*=rot(time);
    // dir.yz*=rot(time);
	//dir.z+=time;
	
	//from.y+=time;
    vec3 col=march(from,dir);
    gl_FragColor=vec4(col,1.);

}