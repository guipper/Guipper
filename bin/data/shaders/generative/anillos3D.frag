#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float rotx1;
uniform float roty1;
uniform float size1;
uniform float tile1;
uniform float gordura;
//uniform float posyasdqwd;



/*****************************************************************************************/
float maxdist=100.;
float det=.001;
vec3 luzdir = vec3(1.,.5,0.);
vec3 color_esfera = vec3(.8,.6,.2);
vec3 color_anillo = vec3(0.3,.2,1.);
vec3 color_cubos = vec3(0.3,0.7,0.3);
float dist_tor=0.;

mat2 rot2D(float a) {
    a=radians(a);
    return mat2(cos(a),sin(a),-sin(a),cos(a));
}

mat3 lookat(vec3 fw,vec3 up){
    fw=normalize(fw);vec3 rt=normalize(cross(fw,normalize(up)));return mat3(rt,cross(rt,fw),fw);
}

float cubo( vec3 p, vec3 b )
{
  return length(max(vec3(0.),abs(p)-b));
}

float toro( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
}

float esfera(vec3 p, float r) {
    return length(p) - r;
}

vec3 tile(vec3 p, float t) {
    return abs(t - mod(p, t*2.));
}

float superficie(vec3 p) {
    float sx=sin(p.x*20.+sin(p.y*10+time*10));
    float sz=sin(p.z*10.);

    p.y+=max(sx,sz)*.1;
    float c=cos(p.y+time);
    return c*.2;
}

float superficie2(vec3 p){

    float a2 = atan(p.x,p.z);
    float amp = 0.01;
    float r= sin(p.x*3+time)*amp
      +sin(p.y*3+time)*amp
      +sin(p.z*3)*amp
      +sin(p.x*3)*amp
      +sin(p.y*10)*amp
      +sin(p.x*10)*amp;

    r = sin(p.x*8+time)*9.+0.1;
    r = smoothstep(0.6,1.0,r);

    //a2 = sin(a2*10)*0.1;
    //a2 = sm(0.0,1.0,a2);
    r = sin(p.y*2+time*10)*0.2;

    //r*=.;

    //r +=sin(r*10+time)*1.4;
    return r;

}

vec2 de(vec3 p) {

    float i=0.;
    vec3 prot = p;
    float r = length(p);

    //p.z+=3.5;
    p.y+=time*2.0;

    p.xz*=rotate2d(time*0.02+p.y*0.02);
    //p.x+=time*2;
	float fsize = mapr(size1,2.0,3.5);
	float ftile = mapr(tile1,2.0,5.0);
    float esf = toro(tile(p,ftile),vec2(fsize,mapr(gordura,0.01,1.0)));

    vec3 p2 = p;
    p2.x+=time*0.02;

    //p2.z-=20.5;

    float tor = toro(p2, vec2(2.2,2.2));
    //esf = min(esf,tor);
    float d = 0;
    d = min(esf,esf);

    if (abs(esf-d)<.001) i=1.;
    if (abs(tor-d)<.001) i=2.;

    dist_tor=abs(tor-d);
    return vec2(d*.5,i);
}

vec3 normal(vec3 p) {
    vec3 e = vec3(0.0,det,0.0);

    return normalize(vec3(
            de(p+e.yxx).x-de(p-e.yxx).x,
            de(p+e.xyx).x-de(p-e.xyx).x,
            de(p+e.xxy).x-de(p-e.xxy).x
            )
        );
}

float shadow(vec3 pos) {
  float sh = 1.0;
  float totdist = 2.*det;
  float d = 10.;
  for (int i = 0; i < 50; i++) {
    if (d > det) {
      vec3 p = pos - totdist * luzdir;
      d = de(p).x;
      sh = min(sh, 50. * d / totdist);
      totdist += d;
    }
  }
  return clamp(sh, 0.0, 1.0);
}

vec3 light(vec3 p, vec3 dir, vec3 col) {
    vec3 n=normal(p);
    float sh=shadow(p);
    float luzdif=max(0.,dot(normalize(luzdir),-n))*sh;
    float luzcam=max(0.,dot(dir,-n));
    vec3 refl=reflect(dir,-n);
    float luzspec=pow(max(0.,dot(refl,-luzdir)),1.9)*sh;
    return col*(luzcam*1.9+luzdif+0.9)+luzspec*.2;
}

vec3 shade(vec3 p, vec3 dir, float i) {
    vec3 col=vec3(0.);
    col += vec3(0.2+superficie2(p),0.3,0.2) * (1.-step(.001,abs(i-1.)) );
    col += color_anillo * (1.-step(.001,abs(i-2.)));
    col += color_cubos * (1.-step(.001,abs(i-3.)));
    col = light(p, dir, col);
    return col;
}
vec3 march(vec3 from, vec3 dir) {
    vec3 col=vec3(0.);
    float totdist=0.;
    float st=0.;
    float glow=0.;
    vec2 d;
    vec3 p;
    for (int i=0; i<100; i++) {
        p=from+totdist*dir;
        d=de(p);
        if (d.x<det || totdist>maxdist) break;
        totdist+=d.x;
        glow+=max(0.,.5-dist_tor)/.5;
        st++;
    }
    if (d.x<det) {
        col=shade(p-det*dir,dir,d.y);
    }

    col +=pow(glow/100.,2.)*5.*(.5+vec3(1.0,0.5,0.5));
    return col;
}
void main()
{
	vec2 uv = gl_FragCoord.xy/resolution.xy;
    uv-=.5;
    vec3 dir = normalize(vec3(uv,0.5));
    vec3 from = vec3(0.,0.,0.0);
    mat2 rotxz = rotate2d(rotx1*PI);
    mat2 rotyz = rotate2d(roty1*PI);


    from.xz*=rotxz;
    dir.xz*=rotxz;
    from.xy*=rotyz;
    dir.xy*=rotyz;


    //dir.x+=time*0.1;
    //from.x-=time*0.1;
    gl_FragColor = vec4(march(from,dir),1.0);
}
