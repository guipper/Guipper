#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform float v1;
uniform float v8;
uniform float v2;
uniform float v5;
uniform sampler2D koko;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	
	fragColor = vec4(1.0,0.0,1.0,1.0);

}
