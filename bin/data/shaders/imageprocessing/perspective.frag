#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float speedX;
uniform float speedY;
uniform float height;
uniform float size;
uniform float fade;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution - .5;
	uv.x*=resolution.x/resolution.y;
	float tx = time * (speedX-.5) * 10.;
	float ty = time * (speedY-.5) * 20.;
	vec2 iuv = vec2(uv.x/uv.y, 1./uv.y*2.)*height*10.;
    iuv.x+=sign(uv.y)*tx;
    iuv.y+=sign(uv.y)*ty;
	vec4 t1 =  texture2D(texture1, abs(0.5-fract(-iuv*size*.5)));
	vec3 fin = t1.rgb * smoothstep(0.,fade,abs(uv.y));
		
	
	gl_FragColor = vec4(fin,1.0);
}
 