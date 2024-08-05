#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float effect_mix = 1.0;
uniform float effect_exp = 0.74;
uniform float color_mix = 0.57;
uniform float sample_size = 0.75;
uniform float saturation = 0.25;

mat2 rot(float a) {
  float c = cos(a);
  float s = sin(a);
  return mat2(c, s, -s, c);
}

void main() {
  float size = 1. + floor(sample_size * 2.);
  float it = floor(size * size);
  vec3 C = texture(input_texture, gl_FragCoord.xy / resolution).rgb;
  vec3 R = vec3(0.);
  for(float i = 0.; i < it; i++) {
    vec2 p = vec2(mod(i, size), floor(i / size)) - floor(size * .5);
    vec3 aC = texture(input_texture, (gl_FragCoord.xy + p) / resolution).rgb;
    R += normalize(cross(C, aC)) * length(aC);
  };
  R /= it * .5;
  R = mix(vec3(length(R) / 1.7), R, saturation);
  R = mix(C, R, effect_mix);
  fragColor = vec4(R, 1.);
}
