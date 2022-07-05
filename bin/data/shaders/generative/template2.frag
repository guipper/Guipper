#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform float v1;

uniform float azul2 ;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	fragColor = vec4(0.,0.0,azul2,1.0);
}
