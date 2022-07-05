#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float scalex;
uniform float scaley;
uniform float speedx=.5;
uniform float speedy=.5;
uniform float pattern;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	float mapspeedx = mapr(speedx,-1.,1.0);
	float mapspeedy = mapr(speedy,-1.,1.0);
	float mapscalex = mapr(scalex,0.0,20.0);
	float mapscaley = mapr(scaley,0.0,20.0);
	
	float patron =mapr(pattern,0.0,6.0);
	
	float e = uv.y;
		
	 float e2 = rxr(vec2(uv.x*mapscalex+time*mapspeedx,
				  uv.y*mapscaley+time*mapspeedy))*0.5-.5;
		  e2 = smoothstep(0.02,0.75,e2);
		  e2 = sin(e2*100.0+time);
	vec3 fin = vec3(e2);
	fragColor = vec4(fin,1.0);
}
