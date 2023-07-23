#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D imagen;

//PARAMETROS
uniform float rotacionLuz; //en grados
uniform float tamanioLuz;
uniform float brilloLuz;

float tamanioMarco = .95;
float det = .001, maxd = 30., g = 0.;

mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, s, -s, c);
}

float box(vec3 p, vec3 c) {
    vec3 b = abs(p) - c;
    return length(max(vec3(0.), b));
}

float de(vec3 p) {
    float x = tamanioMarco * 5.4;
    float y = tamanioMarco * 5.4;
    p.z -= pow(abs(p.x) / x, 30.);
    p.z -= pow(abs(p.y) / y, 30.);
    float d = box(p, vec3(x, y, 1.));
    return min(d, d) * .5;
}

vec3 normal(vec3 p) {
    vec2 e = vec2(0., det);
    return normalize(vec3(de(p + e.yxx), de(p + e.xyx), de(p + e.xxy)) - de(p));
}

vec3 march(vec3 from, vec3 dir) {
    vec3 p, col = vec3(0.);
    float d, td = 0.;
    for(int i = 0; i < 80; i++) {
        p = from + td * dir;
        d = de(p);
        if(d < det || td > maxd)
            break;
        td += d;
    }
    if(d < det) {
        p -= det * dir;
        vec3 n = normal(p);
        vec3 ldir = normalize(vec3(vec2(0., -1.) * rot(radians(mapr(rotacionLuz, 0.0, 360.0))), -1. * mapr(tamanioLuz, 0.0, 1.2)));
        vec3 ref = reflect(ldir, n);
        float spec = pow(max(0., dot(dir, ref)), 15.) * brilloLuz;
        //spec=smoothstep(0.9,1.,spec)*.5;

        col = vec3(spec) + .0;
    }
    vec3 img = texture(imagen, gl_FragCoord.xy / resolution).rgb;
    col += img;
    return col + g * vec3(1., 0.2, 1.) * 0.;
}

void main() {
    vec2 uv = (gl_FragCoord.xy - resolution.xy * .0) / resolution.xy - .5;

    vec3 from = vec3(0, 0, -11.);

    vec3 dir = normalize(vec3(uv, 1.));

    vec3 col = march(from, dir);

    // Output to screen
    fragColor = vec4(col, 1.0);
}

//08108888876
