#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float r;
uniform float g;
uniform float b;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec3 fin = vec3(r,g,b);
	
	gl_FragColor = vec4(fin,1.0);
}