#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float chromared;
uniform float chromagreen;
uniform float chromablue;
uniform float umbral;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture2D(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture2D(textura2, gl_FragCoord.xy/resolution);
	
	vec3 col1 = vec3(chromared,chromagreen,chromablue);
	vec3 fin = t1.rgb;
	
	if(distance(col1,t1.rgb) < umbral){
		//vec3 col2 = vec3(red,green,blue);
		 fin = mix(t2.rgb,t1.rgb,distance(col1,t1.rgb));
	}
	//fin = mix(t1.rgb,t2.rgb,distance(col1,t1.rgb)*0.5);
	gl_FragColor = vec4(fin,1.0); 
}










