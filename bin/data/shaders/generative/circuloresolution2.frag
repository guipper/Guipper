#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float rojo1;

//uniform float posyasdqwd; 
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	
	
    float fx = resolution.x/resolution.y;
	
	

	fragColor = vec4(uv.x,uv.y,1.0,1.0);
}
