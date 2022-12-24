#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float speedX;
uniform float speedY;
uniform float minInv;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution - .5;
	uv.x*=resolution.x/resolution.y;
	float tx = time * (speedX-.5)*5.;
	float ty = time * (speedY-.5)*5.;
	vec2 iuv = fract(uv/clamp(dot(uv,uv),minInv*.05,1.)+vec2(tx,ty));
	vec4 t1 =  texture2D(texture1, iuv);
	vec3 fin = t1.rgb;
		
	
	fragColor = vec4(fin,1.0);
}
 