#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float cnt;
uniform float amp;
uniform float rsc1;
uniform float rsc2;
uniform float rsc3;
uniform float asc_freq;
uniform float asc_amp;
uniform float detalle_amp;
uniform float detalle_freq;
uniform float faser;
uniform float faseg;
uniform float faseb;

float desf(vec2 uv, float fas) {

	float fix = resolution.x / resolution.y;
	float e = 0.0;

	int mcnt = int(floor(mapr(cnt, 1.0, 10.0)));
	int mcnt2 = int(floor(mapr(cnt, 0.0, 10.0)));

	float maprsc1 = mapr(rsc1, 0.0, 100.);
	float maprsc2 = mapr(rsc2, 0.0, 100.);
	float maprsc3 = mapr(rsc3, 0.0, 100.);
	float masc_freq = floor(mapr(asc_freq, 0.0, 30.0));
	float masc_amp = mapr(asc_amp, 0.0, 10.0);
	for(int i = 0; i < mcnt; i++) {

		float fase = i * pi * 2 / mcnt;
		float posx = sin(fase) * amp;
		float posy = cos(fase) * amp;
		vec2 p2 = vec2(0.5 * fix + posx, 0.5 + posy) - uv;
		float r2 = length(p2);
		float a2 = atan(p2.x, p2.y);
		e += sin(r2 * maprsc1 + time + fas + sin(r2 * maprsc2 + sin(r2 * maprsc3)) + sin(a2 * masc_freq) * masc_amp);
		e += sin(e * mapr(detalle_freq, 0.0, 20.)) * mapr(detalle_amp, 0.0, 0.4);
	}

	e /= mcnt;
	return e;
}

void main() {
	vec2 uv = gl_FragCoord.xy / resolution;
	float fix = resolution.x / resolution.y;
	uv.x *= fix;

	vec2 coords = gl_FragCoord.xy;
	vec4 fb = texture(feedback, coords * resolution.xy);

	vec2 p = vec2(0.5 * fix, 0.5);
	float r = length(p);
	float a = atan(p.x, p.y);

	vec3 dib = vec3(0.0);

	float dr = desf(uv, mapr(faser, -pi, pi));
	float dg = desf(uv, mapr(faseg, -pi, pi));
	float db = desf(uv, mapr(faseb, -pi, pi));

	float dr2 = desf(uv, 0.);
	float dg2 = desf(uv, 0.);
	float db2 = desf(uv, 0.);

	dib = vec3(dr, dg, db) + vec3(dr2, dg2, db2);

	vec3 fin = dib;

	fragColor = vec4(fin, 1.0);
}
