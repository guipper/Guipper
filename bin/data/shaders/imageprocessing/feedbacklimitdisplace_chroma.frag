#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float sc;
uniform float limit;
uniform float force;
uniform float speedx;
uniform float speedy;
uniform float fb_fract;
uniform float sctx1;
uniform float scty1;
uniform float ttx1;
uniform float tty1;
uniform float chromar;
uniform float chromag;
uniform float chromab;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	//float fx = resolution.x/resolution.y;
	
	float mapsc = mapr(sc,0.99,1.01);
	vec2 puv = gl_FragCoord.xy;
	
	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	
	puv/=resolution;
	
	
	puv.x+=mapr(speedx,-0.01,0.01);
	puv.y+=mapr(speedy,-0.01,0.01);
	
	float mofx=t1.x*mapr(sctx1,-0.5,0.5);
	float mofy =t1.y*mapr(scty1,-0.5,0.5);
	
	
	float mttx1= t1.x*mapr(ttx1,-0.5,0.5);
	float mtty1 =t1.y*mapr(tty1,-0.5,0.5);
	puv.x+=mttx1;
	puv.y+=mtty1;
	puv-=vec2(0.5);
	puv = scale(vec2(mapsc+mofx,mapsc+mofy))*puv;
	puv+=vec2(0.5);
	//puv = abs(.5-fract(puv*mapr(fb_fract,1.0,3.0)));
	puv*=resolution;
	
	
	
	
	vec4 fb =  texture2D(feedback, puv/resolution);
	

	
	
	vec3 fin = vec3(0.);
	
	
	if(chromar < t1.r && chromag < t1.g && chromab < t1.b){
		fin = t1.rgb;
	}else{
	
		fin = fb.rgb *mapr(force,0.96,1.01);
	}
	//fin = t1.rgb;
	//fin = mix(fb.rgb,t1.rgb,t1.rgb);
		
	
	gl_FragColor = vec4(fin,1.0);
}
