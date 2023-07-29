#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float samples;
uniform sampler2D textura1;

void main() {
	vec2 uv = gl_FragCoord.xy;

	vec3 t1 = texture(textura1, gl_FragCoord.xy / resolution).rgb;

	vec3 res = vec3(0.);

	float scale = sqrt(samples);

	for(float i = 0.; i < samples; i++) {
		vec2 p = vec2(mod(i, scale), floor(i / scale)) - scale * .5;
		vec3 fb = texture(feedback, (gl_FragCoord.xy + p) / resolution).rgb;
		res += abs(t1 - fb);
	}

	res /= scale;

	fragColor = vec4(res, 1.0);
}
