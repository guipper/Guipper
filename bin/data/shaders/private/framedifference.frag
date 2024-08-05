#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float limit;
uniform float force;
uniform sampler2D textura1;
uniform sampler2D textura2;
uniform bool origcolor;
void main() {
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 = texture(textura1, gl_FragCoord.xy / resolution);
	vec4 t2 = texture(textura2, gl_FragCoord.xy / resolution);
	vec4 fb = texture(feedback, gl_FragCoord.xy / resolution);

	vec3 dif = abs(t2.rgb - t1.rgb);

	vec3 fin = vec3(0.0);

	if(dif.r > limit || dif.g > limit || dif.b > limit) {

		if(origcolor) {
			fin = t1.rgb;
		} else {
			fin = dif;
		}
	}

	fin += fb.rgb * mapr(force, 0.0, 1.01);

	fragColor = vec4(fin, 1.0);
}
