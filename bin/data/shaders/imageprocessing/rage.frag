#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float rays_samples;
uniform float ray_step;
uniform float ray_fade;
uniform float ray_brightness;
uniform float orig_mix;
uniform sampler2D textura1;

void main()
{
	vec2 uv = gl_FragCoord.xy;

	vec3 t1 =  texture2D(textura1, gl_FragCoord.xy).rgb;

	vec3 res = vec3(0.);

	float sc=1.;

	vec2 p=gl_FragCoord.xy-resolution*.5;

	float iter=rays_samples*200.;

	for (float i=0.; i<iter; i++) {
		vec3 c = texture2D(textura1, (p*sc+resolution*.5)/resolution).rgb;
		sc*=mapr(1.-ray_step,.9,1.);
		res+=smoothstep(0.,1.,c)*exp(-i*ray_fade*.2);
	}
	res/=iter;
	res=res*ray_brightness*5.+t1*orig_mix;

	gl_FragColor = vec4(res,1.0);
}
