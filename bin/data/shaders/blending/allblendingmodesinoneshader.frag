#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float opacity;
uniform float blendmode;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 =  texture(textura1, gl_FragCoord.xy/resolution);
	vec4 t2 =  texture(textura2, gl_FragCoord.xy/resolution);
	
	int bm = int(mapr(blendmode,0.0,25.0));
	float sourceAlpha = t2.a * clamp(opacity, 0.0, 1.0);
	float outputAlpha = t1.a + sourceAlpha * (1.0 - t1.a);
	vec3 overlap = blendMode(bm,t1.rgb,t2.rgb);
	vec3 premultiplied =
		(1.0 - sourceAlpha) * t1.rgb * t1.a +
		(1.0 - t1.a) * t2.rgb * sourceAlpha +
		t1.a * sourceAlpha * overlap;
	// Keep useful straight RGB even when both inputs are fully transparent;
	// alpha still controls whether it is visible at presentation time.
	vec3 fin = outputAlpha > 0.00001
		? premultiplied / outputAlpha
		: mix(t1.rgb, t2.rgb, clamp(opacity, 0.0, 1.0));

	fragColor = vec4(fin,outputAlpha);
}
