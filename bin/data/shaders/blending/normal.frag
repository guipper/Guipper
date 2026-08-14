#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float opacity;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture(textura2, gl_FragCoord.xy/resolution);
 
	// Ordinary layers: textura1 is the bottom layer and textura2 is the top.
	// Preserve the historical 0..0.5 opacity control while compositing alpha
	// with straight-RGBA source-over semantics.
	float layerOpacity = clamp(opacity * 2.0, 0.0, 1.0);
	float sourceAlpha = t2.a * layerOpacity;
	float outputAlpha = sourceAlpha + t1.a * (1.0 - sourceAlpha);
	vec3 premultiplied = t2.rgb * sourceAlpha +
		t1.rgb * t1.a * (1.0 - sourceAlpha);
	vec3 fin = outputAlpha > 0.00001
		? premultiplied / outputAlpha
		: mix(t1.rgb, t2.rgb, layerOpacity);

	fragColor = vec4(fin,outputAlpha);
}
