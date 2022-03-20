#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float effect_mix;
uniform float samples;
uniform float radius;

void main()
{
    float st=pi*2./(1.+samples*100.);
    float it=pi*2.;
    vec3 R=vec3(0.);
    vec3 C=texture2D(input_texture,(gl_FragCoord.xy)/resolution).rgb;
    float i=0;
    for(float a=0.; a<it; a+=st) {
      i++;
      vec2 p=vec2(sin(a),cos(a))*radius*200.;
      vec3 aC=texture2D(input_texture,(gl_FragCoord.xy+p)/resolution).rgb;
		  R+=normalize(cross(C,aC));
    };
    R/=i;
    R=mix(C,R,effect_mix);
    fragColor = vec4(R,1.);
}
