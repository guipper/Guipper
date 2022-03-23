#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float effect_mix=1.0;
uniform float effect_exp=0.49;
uniform float color_mix=1.0;
uniform float sample_size=0.18;
uniform float brightness=0.36;

vec3 calculateEdge(vec3 input){
	//return vec3(1.0,0.0,0.0);
	float size=2.+sample_size*10.;
    float it=floor(size*size);
	 vec3 R=vec3(0.);
    for(float i=0.; i<it; i++) {
    	vec2 p=vec2(mod(i,size),floor(i/size))-floor(size*.5);
        vec3 aC=texture(input_texture,(gl_FragCoord.xy+p)/resolution).rgb;
		    R+=pow(distance(input,aC),effect_exp*3.);
    };
    R/=it;
    R=mix(R,input*R,color_mix);
    R*=1.+brightness*50.;
    R=mix(input,R,effect_mix);
	return R;
}
vec3 calculateEdge(vec3 input,
					float _effect_mix,
					float _effect_exp,
					float _color_mix,
					float _sample_size,
					float _brightness){
						
	//return vec3(1.0,0.0,0.0);
	float size=2.+_sample_size*10.;
    float it=floor(size*size);
	 vec3 R=vec3(0.);
    for(float i=0.; i<it; i++) {
    	vec2 p=vec2(mod(i,size),floor(i/size))-floor(size*.5);
        vec3 aC=texture(input_texture,(gl_FragCoord.xy+p)/resolution).rgb;
		R+=pow(distance(input,aC),_effect_exp*3.);
    };
    R/=it;
    R=mix(R,input*R,_color_mix);
    R*=1.+_brightness*50.;
    R=mix(input,R,_effect_mix);
	return R;		
}
void main()
{

    vec3 C=texture(input_texture,gl_FragCoord.xy/resolution).rgb;
	vec3 R = calculateEdge(C);
		 R = calculateEdge(C,effect_mix,
		 effect_exp,
		 color_mix,
		 sample_size,
		 brightness);
		
    fragColor = vec4(R,1.);
	/*uniform float effect_mix=1.0;
	uniform float effect_exp=0.49;
	uniform float color_mix=1.0;
	uniform float sample_size=0.18;
	uniform float brightness=0.36;*/
}
