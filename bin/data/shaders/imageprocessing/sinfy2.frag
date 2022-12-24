#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float speed;
uniform float freq;
uniform float mx;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec4 t1 =  texture2D(textura1, gl_FragCoord.xy/resolution);
	
	vec3 fin = sin(t1.rgb*10.*freq+speed*4.*time)*.25+.25 ;
	
	
		 fin = clamp(fin,0.0,1.0);
		 fin = mix(t1.rgb,fin,mx);
	fragColor = vec4(fin,1.0); 
}

