#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float sizedif;
uniform float posx;
uniform float posy;

uniform float size2;
uniform float sizedif2;
uniform float posx2;
uniform float posy2;
//uniform float posyasdqwd;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

    float fx = resolution.x/resolution.y;
	uv.x*=fx;
	//uv.x*=resolution.y/resolution.x;
	vec2 m = vec2(mouse.x*fx,mouse.y);
	
	
	float mov = sin(uv.x*100.0+time)*0.01;
	//float e = poly(uv,m,0.4,0.4,mpuntas,0.0); // pincel
	float e = cir(uv,vec2(posx*fx+mov,posy),size*0.4,sizedif*0.4);
		  e+=cir(uv,vec2(posx2*fx+mov,posy2),size2*0.4,sizedif2*0.4);
	vec3 fin = vec3(e);
	//fin.r*=0.2;
	fragColor = vec4(fin,1.0);
}
