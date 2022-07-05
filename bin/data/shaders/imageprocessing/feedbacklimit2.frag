#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float sc;
uniform float limit;
uniform float force;
uniform float speedx;
uniform float speedy;
uniform float fb_fract;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	//float fx = resolution.x/resolution.y;
	
	float mapsc = mapr(sc,0.9,1.1);
	vec2 puv = gl_FragCoord.xy;
	
	
	
	puv/=resolution;
	
	
	puv.x+=mapr(speedx,-0.01,0.01);
	puv.y+=mapr(speedy,-0.01,0.01);
	puv-=vec2(0.5);
	puv = scale(vec2(mapsc))*puv;
	puv+=vec2(0.5);
	//puv = abs(.5-fract(puv*mapr(fb_fract,1.0,3.0)));
	puv*=resolution;
	
	
	
	
	vec4 fb =  texture2D(feedback, puv/resolution);
	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	
	vec3 fin = vec3(0.);
	
	
	if(limit < t1.r && limit < t1.g && limit < t1.b){
		fin = t1.rgb;
	}else{
		fin = fb.rgb *mapr(force,0.96,0.99);
	}
	//fin = t1.rgb;
	//fin = mix(fb.rgb,t1.rgb,t1.rgb);
		
	
	fragColor = vec4(fin,1.0);
}
