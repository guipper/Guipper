#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float feedbackst;
uniform float decay;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	//float fx = resolution.x/resolution.y;

	vec2 puv = gl_FragCoord.xy;

	vec4 fb =  texture2D(feedback, puv/resolution);
	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	
	vec3 fin = vec3(0.);
	//fin = t1.rgb*eforce+fb.rgb*mapr(feedbackst,0.0,1.5);
	fin = mix(fb.rgb*decay,t1.rgb,t1.rgb*mapr(feedbackst,0.0,1.5));
	
	fragColor = vec4(fin,1.0);
}
