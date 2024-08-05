#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float fase;
uniform float cnt;
uniform float speed;
void main() {
	vec2 uv = gl_FragCoord.xy / resolution;

	vec2 uv2 = gl_FragCoord.xy;

	float mapfase = mapr(fase, -pi / 2, pi / 2);
	float mapspeed = mapr(speed, -2.0, 2.0);
	int mapcnt = int(floor(mapr(cnt, 1.0, 20.0)));

	int cnt = 5;

	vec3 dib = vec3(0.0);
	for(int i = 1; i < mapcnt; i++) {

		float index = i * pi * 2 / mapcnt;

		//uv2 = fract(uv2);

		uv2 -= resolution / 2;
		//uv2 = rotate2d(index*mapfase+mapspeed*time)*uv2;
		uv2 = rotate2d(index + mapspeed * time) * uv2;

		uv2 += resolution / 2;

		/*uv2*=resolution;
		uv = fract(uv2);
		uv2/=resolution;*/
		vec4 t1 = texture(textura1, uv2 / resolution);
		dib += t1.rgb;
	}

	dib /= mapcnt;

	vec3 fin = dib;

	fragColor = vec4(fin, 1.0);
}
