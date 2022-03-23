#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float scalex;
uniform float scaley;
uniform float fasex;
uniform float fasey;
uniform float noisex;
uniform float noisey;
vec2 scale(vec2 uv, float s);

float rad(vec2 uv);
float ang(vec2 uv);


float random (in vec2 _st);
float sm(float v1,float v2,float val){return smoothstep(v1,v2,val);}

float snoise2(vec2 v) {

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
    vec3 g = vec3(0.0);
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * vec2(x1.x,x2.x) + h.yz * vec2(x1.y,x2.y);
    return 130.0 * dot(m, g);
}

float ridge2(float h, float offset) {
    h = abs(h);     // create creases
    h = offset - h; // invert so creases are at top
    h = h * h;      // sharpen creases
    return h;
}
#define OCTAVES 8
float ridgedMF2(vec2 p) {
    float lacunarity = 2.0;
    float gain = 0.5;
    float offset = 0.9;

    float sum = 0.0;
    float freq = 1.0, amp = 0.5;
    float prev = 1.0;
    for(int i=0; i < OCTAVES; i++) {
        float n = ridge2(snoise2(p*freq), offset);
        sum += n*amp;
        sum += n*amp*prev;  // scale by previous octave
        prev = n;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}
void main(void){
    vec2 uv = gl_FragCoord.xy/resolution.xy;
    float fx = resolution.x/resolution.y;
    uv.x *= fx;

    vec2 p = vec2(0.5*fx,0.5) - uv;
    float r = length(p);
    float a = atan(p.x,p.y);

    vec3 color = vec3(0.0);

    float n = snoise(vec2(uv.x*100.*noisex,uv.y*100.*noisey+time*1)*0.002) ;
    int cnt = 3;
    for(int i=0; i<cnt; i++){
        float fas = i*PI*2./cnt;
        uv+=vec2(0.5);
        uv = scale(vec2(1.2))*uv;
        uv-=vec2(0.5);
    }
    uv/=cnt;

    float e = ridgedMF2(vec2(uv.x*0.5*uv.y,uv.y*0.5)
             *(ridgedMF2(vec2(uv.x*3.2,uv.y*10.0+time*0.04)))
             *ridgedMF2(vec2(uv.x*1,uv.y-time*0.002)*ridgedMF2(vec2(uv.x*0.5,uv.y*0.5-time*0.002))))
             *sin(uv.x*200.*scalex+fasex*4.0)*sin(uv.y*200.*scaley+fasey*4.0)
             ;

    vec3 col1 = vec3(0.1,0.1,0.4);
    vec3 col2 = vec3(1.9,0.8,1.-r);

    vec3 fin2 = vec3(smoothstep(0.9,0.5,e*0.0))*(1.-sm(0.0,0.0,r*e*8));
         fin2*=vec3(1.0,0.0,0.0);

    vec3 fin = mix(col1,col2,e);
         fin+=fin2;

    fragColor = vec4(fin,1.0);
}


