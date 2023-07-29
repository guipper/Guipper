#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float mixr = 1.0;
uniform float mixg = 1.0;
uniform float mixb = 1.0;

uniform sampler2D textura1;
uniform bool strobo;
void main() {
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 fb = texture(feedback, gl_FragCoord.xy / resolution);
	vec4 t1 = texture(textura1, gl_FragCoord.xy / resolution);

	vec3 fin_inv = vec3(1.0) - t1.rgb;

	vec3 fin = mix(t1.rgb, fin_inv, vec3(mixr, mixg, mixb));
	if(strobo) {
		fin = 1. - fb.rgb + t1.rgb;
	}

	fragColor = vec4(fin, 1.0);
}
