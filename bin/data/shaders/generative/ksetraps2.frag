#pragma include "../common.frag"
uniform float color1;
uniform float color2;
uniform float velocidad;
uniform float zoom;
uniform float offset_x;
uniform float offset_y;
uniform float fract_iter;
uniform float fract_x;
uniform float fract_y;
uniform float fract_s;
uniform float fract_vary;
uniform bool feedback_mix;
uniform float contraste;
uniform float saturacion;


vec3 fractal(vec2 uv) {
	vec2 p=uv;
	p.x+=(offset_x-.5)*3.;
	p.y+=(offset_y-.5)*3.;
	p*=zoom*5.;
	float t=time*velocidad*.2;
	vec2 s=vec2(sin(t),cos(t))*(.1+fract_vary)*.5;
	vec2 m=vec2(1000.);
	float l=1000.;
	float b=1000.;
	float it=int(mapr(fract_iter,5.,20.));
	for (int i=0; i<it; i++) {
		p.x=abs(p.x);
		p=p*fract_s*3./dot(p,p)-vec2(fract_x+s.x,fract_y+s.y)*2.;
		m=min(m,abs(p));
		l=min(l,min(mod(abs(p.x),.2)/.2,mod(abs(p.y),.2)/.2));
		b=min(b,dot(p,p));
	}
	float otrapl = pow(max(0.,1.-l),20.);
	float otrapb = pow(max(0.,1.-b),3.);
	return otrapl*vec3(m,p.y)+otrapb;
}



void main()
{
		vec2 uv = gl_FragCoord.xy/resolution;
		vec3 fback = texture(feedback,gl_FragCoord.xy/resolution).rgb;
		uv-=.5;
		uv.x*=resolution.x/resolution.y;
		vec3 c = fractal(uv);
		c.xy*=rotate2d(color1*PI*2.);
		c.xz*=rotate2d(color2*PI*2.);
		c=pow(abs(c),vec3(mapr(contraste,1.,3.)));
		c=mix(vec3(length(c)*.7),c,saturacion);
		gl_FragColor = vec4(mix(c,fback,feedback_mix ? .993 : .5),1.);
}
