#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float chroma_red; // @color r
uniform float chroma_green; // @color g
uniform float chroma_blue; // @color b
uniform float threshold;


void main() {
  vec3 col=texture(input_texture, gl_FragCoord.xy/resolution).rgb;

  vec3 target=normalize(vec3(chroma_red,chroma_green,chroma_blue));

  vec3 c = vec3(pow(dot(normalize(col),target),threshold*50.));

  fragColor = vec4(c*col,1.);

}
