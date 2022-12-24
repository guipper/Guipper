#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D textura1;
uniform float red1;
uniform float green1;
uniform float blue1;
uniform float red2;
uniform float green2;
uniform float blue2;


void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	
	vec4 col1 = vec4(red1,green1,blue1,1.0);
	vec4 col2 = vec4(red2,green2,blue2,1.0);
	
	
	//vec3 fin = vec3(0.2,0.9,0.0);
	vec4 fin = t1;
		 fin = mix(col1,col2,t1);
	
	fragColor = fin; 
}










