#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float limitr;
uniform float limitg;
uniform float limitb;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture(textura2, gl_FragCoord.xy/resolution);
	
	vec3 fin = vec3(0.0);
	
	float mlimitr = mapr(limitr,0.0,0.1);
	float mlimitg = mapr(limitg,0.0,0.1);
	float mlimitb = mapr(limitb,0.0,0.1);
	
	if(mlimitr < t1.r &&
	   mlimitg < t1.g &&
	   mlimitb < t1.b){
		
	   fin = t1.rgb;
	}else{
	   fin = t2.rgb;
	}
	//fin = t1.rgb;
	 //fin = vec3(uv.x,uv.y,1.0);
	gl_FragColor = vec4(fin,1.0);

}
