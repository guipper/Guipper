#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float eforce;
uniform float feedbackst;
void main() {
	vec2 uv = gl_FragCoord.xy / resolution;
	//float fx = resolution.x/resolution.y;

	vec2 puv = gl_FragCoord.xy;

	vec4 fb = texture(feedback, puv / resolution);
	vec4 t1 = texture(texture1, gl_FragCoord.xy / resolution);

	vec3 fin = vec3(0.);
	fin = t1.rgb * eforce + fb.rgb * mapr(feedbackst, 0.0, 1.5);

	fragColor = vec4(fin, 1.0);
}
