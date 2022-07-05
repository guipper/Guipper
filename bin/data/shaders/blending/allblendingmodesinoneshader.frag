#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float opacity;
uniform float blendmode;
void main()
{
	vec2 uv = fragCoord.xy / resolution;

	vec4 t1 =  texture2D(textura1, fragCoord.xy/resolution);
	vec4 t2 =  texture2D(textura2, fragCoord.xy/resolution);
	
	int bm = int(mapr(blendmode,0.0,25.0));
	vec3 fin = blendMode(bm,t1.rgb,t2.rgb,opacity);

	fragColor = vec4(fin,1.0);
}
