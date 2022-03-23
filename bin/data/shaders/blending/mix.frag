#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float mixst;
uniform sampler2D textura1;
uniform sampler2D textura2;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture(textura1, gl_FragCoord.xy/ resolution);
	vec4 t2 =  texture(textura2, gl_FragCoord.xy/ resolution);
	
	//vec3 fin = vec3(0.2,0.9,0.0);
	vec4 fin = mix(t1,t2,mixst);
		
	
	gl_FragColor = fin;
}










