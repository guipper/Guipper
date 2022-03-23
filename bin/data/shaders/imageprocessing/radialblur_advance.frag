#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float effect_mix;
uniform float samples;
uniform float radius;
uniform float radstep;


void main()
{
    float rad=floor(radius*10.);
    float it=pi*2.;
    float st=it/(1.+samples*50.);
    vec3 C=texture(input_texture,gl_FragCoord.xy/resolution).rgb;
    vec3 R=vec3(0.);
    float i=0;
    for(float r=1.; r<rad; r++) {
      for(float a=0.; a<it; a+=st) {
        i+=r;
        vec2 p=vec2(sin(a),cos(a))*r*radstep*10.;
        vec3 aC=texture(input_texture,(gl_FragCoord.xy+p)/resolution).rgb;
  		  R+=aC*r;
      };
    }
    R/=i;
    R=mix(C,R,effect_mix);
    fragColor = vec4(R,1.);
}
