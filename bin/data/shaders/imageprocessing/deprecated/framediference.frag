#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float umbral ;
uniform float fback ;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 =  texture2DRect(textura1, gl_FragCoord.xy/resolution);
	vec4 fb =  texture2DRect(feedback, gl_FragCoord.xy/resolution);

	float a = step(dot(t1.rgb,fb.rgb),umbral)+.01;
	
	vec3 fin = abs(t1.rgb - fb.rgb);
	gl_FragColor = vec4(fin,1.0);
}
