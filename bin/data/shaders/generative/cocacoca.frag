#pragma include "../common.frag"

uniform float sc;

vec3 mod2892(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod2892(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute2(vec3 x) { return mod2892(((x*34.0)+1.0)*x); }
float mapr2(float _value,float _low2,float _high2) {
	float val = _low2 + (_high2 - _low2) * (_value - 0.) / (1.0 - 0.);
    //float val = 0.1;
	return val;
}
float random5 (in vec2 _st) {
    return fract(sin(dot(_st.xy,
                         vec2(12.9898,78.233)))*
        43758.56222123);
}
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
    i = mod2892(i);
    vec3 p = permute2(
            permute2( i.y + vec3(0.0, i1.y, 1.0))
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
// Función para generar ruido
float noise2(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453);
}

float grid(vec2 _uv, float scg) {
    vec2 uv2 = _uv;
    uv2.x += snoise2(_uv*1.1 + time*0.01) * 0.5;
    uv2.y += snoise2(_uv*1.1 + time*0.01 + 150.1) * 0.5;
    vec2 p = vec2(0.5) - fract(uv2*scg);
    float r = length(p);
    float e = 1.0 - smoothstep(0.04, 0.04 + 0.30, r);
    return e * 0.5; // Reducir la intensidad del grid
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
	vec2 uvn = gl_FragCoord.xy / resolution;
	uvn.x*=resolution.x/resolution.y;
    float sc2 = mapr2(sc, 0.0, 20.0);

    // Generar ruido para crear el degradado de colores
    float n = noise(uv * 0.8 * sc2, time);
    float n2 = noise(uv * 0.5 * sc2, time + 150.);
    float n3 = noise(uv * 0.7 * sc2, time + 124512.);
    float n4 = noise(uv * 1.7 * sc2, time + 32551.);
    float n5 = noise(uv * 1. * sc2, time + 41563.);
    vec3 fondo = vec3(1.0);

    // Colores normalizados
    vec3 c1 = vec3(0.878, 0.408, 0.788);
    vec3 c2 = mix(vec3(0.463, 0.467, 0.855), vec3(0.388, 0.8, 0.973), 1.0 - n5);
    vec3 fin = mix(c2, c1, 1.0 - n);

    float gr = grid(uvn, 180.0);
    fin = mix(fondo, fin, 1.0 - smoothstep(0.1, 0.7, n3));
    fin += clamp(gr * n4 , 0.0, 0.2); // Reducir la contribución del grid al color final
	fin+= vec3(random5(uv*10.))*0.1;
	
	//fin = vec3(gr);
    fragColor = vec4(fin, 1.0); 
}
