#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float r; // @color r
uniform float g; // @color g
uniform float b; // @color b
uniform float mivariable;


void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec3 fin = vec3(r,g,b);
	
	
	float e = sin(uv.x*10.+time)*.5+.5;
	fragColor = vec4(uv.x,0.0,0.0,1.0); 
}