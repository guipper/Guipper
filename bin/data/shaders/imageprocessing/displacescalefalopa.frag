#pragma include "../common.frag"
precision mediump float;
//vec3 verdejpupper(){return vec3(0.0,1.0,0.8);}

// we need the sketch resolution to perform some calculations

uniform sampler2D texture1;

uniform float offsetx = 0.5;
uniform float offsety = 0.5;
uniform float force1;
uniform float force2;

void main() {
	vec2 uv = gl_FragCoord.xy / resolution;
		// uv.y = 1.-uv.y;
	vec4 t2 = texture(texture1, uv);
	float t2_f = (t2.r + t2.g + t2.b) / 3.;

	vec2 uv2 = uv;
		 //uv2.y = 1.-uv.y;

	float limit = 0.5;
	float moffsetx = mapr(offsetx, -limit, limit);
	float moffsety = mapr(offsety, -limit, limit);

	uv2 += vec2(t2_f * moffsetx, t2_f * moffsety);

	uv2 += vec2(t2_f);
	uv2 *= scale(vec2(1.0 + sin(t2_f * 10. + time * 2.) * mapr(force1, 0.0, 0.8)));
	uv2 -= vec2(t2_f);

	uv2 += vec2(t2_f);
	uv2 *= scale(vec2(1.0 + sin(t2_f * 10. + time) * mapr(force2, 0.0, 0.8)));
	uv2 -= vec2(t2_f);

	//uv2*=resolution;
	vec4 t1 = texture(texture1, uv2);

	vec3 fin = t1.rgb;

	fragColor = vec4(t1.rgb, 1.0);

}
