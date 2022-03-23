#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// Code by Flopine
// Thanks to wsmind, leon, lsdlive, lamogui and XT95 for teaching me! :) <3

#define ITER 100.

vec2 moda (vec2 p, float per)
{
    float a = atan(p.y,p.x);
    float l = length(p);
    a = mod(a-per/2.,per)-per/2.;
    return vec2(cos(a),sin(a))*l;
}

vec2 mo (vec2 p, vec2 d)
{
    p.x = abs(p.x)-d.x;
    p.y = abs(p.y)-d.y;
    if (p.y>p.x) p.xy = p.yx;
    return p;
}

mat2 rot (float a)
{
	float c = cos(a);
    float s = sin(a);
    return mat2(c,s,-s,c);
}

float smin(float a, float b, float k) {
	float h = clamp(.5 + .5*(b - a) / k, 0., 1.);
	return mix(b, a, h) - k * h * (1. - h);
}

float stmin(float a, float b, float k, float n) {
	float s = k / n;
	float u = b - k;
	return min(min(a, b), .5 * (u + a + abs((mod(u - a + s, 2. * s)) - s)));
}

vec2 path(float t)
{
	float a = sin(t*.2 + 1.5), b = sin(t*.2);
	return vec2(a, a*b);
}

// iq's palette
vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 2.*3.141592*(c*t+d) );
}

float cyl (vec2 p, float r)
{
    return length(p)-r;
}

float od(vec3 p, float s) {
	return dot((p), normalize(sign(p))) - s;
}

float adn (vec3 p)
{
    p.xz *= rot(p.y*0.5+iTime);
    p.xz = moda(p.xz, 2.*3.141592/5.);

    p.x -= 2.;

    return cyl(p.xz,.3);
}

float prim1 (vec3 p, float per)
{
    float ad = adn(p);
    //p.y += iTime;
    p.y = mod(p.y-per/2.,per)-per/2.;
    return stmin(ad,od(p, 1.),0.7,4.);
}

float tunnel (vec3 p)
{
    p.yz *= rot(3.141592/2.);
    p.xz *= rot(p.y*0.3);
    p.xz  = moda(p.xz, 2.*3.141592/5.);
    p.x -= 6.;
    return prim1(p,2.5);
}

float map (vec3 p)
{
    p.xy += path(p.z);
    return tunnel(p);
}

vec3 camera(vec3 ro, vec2 uv, vec3 ta) {
	vec3 fwd = normalize(ta - ro);
	vec3 left = cross(vec3(0, 1, 0), fwd);
	vec3 up = cross(fwd, left);
	return normalize(fwd + uv.x*left + up*uv.y);
}

void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = 2.*(fragCoord/iResolution.xy)-1.;
	uv.x *= iResolution.x/iResolution.y;

    float dt = iTime * 3.;
	vec3 ro = vec3(0.001, 0.5, -9. + dt);
	vec3 ta = vec3(0, 0, dt);
    vec3 rd;

	ro.xy += path(ro.z);
	ta.xy += path(ta.z);
	rd = camera(ro, uv, ta);

    vec3 p;
    float t;
	float shad = 0.;

    for (float i=0.;i<ITER; i++)
    {
        p = ro+rd*t;
        float d = map(p);
        if (d<(2./iResolution.y)*(1./3.)*t)
        {
            shad = i/ITER;
            break;
        }
        t+=d*0.12;
    }

    // Time varying pixel color
    vec3 col = vec3(1.-shad);
    vec3 palette = pal(uv.y*0.6,
                      vec3(0.5),
                      vec3(0.5),
                      vec3(1.),
                      vec3(0.3,0.2,0.2));
 col = mix(col,palette*0.8, 1.-exp(-0.003*t*t));
    // Output to screen
    fragColor = vec4(pow(col,vec3(2.2)),1.0);
}