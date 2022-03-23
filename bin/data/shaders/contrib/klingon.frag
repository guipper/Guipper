#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float f1y;
uniform float f1x;

uniform float f2y;
uniform float f2x;


void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;

	vec2 coords = gl_FragCoord.xy ;
	
	//float mf1 = mapr(f1y,0.0,1.0);
	
	float v1 = sin(uv.x*40*f1x)*sin(uv.y*40.*f1y);
	float v2 = sin(uv.x*100*f2x)*sin(uv.y*30.*f2y);
	
	float v3 = sin(uv.x*10)*sin(uv.y*10.);
	
	float v4 = sin(uv.x*10)*sin(uv.y*10.);
	
	v1*=v2;
	
	vec3 fin = vec3(sin(uv.x*10*v1)*sin(uv.y*10.+v1*2.0+time));
	
	gl_FragColor = vec4(fin,1.0);
}