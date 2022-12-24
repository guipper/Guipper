#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform float sc;
uniform float limit;
uniform float fbforce;
uniform float tforce;
uniform float tx;
uniform float ty;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	//float fx = resolution.x/resolution.y;
	
	float mapsc = mapr(sc,0.99,1.01);
	vec2 puv = gl_FragCoord.xy;
	
	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture2D(texture2, gl_FragCoord.xy/resolution);
	
	float mov = (t2.r+t2.g+t2.b)/3.0;
	puv/=resolution;
	puv.x+=mov*mapr(tx,-1.0,1.0);
	puv.y+=mov*mapr(ty,-1.0,1.0);
	puv*=resolution;
	
	vec4 fb =  texture2D(feedback, puv/resolution);
	
	vec3 fin = vec3(0.);
	
	
	fin =t1.rgb*tforce+ fb.rgb *mapr(fbforce,0.0,1.01);
	
	//fin = t1.rgb;
	//fin = mix(fb.rgb,t1.rgb,t1.rgb);
		
	
	fragColor = vec4(fin,1.0);
	//fragColor = vec4(vec3(0.0),1.0);
}
