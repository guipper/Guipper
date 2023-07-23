#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float diffuse;
uniform float offsetx;
uniform float offsety;
uniform float cnt1;
uniform float cnt2;
uniform float zoom;
uniform float scale1;
uniform float freq;
uniform float amp;
uniform float fase;
uniform float speed;
//uniform float asdwd;
uniform float fuerzafeedback;
void main() {

	vec2 uv = gl_FragCoord.xy / resolution.xy;
	vec2 uv2 = uv;

	float fx = resolution.x / resolution.y;
	uv.x *= resolution.x / resolution.y;
	vec3 dib = vec3(0.0);
	float sc1 = 90.8;

	vec2 p = vec2(0.5) - uv2;
	p.x *= fx;
	float a = atan(p.x, p.y);
	float r = length(p);

	float e = 0.;

	float cnt = floor(mapr(cnt1, 2.0, 6.0));
	float mcnt2 = floor(mapr(cnt2, 1.0, 4.0));
	;

	float mffreq = mapr(freq, 20.0, 80.0);
	float mamp = mapr(amp, 1.0, 2.0);
	float mfase = mapr(fase, -PI * 2., PI * 2.);
	float mspeed = mapr(speed, 0.0, 1.0);
	float mkmof = mapr(scale1, 0.2, .7);
	float mzoom = mapr(zoom, 0.1, 1.0);
	vec3 c1 = vec3(sin(uv.x * 4. + time) * .5, sin(uv.y * 4. + time * 2.) * .5, cos(uv.x * 4. + time) * .5);
	vec3 c2 = vec3(sin(uv.y * 4. + time) * .5, sin(uv.x * 4. + time * 2.) * .5, cos(uv.y * 4. + time) * .5);

	c1 = mix(c1, vec3(1.0), 0.3);

	c2 = mix(c2, vec3(1.0), 0.3);

	vec2 m = vec2(mapr(offsetx, 0.0, 2.0) * fx, mapr(offsety, 0.0, 2.0));

	float mfuerzafeedback = mapr(fuerzafeedback, 0.75, 0.9);
	float msize = mapr(size, 0.9, 0.85);
	float mdiffuse = mapr(diffuse, 0.1, 0.2);
	for(float k = 0.0; k < mcnt2; k++) {
		vec3 cf = mix(c2, c1, 0.5);
		for(int i = 0; i < cnt; i++) {
			vec2 uv3 = uv;
		//uv3 =fract(uv3*4.);
			float idx = pi * 2.0 * float(i) / float(cnt) * mfase;

		//m.x-=time*0.02; 

			float def = smoothstep(0.3, 0.69, sin(uv.y * 5. - mspeed * 999.) * 0.1 * sin(uv.x * 5. + time));
			uv3 -= vec2(m.x, m.y);
			uv3 = rotate2d(idx + k * 0.8 + def) * uv3;
			uv3 += vec2(m.x * fx, m.y);

			uv3 -= vec2(m.x, m.y);
			uv3 = scale(vec2(k * mkmof + mzoom)) * uv3;
			uv3 += m;
			uv3.x += sin(mspeed + sin(mspeed * .25 * mspeed)) * 0.08;
			uv3.y += cos(mspeed + sin(mspeed * 0.5 * mspeed) * 0.9) * 0.2;
		//uv3*=sin(time*0.02)*10.;
		//uv3.x+=time*0.2;
			float e2 = sin(uv3.x * 5. + time * mspeed + sin(uv3.y * mffreq * mamp) + cos(uv3.x * mffreq * mamp));

			e2 *= sin(uv3.y * 10. + time * 2. * mspeed
		//+sin(uv3.x*20.+time)
			);

			e2 = smoothstep(msize, msize + mdiffuse, e2);

			e2 = clamp(e2, 0., 1.);
			e += e2;

			vec3 dibtoadd = vec3(e) * mix(c1, c2, float(i) / float(cnt));
	  // dib= mix(dib+vec3(e2)*cf,dib,dib*e2*cf);
			dib = mix(dib, dibtoadd, dibtoadd);
			dib = mix(vec3(length(dib)) * .5, dib, .5);
			dib += dibtoadd * 1.5;

		// dib = mix(dib+vec3(e2)*cf,dib,dib*e2);
	  // dib+= vec3(e2)*cf;
		}
	}

	vec2 puv = gl_FragCoord.xy / resolution;

	vec4 fb = texture(feedback, puv);

	vec3 fin = mix(fb.rgb * mfuerzafeedback, dib, dib);
	/*fin.r = 1.0 -fin.r;
	fin.g = .83 - fin.g;
	fin.b = .64 - fin.b;*/
	fragColor = vec4(fin, 1.0);

}
