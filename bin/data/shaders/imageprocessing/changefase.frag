#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

precision highp float;

uniform float fase_r_x = 0.5;
uniform float fase_r_y = 0.5;
uniform float fase_g_x = 0.5;
uniform float fase_g_y = 0.5;
uniform float fase_b_x = 0.5;
uniform float fase_b_y = 0.5;

uniform sampler2D textura1;

vec2 offset(float fase_x, float fase_y) {
	vec2 off = vec2(mapr(fase_x, -resolution.x / 2, resolution.x / 2), mapr(fase_y, -resolution.y / 2, resolution.y / 2));
	return off;
}
void main() {
	vec2 uv = gl_FragCoord.xy / resolution;

	vec2 uvr = gl_FragCoord.xy + offset(fase_r_x, fase_r_y);
	vec2 uvg = gl_FragCoord.xy + offset(fase_g_x, fase_g_y);
	vec2 uvb = gl_FragCoord.xy + offset(fase_b_x, fase_b_y);

	vec4 tr = texture(textura1, uvr / resolution);
	vec4 tg = texture(textura1, uvg / resolution);
	vec4 tb = texture(textura1, uvb / resolution);

	vec3 fin = vec3(tr.r, tg.g, tb.b);
	fragColor = vec4(fin, 1.0);

}
