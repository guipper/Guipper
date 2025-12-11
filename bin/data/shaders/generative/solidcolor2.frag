#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float r;
uniform float g;
uniform float b;
uniform float mivariable;
uniform sampler2D f1;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution.xy;
	
	vec4 tx = texture(f1,uv); 
	vec3 fin = vec3(r,g,b);
	
	tx.r*=0.0;
	float e = sin(uv.x*10.+time)*.5+.5;
	fragColor = tx; 
}