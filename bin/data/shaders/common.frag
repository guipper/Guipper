#version 150

//UNIVERSAL UNIFORMS
uniform sampler2D feedback;
uniform float time;
uniform vec4 mouse;
uniform vec2 resolution;
uniform vec2 window_mouse;
//uniform sampler2DRect camara;
#define iTime time
#define iFrame floor (time*60.)
#define iResolution resolution
#define iMouse mouse
#define fragCoord gl_FragCoord.xy

vec3 verdejpupper(){return vec3(0.0,1.0,0.8);}

vec3 amarillo(){return vec3(1.0,1.0,0.0);}
vec3 cyan(){return vec3(0.0,1.0,1.0);}

vec3 verde(){return vec3(0.0,1.0,0.0);}
vec3 rojo(){return vec3(1.0,0.0,0.8);}
vec3 azul(){return vec3(0.0,0.0,1.0);}

vec3 luminanceVector = vec3(0.2125, 0.7154 , 0.0721);
#define pi 3.14159264359
float cir(vec2 uv,vec2 p, float s,float d);
vec4 manodraw(vec2 uv,vec2 p);
float def(vec2 uv,vec2 p1,vec2 p2,float f);
float mapr(float _value,float _low2,float _high2) {
	float val = _low2 + (_high2 - _low2) * (_value - 0.) / (1.0 - 0.);
    //float val = 0.1;
	return val;
}

vec2 scale(vec2 _uv, vec2 _scale){
	_uv-=vec2(0.5);
	//_uv = scale(_scale) *_uv;
	_uv+=vec2(0.5);
	return _uv;
}


mat2 scale(vec2 _scale){
    return mat2(_scale.x,0.0,
                0.0,_scale.y);
}
mat2 rotate2d(float _angle){
    return mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle));
}

vec3 lm(vec3 col, vec3 mx, vec3 dec){
	if(col.r > mx.r){
		col.r -=dec.r;
	}
	if(col.g > mx.g){
		col.g -=dec.g;
	}
	if(col.b > mx.b){
		col.b -=dec.b;
	}
	return col;
}

mat2 scale(vec2 _scale);
mat2 rotate2d(float _angle);
float atan2(float x,float y);
float random (in vec2 _st);
float cir(vec2 uv,vec2 p, float s, float d);
float noise (in vec2 st,float fase);
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod289(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute(vec3 x) { return mod289(((x*34.0)+1.0)*x); }
float snoise(vec2 v);
vec3 rgb2hsb( in vec3 c );
vec3 hsb2rgb( in vec3 c );
float poly(vec2 uv,vec2 p, float s, float d,int N,float a);
vec2 lerp(vec2 p1,vec2 p2,float f);

out vec4 fragColor;
#define PI 3.14159265359
#define TWO_PI 6.28318530718

vec2 lerp(vec2 p1,vec2 p2,float f){
	vec2 e = p1 * f + p2 *(1.-f);
	return e;
}
float def(vec2 uv,vec2 p1,vec2 p2,float f){

	float e=0.;

	float cant = 5;
	for (int i=1; i<cant; i++){
		vec2 mid = lerp(p1,p2,i/cant) -fract(uv);
		float r = length(mid);
		float a = atan(mid.x,mid.y);
		e+=sin(r*pi*20+time+f+sin(r*pi*2.+mid.x*pi*2.*sin(uv.y*pi*2.)+time*2.));

	}


	return abs(e*0.2);
}

vec4 manodraw(vec2 uv,vec2 p){

	//ESTE PARA TIRARLE
	vec2 p2 = p - uv;
	float r = length(p2);
	float a = atan(p2.x,p2.y);

	float size = 0.7;
	float em = sin(r*pi*100.+sin(a*5.+time+sin(r*40.+time*2.)*5.)*2.+time*5.);
	float r2 = cir(uv,p,0.0,0.15*size+em*0.05*size)*1.5;
	float g2 = cir(uv,p,0.0,0.1*size+em*0.005*size);
	float b2 = cir(uv,p,0.0,0.1*size+sin(em*50.*size)*0.01)*5.;

	return vec4(r2,g2,b2,1.0)*0.5;
}

float cir(vec2 uv,vec2 p,float s, float d){

	vec2 p2 = p - uv;
	float a = atan(p2.x,p2.y);
	float r = length(p2);

	float e = 1.-smoothstep(s,s+d,r);
	return e;
}

float poly(vec2 uv,vec2 p, float s, float dif,int N,float a){

    // Remap the space to -1. to 1.

    vec2 st = p - uv ;


    // Angle and radius from the current pixel
    float a2 = atan(st.x,st.y)+a;
    float r = TWO_PI/float(N);

    float d = cos(floor(.5+a2/r)*r-a2)*length(st);
    float e = 1.0 - smoothstep(s,dif,d);

    return e;
}


float atan2(float x,float y){
    if(x>0){
        return atan(y/x);
    } else if (x< 0 && y >= 0){
        return atan(y/x)+PI;
    } else if (x < 0 && y < 0){
        return atan(y/x)-PI;
    }else if(x == 0 && y > 0){
        return PI/2;
    } else if(x == 0 && y<0){
        return -PI/2;
    }else{
        return 0.0;
    }
}

float random2 (in vec2 _st,float _time) {
    return fract(sin(dot(floor(_st.xy),
                         vec2(12.9898,78.233)))*
        43000.3+_time);
}
float random2 (in vec2 _st) {
    return fract(sin(dot(floor(_st.xy),
                         vec2(12.9898,78.233)))*
        43000.3);
}

float random (in vec2 _st) {
    return fract(sin(dot(_st.xy,
                         vec2(12.9898,78.233)))*
        43758.56222123);
}

float sin2(float f){
    return sin(f)*0.5+0.5;
}

float cos2(float f){
    return cos(f)*0.5+0.5;
}

float noise (in vec2 st,float fase) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    float fase2 = fase;
    // Four corners in 2D of a tile
    float a = sin(random(i)*fase2);
    float b =  sin(random(i + vec2(1.0, 0.0))*fase2);
    float c =  sin(random(i + vec2(0.0, 1.0))*fase2);
    float d =  sin(random(i + vec2(1.0, 1.0))*fase2);

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    vec2 u = f*f*(3.0-2.0*f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

float noise (in vec2 _st) {
    vec2 i = floor(_st);
    vec2 f = fract(_st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}
float snoise(vec2 v) {

    // Precompute values for skewed triangular grid
    const vec4 C = vec4(0.211324865405187,
                        // (3.0-sqrt(3.0))/6.0
                        0.366025403784439,
                        // 0.5*(sqrt(3.0)-1.0)
                        -0.577350269189626,
                        // -1.0 + 2.0 * C.x
                        0.024390243902439);
                        // 1.0 / 41.0

    // First corner (x0)
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);

    // Other two corners (x1, x2)
    vec2 i1 = vec2(0.0);
    i1 = (x0.x > x0.y)? vec2(1.0, 0.0):vec2(0.0, 1.0);
    vec2 x1 = x0.xy + C.xx - i1;
    vec2 x2 = x0.xy + C.zz;

    // Do some permutations to avoid
    // truncation effects in permutation
    i = mod289(i);
    vec3 p = permute(
            permute( i.y + vec3(0.0, i1.y, 1.0))
                + i.x + vec3(0.0, i1.x, 1.0 ));

    vec3 m = max(0.5 - vec3(
                        dot(x0,x0),
                        dot(x1,x1),
                        dot(x2,x2)
                        ), 0.0);

    m = m*m ;
    m = m*m ;

    // Gradients:
    //  41 pts uniformly over a line, mapped onto a diamond
    //  The ring size 17*17 = 289 is close to a multiple
    //      of 41 (41*7 = 287)

    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;

    // Normalise gradients implicitly by scaling m
    // Approximation of: m *= inversesqrt(a0*a0 + h*h);
    m *= 1.79284291400159 - 0.85373472095314 * (a0*a0+h*h);

    // Compute final noise value at P
    vec3 g = vec3(1);
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * vec2(x1.x,x2.x) + h.yz * vec2(x1.y,x2.y) ;
    return 20.0 * dot(m, g);
}

vec3 rgb2hsb( in vec3 c ){
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz),
                 vec4(c.gb, K.xy),
                 step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r),
                 vec4(c.r, p.yzx),
                 step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)),
                d / (q.x + e),
                q.x);
}

//  Function from Iñigo Quiles
//  https://www.shadertoy.com/view/MsS3Wc
vec3 hsb2rgb( in vec3 c ){
    vec3 rgb = clamp(abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),
                             6.0)-3.0)-1.0,
                     0.0,
                     1.0 );
    rgb = rgb*rgb*(3.0-2.0*rgb);
    return c.z * mix(vec3(1.0), rgb, c.y);
}


float heart(vec2 st, vec2 translate, float radius, float smoothRange)
{
    vec2 uv = st - translate;

    // Two partially overlapping circles for the +ve y quadrants
    float top = step(0.0, uv.y) * (smoothstep(radius + 0.025, radius + 0.025 - smoothRange, length(abs(uv) - vec2(radius - 0.025,0.0))));

    // Two symmetric sin curves for the -ve y quadrants
    uv.x = abs(uv.x);

    float bottom = step(-PI, uv.y * PI) * step(0.0, -uv.y) *
        smoothstep(0.0, smoothRange, (radius * sin(uv.y * PI + PI / 2.0) - uv.x + 1.0 * radius));

    // Put them all together
    return top + bottom;
}
float heart2(in vec2 pt, in float radius) {
    float x = pt.x / radius * 0.75;
    float y = pt.y / radius;
    float r = pow(x, 2.0) + pow(y+0.5 - sqrt(abs(x)), 2.0);
    return smoothstep(r-0.4, r+0.4, 2.0);
}

float ridge(float h, float offset) {
    h = abs(h);     // create creases
    h = offset - h; // invert so creases are at top
    h = h * h;      // sharpen creases
    return h;
}
#define OCTAVES 8
float ridgedMF(vec2 p) {
    float lacunarity = 2.0;
    float gain = 0.5;
    float offset = 0.9;
    float sum = 0.0;
    float freq = 1.0, amp = 0.5;
    float prev = 1.0;
    for(int i=0; i < OCTAVES; i++) {
        float n = ridge(snoise(p*freq), offset);
				//float n = snoise(p*freq);
        sum += n*amp;
        sum += n*amp*prev;  // scale by previous octave
        prev = n;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum*.2;
}
float rxr(vec2 uv){
    float e = 0.;
    e = ridgedMF(vec2(ridgedMF(vec2(uv.x,uv.y))));
    return e;
}
vec2 rot(vec2 uv,float _angle){
	uv-=vec2(0.5);
	uv = rotate2d(_angle) * uv;
	uv+=vec2(0.5);
	return uv;
}



/*float random2(vec2 uv){
	return 0.0;
}*/
/*vec2 random2( vec2 p ) {
    return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}*/

//Este pasa como textura el circulo digamos
float voronoi(vec2 uv,float circle){
    // Scale
    // uv *= 10.;

    // Tile the space
    vec2 i_st = floor(uv);
    vec2 f_st = fract(uv);


    //float e2 = cir(uv,vec2(0.5),0.0,0.1);
    float m_dist = 0.9 - circle*0.9;  // minimun distance
    vec2 m_point ;        // minimum point

    for (int j=-1; j<=1; j++ ) {
        for (int i=-1; i<=1; i++ ) {
            vec2 neighbor = vec2(float(i),float(j));
            vec2 point = vec2(random2(i_st + neighbor));
            point = sin(6.2831*point+time)*0.1+0.5;
            vec2 diff = neighbor + point - f_st;
            float dist = length(diff);

            if( dist < m_dist ) {
                m_dist = dist;
                m_point = point;
            }
        }
    }

    float e = dot(m_point,vec2(0.7,0.7));
   //e+=abs(sin(uv.x+time*5)*sin(uv.y+time*5))*0.2;
    return e;
}
float fbm (in vec2 uv) {
    // Initial values
    float value = 0.5;
    float amplitude = 0.5;
    float frequency = 0.;
    vec2 shift = vec2(100);
    mat2 rot2 = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));
    // Loop of octaves
    for (int i = 0; i < 16; i++) {
        value += amplitude * noise(uv,time);
        uv = rot2 * uv * 2.0 + shift;
        amplitude *= .5;
    }
    return value;
}
float fbm (in vec2 uv,in float _time) {
    // Initial values
    float value = 0.5;
    float amplitude = 0.5;
    float frequency = 0.;
    vec2 shift = vec2(100);
    mat2 rot2 = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));
    // Loop of octaves
    for (int i = 0; i < 16; i++) {
        value += amplitude * noise(uv,_time);
        uv = rot2 * uv * 2.0 + shift;
        amplitude *= .5;
    }
    return value;
}


/*************************************************/

// TODAS LAS FUNCIONES DE BLENDING SACADAS DE  https://github.com/jamieowen/glsl-blend


///SOFT LIGHT
#define ADD 1 
#define	AVERAGE 2
#define	COLOR_BURN 3
#define	COLOR_DODGE 4
#define	DARKEN 5
#define	DIFFERENCE 6
#define	EXCLUSION 7
#define	GLOW 8
#define	HARD_LIGHT 9
#define	HARD_MIX 10
#define	LIGHTEN 11
#define	LINEAR_BURN 12
#define	LINEAR_DODGE 13
#define	LINEAR_LIGHT 14
#define	MULTIPLY 15
#define	NEGATION 16
#define	NORMAL 17
#define	OVERLAY 18
#define	PHOENIX 19
#define	PIN_LIGHT 20
#define	REFLECT 21
#define	SCREEN 22
#define	SOFT_LIGHT 23
#define	SUBTRACT 24
#define	VIVID_LIGHT 25


float blendSoftLight(float base, float blend) {
	return (blend<0.5)?(2.0*base*blend+base*base*(1.0-2.0*blend)):(sqrt(base)*(2.0*blend-1.0)+2.0*base*(1.0-blend));
}

vec3 blendSoftLight(vec3 base, vec3 blend) {
	return vec3(blendSoftLight(base.r,blend.r),blendSoftLight(base.g,blend.g),blendSoftLight(base.b,blend.b));
}

vec3 blendSoftLight(vec3 base, vec3 blend, float opacity) {
	return (blendSoftLight(base, blend) * opacity + base * (1.0 - opacity));
}

//ADD : 
float blendAdd(float base, float blend) {
	return min(base+blend,1.0);
}

vec3 blendAdd(vec3 base, vec3 blend) {
	return min(base+blend,vec3(1.0));
}

vec3 blendAdd(vec3 base, vec3 blend, float opacity) {
	return (blendAdd(base, blend) * opacity + base * (1.0 - opacity));
}


vec3 blendAverage(vec3 base, vec3 blend) {
	return (base+blend)/2.0;
}

vec3 blendAverage(vec3 base, vec3 blend, float opacity) {
	return (blendAverage(base, blend) * opacity + base * (1.0 - opacity));
}
  
float blendColorBurn(float base, float blend) {
	return (blend==0.0)?blend:max((1.0-((1.0-base)/blend)),0.0);
}

vec3 blendColorBurn(vec3 base, vec3 blend) {
	return vec3(blendColorBurn(base.r,blend.r),blendColorBurn(base.g,blend.g),blendColorBurn(base.b,blend.b));
}

vec3 blendColorBurn(vec3 base, vec3 blend, float opacity) {
	return (blendColorBurn(base, blend) * opacity + base * (1.0 - opacity));
}
float blendColorDodge(float base, float blend) {
	return (blend==1.0)?blend:min(base/(1.0-blend),1.0);
}

vec3 blendColorDodge(vec3 base, vec3 blend) {
	return vec3(blendColorDodge(base.r,blend.r),blendColorDodge(base.g,blend.g),blendColorDodge(base.b,blend.b));
}

vec3 blendColorDodge(vec3 base, vec3 blend, float opacity) {
	return (blendColorDodge(base, blend) * opacity + base * (1.0 - opacity));
}

float blendDarken(float base, float blend) {
	return min(blend,base);
}

vec3 blendDarken(vec3 base, vec3 blend) {
	return vec3(blendDarken(base.r,blend.r),blendDarken(base.g,blend.g),blendDarken(base.b,blend.b));
}

vec3 blendDarken(vec3 base, vec3 blend, float opacity) {
	return (blendDarken(base, blend) * opacity + base * (1.0 - opacity));
}


vec3 blendDifference(vec3 base, vec3 blend) {
	return abs(base-blend);
}

vec3 blendDifference(vec3 base, vec3 blend, float opacity) {
	return (blendDifference(base, blend) * opacity + base * (1.0 - opacity));
}


vec3 blendExclusion(vec3 base, vec3 blend) {
	return base+blend-2.0*base*blend;
}

vec3 blendExclusion(vec3 base, vec3 blend, float opacity) {
	return (blendExclusion(base, blend) * opacity + base * (1.0 - opacity));
}

float blendLighten(float base, float blend) {
	return max(blend,base);
}

vec3 blendLighten(vec3 base, vec3 blend) {
	return vec3(blendLighten(base.r,blend.r),blendLighten(base.g,blend.g),blendLighten(base.b,blend.b));
}

vec3 blendLighten(vec3 base, vec3 blend, float opacity) {
	return (blendLighten(base, blend) * opacity + base * (1.0 - opacity));
}
float blendLinearBurn(float base, float blend) {
	// Note : Same implementation as BlendSubtractf
	return max(base+blend-1.0,0.0);
}

vec3 blendLinearBurn(vec3 base, vec3 blend) {
	// Note : Same implementation as BlendSubtract
	return max(base+blend-vec3(1.0),vec3(0.0));
}

vec3 blendLinearBurn(vec3 base, vec3 blend, float opacity) {
	return (blendLinearBurn(base, blend) * opacity + base * (1.0 - opacity));
}
float blendLinearDodge(float base, float blend) {
	// Note : Same implementation as BlendAddf
	return min(base+blend,1.0);
}

vec3 blendLinearDodge(vec3 base, vec3 blend) {
	// Note : Same implementation as BlendAdd
	return min(base+blend,vec3(1.0));
}

vec3 blendLinearDodge(vec3 base, vec3 blend, float opacity) {
	return (blendLinearDodge(base, blend) * opacity + base * (1.0 - opacity));
}
float blendLinearLight(float base, float blend) {
	return blend<0.5?blendLinearBurn(base,(2.0*blend)):blendLinearDodge(base,(2.0*(blend-0.5)));
}

vec3 blendLinearLight(vec3 base, vec3 blend) {
	return vec3(blendLinearLight(base.r,blend.r),blendLinearLight(base.g,blend.g),blendLinearLight(base.b,blend.b));
}

vec3 blendLinearLight(vec3 base, vec3 blend, float opacity) {
	return (blendLinearLight(base, blend) * opacity + base * (1.0 - opacity));
}
vec3 blendMultiply(vec3 base, vec3 blend) {
	return base*blend;
}

vec3 blendMultiply(vec3 base, vec3 blend, float opacity) {
	return (blendMultiply(base, blend) * opacity + base * (1.0 - opacity));
}


vec3 blendNegation(vec3 base, vec3 blend) {
	return vec3(1.0)-abs(vec3(1.0)-base-blend);
}

vec3 blendNegation(vec3 base, vec3 blend, float opacity) {
	return (blendNegation(base, blend) * opacity + base * (1.0 - opacity));
}

vec3 blendNormal(vec3 base, vec3 blend) {
	return blend;
}

vec3 blendNormal(vec3 base, vec3 blend, float opacity) {
	return (blendNormal(base, blend) * opacity + base * (1.0 - opacity));
}

float blendOverlay(float base, float blend) {
	return base<0.5?(2.0*base*blend):(1.0-2.0*(1.0-base)*(1.0-blend));
}

vec3 blendOverlay(vec3 base, vec3 blend) {
	return vec3(blendOverlay(base.r,blend.r),blendOverlay(base.g,blend.g),blendOverlay(base.b,blend.b));
}

vec3 blendOverlay(vec3 base, vec3 blend, float opacity) {
	return (blendOverlay(base, blend) * opacity + base * (1.0 - opacity));
}
vec3 blendPhoenix(vec3 base, vec3 blend) {
	return min(base,blend)-max(base,blend)+vec3(1.0);
}

vec3 blendPhoenix(vec3 base, vec3 blend, float opacity) {
	return (blendPhoenix(base, blend) * opacity + base * (1.0 - opacity));
}
float blendPinLight(float base, float blend) {
	return (blend<0.5)?blendDarken(base,(2.0*blend)):blendLighten(base,(2.0*(blend-0.5)));
}

vec3 blendPinLight(vec3 base, vec3 blend) {
	return vec3(blendPinLight(base.r,blend.r),blendPinLight(base.g,blend.g),blendPinLight(base.b,blend.b));
}

vec3 blendPinLight(vec3 base, vec3 blend, float opacity) {
	return (blendPinLight(base, blend) * opacity + base * (1.0 - opacity));
}

float blendReflect(float base, float blend) {
	return (blend==1.0)?blend:min(base*base/(1.0-blend),1.0);
}

vec3 blendReflect(vec3 base, vec3 blend) {
	return vec3(blendReflect(base.r,blend.r),blendReflect(base.g,blend.g),blendReflect(base.b,blend.b));
}

vec3 blendReflect(vec3 base, vec3 blend, float opacity) {
	return (blendReflect(base, blend) * opacity + base * (1.0 - opacity));
}
  
float blendScreen(float base, float blend) {
	return 1.0-((1.0-base)*(1.0-blend));
}

vec3 blendScreen(vec3 base, vec3 blend) {
	return vec3(blendScreen(base.r,blend.r),blendScreen(base.g,blend.g),blendScreen(base.b,blend.b));
}

vec3 blendScreen(vec3 base, vec3 blend, float opacity) {
	return (blendScreen(base, blend) * opacity + base * (1.0 - opacity));
}
float blendSubstract(float base, float blend) {
	return max(base+blend-1.0,0.0);
}

vec3 blendSubstract(vec3 base, vec3 blend) {
	return max(base+blend-vec3(1.0),vec3(0.0));
}

vec3 blendSubstract(vec3 base, vec3 blend, float opacity) {
	return (blendSubstract(base, blend) * opacity + blend * (1.0 - opacity));
}
float blendVividLight(float base, float blend) {
	return (blend<0.5)?blendColorBurn(base,(2.0*blend)):blendColorDodge(base,(2.0*(blend-0.5)));
}

vec3 blendVividLight(vec3 base, vec3 blend) {
	return vec3(blendVividLight(base.r,blend.r),blendVividLight(base.g,blend.g),blendVividLight(base.b,blend.b));
}

vec3 blendVividLight(vec3 base, vec3 blend, float opacity) {
	return (blendVividLight(base, blend) * opacity + base * (1.0 - opacity));
}
vec3 blendHardLight(vec3 base, vec3 blend) {
	return blendOverlay(blend,base);
}

vec3 blendHardLight(vec3 base, vec3 blend, float opacity) {
	return (blendHardLight(base, blend) * opacity + base * (1.0 - opacity));
}
vec3 blendGlow(vec3 base, vec3 blend) {
	return blendReflect(blend,base);
}

vec3 blendGlow(vec3 base, vec3 blend, float opacity) {
	return (blendGlow(base, blend) * opacity + base * (1.0 - opacity));
}

float blendHardMix(float base, float blend) {
	return (blendVividLight(base,blend)<0.5)?0.0:1.0;
}

vec3 blendHardMix(vec3 base, vec3 blend) {
	return vec3(blendHardMix(base.r,blend.r),blendHardMix(base.g,blend.g),blendHardMix(base.b,blend.b));
}

vec3 blendHardMix(vec3 base, vec3 blend, float opacity) {
	return (blendHardMix(base, blend) * opacity + base * (1.0 - opacity));
}


vec3 blendMode( int mode, vec3 base, vec3 blend ){
	if( mode == 1 ){
		return blendAdd( base, blend );
	}else
	if( mode == 2 ){
		return blendAverage( base, blend );
	}else
	if( mode == 3 ){
		return blendColorBurn( base, blend );
	}else
	if( mode == 4 ){
		return blendColorDodge( base, blend );
	}else
	if( mode == 5 ){
		return blendDarken( base, blend );
	}else
	if( mode == 6 ){
		return blendDifference( base, blend );
	}else
	if( mode == 7 ){
		return blendExclusion( base, blend );
	}else
	if( mode == 8 ){
		return blendGlow( base, blend );
	}else
	if( mode == 9 ){
		return blendHardLight( base, blend );
	}else
	if( mode == 10 ){
		return blendHardMix( base, blend );
	}else
	if( mode == 11 ){
		return blendLighten( base, blend );
	}else
	if( mode == 12 ){
		return blendLinearBurn( base, blend );
	}else
	if( mode == 13 ){
		return blendLinearDodge( base, blend );
	}else
	if( mode == 14 ){
		return blendLinearLight( base, blend );
	}else
	if( mode == 15 ){
		return blendMultiply( base, blend );
	}else
	if( mode == 16 ){
		return blendNegation( base, blend );
	}else
	if( mode == 17 ){
		return blendNormal( base, blend );
	}else
	if( mode == 18 ){
		return blendOverlay( base, blend );
	}else
	if( mode == 19 ){
		return blendPhoenix( base, blend );
	}else
	if( mode == 20 ){
		return blendPinLight( base, blend );
	}else
	if( mode == 21 ){
		return blendReflect( base, blend );
	}else
	if( mode == 22 ){
		return blendScreen( base, blend );
	}else
	if( mode == 23 ){
		return blendSoftLight( base, blend );
	}else
	if( mode == 24 ){
		return blendSubstract( base, blend );
	}else
	if( mode == 25 ){
		return blendVividLight( base, blend );
	}
	return vec3(0.0);
}
vec3 blendMode( int mode, vec3 base, vec3 blend ,float opacity){
	if( mode == 1 ){
		return blendAdd( base, blend ,opacity);
	}else
	if( mode == 2 ){
		return blendAverage( base, blend ,opacity);
	}else
	if( mode == 3 ){
		return blendColorBurn( base, blend ,opacity);
	}else
	if( mode == 4 ){
		return blendColorDodge( base, blend ,opacity);
	}else
	if( mode == 5 ){
		return blendDarken( base, blend ,opacity);
	}else
	if( mode == 6 ){
		return blendDifference( base, blend ,opacity);
	}else
	if( mode == 7 ){
		return blendExclusion( base, blend,opacity );
	}else
	if( mode == 8 ){
		return blendGlow( base, blend ,opacity);
	}else
	if( mode == 9 ){
		return blendHardLight( base, blend ,opacity);
	}else
	if( mode == 10 ){
		return blendHardMix( base, blend,opacity );
	}else
	if( mode == 11 ){
		return blendLighten( base, blend ,opacity);
	}else
	if( mode == 12 ){
		return blendLinearBurn( base, blend ,opacity);
	}else
	if( mode == 13 ){
		return blendLinearDodge( base, blend,opacity );
	}else
	if( mode == 14 ){
		return blendLinearLight( base, blend ,opacity);
	}else
	if( mode == 15 ){
		return blendMultiply( base, blend ,opacity);
	}else
	if( mode == 16 ){
		return blendNegation( base, blend ,opacity);
	}else
	if( mode == 17 ){
		return blendNormal( base, blend ,opacity);
	}else
	if( mode == 18 ){
		return blendOverlay( base, blend ,opacity);
	}else
	if( mode == 19 ){
		return blendPhoenix( base, blend ,opacity);
	}else
	if( mode == 20 ){
		return blendPinLight( base, blend ,opacity);
	}else
	if( mode == 21 ){
		return blendReflect( base, blend ,opacity);
	}else
	if( mode == 22 ){
		return blendScreen( base, blend ,opacity);
	}else
	if( mode == 23 ){
		return blendSoftLight( base, blend ,opacity);
	}else
	if( mode == 24 ){
		return blendSubstract( base, blend ,opacity);
	}else
	if( mode == 25 ){
		return blendVividLight( base, blend ,opacity);
	}
	return vec3(0.0);
}




