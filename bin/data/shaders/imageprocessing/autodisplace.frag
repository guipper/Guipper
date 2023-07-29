#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;

uniform float offsetx = .5;
uniform float offsety = .5;
uniform float rotacion = .5;
uniform float sc = .5;
void main() {
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t2 = texture(textura1, gl_FragCoord.xy / resolution);
	float t2_f = (t2.r + t2.g + t2.b) / 3.;

	vec2 uv2 = gl_FragCoord.xy / resolution;

	float limit = 0.5;
	float moffsetx = mapr(offsetx, -limit, limit);
	float moffsety = mapr(offsety, -limit, limit);

	uv2 += vec2(t2_f * moffsetx, t2_f * moffsety);

	uv2 -= vec2(.5);
	/*uv2 = scale(vec2(1.0,
				1.0))*uv2;
	uv2 = rotate2d(t2_f*mapr(rotacion,-PI,PI))*uv;
    */
	uv2 *= scale(vec2(1.0 + t2_f * mapr(sc, -1., 1.)));
	uv2 *= rotate2d(t2_f * mapr(rotacion, -PI, PI));
	uv2 += vec2(.5);

	vec4 t1 = texture(textura1, uv2);

	vec3 fin = t1.rgb;

	fragColor = vec4(fin, 1.0);
}
