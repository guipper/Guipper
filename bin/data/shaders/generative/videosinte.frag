#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float v1;
uniform float v1_amp;
uniform float v2;
uniform float v2_amp;
uniform float v3;
uniform float v3_amp;
uniform float v4;
uniform float v4_amp;
uniform float faser;
uniform float faseg;
uniform float faseb;
uniform float speed;
uniform float e_force;

float desf(vec2 uv, float _fase){
		
	float e = 0.;
	float mt = time * speed * 10.;//MAP TIME 
	
	float ampmax = 5.0;
	float mv1_amp = mapr(v1_amp,0.,ampmax);
	float mv2_amp = mapr(v2_amp,0.,ampmax);
	float mv3_amp = mapr(v3_amp,0.,ampmax);
	float mv4_amp = mapr(v4_amp,0.,ampmax);
	
	e = sin(uv.y*mapr(v1,0.,100.)+mt+_fase
		  +sin(uv.y*mapr(v2,0.,100.)+mt+_fase
		  +sin(uv.x*mapr(v3,0.,100.)+mt+_fase
		  +sin(uv.y*mapr(v4,0.,100.)+mt+_fase
		  )*mv4_amp*0.5+mv4_amp*0.5
		  )*mv3_amp*0.5+mv3_amp*0.5
		  )*mv2_amp*0.5+mv2_amp*0.5
		  )*mv1_amp*0.5+mv1_amp*0.5;
	
	return e;
}


void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;

	vec2 coords = gl_FragCoord.xy ;
	//coords.y = resolution.y -coords.y;
	vec4 fb =  texture2D(feedback, coords/resolution);
	
	float mt = time * speed * 10.;//MAP TIME 
	
	
	float m_faser = faser * pi * 2.;
	float m_faseg = faseg * pi * 2.;
	float m_faseb = faseb * pi * 2.;
	
	float e1 = desf(uv,m_faser);
	float e2 = desf(uv,m_faseg);
	float e3 = desf(uv,m_faseb);
			  
	vec3 dib = vec3(e1,e2,e3);
	
	vec3 fin = vec3(0);
	
	fin = dib * e_force;
	
	fragColor = vec4(fin,1.0); 
}









