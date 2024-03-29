#pragma include "../common.frag"
uniform float color1;
uniform float color2;
uniform float grosor;
uniform float brillos;
uniform float velocidad;
uniform float zoom;
uniform float fract_c;
uniform float fract_s;
uniform float fract_iter;
uniform float feedback_mix;
uniform float contraste;
uniform float brillo;
uniform float saturacion;
uniform float image_scale;
uniform sampler2DRect input_image;

vec3 fractal(vec2 uv) {
	vec3 p = vec3(uv*2.,sin(time*velocidad*.5));
	p.xy*=zoom*5.;
	float it=int(mapr(fract_iter,5.,25.));
	for (int i=0; i<it; i++) {
		p=abs(p)/dot(p,p)*mapr(fract_s,.5,3.)-mapr(fract_c,.2,2.);
	}
	return p;
}



void main()
{
		vec2 uv = fragCoord.xy/resolution;
		vec3 fback = texture2DRect(feedback,fragCoord.xy).rgb;
		uv-=.5;
		uv.x*=resolution.x/resolution.y;
		vec2 uvtex = mod(fractal(uv).xy*resolution*image_scale*3.,resolution);
		vec3 c = texture2DRect(input_image,uvtex).rgb;
		c.xy*=rotate2d(color1*PI*2.);
		c.xz*=rotate2d(color2*PI*2.);
		c=pow(abs(c),vec3(mapr(contraste,.5,3.)))*mapr(brillo,.5,3.);
		c=mix(vec3(length(c)*.7),c,saturacion);
		fragColor = vec4(mix(c,fback,mapr(feedback_mix,.5,1.)),1.);
}
