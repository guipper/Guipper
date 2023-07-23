#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float cnt;
uniform float ite_scale;
uniform float speedrdm;
uniform float speedx = 0.5;
uniform float speedy = 0.5;
uniform float speedrot;
uniform float noisify;
uniform float golpe;
uniform float amp2;
uniform sampler2D tx;

void main() {
	vec2 uv = gl_FragCoord.xy / resolution;
	float fix = resolution.x / resolution.y;
	uv.x *= fix;
	vec2 puv = gl_FragCoord.xy;
	vec4 fb = texture(feedback, puv / resolution);

	vec3 dib = vec3(1.0);

	int mcnt = int(floor(mapr(cnt, 4.0, 15.0)));
	float mite_scale = mapr(ite_scale, .2, 0.95);
	float mspeedx = mapr(speedx, -0.005, 0.005);
	float mspeedy = mapr(speedy, -0.005, 0.005);
	float mspeedrot = mapr(speedrot, -0.005, 0.005);
	float mspeedrdm = mapr(speedrdm, 0.0, 0.1);

	//col1 = vec3(1.0,0.0,0.0);
	//col2 = vec3(0.0,0.0,1.0);
	for(int i = 1; i < mcnt; i++) {
		float fase = i * pi * 2. / mcnt;
		vec2 uv2 = uv;
		float indx = i / mcnt;
		uv2.x += time * mspeedx;
		uv2.y += time * mspeedy;

		uv2 -= vec2(0.5 * fix, 0.5);
		uv2 = rotate2d(mspeedrot * time) * uv2;
		uv2 += vec2(0.5 * fix, 0.5);

		uv2 -= vec2(0.5 * fix, 0.5);
		uv2 = scale(vec2(mite_scale * i)) * uv2;
		uv2 += vec2(0.5 * fix, 0.5);

		float e4 = sin(uv.y * 10. + golpe * 1. + time + sin(uv.x * 10.) * .5 + .5) * mapr(amp2, 0.0, 0.4);

		//uv2+=random2(uv2*uv.x*200.*f1+sin(uv.y*10.*f2+time)*0.1+time)*e4*4.*sin(uv.x*10.*f3+time);

		vec4 txx = texture(tx, gl_FragCoord.xy / resolution);
		float prom = (txx.r + txx.g + txx.b) / 3.;

		prom *= 10.5;

		float e = random2(uv2 * mite_scale * i, time * mspeedrdm + fase) * prom * .1;
		vec3 col1 = hsb2rgb(vec3(0.8, 1.0, 1.0));
		vec3 col2 = hsb2rgb(vec3(0.2, 0.8, 1.0));

		float cnt_cols = 5.;

		col1 = vec3(sin(e * 10.) * 1.5, 0.0, 0.5);
		col2 = vec3(sin(e * 10. + time) * 0.5, sin(e * 10.) * 1.5, 0.0);

		dib += vec3(e) * mix(col2, col1, e) * 2.5;
	}

	dib /= (mcnt + 1.);

	//dib = smoothstep(sm1,sm2,dib);
	dib = smoothstep(.0, 1., dib);

	vec3 fin = dib;

	fragColor = vec4(fin, 1.0);
}
