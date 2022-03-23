#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float angle;
uniform float rotspeed;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec2 uv2 = gl_FragCoord.xy ;
	
	uv2-=resolution/2;
	uv2 = rotate2d(rotspeed*time*3.0+mapr(angle,0.0,pi))*uv2;
	uv2+=resolution/2;
	
	vec4 t1 =  texture(textura1, uv2/resolution);	
	vec3 fin = t1.rgb;
	
	gl_FragColor = vec4(fin,1.0);
}