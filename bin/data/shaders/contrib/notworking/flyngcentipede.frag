#define iTime time
#define iFrame floor (iTime*60.)
#define iResolution resolution
#define iMouse mouse
#define texture(tex,uv) texture2DRect(tex,(uv)*resolution)
#define gl_FragCoord.xy gl_FragCoord.xy.xy
//#pragma include "../common.frag"
#define PI 3.14159265359
#define rot(a) mat2(cos(a + PI*0.5*vec4(0,1,3,0)))
#define Z min(iFrame, 0)

// how many segments there are
#define SEGMENTS 21
// only display the nth segment
//#define SEGMENT 0

#define COLOR_SHELL vec4(0.015, 0.01, 0.005, 0.3)
#define COLOR_SHELL2 vec4(0.15, 0.01, 0.001, 0.6)
#define COLOR_ORGAN vec4(0.3, 0.1, 0.01, 0.7)
#define COLOR_SUB vec3(0.5, 0.8, 0.9)

#define VR_SCALE 0.5

vec4 dummy = vec4(0.);

// iq's distance to a box
float sdBox( vec3 p, vec3 b ) {
    vec3 d = abs(p) - b;
    return length(max(d,0.0)) + min(max(d.x,max(d.y,d.z)),0.0);
}

// iq's distance to capped cylinder
float sdCappedCylinder( vec3 p, vec2 h ) {
    vec2 d = abs(vec2(length(p.xz),p.y)) - h;
    return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

// iq's smooth minimum
float smin( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h);
}

// iq's box filtered grid
float grid( in vec2 p ) {
    vec2 dpdx = dFdx(p);
    vec2 dpdy = dFdy(p);
    const float N = 15.0;
    p += 1.0/N*0.5;
    vec2 w = max(abs(dpdx), abs(dpdy));
    vec2 a = p + 0.5*w;                        
    vec2 b = p - 0.5*w;           
    vec2 i = (floor(a)+min(fract(a)*N,1.0)-
              floor(b)-min(fract(b)*N,1.0))/(N*w);
    return (1.0-i.x)*(1.0-i.y);
}

// dave hoskins hash
vec3 hash33( vec3 p3 ) {
	p3 = fract(p3 * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz+19.19);
    return fract((p3.xxy + p3.yxx)*p3.zyx);
}

// path parametric equation
vec3 path( float x ) {
    x += iTime*0.7;
    return vec3(cos(x), sin(x*2.3312)*0.3 + sin(x*1.456)*0.4, sin(x))*1.0;
}

// path derivative
vec3 pathd( float x ) {
    x += iTime*0.7;
    return vec3(-sin(x), cos(x*2.3312)*0.3*2.3312+cos(x*1.456)*0.4*1.456, cos(x))*1.0;
}

// path scale
float paths( float x ) {
    return 1.0 - cos(x*2.0)*0.3;
}

// one leg
float leg( vec3 p, const bool complex, out vec4 albedo, float parm, float phase, int end ) {
    
    float scaleP = paths(parm);
    
    // walk animation
    float xro = iTime*-3.0+phase*2.0*PI + parm*12.0;
    float sro = sin(xro);
    float cro = cos(xro);
    
    float ends = 1.0;
    if (end == 0) ends = 0.2;
    if (end == SEGMENTS-1) ends = 0.4;
    
    p = p.yxz;
    
    p.yz *= rot(sro*0.3*ends);
    p.yx *= rot(-0.2);
    p.y += 0.02;
    
    float de = length(p)-0.03;
    float s = 0.0;
    float scale = 1.0;
    float col = 0.0;
    
    float temp = (cro*-0.5+0.5)*0.5*ends;
    
    for (int i = Z ; i < 3 ; i++) {
    	
        // offset the leg segment
        p.y -= 0.04;
        p.yx *= rot( temp + 0.1*scale );
        p.y -= 0.04;
        
        // bend toward a point
        s += smoothstep(-0.04, 0.04, p.y)*scale;
        // find out the segment dimension
        vec2 dim = vec2(0.039 - 0.02*(scale *= 1.1) - 0.003*s, 0.04);
        // get distance
        float d = sdCappedCylinder( p, dim ) - 0.002;
        de = smin(de, d, 0.01);
        
        // colorize a bit
        if ( complex ) {
            float f = smoothstep(0.01, -0.01, d);
            float c = sin(p.y*60.0)*-0.5+0.5;
        	col += f*c;
        }
    }
    
    if ( complex ) {
        float f = max(0.2, 0.75-col*0.7);
        vec4 colOrg = mix(COLOR_ORGAN, COLOR_SHELL2, clamp((1.0-scaleP)*3.0+0.5, 0.0, 1.0));
        albedo = mix(colOrg, COLOR_SHELL, f);
    }
    
    return de;
    
}

// one body part with two legs, return organic/shell as vec2
vec2 body( vec3 p, const bool complex, out vec4 albedo, float parm, int index ) {
    
    float scaleP = paths(parm);
    p /= scaleP;
    
    // add a base body
    vec3 dim = vec3(0.04, 0.01, 0.05);
    dim += cos(p.zxx*vec3(48,38,30))*vec3(0.01, 0.005, 0.02);
   	float org = sdBox(p, dim) - 0.04;
    
    // shell above it
    vec3 inShell = p - vec3(0, 0.055, -0.01);
    inShell.yz *= rot(-0.15);
    inShell.yz -= cos(inShell.xx*vec2(20, 15))*0.02;
    inShell.x = abs(inShell.x) - cos(inShell.z*20.0+1.0)*0.01;
    float de = sdBox(inShell, vec3(0.08, 0, 0.09)) - 0.01;
    
    // legs
    vec3 inLe = p;
    float phase = step(p.x, 0.0)*0.5;
    inLe.x = abs(inLe.x);
    if (index == 0) {
        // recycle legs into antennas
        phase *= 0.4;
        inLe.xz *= rot(-1.2);
        inLe -= vec3(0.12, 0.02, 0.02);
        inLe.xy *= rot(-0.3);
        // add mandibles while we're here
        vec3 inMandi = p;
       	inMandi.x = abs(inMandi.x) - 0.04;
        inMandi.yz += vec2(0.03, -0.1);
        inMandi.xy *= rot(0.3);
        inMandi.yz *= rot(-0.3);
        float mandi = sdBox(inMandi, vec3(0.01, 0.03, 0.04)) - 0.025;
        de = min(de, mandi);
    } else if (index == SEGMENTS-1) {
        phase *= 0.2;
        inLe.xz *= rot(1.2);
        inLe -= vec3(0.1, 0.02, -0.01);
        inLe.xy *= rot(-0.3);
    } else {
        inLe.xy += vec2(-0.07, 0.02);
    }
    
    // add more stuff between the body and leg
    vec4 leAlbedo = vec4(0);
    float le = leg(inLe, complex, leAlbedo, parm, phase, index);
    float bum = length(inLe)-0.03;
    le = smin(le, bum, 0.03);
    
    // add bump mapping and coloring
    /*if ( complex ) {
        float tex1 = textureLod(iChannel0, inShell.xz*1.0 + float(index)*0.1424, 0.0).r;
        float tex2 = textureLod(iChannel0, inShell.xz*1.4 + float(index)*0.1424, 0.0).r;
        float she = smoothstep(0.0, 0.025, de+tex1*0.005);
        de += tex1*0.03;
        org -= tex2*0.03;
        vec4 colOrg = mix(COLOR_ORGAN, COLOR_SHELL2, clamp((1.0-(scaleP*scaleP))*3.0-0.5, 0.0, 1.0));
        float leAl = smoothstep(0.01, 0.0, le);
        colOrg = mix(colOrg, leAlbedo, leAl);
        vec4 baseColor = mix(COLOR_SHELL, colOrg, she);
        albedo = baseColor;
    }*/
    
    // then add the legs to the shell
    de = smin(de, le, 0.005);
    
    // return organic + shell to composite later
    return vec2(org, de) * scaleP;
}

// distance estimator
float de( vec3 p, const bool complex, out vec4 albedo ) {
    
    #ifdef SEGMENT
    vec2 seg = body(p, complex, albedo, 0.0, SEGMENT);
    return smin(seg.x, seg.y, 0.01);
    #endif
    
    // find the two closest points
    vec3[2] closestParmDistIndex = vec3[2](vec3(0., 99., 0.), vec3(0., 99., 0.));
    float currentParm = 0.0;
    for (int i = Z ; i < SEGMENTS ; i++) {
        vec3 current = path(currentParm);
        vec3 currentD = p - current;
        float currentDist = dot(currentD, currentD);
        if (currentDist < closestParmDistIndex[0].y) {
            closestParmDistIndex[1] = closestParmDistIndex[0];
            closestParmDistIndex[0] = vec3(currentParm,currentDist,float(i));
        } else if (currentDist < closestParmDistIndex[1].y) {
            closestParmDistIndex[1] = vec3(currentParm,currentDist,float(i));
        }
        currentParm -= 0.17 / length(pathd(currentParm)) * paths(currentParm);
    }
    
    float dOrg = 8.0;
    float dShe = 8.0;
    
    albedo = COLOR_SHELL;
    
    for (int i = Z; i < 2 ; i++) {
        float centerParm = closestParmDistIndex[i].x;
        int centerIndex = int(closestParmDistIndex[i].z);
        vec3 center = path(centerParm);
        
        // create basis
        vec3 upZ = path(centerParm + 0.3) - center;
        vec3 forward = normalize(path(centerParm + 0.1) - center);
        vec3 right = normalize(cross(forward, upZ));
        vec3 up = cross(forward, right);
        // offset and transform
        vec3 tp = p - center;
        tp = tp * mat3(right, up, forward);
		
        // get distance to the object
        vec4 albObj = vec4(0.);
        vec2 de = body(tp, complex, albObj, centerParm, centerIndex);
        float minDe = min(de.x, de.y);
        
        // composite organic/shell differently so shell/legs don't merge together
        dOrg = smin(dOrg, de.x, 0.07);
        dShe = min(dShe, de.y);
        
        if (complex) {
            albedo = mix(albedo, albObj, smoothstep(0.01, -0.01, minDe));
        }
    }
    
    float d = smin(dShe, dOrg, 0.02);
    
    return d;
}

// normal function, call de() in a for loop for faster compile times.
vec3 getNormal( vec3 p, float here ) {
    vec3 n = vec3(0);
    for (int i = Z ; i < 3 ; i++) {
        vec3 s = p;
        s[i] += 0.01;
        n[i] = de(s, true, dummy);
    }
    return normalize(n.xyz-here);
}

// PBR WORKFLOW BELOW

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 computeLighting(in vec3 normal, in vec3 viewDir,
                     in vec3 albedo, in float metallic, in float roughness,
                     in vec3 lightDir, in vec3 radiance) {
    
    vec3 result = vec3(0.);
    
    // find half way vector
    vec3 halfwayDir = normalize(viewDir + lightDir);
    
    // figure out surface reflection
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // find the PBR terms
    float NDF = DistributionGGX(normal, halfwayDir, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    vec3 F = fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    // Cook Torrance BRDF
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);  
    
    // add light contribution
    float NdotL = max(dot(normal, lightDir), 0.0);
    result += (kD * albedo / PI + specular) * radiance * NdotL;
    
    return result;
}

// PBR WORKFLOW ABOVE

// fake subsurface scattering
vec3 computeSSS(in vec3 normal, in vec3 viewDir, 
                in vec3 albedo, in float trans, in float index,
                in vec3 lightDir, in vec3 radiance) {
    float add = 1.0 - index;
    add *= add;
    add *= add;
    add *= add;
    add *= add;
    float fr = dot(viewDir, normal)*0.5+0.5;
    float lu = dot(viewDir, lightDir)*-0.5+0.5;
    add *= fr*fr;
    add *= lu;
    return radiance*add*1.0*trans*albedo;
}

// soft shadow function
float traceShadow( in vec3 from, in vec3 dir, in vec3 normal, const float sinTheta ) {
    if (dot(dir, normal) < 0.0) return 0.0;
    float minAlpha = 1.0;
    float totdist = 0.0;
    //#define SHADOW_STEPS 20
    for (int i = Z ; i < 20 ; i++) {
        vec3 p = from+dir*totdist;
        if (dot(p, p) > 6.0) return minAlpha;
        float dist = de(p, false, dummy);
        float rad = dist / (totdist*sinTheta);
        float alpha = rad * 0.5 + 0.5;
        if (alpha <= 0.0) {
            return 0.0;
        } else if (alpha < minAlpha) {
            minAlpha = alpha;
        }
        totdist += max(0.01, dist*0.8);
    }
    return minAlpha;
}

// get lighting here
vec3 getLighting( in vec3 p, in vec3 dir, float index ) {
    // get surface albedo and roughness
    vec4 albedo = vec4(0.);
    float d = de(p, true, albedo);
    // get surface normal
    vec3 n = getNormal(p, d);
    
    vec3 result = vec3(0.);
    const vec3 sunDir = normalize(vec3(1., 4., 2.));
    const vec3 subDir = normalize(vec3(2., -7., 3.));
    
    // add two lights, main one with shadows
    float shadow = traceShadow(p+n*0.01, sunDir, n, 0.05);
    result += computeLighting(n, dir, albedo.rgb, (1.0-albedo.a)*0.3, albedo.a, sunDir,
                              vec3(0.9, 0.85, 0.5)*10.0)*shadow;
    result += computeLighting(n, dir, albedo.rgb, (1.0-albedo.a)*0.3, albedo.a, subDir,
                              COLOR_SUB*0.5);
    // and add subsurface scattering
    result += computeSSS(n, dir, albedo.rgb, albedo.a, index, sunDir,
                         vec3(0.9, 0.85, 0.5)*10.0);
    result += computeSSS(n, dir, albedo.rgb, albedo.a, index, subDir,
                         COLOR_SUB*0.5);
    
    return result;
}

// get background
vec3 getBackground( in vec3 dir ) {
    // raytrace two plane
    dir.y = abs(dir.y);
    float d = 1.0 / dir.y;
    vec2 p = (dir*d).xz;
    // add a simple grid
    vec3 base = mix(COLOR_SUB*0.007, vec3(0.001), grid(p));
    // and a black fog over it
    base = mix(base, vec3(0.0002), 1.0-exp(-d*0.4));
    return base;
}

// return ray color for this position and direction
vec4 getColor( in vec2 gl_FragCoord.xy, in vec3 from, in vec3 dir ) {
    
    // get random stuff
    vec3 rnd = hash33(vec3(gl_FragCoord.xy, iFrame*2.+0.));
    
    // find the angular extent of this pixel
    vec3 ddir = normalize(dir+fwidth(dir)*0.5);
    float sinPix = length(cross(dir, ddir));
    // add some noise to the base distance as dithering
    float totdist = 0.2*rnd.x;
    
    // keep track of the closest position
    vec3 fullPos = vec3(0.);
    float fullAlpha = 0.0;
    int fullIndex = 0.;
    
    #define STEPS 100
    for (int i = Z ; i < STEPS ; i++) {
        vec3 p = from + dir*totdist;
        // found sky, break early
        if (dot(p, p) > 6.0) break;
        // find out the average alpha here
        float dist = de(p, false, dummy);
        float rad = dist / (totdist*sinPix);
        float alpha = rad * -0.5 + 0.5;
        
        // we have a covering sample, consider it
        if (alpha > 0.0 && alpha > fullAlpha) {
            fullPos = p;
            fullAlpha = alpha;
            fullIndex = i;
            // sample is fully opaque, break early
            if (alpha >= 1.0) break;
        }
            
        // move to next sample point
        totdist += max(0.0004, dist*0.8);
    }
    
    // if we found the surface add lighting
    if (fullAlpha > 0.0) {
        float index = float(fullIndex)/float(STEPS-1);
        fullAlpha = clamp(fullAlpha, 0.0, 1.0);
        return vec4(getLighting(fullPos, -dir, index), fullAlpha);
    }
    
    return vec4(0.0);
}

// vr entry point
void mainVR( out vec4 fragColor, in vec2 gl_FragCoord.xy, in vec3 fragRayOri, in vec3 fragRayDir ) {
    
    // get color and mix with background
    vec4 col = getColor(gl_FragCoord.xy, fragRayOri/VR_SCALE, fragRayDir);
    fragColor.rgb = mix(getBackground(fragRayDir), col.rgb, col.a);
    // tonemapping
    fragColor.rgb = fragColor.rgb / (fragColor.rgb + vec3(1.0));
    // gamma correction
    fragColor.rgb = pow(fragColor.rgb, vec3(1.0/2.2));
    // add noise
    vec3 rnd = hash33(vec3(gl_FragCoord.xy, iFrame*2.0+1.0));
    fragColor.rgb += (rnd-0.5)*0.08;
    
    fragColor.a = 1.0;
}

// main entry point, simulate vr input
void main() {
    
    vec2 uv = gl_FragCoord.xy - iResolution.xy * 0.5;
    uv /= iResolution.y;
    
    vec3 from = vec3(0, 0, -2.0);
    
    from = vec3(0, 0, -0.5);
   
    vec3 dir = normalize(vec3(uv, 1.0));
    
    vec2 r = vec2(iTime*0.1, 0.3);
    
    if (iMouse.z > 0.25) {
        r = iMouse.xy - iResolution.xy*0.5;
        r *= -0.01;
    }
    
    dir.yz *= rot(r.y+0.1);
    from.yz *= rot(r.y);
    dir.xz *= rot(r.x);
    from.xz *= rot(r.x);
    
    mainVR(fragColor, gl_FragCoord.xy, from*VR_SCALE, dir);
    
    // add some vignetting in non vr
    vec2 uvv = gl_FragCoord.xy / iResolution.xy * 2.0 - 1.0;
    float vigD = dot(uvv, uvv);
    fragColor.rgb = mix(fragColor.rgb, vec3(0), vigD*0.3);
    
}
