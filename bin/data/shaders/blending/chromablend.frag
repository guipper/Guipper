#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float chromared; // @color r chroma
uniform float chromagreen; // @color g chroma
uniform float chromablue; // @color b chroma
uniform float red; // @color r main
uniform float green; // @color g main
uniform float blue; // @color b main
uniform float umbral;
uniform float textureblend;




void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture(textura2, gl_FragCoord.xy/resolution);
	vec3 col1 = vec3(chromared,chromagreen,chromablue);
	vec3 fin = t1.rgb;
	
	if(distance(col1,t1.rgb) < umbral){
		vec3 col2 = t2.rgb;
		
		col2 = mix(t2.rgb,vec3(red,green,blue),1.-textureblend);
		fin = col2;
	}
	
	fragColor = vec4(fin,1.0); 
}










