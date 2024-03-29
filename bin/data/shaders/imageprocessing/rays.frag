#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float rays_samples;
uniform float ray_step;
uniform float ray_fade;
uniform float ray_r;
uniform float ray_g;
uniform float ray_b;
uniform float ray_brightness;
uniform float chroma_r;
uniform float chroma_g;
uniform float chroma_b;
uniform float color_threshold;
uniform float brightness_threshold;
uniform float orig_mix;
uniform sampler2D textura1;

void main()
{
	vec2 uv = gl_FragCoord.xy;

	vec3 t1 =  texture2D(textura1, gl_FragCoord.xy/resolution).rgb;

	vec3 res = vec3(0.);

	float sc=1.;

	vec3 ray_color=normalize(vec3(ray_r,ray_g,ray_b));

	vec3 chroma=normalize(vec3(chroma_r,chroma_g,chroma_b));

	vec2 p=gl_FragCoord.xy-resolution*.5;

	float iter=rays_samples*50.;

	for (float i=0.; i<iter; i++) {
		vec3 c = texture2D(textura1, (p*sc+resolution*.5)/resolution).rgb;
		sc*=mapr(1.-ray_step,.9,1.);
		res+=step(color_threshold,1.-max(0.,dot(1.-chroma,normalize(c))))
		*step(brightness_threshold*2.,length(c))
		*exp(-i*ray_fade*.2);
	}
	res/=iter;
	res=res*ray_color*ray_brightness*10.+t1*orig_mix;

	fragColor = vec4(res,1.0);
}
