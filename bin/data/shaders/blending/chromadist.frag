#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float chromared;
uniform float chromagreen;
uniform float chromablue;
uniform float umbral;
uniform float diststr;
void main()
{	
	vec2 uv = fragCoord.xy / resolution;
	
	vec4 t1 =  texture2D(textura1, fragCoord.xy/resolution);
	vec4 t2 =  texture2D(textura2, fragCoord.xy/resolution);
	
	vec3 col1 = vec3(chromared,chromagreen,chromablue);
	vec3 fin = t1.rgb;
	
	if(distance(col1,t1.rgb) < umbral){
		 fin = mix(t2.rgb,t1.rgb,distance(col1,t1.rgb)*diststr*15.0);
	}
	
	fragColor = vec4(fin,1.0); 
}










