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

mat2 rot(float a) {
	float s=sin(a),c=cos(a);
	return mat2(c,s,-s,c);
}


vec3 fractal(vec2 uv) {
	vec3 p = vec3(uv*2.,sin(time*velocidad*.5));
	p.xy*=zoom*5.;
	vec3 m=vec3(1000.);
	float l=1000.;
	float it=int(mapr(fract_iter,5.,25.));
	for (int i=0; i<it; i++) {
		p.x*= sign(p.y);
		p.y=abs(p.y);
		p.xy*=rot(iTime*velocidad);
		p.xz*=rot(iTime*velocidad*.5);
		p=p*fract_s*2.-fract_c*2.;
		m=min(m,abs(p));
		l=min(l,length(p));
	}
	vec3 otrapm = pow(max(vec3(0.),1.-m),vec3(mapr(grosor,10.,100.)));
	float otrapl = pow(max(0.,1.-l),10.);
	return otrapm+otrapl*brillos*3.;
}



void main()
{
		vec2 uv = gl_FragCoord.xy/resolution;
		vec3 fback = texture2D(feedback,gl_FragCoord.xy/resolution).rgb;
		uv-=.5;
		uv.x*=resolution.x/resolution.y;
		vec3 c = fractal(uv);
		c.xy*=rotate2d(color1*PI*2.);
		c.xz*=rotate2d(color2*PI*2.);
		c=pow(abs(c),vec3(mapr(contraste,.5,3.)))*mapr(brillo,.5,3.);
		c=mix(vec3(length(c)*.7),c,saturacion);
		fragColor = vec4(mix(c,fback,mapr(feedback_mix,.5,1.)),1.);
}
