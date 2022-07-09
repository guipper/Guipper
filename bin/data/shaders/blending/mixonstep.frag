#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float mix_r;
uniform float mix_b;
uniform float mix_g;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture2D(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture2D(textura2, gl_FragCoord.xy/resolution);
	
	vec3 fin = vec3(0.0);
	
	/*float mlimitr = mapr(limitr,0.0,1.0);
	float mlimitg = mapr(limitg,0.0,1.0);
	float mlimitb = mapr(limitb,0.0,1.0);
	*/
	/*if(mlimitr < t1.r && 
	   mlimitg < t1.g && 
	   mlimitb < t1.b){
		
	   fin = t1.rgb;
	}else{
	   fin = t2.rgb;
	}*/
	vec3 mixf = vec3(mix_r,mix_b,mix_g);
	fin = smoothstep(t1.rgb,t2.rgb,mix(t1.rgb,t2.rgb,0.5));
	//fin = t1.rgb;
	 //fin = vec3(uv.x,uv.y,1.0);
	fragColor = vec4(fin,1.0);

}
