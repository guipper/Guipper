#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float sizedif;

uniform bool isColorize;
uniform float sat;




uniform float size1;
uniform float size2;
uniform float size3;
uniform float size4;



uniform float posx;
uniform float posy;

uniform float posx2;
uniform float posy2;


uniform float posx3;
uniform float posy3;

uniform float posx4;
uniform float posy4;

uniform float posx5;
uniform float posy5;

//uniform float posyasdqwd;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

    float fx = resolution.x/resolution.y;
	uv.x*=fx;
	//uv.x*=resolution.y/resolution.x;
	vec2 m = vec2(mouse.x*fx,mouse.y);

	//float e = poly(uv,m,0.4,0.4,mpuntas,0.0); // pincel
	float e = cir(uv,vec2(posx*fx,posy),size1,sizedif);
	float e2 = cir(uv,vec2(posx2*fx,posy2),size2,sizedif);
	float e3 = cir(uv,vec2(posx3*fx,posy3),size3,sizedif);
	float e4 = cir(uv,vec2(posx4*fx,posy4),size4,sizedif);
	
	vec3 fin = vec3(0.0);
	
	if(isColorize){
		
		fin = vec3(e)*vec3(1.0,sat,sat)+
			  vec3(e2)*vec3(1.0,1.0,sat)+
			  vec3(e3)*vec3(sat,sat,1.0)+
			  vec3(e4)*vec3(1.0,sat,1.0);
	
	}else{
		fin = vec3(e+e2+e3+e4);
	
	}
	//fin = vec3(e+e2+e3+e4);
	//fin.r*=0.2;
	fragColor = vec4(fin,1.0);
}
