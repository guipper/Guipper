#pragma include "../common.frag" // Esta línea tiene todas las definiciones de las funciones globales

uniform float nforce;
uniform float lineWidth;
uniform float globalSpeed;
uniform float masky;
uniform float noisex; //FUERZA NOISE X
uniform float noisey; //FUERZA NOISE X

#define BACKGROUNDCOLOR vec3(0.0,0.0,0.0)
#define GENERALCOLOR vec3(15.0, 0.0, 0.0) // Multiplicado por 10
#define INTERSECTIONCOLOR vec3(0.0,0.0,0.0) // Amarillo para las intersecciones
#define SF 1./min(iResolution.x,iResolution.y)
#define MOD3 vec3(.1031,.11369,.13787)

vec3 hash33(vec3 p3)
{
    p3 = fract(p3 * MOD3);
    p3 += dot(p3, p3.yxz+19.19);
    return -1.0 + 2.0 * fract(vec3((p3.x + p3.y)*p3.z, (p3.x+p3.z)*p3.y, (p3.y+p3.z)*p3.x));
}

float simplex_noise(vec3 p)
{
    const float K1 = 0.333333333;
    const float K2 = 0.166666667;

    vec3 i = floor(p + (p.x + p.y + p.z) * K1);
    vec3 d0 = p - (i - (i.x + i.y + i.z) * K2);

    vec3 e = step(vec3(0.0), d0 - d0.yzx);
    vec3 i1 = e * (1.0 - e.zxy);
    vec3 i2 = 1.0 - e.zxy * (1.0 - e);

    vec3 d1 = d0 - (i1 - 1.0 * K2);
    vec3 d2 = d0 - (i2 - 2.0 * K2);
    vec3 d3 = d0 - (1.0 - 3.0 * K2);

    vec4 h = max(0.6 - vec4(dot(d0, d0), dot(d1, d1), dot(d2, d2), dot(d3, d3)), 0.0);
    vec4 n = h * h * h * h * vec4(dot(d0, hash33(i)), dot(d1, hash33(i + i1)), dot(d2, hash33(i + i2)), dot(d3, hash33(i + 1.0)));

    return dot(vec4(31.316), n);
}

void main( )
{
    vec2 uv2 = gl_FragCoord.xy / iResolution.xy ;
    vec2 ouv = (gl_FragCoord.xy - iResolution.xy*.5)/iResolution.y;
    vec2 q = gl_FragCoord.xy / iResolution.xy;

    float COUNT = floor(iResolution.y / 50.);
    float wSize = 5.;

    float sf = SF * COUNT;

    vec2 uv = ouv * vec2(0.9, COUNT);
    vec2 gid = vec2(uv.x, floor(uv.y));
    vec2 guv = vec2(uv.x, fract(uv.y))  - vec2(0., .5);

    float g = 0.0; // Inicializar la transparencia en 0

    for(float i=-wSize; i<=wSize; i+=1.) {                
        vec2 iuv = guv + vec2(0.0,i);    
        vec2 iid = gid - vec2(0.0,i);  
        vec2 nuv = iid / vec2(0.4, COUNT/mapr(noisey,0.0,20.5));
		
        vec2 uv = iuv + simplex_noise(vec3(nuv*COUNT/mapr(noisex,1.0,30.0), iTime*globalSpeed))*wSize*nforce;
        float defx = (sin(uv2.x*10.+time*0.4+sin(uv.y*4.)*1.)*.5+.5)*.08;
        
        // Sumar la transparencia de forma aditiva
        g += smoothstep(abs(uv.y), mapr(lineWidth, 0.01, .15) + defx , sf*.9);
    }

    float msk = smoothstep(mapr(masky,0.0,0.994),0.995,uv2.y) ;
    
    vec3 col;
    // Si la suma de transparencia supera un cierto umbral, considerar que hay una intersección
    if (g > 0.99 * (wSize * 2.0 + 1.0)) {
        col = INTERSECTIONCOLOR;
    } else {
        col = mix(GENERALCOLOR, BACKGROUNDCOLOR, g / (wSize * 2.0 + 1.0)); // Normalizar el valor de transparencia
    }

    // Border dark
    col *= 0.2 + 0.8 * pow(32.0 * q.x * q.y * (1.0 - q.x) * (1.0 - q.y), 0.3);
    col *= msk;
    fragColor = vec4(col, 1.0);
}
