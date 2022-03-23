#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float min;
uniform float dif;
//uniform float max;

uniform float frequency;
uniform float speed = 0.5;
uniform float f1;
uniform float f2;
uniform float f3;
uniform float f4;
uniform float estatica;
void main(){	
	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float d2 = 0;
	
	float maptime = mapr(speed,-1.0,1.0);
	float mapmin = mapr(min,0.0,1.0);
	
	float mtime = maptime*time*1.0;
	
		d2 = abs(fract(uv.x*floor(10.0*f1)+mtime
				+abs(fract(uv.y*floor(10.0*f2)+mtime
				+abs(fract(uv.x*floor(10.0*f3)+mtime
				+abs(fract(uv.y*floor(10.0*f4)+mtime)*2.0-1.))*2.0-1.))*2.0-1.))*2.0-1.0);
		
		//d2 = abs(fract(uv.y*floor(10.0*f4)+mtime)*2.0-1.);
		
		/*d2 = abs(fract(uv.x*floor(5.0*f1)+mtime
				+abs(fract(uv.y*floor(5.0*f2)+mtime
				+abs(fract(uv.x*floor(5.0*f3)+mtime
				+abs(fract(uv.y*floor(5.0*f4)+mtime)*2.0-1.))*2.0-1.))*2.0-1.))*2.0-1.0);
		*/
	float d = 1.-smoothstep(mapmin,mapmin+dif,d2);
		  d =mix(d,fract(d*200000.0),d*estatica);
	vec3 fin = vec3(d
	);
	gl_FragColor = vec4(fin,1.0);
}


