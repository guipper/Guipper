#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float effect_mix;
uniform float samples;
uniform float radius;
uniform float radstep;
uniform float focus;
uniform float dof;


void main()
{
    float rad=floor(radius*10.);
    float it=pi*2.;
    float st=it/(1.+samples*50.);
    vec4 C=texture2DRect(input_texture,gl_FragCoord.xy);
    vec3 R=vec3(0.);
    float i=0;
    float ff=pow(abs(C.a-focus),dof*5.);
    for(float r=1.; r<rad; r++) {
      for(float a=0.; a<it; a+=st) {
        i+=r;
        vec2 p=vec2(sin(a),cos(a))*r*radstep*10.*ff;
        vec3 aC=texture(input_texture,(gl_FragCoord.xy+p)/resolution).rgb;
  		  R+=aC*r;
      };
    }
    R/=i;
    R=mix(C.rgb,R,effect_mix);
    fragColor = vec4(R,1.);
}
