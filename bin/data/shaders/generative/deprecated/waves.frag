#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2DRect tex;
uniform float turbulence;
uniform float particlesbright;
uniform float height;
uniform float blur;
uniform float opacity;
uniform float watercolor_r;
uniform float watercolor_g;
uniform float watercolor_b;
uniform float colorvariation;

#pragma include "kset2.frag" 



float waves(vec2 p, float y, float blur) {
	float w=0.;
	float a=.3+pow(turbulence,2.)*.7;
	float s=.05;
	float t=time*10.;
	for (int i=0; i<3; i++) {
		p.x-=3.234+y*10.;
		p.x*=1.5;
		p.y+=sin(p.x*7.+t)*a*s;
		s*=.6543;
		t*=.5654;
    }
	return smoothstep(.0,.01+blur*.1,p.y+w-y+height-.3);
}



mat2 rot(float a) {
	float s=sin(a);
	float c=cos(a);
	return mat2(c,s,-s,c);
}

float part(vec2 p) {
	float y=smoothstep(.0,.5,p.y+height-.3);
	p.x*=1.-p.y*1.5;
	float c=0.;
	for (int i=0; i<10; i++) {
		vec3 p3=vec3(p,float(i)*.1+time*.05)+vec3(.3,.4,.5);
		p3.y-=time*(1.+float(i)*.01)*.3;
		p3.xz*=rot(.8);
		c+=kset(p3);
	}
	return c*y;
}

void main()
{
	vec3 back = texture2DRect(tex,gl_FragCoord.xy).rgb;
    vec2 uv = gl_FragCoord.xy / resolution.xy;
	vec2 d=vec2(0.,smoothstep(0.,.5,abs(uv.x-.5)));
	float w1=waves(uv, .55,0.);
	float w2=waves(uv, .7,blur);
	float w3=waves(uv, .85,blur);
	vec3 col=w1*vec3(watercolor_r,watercolor_g,watercolor_b)*.5;
	col-=w2*colorvariation*.5;
	col-=w3*colorvariation*.5;
	col = mix(col,back,1.-w1*opacity);
	uv-=.5;
	uv.x*=resolution.x/resolution.y;
	col+=part(uv);
	gl_FragColor = vec4(col, 1.); 
}