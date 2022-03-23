#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float effect_mix=1.0;
uniform float effect_exp=0.49;
uniform float color_mix=1.0;
uniform float sample_size=0.18;
uniform float brightness=0.36;

void main()
{
    float size=2.+sample_size*10.;
    float it=floor(size*size);
    vec3 C=texture2D(input_texture,gl_FragCoord.xy/resolution).rgb;
	  vec3 R=vec3(0.);
    for(float i=0.; i<it; i++) {
    	vec2 p=vec2(mod(i,size),floor(i/size))-floor(size*.5);
        vec3 aC=texture2D(input_texture,(gl_FragCoord.xy+p)/resolution).rgb;
		    R+=pow(distance(C,aC),effect_exp*3.);
    };
    R/=it;
    R=mix(R,C*R,color_mix);
    R*=1.+brightness*50.;
    R=mix(C,R,effect_mix);
    fragColor = vec4(R,1.);
}
