#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;

	fragColor = vec4(1.0,0.5,1.0,1.0); 
}









