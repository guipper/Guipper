#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float brightness =0.5;
uniform float contrast=0.13;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	t1+=mapr(brightness,-2.0,2.0);
	t1*=mapr(contrast,0.0,8.0);
	gl_FragColor = vec4(t1.rgb,1.0);
}
