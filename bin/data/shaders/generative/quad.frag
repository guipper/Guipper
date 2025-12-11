#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float size;
uniform float sizedif;
uniform float sizspeed = 0.0;
uniform float amp;
uniform float offsetx = 0.5;
uniform float offsety = 0.5;
uniform float scalex = 0.5;
uniform float scaley = 0.5;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float fx = resolution.x/resolution.y;
	
	float ofx = mapr(offsetx,-1.0,1.0);
	
	float ofy = mapr(offsety,-1.0,1.0);
	
	
	uv-=vec2(0.5);
	uv*=scale(vec2(scalex*2.5,scaley*2.5));
	uv+=vec2(0.5);
	
	
	
	uv.x+=ofx;
	uv.y+=ofy;
	uv.x *=fx;

	
	float mapsize = mapr(size,0.0,0.5)+sin(time*sizspeed*10.0)*mapr(amp,0.0,2.0)+mapr(amp,0.0,2.0);
	float mapsizedif = mapr(sizedif,0.0,0.5)+sin(time*sizspeed*10.0)*mapr(amp,0.0,2.0)+mapr(amp,0.0,2.0);
	float e = poly(uv,vec2(0.5*fx,0.5),mapsize,mapsizedif,4,0.0);

	vec2 puv = gl_FragCoord.xy;
	vec4 fb =  texture2D(feedback, puv/resolution);


	vec3 fin = vec3(e);

	fragColor = vec4(fin,1.0);
}
