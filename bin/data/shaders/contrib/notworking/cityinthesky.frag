#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

/**
	Wanted to try out rendering a city with a radial layout procedurally, and rendering clouds is always fun :)
	Technically there is just one box that is being rendered for the buildings, with different properties for each tile.
	The way I wrote the space mirroring could have been simpler and I realized that when I was finished with it, 
	so it is what it is :) Anyway it could be useful for other purposes as well. 
	
	In case you want to see how it looks, I have it in a unlisted shader https://www.shadertoy.com/view/MstfDj

	I tried to make the clouds as light weight as possible without sacrificing too much visual quality,
	so I ended up jittering the step size for each step randomly, which does decent job at removing the banding artefacts
	(though they are still visible but not horrible)

	if you haven't yet heard of the directional derivate go check it out 
	(http://www.iquilezles.org/www/articles/derivative/derivative.htm)
	it's a must for realtime volumetric rendering.


*/

#define PI 3.14159265358
#define TAU (PI*2.)
#define RESOLUTION 120.
#define C_RES 40.
#define CITY_RADIUS 49.

#define NOTHING 0.
#define ROAD_RADIAL 1.
#define ROAD_VERTICAL 2.
#define BUILDING 3.
#define FLOOR 4.
#define BOTTOM 5.
#define ROAD_CROSSROAD 6.

#define DEPTH_MAX 99999.
#define MISS vec3(1000000.)
#define CITY_SCALE .1
#define CLOUD_PLANE_HEIGHT 2.

mat3 rotx(float a) { mat3 rot; rot[0] = vec3(1.0, 0.0, 0.0); rot[1] = vec3(0.0, cos(a), -sin(a)); rot[2] = vec3(0.0, sin(a), cos(a)); return rot; }
mat3 roty(float a) { mat3 rot; rot[0] = vec3(cos(a), 0.0, sin(a)); rot[1] = vec3(0.0, 1.0, 0.0); rot[2] = vec3(-sin(a), 0.0, cos(a)); return rot; }
mat3 rotz(float a) { mat3 rot; rot[0] = vec3(cos(a), -sin(a), 0.0); rot[1] = vec3(sin(a), cos(a), 0.0); rot[2] = vec3(0.0, 0.0, 1.0); return rot; }
float rand(vec2 co) { return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }

vec3 cityOffset = vec3(.0);

// http://iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sdBox( vec3 p, vec3 b )
{
  vec3 d = abs(p) - b;
  return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

// http://iquilezles.org/www/articles/distfunctions/distfunctions.htm
float sdCappedCylinder( vec3 p, vec2 h )
{
  vec2 d = abs(vec2(length(p.xz),p.y)) - h;
  return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

vec4 tex3d( sampler2D tex, in vec3 p, in vec3 n )
{
    p *= 2.0;
    vec4 color = abs(n.z) * texture(tex, p.xy) + abs(n.x) * texture(tex, p.yz) + abs(n.y) * texture(tex, p.xz);
    return clamp(color/1.4, 0.0, 1.0);
}


vec3 traceSphere(in vec3 ro, in vec3 rd, float r, out float t1, out float t2)
{
    t1=t2=-1.0;
    vec3 x = ro + rd * (dot(normalize(-ro), rd)) * length(ro);
    float disc = r*r-dot(x, x);
    if (disc < 0.0) return MISS;
    disc=sqrt(disc);
    vec3 p=x-disc*rd;
    t1=length(p-ro);t2=t1+disc*2.;
    return p;
}

vec3 tracePlane(in vec3 o, in vec3 d, in vec3 n, float D)
{
    float a = dot(n, d);
    if(a > 0.0) return MISS;
    float t = ((-dot(n, o) + D) / a);
    return o + t * d;
}

float getCloud(in vec3 rp)
{
    rp.x += iTime * .6;
    float scl = .007;
    rp *= scl;
    float d = 0.;
    for (float i = 1.0; i <= 3.0; i = i + 1.0)
    {
        d += texture(iChannel3, rp * pow(2.0, i)).r * pow(0.5, i);
    }
    d = max(d - 0.1, 0.0); 
    d += smoothstep(0.01, -0.02, rp.y);
    d = clamp(d, 0.0, 1.0);
    return max(0., (d-.35))/.65 * smoothstep(scl * CLOUD_PLANE_HEIGHT, scl * CLOUD_PLANE_HEIGHT - scl * 1., rp.y) * 1.;
}


#define EPS 1.

void traceClouds(in vec3 rp, in vec3 rd, inout vec4 color, float depth)
{
    if(rd.y > 0.0)
    {
        return;
    }
    vec3 _ro = rp;
    bool hit = false;
    float rnd = rand(rp.xz);
    rp = tracePlane(rp, rd, vec3(0.0, 1.0, 0.0), CLOUD_PLANE_HEIGHT);
    
    vec3 ro = rp;
    vec3 ld = normalize(vec3(.0, -1.0, .0));
    float dens = 0.;
    vec4 clouds = vec4(.0);
    float dist = 0.;
    for (int i = 0; i < 64; ++i)
    {
        dens = getCloud(rp);
        
        // directional derivate for sun and sky coloring
        float toSun = clamp((getCloud(rp +ld*EPS) - dens) / EPS, 0., 1.0);
        float toSky = clamp((getCloud(rp + vec3(.0, -1.0, .0) * EPS) - dens) / EPS, 0., 1.0);
        
        // wrap the skylight around a bit
        float wrap = .6;
        toSky = (toSky + wrap) / (1.0 + wrap);
        
        // dark for dense parts, whiter for non-dense
        vec4 cloudCol = mix(vec4(1., 1., 1., dens), vec4(.0, .0, .0, dens), clamp(dens, 0.0, 1.0));
        // lighting from sun
        cloudCol.rgb = mix(cloudCol.rgb, vec3(.2, .3, .0), toSun);
        
        // sky 
        vec3 skyCol = vec3(.3, .5, 1.0);
        cloudCol.rgb += mix(vec3(.0), skyCol, toSky * .8);
        
        cloudCol.rgb = clamp(cloudCol.rgb, 0.0, 1.0);
        
        // mix colors
        clouds.rgb = mix(clouds.rgb, cloudCol.rgb, (1.0 - clouds.a) * cloudCol.a);
		
        // accumulate alpha
        clouds.a += max(0.001, (1.0 - clouds.a) * cloudCol.a);
        
        if(dist > 60.0) break;
        
        if(clouds.a > 1.) break;
        
        // step ray forward with a bit of jittering to get rid of banding
        // taking longer steps at distance
	    float rnd = rand(rp.xz);
        float stp = (.5 + rnd * rnd *.1) * log(2.+max(0.0, -20. + dist * 1.5));
        dist += stp;
        rp += rd  * stp;
        if (dot(_ro - rp, _ro - rp) > depth * depth) break;
        
    }
    color = mix(color, clouds, clouds.a);
}


vec4 uv2radial(in vec2 uv)
{
    float len = length(uv);
	// radius of cocenctric circle
    float radius = max(0.0001, floor(len * C_RES) / C_RES);
    float angle = PI + atan(uv.y, uv.x); // 0..2PI
    
    // amount of cells in circle
    float res = radius * RESOLUTION;
		
    // cell ID
    float cellId = (floor(angle * res / TAU)) / res;
    
    // coordinates in the cell
    // in range 0..1 for each axis.
   	float aMod = mod(angle, TAU / res) * res;
    return vec4(aMod * (1./TAU), mod(len, 1.0 / C_RES) * C_RES, cellId, radius);
}


vec2 getCellId(in vec4 radialData)
{
    float radialId = floor(radialData.z * 157.);
    float radiusId = mod(floor(radialData.w * C_RES), 4.);
    return vec2(radialId, radiusId);
}

vec2 cellIdFromPosition(in vec3 rp)
{
    vec2 uv = rp.xz*CITY_SCALE;
    vec4 radialUv = uv2radial(uv);
    return getCellId(radialUv);
}


float map(in vec3 rp)
{
    rp += cityOffset;
    if(rp.y < 0.) return rp.y;
    
    vec2 uv = rp.xz*CITY_SCALE;
    vec4 radialUv = uv2radial(uv);
    vec2 radId = getCellId(radialUv);
    
    if(dot(rp.xz, rp.xz) < CITY_RADIUS)
	    uv = radialUv.xy - .5;
    
    float id = rand(radialUv.zz);
    float h = smoothstep(.7, 0., radialUv.w);
    float buildingH = 1.5 * h * id + .2;
    float buildingW1 = .45 - id*.2;
    float buildingW2 = .45 - id*.3;
    
    if(radId.x == 0. || radId.y == 0.) { buildingH = 0.001; buildingW1 = .5;buildingW2 = .5; }
    float m = sdBox(vec3(uv, rp.y), vec3(buildingW1, buildingW2, buildingH ));
    
	return m;
}


vec3 grad(in vec3 rp)
{
    vec2 off = vec2(0.00001, 0.0);
    vec3 g = vec3(map(rp + off.xyy) - map(rp - off.xyy),
                  map(rp + off.yxy) - map(rp - off.yxy),
                  map(rp + off.yyx) - map(rp - off.yyx));
    return normalize(g);
}
    
mat3 lookat(vec3 from, vec3 to)
{
    vec3 f = normalize(to - from);
    vec3 r = normalize(cross(f, vec3(0.0, 1.0, 0.0)));
    vec3 u = cross(r, f);
    return mat3(r, u, f);
}


struct Hit
{
    float mat;
    float depth;
    vec3 normal;
    vec3 pos;
};

Hit noHit()
{
    Hit h;
    h.mat = NOTHING;
    h.depth = DEPTH_MAX;
    h.normal = vec3(0.0, 1.0, 0.0);
    h.pos = vec3(0.0);
    return h;
}

float mapShadow(in vec3 rp, in vec3 toLight)
{
    float s = 1.;
    float dst = .05;
    for (int i = 0; i < 20; ++i)
    {
    	rp += toLight * dst;
        s = min(s, map(rp) / dst);
    }
    return max(s, 0.);
}

float mapAO(in vec3 rp, in vec3 n)
{
    float res = 1.;
    float stp = 0.1;
    rp += n * stp;
    float dist = map(rp);
    res = clamp(dist/stp, 0.0, 1.0);
    return res;
}

float mapCityBottom(in vec3 rp)
{
    rp += cityOffset;
    float h = 1.8;
    float d =  sdCappedCylinder(rp + vec3(0.0, -.35+  h * 1.2, 0.0), vec2(7.4, h));
    return d;
}

vec3 gradCityBottom(in vec3 rp)
{
    vec2 off = vec2(0.001, 0.0);
    vec3 g = vec3(mapCityBottom(rp + off.xyy) - mapCityBottom(rp - off.xyy),
                  mapCityBottom(rp + off.yxy) - mapCityBottom(rp - off.yxy),
                  mapCityBottom(rp + off.yyx) - mapCityBottom(rp - off.yyx));
    return normalize(g);
}

void traceCityBottom(in vec3 rp, in vec3 rd, inout Hit prevHit)
{
    
    vec3 ro = rp;
    Hit hit = noHit();
    rp = tracePlane(rp, rd, vec3(0.0, 1.0, 0.0), -cityOffset.y);
    if (rp == MISS) return;
    
    float material = NOTHING;
    for (int i = 0; i < 20; ++i)
    {
        float dist = mapCityBottom(rp);
        rp += max(0.001, dist) * rd;
        if(dist < 0.0)
        {
            material = BOTTOM;
            break;
        }
    }
    
    if(material != NOTHING)
    {
        float depth = length(ro-rp);
        if(depth > prevHit.depth) return;
        
        hit.mat = material;
        hit.depth = depth;
        hit.pos = rp;
        hit.normal = gradCityBottom(rp);
        prevHit = hit;
    }
    
}

void traceCity(in vec3 rp, in vec3 rd, inout Hit prevHit)
{
    vec3 ro = rp;
    rp = tracePlane(rp, rd, vec3(0.0, 1.0, 0.0), 1. - cityOffset.y);
    if(rp == MISS) return;
    
    Hit hit = noHit();
    float dist = 0.;
    float travelled = 0.;
    float material = NOTHING;
    
    for (int i = 0; i < 200; ++i)
    {
        dist = map(rp);
        float stp = max(0.001, dist * .17);
        
        if(dist < 0.)
        {
            material = BUILDING;
            break;
        }
        
        travelled += stp;
        
        rp += rd * stp;
        
        if(rp.y < -cityOffset.y)
        {
            rp = tracePlane(ro, rd, vec3(0.0, 1.0, 0.0), -cityOffset.y);
            dist = 0.;
            material = FLOOR;
            break;
        }
        
    }
	
    if(dot(rp.xz + cityOffset.xz, rp.xz + cityOffset.xz) > CITY_RADIUS * 1.1) material = NOTHING;
    
    if(material != NOTHING)
    {
        
        if(dist != 0.0)
        {
            for (int i = 1; i < 5; ++i)
            {
                rp += rd * dist * pow(.5, float(i));
                dist = map(rp);
            }
        }
        
        float depth = length(ro-rp);
        if(depth > prevHit.depth) return;
        
        hit.normal = grad(rp);
	    hit.depth = depth;
        
        vec2 cellId = cellIdFromPosition(rp + cityOffset);
        
        if(cellId.x == 0.) material = ROAD_RADIAL;
        if(cellId.y == 0.) material = ROAD_VERTICAL;
        if(cellId.x + cellId.y == 0.) material = ROAD_CROSSROAD;
        
        
		hit.mat = material;
    	hit.pos = rp;
        prevHit = hit;
    }
}


struct Material
{
	float refl;
    vec3 albedo;
};

    
vec3 getRoadTexVertical(in vec3 rp)
{
    rp += cityOffset;
    vec2 uv = rp.xz*CITY_SCALE;
    vec4 radialUv = uv2radial(uv);
    vec3 col = vec3(.05);
    col.rgb += smoothstep(0.3, 0.5, abs(radialUv.y - .5));
    col.rg += smoothstep(0.08, 0., abs(radialUv.y - .5)) * step(0., radialUv.x - .5);
    return col;
}

vec3 getRoadTexCrossroad(in vec3 rp)
{
    rp += cityOffset;
    vec2 uv = rp.xz*CITY_SCALE;
    vec4 radialUv = uv2radial(uv);
    vec3 col = vec3(.05);
    col.rg += smoothstep(0.08, 0., abs(radialUv.x - .5)) * step(0., radialUv.y - .5);
    return col;
}

vec3 getRoadTexRadial(in vec3 rp)
{
    rp += cityOffset;
    vec2 uv = rp.xz*CITY_SCALE;
    vec4 radialUv = uv2radial(uv);
    vec3 col = vec3(.05);
    col.rgb += smoothstep(0.4, 0.5, abs(radialUv.x - .5));
    col.rg += smoothstep(0.08, 0., abs(radialUv.x - .5)) * step(0., radialUv.y - .5);
    return col;
}

Material getMaterial(in Hit hit)
{
    Material material;
	material.refl = 0.;
    
    vec2 uv = (hit.pos.xz + cityOffset.xz)*CITY_SCALE;
    vec4 radialUv = uv2radial(uv);
    
    if(hit.mat == FLOOR)
    {
        material.refl = .0;
		material.albedo = texture(iChannel2, hit.pos.xz + cityOffset.xz).rgb; 
    }
    
    if(hit.mat == ROAD_RADIAL) 
    {
        material.refl = .1;
        material.albedo = getRoadTexRadial(hit.pos);
    }

    if(hit.mat == ROAD_CROSSROAD) 
    {
        material.refl = .1;
        material.albedo = getRoadTexCrossroad(hit.pos);
    }
    
    if(hit.mat == ROAD_VERTICAL) 
    {
        material.refl = .1;
        material.albedo = getRoadTexVertical(hit.pos);
    }
    
    if(hit.mat ==  BUILDING)
    {
        vec3 col =  tex3d(iChannel0, hit.pos + cityOffset, hit.normal).rgb;
        vec3 final_col = vec3(.0);
        vec2 radId = getCellId(radialUv);
        float rnd = rand(radId);
		float idColor = clamp(.35 + rnd * .5, 0., 1.);
        
        float baseColorId = mod(floor(rnd * 10.), 3.);
        vec3 albedo = vec3(.1, .7, .7);
        if(baseColorId == 1.) albedo = vec3(.7, 0.9, 0.5);
        if(baseColorId == 2.) albedo = vec3(0.4, .3, .3);
        
        material.albedo = albedo * idColor;
        material.refl = smoothstep(0.5, 0.75, col.r) * max(0.0, dot(hit.normal.zx, hit.normal.zx)) + .1 ;
    }
    
    if(hit.mat ==  BOTTOM)
    { 
        material.albedo = tex3d(iChannel2, hit.pos+ cityOffset, hit.normal).rgb;
        material.refl =  0.;
    }
    
   return material;
}


void main()
{
	vec2 uv = ((fragCoord.xy - iResolution.xy*.5) / iResolution.xy)*vec2(1.0, iResolution.y/iResolution.x);
    vec2 im = 4. * ((iMouse.xy / iResolution.xy) - vec2(0.5));
    fragColor = vec4(0.0);
    
	// setup camera    
    vec3 rd = normalize(vec3(uv, 1.0));
    vec3 rp = vec3(0.0, .5, -17.0);
    vec3 _rp = rp;
    
    if(iMouse.z > 0.)
    {
        rp = roty(im.x) * rp;
        rp.y = (rotx(-.5 + im.y * .2) *_rp).y;
    }
    else
    {
        float T = iTime * .25;
        rp = roty(sin(T * .1)) * rp;
        rp.y = (-rotx(cos(T * .05) * .2 + .5) *_rp).y;
        rp.z -= sin(T * .4) * 10.;
    }
    
    vec3 ro = rp;
    rd = lookat(rp, vec3(0.0, -2.0, 0.0)) * rd;

	// draw city    
    cityOffset += vec3(sin(iTime * 1.) * .15, sin(iTime * .4) * .5, cos(iTime * .2) * .35);
	Hit data = noHit();
    traceCityBottom(ro, rd, data);
    traceCity(ro, rd, data);
    
    fragColor.rgb = textureLod(iChannel1, rd, 5.).rgb;
    
    
    if(data.mat != NOTHING)
    {
        vec3 ld = normalize(vec3(-1.0, 3.5, 1.0));
        float d = max(0.6, dot(ld, data.normal));
        Material mat = getMaterial(data);
       	vec3 col = mat.albedo * d;
       
        vec3 refl_col = clamp(texture(iChannel1, reflect(rd, data.normal)).rgb * mat.refl, 0.0, 15.0);
		col += refl_col;
        col *= .7 + .3 * mapShadow(data.pos + data.normal * .01, ld);
        col *= .7 + .3 * mapAO(data.pos, data.normal);
        fragColor.rgb = col;
    } 
    
    traceClouds(ro, rd, fragColor, data.depth);
	// contrast    
    fragColor = smoothstep(0.0, 1.0, fragColor);
    // gamma
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0 / 2.2));
    
}