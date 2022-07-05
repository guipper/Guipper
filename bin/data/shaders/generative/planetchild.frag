#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform bool alphabg;
uniform float semilla;
float hash(float p)
{
    p*=1234.5678;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}


#define fxrand (semilla)
#define hash1 hash(fxrand)
#define hash2 hash(fxrand+.1)
#define hash3 hash(fxrand+.2)
#define hash4 hash(fxrand+.3)
#define hash5 hash(fxrand+.4)
#define hash6 hash(fxrand+.5)
#define hash7 hash(fxrand+.6)
#define hash8 hash(fxrand+.7)
#define hash9 hash(fxrand+.8)
#define hash10 hash(fxrand+.83)
#define hash11 hash(fxrand+.84)
#define hash12 hash(fxrand+.85)

const float det = .001;
vec3 ldir = vec3(0.01, 0.01, -0.02);
float objid, objcol, coast;
const vec3 water_color = vec3(0., .4, .8);
const vec3 land_color1 = vec3(.6, 1., .5);
const vec3 land_color2 = vec3(.6, .2, .0);
const vec3 atmo_color = vec3(.4, .65, .9);
const vec3 cloud_color = vec3(1.3);


#define land_color1 hsv2rgb(vec3(-.2+hash6*.4,.5,1.))
#define land_color2 hsv2rgb(vec3(hash7*.25,.5,1.))
#define water_color hsv2rgb(vec3(hash8*.2+.5,.5,1.))
#define cloud_color hsv2rgb(vec3(hash9,.1,2.5))
mat2 rot(float a) {
	float s = sin(a), c = cos(a);
    return mat2(c, s, -s, c);
}

float kset(int it, vec3 p, vec3 q, float sc, float c) {
    p.xz *= rot(iTime * .2);
    p += q;
    p *= sc;
    if(it!=20) p.x+=p.y*3.;
    float l = 0., l2;
    for (int i = 0; i < it; i++) {
    	p = abs(p) / dot(p, p) - c;
		l += exp(-1. * abs(length(p) - l2));
	    l2 = length(p);
    }
    return l * .08;    
}

float clouds(vec3 p2, vec3 dir) {
		p2 -= .1 * dir;
    	p2.y *= 3.;
    	float cl1 = 0., cl2 = 0.;
        for (int i = 0; i < 15; i++) {
			p2 -= .05 * dir;
			
            cl1 += kset(20, p2, vec3(1.7, 3., .54), hash1*.5, .95);
            //cl2 += kset(18, p2, vec3(1.2, 1.7, 1.4), hash2*.5, .85);
        }    
        cl1 = pow(cl1 * .045, 10.);
        cl2 = pow(cl2 * .055, 15.);
		
		cl2 = smoothstep(0.65,0.9,cl2);
		cl1 = smoothstep(0.65,0.9,cl1);
		return cl1 - cl2;
}

float de(vec3 p) {
    float size = 1.;
    float surf1 = kset(7, p, vec3(.523, 1.547, .754)-hash3*.5, .3, .9);
    float surf2 = kset(9, p, vec3(.723, 1.247, .354)-hash4*.5, .2, .8) * .7;
    float surf3 = kset(10, p, vec3(1.723, .347, .754), .3, .6) * .5;
    //objcol = pow(surf1 + surf2 + surf3, 5.)*.5;
	float land = length(p) - 2. - surf1 * .8 - surf2 * .1 - size;
    float water = length(p) - 2.27 -size ;
    float d = min(land, water);
	objid = step(water, d) + step(land, d) * 2.;
	coast = max(0., .03 - abs(land - water)) / .03;
    d=water;
    return d * .8;
}

float de_clouds(vec3 p, vec3 dir) {
    return length(p)-clouds(p, dir)*.0;
}


vec3 normal(vec3 p) {
    vec3 eps = vec3(0., .01, 0.);
	return normalize(vec3(de(p + eps.yxx), de(p + eps.xyx), de(p + eps.xxy)) - de(p));
}

vec3 normal_clouds(vec3 p, vec3 dir) {
    vec3 eps = vec3(0., .05, 0.);
	vec3 n = normalize(vec3(de_clouds(p + eps.yxx, dir), de_clouds(p + eps.xyx, dir), de_clouds(p + eps.xxy, dir)) - de_clouds(p, dir));
	return n;
}

float shadow(vec3 desde) {
    ldir=normalize(ldir);
    float td=.1,sh=1.,d;
    for (int i=0; i<10; i++) {
		vec3 p=desde+ldir;
        d=de(p);
        td+=d;
		sh=min(sh,20.*d/td);
		if (sh<.001) break;
    }
    return clamp(sh,0.,1.);
}

vec3 color(float id, vec3 p) {
	vec3 c = vec3(0.);
    float k = smoothstep(.0, .7, kset(9, p, vec3(.63, .7, .54), .1, .8));
    vec3 land = land_color2; 
    vec3 water = water_color * (objcol + .5) + coast * .7; 
	float polar = pow(min(1.,abs(p.y * .4 + k * .3 - .1)),10.);
    land = mix(land, vec3(1.), polar);
	water = mix(water, vec3(1.5), polar);
    c += water * step(abs(id - 1.), .1);
    c += land * step(abs(id - 2.), .1) * 2.;
    return c;
}


vec3 shade(vec3 p, vec3 dir, vec3 n, vec3 col, float id) {
	ldir = normalize(ldir);
    float amb = .05;
    float sh = shadow(p);
    float dif = max(0., dot(ldir, n)) * .7 * sh;
    vec3 ref = reflect(ldir, n) * sh;
    float spe = pow(max(0., dot(ref, dir)), 10.) * .5 * (.3+step(abs(id - 1.), .1));
    return (amb + dif) * col + spe;
}

vec4 march(vec3 from, vec3 dir) {
	float td, d, g = 0.;
    vec3 c = vec3(0.), p;
    for (int i = 0; i < 60; i++) {
    	p = from + dir * td;
        d = de(p);
        td += d;
        if (td > 50. || d < det) break;
		g += smoothstep(-4.,1.,p.x);
    }
    if (d < det) {
    	p -= det * dir * 2.;
        vec3 col = color(objid, p);
        vec3 n = normal(p);
        c = shade(p, dir, n, col, objid);
        //cl1 = clamp(cl1, 0., 1.);
        float cl1 = clouds(p, dir);
		vec3 nc = normal_clouds(p, dir);
        c = mix(c, .1 + cloud_color * max(0., dot(normalize(ldir), nc)), clamp(cl1,0.,1.));
    }
    else
    {
		if(alphabg){
     return vec4(0.,0.,0.,1.);
		}else{
		     return vec4(0.,0.,0.,0.);
		}
     }
    g /= 60.;
    return vec4(c + (pow(g, 1.3) + pow(g,1.7) * .5) * .5,1.);
}


void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = (gl_FragCoord - iResolution.xy * .5) / iResolution.y;
	float a, b;
    vec3 from = vec3(0., a, -10.);
	vec3 dir = normalize(vec3(uv, min(1.1, iTime * .5)));
    vec4 col = march(from, dir);
    col.rgb*=.85;
    
    // Output to screen
    float x=uv.x;
    fragColor = vec4(col);
}