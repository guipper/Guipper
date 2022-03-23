#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float f1;
uniform float f2;
uniform float f3;
uniform float a4;
uniform float f4;
uniform float faser;
uniform float faseg;
uniform float faseb;


float desf (vec2 uv, float fase){

	//float mf1 = mapr(f1y,0.0,1.0);
	float v1 = 0.;
	v1+= sin(uv.x*40*f1+time) * sin(uv.y*40.*f1);
	float v2 = cos(uv.x*40*f2)*sin(uv.y*40.*f2);
	v1*=sin(v2*1+time);
	float v3 = cos(uv.x*40*f4+time) + sin(uv.y*40.*f4+time);
	
	float ff = sin(uv.x*10*v1*f3+fase)*sin(uv.y*10.+v1*10.0*f3+time+fase);
	
	ff = mix(ff,v3,a4);
	return ff;
}

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float r = desf(uv,mapr(faser,0.0,pi*2.0));
	float g = desf(uv,mapr(faseg,0.0,pi*2.0));
	float b = desf(uv,mapr(faseb,0.0,pi*2.0));
	
	vec3 fin = vec3(r,g,b);
	
	gl_FragColor = vec4(fin,1.0);
}