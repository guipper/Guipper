#pragma include "../common.frag"

#define STEPS 20.0
#define EPSILON 0.01
#define PI 3.14159265359
#define PIXELR 0.5/iResolution.x

#define FOG_COLOR vec3(0.55, 0.6, 0.75)
#define SUN_COLOR vec3(0.64, 0.62, 0.6)

//Hash method from https://www.shadertoy.com/view/4djSRW
#define HASHSCALE1 0.1031

// 3D noise function (IQ)
float noise(vec3 p){
	vec3 ip = floor(p);
    p -= ip;
    vec3 s = vec3(7.0,157.0,113.0);
    vec4 h = vec4(0.0, s.yz, s.y+s.z)+dot(ip, s);
    p = p*p*(3.0-2.0*p);
    h = mix(fract(sin(h)*43758.5), fract(sin(h+s.x)*43758.5), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z);
}

float dist(vec2 p){
    return noise(p.xyx)+(sin(p.x)*sin(p.y*2.0)+1.0);
}


//Based on Iq's terrain marching article
//https://iquilezles.org/articles/terrainmarching
float march(in vec3 ro, in vec3 rd, out bool hit){
    
    float delta = EPSILON;
    float lh = 0.0;
    float ly = 0.0;
    float t = 0.0;
    for(float i = EPSILON; i < STEPS; i += delta){
        vec3 p = ro+rd*i;
        float h = dist(p.xz);
        if(p.y < h){
            t = i-delta+delta*(lh-ly)/(p.y-ly-h+lh);
            hit = true;
            break;
        }
        delta = EPSILON*i;
        lh = h;
        ly = p.y;
    }
    
    return t;
}

vec3 normals(vec3 p){
    vec2 eps = vec2(PIXELR, 0.0);
    return normalize(vec3(
        dist(p.xz-eps.xy) - dist(p.xz+eps.xy),
        2.0*eps.x,
        dist(p.xz-eps.yx) - dist(p.xz+eps.yx)
    ));
}


//Ambient occlusion method from https://www.shadertoy.com/view/4sdGWN
//Random number [0:1] without sine from https://www.shadertoy.com/view/4djSRW
float hash(float p){
	vec3 p3  = fract(vec3(p) * HASHSCALE1);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 randomSphereDir(vec2 rnd){
	float s = rnd.x*PI*2.;
	float t = rnd.y*2.-1.;
	return vec3(sin(s), cos(s), t) / sqrt(1.0 + t * t);
}
vec3 randomHemisphereDir(vec3 dir, float i){
	vec3 v = randomSphereDir( vec2(hash(i+1.), hash(i+2.)) );
	return v * sign(dot(v, dir));
}

float ambientOcclusion( in vec3 p, in vec3 n, in float maxDist, in float falloff ){
	const int nbIte = 32;
    const float nbIteInv = 1./float(nbIte);
    const float rad = 1.-1.*nbIteInv; //Hemispherical factor (self occlusion correction)
    
	float ao = 0.0;
    
    for( int i=0; i<nbIte; i++ )
    {
        float l = hash(float(i))*maxDist;
        vec3 rd = normalize(n+randomHemisphereDir(n, l )*rad)*l; // mix direction with the normal
        													    // for self occlusion problems!
        
        ao += (l - max(dist( p.xz + rd.xz ),0.)) / maxDist * falloff;
    }
	
    return clamp( 1.-ao*nbIteInv, 0., 1.);
}


vec3 shade(vec3 p, vec3 rd, vec3 ld){
    vec3 n = normals(p);
    float lambertian = max(dot(n, ld), 0.0);
    
    vec3 color = vec3(0.5, 0.4, 0.65) * ambientOcclusion(p, n, 4.0, 2.0) +
        lambertian*vec3(0.6, 0.6, 0.65);
    
    return color;
}

//Fog introduced in https://iquilezles.org/articles/fog
vec3 fog(vec3 col, vec3 p, vec3 ro, vec3 rd, vec3 ld){
    float dist = length(p-ro);
	float sunAmount = max( dot( rd, ld ), 0.0 );
	float fogAmount = 1.0 - exp( -dist*0.28);
	vec3  fogColor = mix(FOG_COLOR, SUN_COLOR, pow(sunAmount, 2.0));
    return mix(col, fogColor, fogAmount);
}


void main()
{
	vec2 uv = fragCoord.xy / iResolution.xy;
	uv.y = 1.0-uv.y;
    vec2 q = -1.0+2.0*uv;
    q.x *= iResolution.x/iResolution.y;
    
    vec3 ro = vec3(0.0, 2.0, iTime*0.5+1.0);
    vec3 rt = vec3(0.0, 1.5, ro.z+10.0);
    
    vec3 z = normalize(rt-ro);
    vec3 x = normalize(cross(z, vec3(0.0, 1.0, 0.0)));
    vec3 y = normalize(cross(x, z));
    
    vec3 rd = normalize(mat3(x, y, z)*vec3(q, radians(70.0)));
    vec3 ld = (rt-vec3(0.5, 2.0, 0.0))/distance(rt, vec3(0.5, 2.0, 0.0));
    
    bool hit = false;
    float t = march(ro, rd, hit);
    vec3 p = ro+rd*t;
    
    vec3 color = FOG_COLOR;
    if(hit){
        color = shade(p, rd, ld);
    }
    
    color = fog(color, p, ro, rd, ld);
    color = smoothstep(-0.3, 1.0, color);
    
    color = pow(smoothstep(0.08, 1.1, color)*smoothstep(0.8, 0.005*0.799,
        distance(uv, vec2(0.5))*(0.8 + 0.005)), 1.0/vec3(2.2));
        
	fragColor = vec4(color,1.0);
}