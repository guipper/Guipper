#pragma include "../common.frag"

// Sorry I changed the shader, I just really don't like the one original day 79.
// I've commented the old one if you would like to see it.

// So this is actually Day 110
//uniform sampler2D iChannel0;

uniform float fush1 ;

uniform float par1 ;
uniform float par2 ;
uniform float par3 ;
uniform float speed ;



#define tsp iTime + 1.1


#define pmod(p,z) mod(p,z) - 0.5*z
#define dmin(a,b) a.x < b.x ? a : b
#define tau (2.*pi)

#define rot(x) mat2(cos(x),-sin(x),sin(x),cos(x)) 
#define pal(a,b,c,d,e) ((a) + (b)*sin((c)*(d) + (e)))
vec3 glow = vec3(0);

vec3 path (float z){
    z *= 0.29;
	return vec3(0. + sin(z),0. + cos(z),0.)*1.;
}

float sdBox(vec3 p, vec3 s){
	p = abs(p) - s;
	return max(p.y, max(p.z,p.x));
}


float opSmoothUnion( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h); }

float opSmoothSubtraction( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); }

float opSmoothIntersection( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) + k*h*(1.0-h); }

vec2 map(vec3 p){
	vec2 d= vec2(10e7);
    
    vec3 k = p;
    p -= path(p.z);
    
    
    p.z *= 0.5;
    
    
    vec3 par = vec3(mapr(par1,0.5,5.0),mapr(par2,1.5,2.0),mapr(par3,1.5,2.0));
    vec4 q = vec4(p.xyz, 1.);
    
    
    for(float i = 0.; i < 7.;i++){
        q.xyz = abs(mod(q.xyz - par*0.5,par) - 0.5*par);
        
    	float dpp = dot(q.xyz, q.xyz);
        
        q.xy *= rot(0.5);
        q = q/dpp;
    }
    
    float dF = length(q.xz)/q.w;
    d.x = min(d.x, dF);
    
    d.x = opSmoothIntersection( d.x, -length(p.xy) + mapr(fush1,0.0,0.7), 0.4 );
    
    d.x -= 0.04;
    d.x = abs(d.x) + 0.003;
    glow -= 0.1/(0.001 + d.x*d.x*200.);
    
    
    
    float dL = length(q.zx)/q.w;

    d.x = max(d.x,  -length(p.xy) + 0.2 );
    
    d.x *= 0.45;
    
    
    return d;
}

float dith;

vec2 march(vec3 ro, vec3 rd, inout vec3 p, inout float t, inout bool hit){
	vec2 d = vec2(10e7);
    
    p = ro;; t = 0.; hit = false;
    
    for(float i = 0.; i < 150.; i++){
    	d = map(p)*dith;
        float eps = 0.001 + 0.001*5.0*pow(float(i)/200.0,2.0);
        if(d.x < eps){
            t += 0.005;
        }
        
    	t += d.x;
        p = ro + rd*t;
    }
    return d;
}

vec3 getRd(vec3 ro, vec3 lookAt, vec2 uv){
    vec3 dir = normalize(lookAt - ro);
    vec3 right = normalize(cross(vec3(0,1,0), dir));
    vec3 up = normalize(cross(dir, right));
    return normalize(dir + right*uv.x + up*uv.y);
}

vec3 getNormal(vec3 p){
	vec2 t = vec2(0.001, 0.);
    return -normalize(vec3(
        map(p - t.xyy).x - map(p + t.xyy).x,
        map(p - t.yxy).x - map(p + t.yxy).x,
        map(p - t.yyx).x - map(p + t.yyx).x
    ));
}



void main()
{
    vec2 uv = (gl_FragCoord.xy - 0.5*iResolution.xy)/iResolution.y;

    uv *= rot(sin(iTime*0.5*speed)*0.6);
    
    uv *= 1. - dot(uv,uv)*.1;
    
    //dith = mix(0.2,1.,texture(iChannel0,iResolution.xy*(uv + iTime*10.)/256.).x);
    //dith = mix(0.2,1.,texture(iChannel0,iResolution.xy*(uv + iTime*10.)/256.).x);
    dith = 1.0;
	
    vec3 col = vec3(0);

	vec3 ro = vec3(0);
    
    ro.z += iTime*1.7 *speed+ 0.1;
    
    ro += path(ro.z);
    
    vec3 lookAt = vec3(0);
    lookAt.z = ro.z + 2.;
    lookAt += path(lookAt.z + .5);
    
    vec3 rd = getRd(ro, lookAt,uv);
    
    
    vec3 p; float t; bool hit;
    vec2 d = march(ro, rd, p, t, hit);
    
    col += glow*0.001;
    
    vec3 fc = pal(0.6,0.5,vec3(1.,0.5,0.6) + cos(rd.xyz)*1., 1.7  - dot(uv,uv)*0.2,-2.1 - dot(uv,uv)*0.2);
    fc = max(fc,0.);
    col = mix(col,fc, smoothstep(0.,1.,t*0.1));
    
    fragColor = vec4(col,1.0);
}
