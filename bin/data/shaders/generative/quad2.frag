#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float size;
uniform float sizedif;
uniform float sizspeed;
uniform float amp;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float fx = resolution.x/resolution.y;
	uv.x *=fx;


	float mapsize = mapr(size,0.0,0.5)+sin(time*sizspeed*10.0)*mapr(amp,0.0,2.0)+mapr(amp,0.0,2.0);
	float mapsizedif = mapr(sizedif,0.0,0.5)+sin(time*sizspeed*10.0)*mapr(amp,0.0,2.0)+mapr(amp,0.0,2.0);
	
	
	
	
	float e = poly(uv,vec2(0.5*fx,0.5),size,size,4,0.0);

	vec2 puv = gl_FragCoord.xy;
	vec4 fb =  texture2D(feedback, puv/resolution);


	vec3 fin = vec3(e);

	gl_FragColor = vec4(fin,1.0);
}
