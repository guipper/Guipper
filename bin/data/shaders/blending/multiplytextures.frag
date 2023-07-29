#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform sampler2D textura2;

void main() {
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 = texture(textura1, gl_FragCoord.xy / resolution);
	vec4 t2 = texture(textura2, gl_FragCoord.xy / resolution);

	vec3 fin = t1.rgb * t2.rgb;

	fragColor = vec4(fin, 1.0);
}
