#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float cant;
uniform float offset;
uniform float scale_factor;
uniform float zoom;

void main()
{
	vec2 uv = gl_FragCoord.xy-resolution*.5;
	float c=floor(cant*20.);
	float an=pi/c;
	float sc=mapr(scale_factor,.5,2.);
	uv/=sc;
	uv*=zoom;
	for (float i=0.; i<c; i++) {
		uv=abs(uv);
		uv*=sc;
		uv-=offset*resolution.x;
		uv*=rotate2d(an);
	}

	vec2 uv2 = abs(resolution-mod(uv+resolution*.5,resolution*2.));
	vec4 t1 =  texture2D(textura1, uv2/resolution);
	vec3 fin = t1.rgb;

	gl_FragColor = vec4(fin,1.0);
}
