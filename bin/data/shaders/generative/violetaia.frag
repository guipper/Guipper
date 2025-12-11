#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float r;
uniform float g;
uniform float b;
uniform float mivariable;


void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec3 fin = vec3(189./255.,33./255.,207./255.);
	
	fragColor = vec4(fin,1.0); 
}