#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D tx;
uniform float rd1;
uniform float rd2;
uniform float rd3;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution.xy;


	vec4 t = texture(tx,uv);
	
	
	t.rg *=rotate2d(rd1*TWO_PI); 
	t.bg *=rotate2d(rd2*TWO_PI); 
	t.rb *=rotate2d(rd3*TWO_PI); 
	
	
	t = abs(t);
	fragColor = t;
}