#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float min;
uniform float max;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 uv2 = gl_FragCoord.xy ;
	
	vec4 t1 =  texture2D(textura1, uv2/resolution);	
	vec3 fin = smoothstep(min,max,t1.rgb);
	
	fragColor = vec4(fin,1.0); 
}