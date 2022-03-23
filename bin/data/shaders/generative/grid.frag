#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float filas;
uniform float columnas;
uniform float speedx;
uniform float speedy;
uniform float cantidadpuntas;
uniform float size;
uniform float sizedif;
uniform float feedbackst;
uniform float rotspeed;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float fx = resolution.x/resolution.y;
	uv.x *=fx;

  int puntas = int(floor(mapr(cantidadpuntas,2.0,20.0)));
	vec2 speed = vec2(mapr(speedx,-1.0,1.0),mapr(speedy,-1.0,1.0));


  float mapsize = mapr(size,0.0,0.5);
	float mapsizedif = mapr(sizedif,0.0,0.5);
	uv = fract(vec2(uv.x*filas*20.,uv.y*columnas*20.)+speed*time);
	float e = poly(uv,vec2(0.5,0.5),mapsize,mapsizedif,puntas,mapr(rotspeed,0.0,5.0)*time);

	vec2 puv = gl_FragCoord.xy;
	vec4 fb =  texture(feedback, puv/resolution);


	vec3 fin = vec3(e)+fb.rgb*feedbackst;

	gl_FragColor = vec4(fin,1.0);
}
