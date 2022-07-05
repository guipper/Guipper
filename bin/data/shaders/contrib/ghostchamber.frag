#pragma include "../common.frag"


uniform float speed;
// Created by Hepp Maccoy 2019, hepp@audiopixel.com | http://audiopixel.com
// Distance functions by Inigo Quilez, iquilezles.org
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License

vec3 glow;
float d1;
float t1;
vec3 b_8(vec3 c1, vec3 c2){
    return (c1 + c2) - (c1 * c2);
}

vec3 b_6(vec3 c1, vec3 c2){
    return (c1 + c2) - 1.0;
}

float soc(vec3 p) {
    vec3 n = normalize(sign(p+1e6));
    return min(min(dot(p.xy, n.xy), dot(p.yz, n.yz)), dot(p.xz, n.xz));
}

float sinc(float x, float k) {
    float a = PI * (float(k)*x-1.0);
    return sin(a)/a;
}

mat2 r2d(float a) {
    float sa=sin(a);
    float ca=cos(a);
    return mat2(ca,sa,-sa,ca);
}

vec2 mo(inout vec2 p, vec2 d) {
    vec2 q = p;
    q.x = abs(q.x) - d.x;
    q.y = abs(q.y) - d.y;
    if (q.y > q.x) q = q.yx;
    return q;
}

float map(vec3 p) {
    float d = 0.328;
    float a = abs(p.y);
    p.yz *= r2d(sign(a) * 5.6);
    p.xz *= r2d(sign(a) * d1 * 4.2);
    p.xz = mo(p.xz, vec2((-d1 * 29.8) - 1., -7.3676));
    p.zx = mo(p.xz, vec2(8.4443, 4.0344));
    p.xz = mo(p.xz, vec2(-12.9879, -9.1065));
    p.zx = mo(p.zx, vec2(5.4177, 2.3984));
    p.xz = max(abs(p.xz) - -8.7279, 10.023);
    p.xz = mo(p.xz, vec2((d1 * 2.) - 4., -12.7982));
    p.zx = mo(p.xz, vec2(-16.7502, -8.5611));
    p.z = mod(p.z, -16.381445)-(-16.381445 *.5);
    p.x = mod(p.x, 76.08105)-(76.08105 *.5);
    p.y = mod(p.y + t1 * 21.2, 28.46435) - 5.;
    p.zy = mo(p.zy, vec2(0.0, (d1 * 2.8)));
    p.xz = mo(p.xz, vec2(0.0, 14.1212));
    p.yx = mo(p.yx, vec2(2.3815, 0.5875));
    d = min(d, soc(max(abs(p) - 1.3103, -0.9418)));
    glow += vec3(0.8784314,0.8784314,0.8784314) * 0.0471 / (0.0539 + d*d);
    return (length(p * -0.6424) - -0.7392) * 0.1944 - (d * -1.939);
}

void main() {
    t1 = (iTime * .78*speed) + 166.;
    d1 = sin(t1 * .2);
    vec2 st = (gl_FragCoord.xy.xy / iResolution.xy) * 2.1 - 1.;
    st.x *= iResolution.x / iResolution.y;
    vec3 ro = vec3(st, 3.0);
    vec3 rd = normalize(vec3(st + vec2(0.), -0.35938));
    vec3 mp;
    mp = ro;
    float md;
    for(int i=0; i<50; i++) {
        md = map(mp);
        mp += (rd * 0.5921) * md;
    }
    float b = length(ro - mp);
    float dA = 1.6979 - (b * 0.1462) * -0.6102;
    float dB = 0.9117 - (b * 0.1348) * -1.998;
    dA = sinc(dA, 0.3252);
    dB = sinc(dB, 0.2127);
    float src1 = dA * -0.3094;
    float src2 = dB * -1.0;
    float src3 = dB * -1.0;
    float src4 = dA * 0.4847;
    vec3 c;
    src1 *= 2.0;
    src2 *= 0.9038;
    c = b_6((mix(vec3(0.,0.5882353,1.0), vec3(0.0), src1) * 1.0),(mix(vec3(0.0), vec3(1.0,0.0,0.9411765), src2) * 1.211));
    c = c * vec3(src3);
    c = b_6(c,vec3(src4));
    vec3 gt = c + (glow * 0.1);
    c = b_8(c,gt);
    fragColor = vec4(c, 1.);
}