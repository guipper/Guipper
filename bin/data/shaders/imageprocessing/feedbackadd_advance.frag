#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float eforce;
uniform float feedbackst;
uniform float fb_scale;
uniform float fb_rotate;
uniform float fb_fract;
uniform bool mode;
void main() {
	vec2 uv = gl_FragCoord.xy / resolution;
	//float fx = resolution.x/resolution.y;

	vec2 puv = gl_FragCoord.xy;
	puv -= resolution * .5;
	puv /= resolution;
	puv = fract(puv * mapr(fb_fract, 1.0, 3.0));
	puv *= resolution;

	puv -= resolution / 2;
	puv = scale(vec2(mapr(fb_scale, 0.5, 1.5))) * puv;
	puv += resolution / 2;
	puv -= (resolution / 2);
	puv = rotate2d(mapr(fb_rotate, -pi / 8, pi / 8)) * puv;
	puv += (resolution / 2);

	vec4 fb = texture(feedback, puv / resolution);
	vec4 t1 = texture(texture1, gl_FragCoord.xy / resolution);

	vec3 fin = vec3(0);
	if(mode) {
		fin = t1.rgb * eforce + fb.rgb * mapr(feedbackst, 0.0, 1.5);
	} else {
		fin = mix(fb.rgb, t1.rgb, t1.rgb);
	}
	fragColor = vec4(fin, 1.0);
}
