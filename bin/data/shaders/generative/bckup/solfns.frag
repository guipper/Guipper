#pragma include "../common.frag"

#define t time*.5
#define det .01

uniform float contrastsun;
uniform float rayfreq;
uniform float rayamp;
uniform float rayrandom;
uniform float aspectratio;
uniform float size;

mat2 rot(float a) {
    float si = sin(a);
    float co = cos(a);
    return mat2(co,si,-si,co);
}

float sphere(vec3 p, vec3 rd, float r){
	float b = dot( -p, rd ), i = b*b - dot(p,p) + r*r;
	return i < 0. ?  -1. : b - sqrt(i);
}

float hash(vec2 p)
{
   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(float p) {
	float fl = floor(p);
	float fr = fract(p);
	return mix(hash(vec2(fl)),hash(vec2(fl+1.)),fr);
}

float snois(vec3 uv, float res)
{
    const vec3 s = vec3(1e0, 1e2, 1e4);	
	uv *= res;	
	vec3 uv0 = floor(mod(uv, res))*s;
	vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;	
	vec3 f = fract(uv); f = f*f*(3.0-2.0*f);	
	vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,
		      	  uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);	
	vec4 r = fract(sin(v*1e-3)*1e5);
	float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);	
	r = fract(sin((v + uv1.z - uv0.z)*1e-3)*1e5);
	float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);	
	return mix(r0, r1, f.z)*2.-1.;
}

float ksetsun(vec3 p) {
    float m = 1000.;
    for (int i = 0; i < 8; i++) {
        float d=dot(p,p);
        p = abs(p) / dot(p,p) - .7;
        m = min(m, abs(d - .5));
    }
    m = pow(max(0., 1. - m), 8.);
    return m;
}

float sun(vec3 from, vec3 dir) {
	from.xz*=rot(t*.5);
    dir.xz*=rot(t*.5);
    float c = 0.;
    float d = sphere(from, dir, 1.);
	float s = step(0.,d);
    for (float i = 0.; i < 10.; i++) {
        d = sphere(from, dir, 1. + i * .015);
        vec3 p = from + dir * d;
        float a = .3 * i + t*.15;
        p.yz*=rot(a);
        p.xy*=rot(a);
        c += ksetsun(p) * .15 * s;            
    }
    return c*.8*(1.15-hash(dir.xy+mod(t,10.))*.3);
}

float cor(vec2 p) {
	float ti=t*.02+200.;
    float d=length(p);
	float fad = exp(-4.*d) * (1. - smoothstep(.2, .1, d));
    float v1 = fad;
	float v2 = fad;
	float angle = atan( p.x, p.y )/3.1416;
	float dist = length(p)*.24;
	vec3 crd = vec3( angle, dist, ti * .1 );
    float ti2=ti+fad*.8*.24;
    float t1=abs(snois(crd+vec3(0.,-ti2*1.,ti2*.1),15.));
	float t2=abs(snois(crd+vec3(0.,-ti2*.5,ti2*.2),45.));	
    float it=6.;
 	for( int i=1; i<=int(it); i++ ){
		ti*=1.5;
        float pw = pow(1.5,float(i));
		v1+=snois(crd+vec3(0.,-ti,ti*.02),(pw*50.*(t1+1.)))/it*.13;
		v2+=snois(crd+vec3(0.,-ti,ti*.02),(pw*50.*(t2+1.)))/it*.13;
    }
	float co = pow(v1 * fad, 1.5); 
	co += pow(v2 * fad, 1.5)*.5;
	co *= 1. - t1 * .3 * (1. - fad * .3);
    return clamp(co * 6.2, 0., 1.)*(1.2-hash(p+mod(t,10.))*.3);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution.xy - .5;
    uv.x *= resolution.x/resolution.y;
    
    // Aplicar size al uv para controlar el tamaño del sol
    uv /= (size * 1.0 + 0.5);
    
    // Sol siempre en el centro
    vec3 sun_pos = vec3(0., 0., 0.);
    
    vec3 from = vec3(0., 0., -50.);
    vec3 dir = normalize(vec3(uv, 10.));
    
    // Color base rojo para el sol
    vec3 color_sun = vec3(1.0, 0.25, 0.15);
    vec3 col = vec3(0.);
    
    // Calcular el sol con textura fractal
    float s = sun(from + sun_pos, dir);
    s = pow(abs(s), contrastsun*3.);
    // Mezcla amarillo en el centro, rojo en los bordes
    vec3 sun_center = mix(vec3(1.0, 0.9, 0.3), color_sun, s * 0.5);
    col = vec3(s, s * s, s * s * s) * sun_center * 1.3;
    
    // Corona con el tamaño correcto y más intensa
    vec2 uv_corona = uv * .95;
    float c = cor(uv_corona);
    vec3 corona_color = mix(vec3(1.0, 0.8, 0.2), color_sun, c * 0.7);
    col = max(col, vec3(c, c * c, c * c * c) * corona_color * 1.5);
    
    // Rayos del sol con gradiente de color
    float at = atan(uv.y, uv.x);
    float atr = at*int(rayfreq*30.)-t*5.;
    float a = abs(.5-fract(at/3.14*5.))*length(uv)*.5*sin(atr);
    float dist_from_center = length(uv);
    vec3 colsun = mix(vec3(1.0, 0.85, 0.4), color_sun, smoothstep(0.1, 0.4, dist_from_center));
    colsun *= 1.-smoothstep(0.15, (rayamp*.5+.05)*5.+(noise(a*rayfreq*5.)-.5)*rayrandom*rayamp*5., length(uv*vec2(aspectratio,1.)));
    col += pow(pow(max(0.,1. - dist_from_center + a), 4.7) * .7 * colsun, vec3(2.));
    
    // Fondo completamente negro
    
	fragColor = vec4(col, 1.); 
}
