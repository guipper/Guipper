#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float v1;
uniform float v2;
uniform float v3;


float lala = 1.0;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;

	
	fragColor = vec4(v2,lala,0.0,1.0); 
}









