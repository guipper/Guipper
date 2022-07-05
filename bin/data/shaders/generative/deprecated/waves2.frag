#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2DRect tex;
uniform float turbulence;
uniform float particlesbright;


float waves(vec2 p, float y) {
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
	return smoothstep(.0,.015,p.y+w-y);
}

float kset(vec3 p) {
	p=abs(.5-fract(p*.2));
	float m=100.;
	for (int i=0; i<9; i++) {
		p=abs(p)/dot(p,p)-.9;
		m=min(m,abs(length(p)-.5));
	}
	m=1.-pow(smoothstep(.0,.05,m),10.);
	return m*turbulence*.15*particlesbright;
}

mat2 rot(float a) {
	float s=sin(a);
	float c=cos(a);
	return mat2(c,s,-s,c);
}

float part(vec2 p) {
	float y=smoothstep(.0,.5,p.y);
	p.x*=1.-p.y*1.5;
	float c=0.;
	for (int i=0; i<10; i++) {
		vec3 p3=vec3(p,float(i)*.1+time*.05)+vec3(.3,.4,.5);
		p3.y-=time*(1.+float(i)*.01)*.2;
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
	float w1=waves(uv, .55);
	float w2=waves(uv, .7);
	float w3=waves(uv, .85);
	vec3 col=w1*vec3(0.,.15,.2);
	col-=w2*.15;
	col-=w3*.15;
	col = mix(col,back,1.-w1*.7);
	uv-=.5;
	uv.x*=resolution.x/resolution.y;
	col+=part(uv);
	fragColor = vec4(col, 1.); 
}