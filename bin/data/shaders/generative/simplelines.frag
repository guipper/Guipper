#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float min;
//uniform float max;

uniform float frequency;
uniform bool xy;
uniform float speed;

void main(){	
	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float d2 = 0;
	
	float maptime = mapr(speed,-2.0,2.0);
	float mapmin = mapr(min,0.0,1.0);
	if(xy){
		d2 = abs(sin(uv.x*frequency*100.+maptime*time*5.));
	}else{
		d2 = abs(sin(uv.y*frequency*100.+maptime*time*5.));
	}
	
	float d = smoothstep(mapmin,1.0,d2);
	vec3 fin = vec3(d);
	fragColor = vec4(fin,1.0); 
}


