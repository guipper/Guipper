#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float limitr;
uniform float limitg;
uniform float limitb;
uniform float decr;
uniform float decg;
uniform float decb;
uniform float feedbackst_low;
uniform float feedbackst_high;
uniform float eforce;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec2 puv = gl_FragCoord.xy;
	puv.x+=0.1;
	vec4 fb =  texture(feedback, puv/resolution);
	vec4 t1 =  texture(texture1, gl_FragCoord.xy/resolution);
	vec3 fin = t1.rgb*mapr(eforce,0.0,0.8)
			
			   +fb.rgb*mapr(feedbackst_low,0.9,1.0);

	fin = lm(fin,vec3(limitr,limitg,limitb),vec3(decr,decg,decb));
	gl_FragColor = vec4(fin,1.0);
}
