#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;

uniform float r; // @color r
uniform float g; // @color g
uniform float b; // @color b

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	
	vec3 fin = t1.rgb * vec3(r,g,b) * 2.;
	
	fragColor = vec4(fin,1.0); 
}

