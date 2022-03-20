#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;

uniform float value;
uniform float degrade;
uniform float defy;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 =  texture2D(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture2D(textura2, gl_FragCoord.xy/resolution);

	float ey = abs(sin(uv.y*50.+sin(uv.x*200.-time)+time*2.))*defy;
	//ey*=ridgedMF(uv*500.0);
	
	float e = smoothstep(uv.x-degrade,
	uv.x,value);
	
	vec3 fin = mix(t1.rgb,
	t2.rgb,
	e*t3.rgb);

	gl_FragColor = vec4(fin,1.0);
}
