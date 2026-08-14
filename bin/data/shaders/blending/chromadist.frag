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
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture(textura2, gl_FragCoord.xy/resolution);
	
	vec3 col1 = vec3(chromared,chromagreen,chromablue);
	vec4 fin = t1;
	
	if(distance(col1,t1.rgb) < umbral){
	 float amount = clamp(distance(col1,t1.rgb)*diststr*15.0, 0.0, 1.0);
	 fin = mix(t2,t1,amount);
	}
	
	fragColor = fin;
}









