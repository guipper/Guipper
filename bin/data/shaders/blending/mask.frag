#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

//uniform float mixst;
uniform sampler2D textura1;
uniform sampler2D textura2;
uniform sampler2D textura3;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture2D(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture2D(textura2, gl_FragCoord.xy/resolution);
	vec4 t3 =  texture2D(textura3, gl_FragCoord.xy/resolution);
	
	//vec3 fin = vec3(0.2,0.9,0.0);
	vec4 fin = mix(t1,t2,vec4(t3));
		
	
	gl_FragColor = fin;
}










