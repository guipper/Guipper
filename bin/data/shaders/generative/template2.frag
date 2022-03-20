#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform float v1;


void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	gl_FragColor = vec4(uv.x,uv.y,0.0,1.0);
}
