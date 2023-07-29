#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;

uniform float force;
uniform float sm1;
uniform float sm2;
uniform float it;
const float max_rad = .05;
//const float it=80.;

mat2 rot(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, s, -s, c);
}
float hash(vec2 p) {
    p *= 1342.;
    vec3 p3 = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;

    vec4 t1 = texture(texture1, uv);
    vec2 uv2 = floor(uv * 10.) / 50.;
    mat2 spin = rot(2.39996);
    vec2 p = vec2(0., 1.);
    vec3 res = vec3(0.);
    float mit = mapr(it, 0.0, 30.0);
    float rad_step = max_rad / mit + hash(uv + time) * .001;
    float rad = 0.;
    float ti = mod(time, 10.);
    float vhs = step(.92, hash(uv.yy + ti)) * (1. + sin(uv.y * 5. + ti)) * step(.5, hash(uv.xy + ti)) * smoothstep(0., 1., uv.y);
    //vec4 col=texture(tx,uv+.05*step(.98,hash(uv2.yy+floor(time*10.)))*step(.5,hash(uv2.xx+time))*.7);
    //uv.y+=vhs*.3;

    vec4 col = texture(texture1, uv);
    for(float i = 0.; i < mit; i++) {
        rad += rad_step;
        p *= spin;
        vec4 col = texture(texture1, uv + p * rad);
        res += smoothstep(.6, .8, col.rgb) * 3.5;
    };
    res /= mit;

    res = smoothstep(sm1, sm2, res);
    vec3 fin = col.rgb + res * mapr(force, 0.0, 2.0);
    fragColor = vec4(fin.rgb, 1.0);
}
