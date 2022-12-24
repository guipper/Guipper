#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;

uniform float offsetx;
uniform float offsety;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;	
	
	vec4 t2 =  texture2D(textura2, gl_FragCoord.xy/resolution);
	float t2_f = (t2.r+t2.g+t2.b)/3.;
	
	vec2 uv2 = gl_FragCoord.xy / resolution;
	
	float limit = 0.5;
	float moffsetx = mapr(offsetx,-limit,limit);
	float moffsety = mapr(offsety,-limit,limit);
	
	//uv2+=vec2(t2_f*moffsetx,t2_f*moffsety);
	
	
	uv2-=vec2(0.5);
	uv2*=scale(vec2(0.9-t2_f*0.9));
	uv2+=vec2(0.5);
	
	
	vec4 t1 =  texture2D(textura1, uv2);
	
	
	
	vec3 fin = t1.rgb;
	
	fragColor = vec4(fin,1.0); 
}