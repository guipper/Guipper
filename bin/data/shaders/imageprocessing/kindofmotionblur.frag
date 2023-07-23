#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float blur_mix;
uniform float blur_size;

void main() {
    float size = 1. + pow(2., floor(1. + blur_size * 2.));
    float it = floor(size * size);
    vec3 C = texture(input_texture, gl_FragCoord.xy / resolution).rgb;
    vec3 R = vec3(0.);
    for(float i = 0.; i < it; i++) {
        vec2 p = vec2(mod(i, size), floor(i / size)) - floor(size * .5);
        vec3 aC = texture(feedback, (gl_FragCoord.xy + p) / resolution).rgb;
        R += aC;
    };
    R /= it;
    R = mix(R, C, .3 - blur_mix * .3);
    fragColor = vec4(R, 1.);
}
