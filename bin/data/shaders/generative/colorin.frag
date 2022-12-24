#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

float maxdist=80.;
float det=.002;
vec3 luzdir = vec3(0.,1.,0.);
vec3 poscol=vec3(0.);

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

vec2 de(vec3 p) {
    
    p.z += sin(p.y*.3)*2.;
    p.x += cos(p.y*.3)*2.;
    
    float i=0.;
    vec3 prot = p;
    float r = length(p);
    
    
    vec3 p3 = p;
    
    p3.y-=time*2.;
    poscol=p;
    vec3 p4 = p;
    p4.xz=rot2D(p4.y*20.)*p4.xz;
    p4=abs(p4);
    p4.y-=time*3.;
    
    p4.y = abs(.5-fract(p4.y*.5));
    p3.y = abs(.5-fract(p3.y));
    float esf = toro(p3,vec2(2.+sin(p.y)*.5,0.1));
    vec3 p2 = p;
    

    float cub = cubo(p4-vec3(3.,0.,0.), vec3(.4));
    cub = min(cub,cubo(p4-vec3(0.,0.,3.), vec3(.4)));
    //esf = min(esf,tor);
    float d = esf;
    d = min(d,cub*.7);
    if (abs(esf-d)<.001) i=1.;
    if (abs(cub-d)<.001) i=2.;    
    //d = max(d,abs(p.y)-80.);
    return vec2(d*.6,i);
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
    float sh=1.;
    float luzdif=max(0.,dot(normalize(luzdir),-n))*sh;
    float luzcam=max(0.,dot(dir,-n));
    vec3 refl=reflect(dir,-n);
    float luzspec=pow(max(0.,dot(refl,-luzdir)),3.)*sh;
    return col*(luzcam+luzdif+0.1)+luzspec*.5;
}

vec3 shade(vec3 p, vec3 dir, float i) {
    float c = 1.-step(.001,abs(i-1.));
    // color de los rectangulos, aca lo hice variar segun la pos x
    vec3 colrect = vec3(0.3,cos(p.x)*.3,sin(p.y*.5)*.2); 
    // color de los aros - poscol toma la posicion 3D como color rgb, 
    // seteado en la funcion de(), le mande seno para que se repita
    vec3 colaros = vec3(abs(sin(poscol*.5)))*.3; 
    vec3 col = mix(colrect,colaros,c);
    col = light(p, dir, col);
    luzdir*=-1.;
    col = light(p, dir, col);
    return col;
}
vec3 march(vec3 from, vec3 dir) {
    vec3 col=vec3(0.);
    float totdist=fract(sin(dot(vec2(1.213,2.234),dir.xy)));
    float st=0.;
    float glow=0.;
    vec2 d;
    vec3 p;
    for (int i=0; i<80; i++) {
        p=from+totdist*dir;
        d=de(p);
        if (d.x<det || totdist>maxdist) break;
        totdist+=d.x;
        glow+=1.;
    }
    float l = abs(dot(dir.xy,vec2(0.,1.)));
    float luzglow = pow(l,10.)*1.5;
    // color background, uso la dir para colorinchear
    vec3 colback = vec3(1.5,1.,0.5)*(1.2+cos(dir*25.));
	// color del glow, fijo
    vec3 colglow = vec3(2.,.7,.3);
    vec3 backcol=colback*(mod(pow(l,3.),.05)+luzglow);
    if (d.x<det) {
        col=shade(p-det*dir,dir,d.y);
        col = mix(col,luzglow*colglow,min(1.,totdist/500.));
    } else {
        col=backcol;
    }
    col +=pow(glow/80.,3.)*colglow;
    return col;
}

vec3 path(float t){
    t*=.5;
    vec3 p=vec3(cos(t*2.)*3.,-cos(t)*25.,4.-sin(t)*10.);
    return p;
}

void main(){   
    float v=time*.5;
    vec2 uv = gl_FragCoord.xy/resolution.xy;
    uv-=.5;
    uv.x*=resolution.x/resolution.y;
    vec3 dir = normalize(vec3(uv,1.3));
    vec3 from = path(v);
    vec3 dircam = normalize(-from);
    dir = lookat(dircam, vec3(1.,1.,0.)) * dir;    
    fragColor = vec4(march(from,dir),1.0);
}

