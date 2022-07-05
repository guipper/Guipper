#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float rojo1;
uniform float verde1;
uniform float azul1;

uniform float rojo2;
uniform float verde2;
uniform float azul2;

uniform float pattern;
uniform float waveform;
uniform float frequency;

uniform bool aspectradio;
uniform float speed;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float fx = resolution.x/resolution.y;
	
	vec2 p = vec2(0.0);
	if(aspectradio){
		uv.x*=fx;
		p = vec2(0.5*fx,0.5) - uv;
	}else{
		p = vec2(0.5,0.5) - uv;
	}
	//uv.x*=fx;
	//p = vec2(0.5*fx,0.5) - uv;
	float r = length(p);
	float a = atan(p.x,p.y);
	
	vec3 col1 = vec3(rojo1,verde1,azul1);
	vec3 col2 = vec3(rojo2,verde2,azul2);
	

	int patron = int(floor(mapr(pattern,0.0,3.0)));
	int waveform = int(floor(mapr(waveform,0.0,3.0)));
	float mfrequency = mapr(frequency,1.0,20.0);
	float mspeed = mapr(speed,-5.0,5.0);
	mspeed*=time;
	
	
	vec3 fin = vec3(0.);
	
	
	float e = 0.;
	float e1 =0.;
	if(patron == 0){
		e1 = uv.x;
	}else if(patron == 1){
		e1 = uv.y;
	}else if(patron == 2){
	    //e = abs(sin(r));
		e1 = r;
	}else if(patron == 3){
		e1 = a;
		//mfrequency = mfrequency;
		//e1 = abs(sin(a)*0.5+0.5);
	}
	
	
	
	
	//SACADO DEL MODELO DE FORMAS DE ONDAS PROPUESTAS POR KEVIN KRIPPER
	//LAS CUALES LAS UTILIZA EN SU VSINTH
	
	float ramp = fract(e1*mfrequency+mspeed);
    float seno = sin(ramp*TWO_PI)*0.5+0.5;
    float saw  = 1.- ramp; 
    float tri  = abs(ramp*2.-1.);
	float pulse = 1.- smoothstep(0.49,0.5001,ramp);
	
	if(waveform == 0){
		e = ramp;
	}else if(waveform == 1){
		e = seno;
	}else if(waveform == 2){
		e = tri;
	}else if(waveform == 3){
		e = pulse;
	}
	
	
	
	//e = sin(e*mfrequency+mspeed*time);
	
	
	fin = mix(col1,col2,e);
	
	fragColor = vec4(fin,1.0);
}
