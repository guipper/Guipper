#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float speedx;
uniform float speedy;
uniform float scalex;
uniform float scaley;

uniform float flush;


void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	float mapspeedx = mapr(speedx,-1.,1.0);
	float mapspeedy = mapr(speedy,-1.,1.0);
	
	float mapscalex = mapr(scalex,0.0,30.0);
	float mapscaley = mapr(scaley,0.0,30.0);
	float mapscale2 = mapr(flush,1.0,100.0);
	//float mapscaley2 = mapr(scaley2,0.0,50.0);
	float e = 0;
	e = fbm(vec2(uv.x*mapscalex+time*mapspeedx
				,uv.y*mapscaley+time*mapspeedy))*1.0;
	
    // e=0.5;  
	float e2 = fbm(vec2(mapscale2,
				        mapscale2)*e);
	
	vec3 fin = vec3(e2);
	fragColor = vec4(fin,1.0);

}
