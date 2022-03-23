#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;

uniform float r;
uniform float g;
uniform float b;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	
	vec3 fin = t1.rgb * vec3(r,g,b) * 2.;
	
	gl_FragColor = vec4(fin,1.0);
}

