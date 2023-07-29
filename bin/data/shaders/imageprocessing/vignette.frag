#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float size;
uniform float difuse;
uniform bool respectaspectradio;
uniform bool quadvignette;

void main() {

	vec2 uv = gl_FragCoord.xy / resolution;
	float fx = resolution.x / resolution.y;
	vec2 uv2 = gl_FragCoord.xy;

	vec2 p = vec2(0.0);

	float mapsize = 1. - size;
	float mapdifuse = difuse;
	//uv.x*=fx;
	if(respectaspectradio) {
		uv.x *= fx;
		p = vec2(0.5 * fx, 0.5) - uv;
	} else {
		p = vec2(0.5) - uv;
	}

	float r = length(p);

	vec4 t1 = texture(textura1, uv2 / resolution);

	float v = 0.0;

	if(quadvignette) {
		if(respectaspectradio) {
			vec2 uv3 = vec2(uv.x / fx, uv.y);
			v = smoothstep(mapsize, mapsize + mapdifuse, uv3.x) *
				smoothstep(mapsize, mapsize + mapdifuse, 1. - uv3.x) *
				smoothstep(mapsize, mapsize + mapdifuse, 1. - uv3.y) *
				smoothstep(mapsize, mapsize + mapdifuse, uv3.y);
		} else {
			v = smoothstep(mapsize, mapsize + mapdifuse, uv.x) *
				smoothstep(mapsize, mapsize + mapdifuse, 1. - uv.x) *
				smoothstep(mapsize, mapsize + mapdifuse, 1. - uv.y) *
				smoothstep(mapsize, mapsize + mapdifuse, uv.y);
		}
	} else {
		v = (1. - smoothstep(mapsize, mapsize + mapdifuse, r));
	}

	v = smoothstep(mapsize, mapsize + mapdifuse, uv.x) *
		smoothstep(mapsize, mapsize + mapdifuse, 1. - uv.x) *
		smoothstep(mapsize, mapsize + mapdifuse, 1. - uv.y) *
		smoothstep(mapsize, mapsize + mapdifuse, uv.y);

	vec3 fin = t1.rgb * v;
	//fin = vec3(cir(uv,vec2(0.5*fx,0.5),0.4,0.0));
	fragColor = vec4(fin, 1.0);
}
