#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
//uniform float feedbackst;
//uniform float decay;
uniform float limit;
uniform float chromar;
uniform float chromag;
uniform float chromab;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	//float fx = resolution.x/resolution.y;

	vec2 puv = gl_FragCoord.xy;

	vec4 fb =  texture(feedback, puv/resolution);
	vec4 t1 =  texture(texture1, gl_FragCoord.xy/resolution);
	
	vec3 fin = vec3(0.);
	
	
	if(limit > abs(chromar-t1.r) ||
	   limit > abs(chromag-t1.g) ||
	   limit > abs(chromab-t1.b)){
		fin = t1.rgb;
	}else{
		fin = fb.rgb ;
	}
	//fin = t1.rgb;
	//fin = mix(fb.rgb,t1.rgb,t1.rgb);
		
	
	gl_FragColor = vec4(fin,1.0);
}
