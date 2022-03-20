#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float scaley;
uniform float scalex;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec2 uv2 = gl_FragCoord.xy ;
	
	uv2-=resolution/2;
	uv2 = scale(vec2(mapr(scalex,0.0,2.0),
				mapr(scaley,0.0,2.0)))*uv2;
	uv2+=resolution/2;
	
	vec4 t1 =  texture2D(textura1, uv2/resolution);	
	vec3 fin = t1.rgb;
	
	gl_FragColor = vec4(fin,1.0); 
}