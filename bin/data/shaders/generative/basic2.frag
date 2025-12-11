#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float e = sin(uv.x*10.+time)*.5+.5;
	fragColor = vec4(e,0.5,1.0,1.0); 
}









