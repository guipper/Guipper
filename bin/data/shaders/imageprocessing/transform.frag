#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float scaley=.5;
uniform float scalex=.5;
uniform float offsetx=.5;
uniform float offsety=.5;
uniform float rotacion=.5;
// Declared LAST on purpose. Saved compositions store <param> blocks by
// position, so a uniform added anywhere above this line would shift every
// parameter of every composition already using this shader. Guipper gives a
// uniform with this name the standard 0.1x-4x zoom range and a 1.0 neutral,
// matching the scale ratio on the media and camera boxes.
uniform float scaleratio=1.0;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	uv.x+=mapr(offsetx,-.5,.5);
	uv.y+=mapr(offsety,-.5,.5);
	uv-=vec2(.5);
	// Divided, not multiplied: scale() multiplies the uv, so a larger factor
	// samples further out and shrinks the image. Dividing makes a bigger ratio
	// mean a bigger picture, the same direction as every other scale ratio.
	// At the neutral 1.0 this is exactly the previous expression.
	float zoom = max(scaleratio, 0.0001);
	uv = scale(vec2(mapr(scalex,0.0,2.0)/zoom,
				mapr(scaley,0.0,2.0)/zoom))*uv;
	uv = rotate2d(mapr(rotacion,-PI,PI))*uv;

	uv+=vec2(.5);
	
	vec4 t1 =  texture(textura1, uv);	
	vec3 fin = t1.rgb;
	
	fragColor = vec4(fin,1.0); 
}