#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float scaley=.5;
uniform float scalex=.5;
uniform float offsetx=.5;
uniform float offsety=.5;
uniform float rotacion=.5;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	uv.x+=mapr(offsetx,-.5,.5);
	uv.y+=mapr(offsety,-.5,.5);
	uv-=vec2(.5);
	uv = scale(vec2(mapr(scalex,0.0,2.0),
				mapr(scaley,0.0,2.0)))*uv;
	uv = rotate2d(mapr(rotacion,-PI,PI))*uv;

	uv+=vec2(.5);
	
	vec4 t1 =  texture2D(textura1, uv);	
	vec3 fin = t1.rgb;
	
	fragColor = vec4(fin,1.0); 
}