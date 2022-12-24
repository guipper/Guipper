#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float opacity;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	
	float e = (t1.r + t1.g + t1.b)/3.;
	vec3 fin = mix(vec3(e),t1.rgb,opacity); 
	//t1 =vec3(t1.r);
	fragColor = vec4(fin,1.0);
}
