//#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform vec2 resolution;
uniform float r;
uniform float g;
uniform float b;
uniform float var1;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec3 fin = vec3(uv.x,0.0,uv.y);
	
	fragColor = vec4(1.0,0.0,1.0,1.0); 
}