#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
// Example Pixel Shader


void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
	float fx = resolution.x/resolution.y;
	
	float d = 0.1;
	float s = 0.00;
	float gridscale = 30;
	
	uv=fract(uv*gridscale);
	
	float lx = 1.-(smoothstep(s,s+d,uv.x) * smoothstep(s,s+d,1.-uv.x) );
	float ly = 1.-(smoothstep(s,s+d,uv.y) * smoothstep(s,s+d,1.-uv.y) );
			  
	vec3 fin = vec3(lx+ly);
	fragColor = vec4(fin,1.);
	
	
}
