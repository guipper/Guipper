#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform float v1;
uniform float v2;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float f = sin(uv.y*20.+time*1.+sin(uv.x*200.*v2));
	gl_FragColor = vec4(vec3(f),1.0);
}
