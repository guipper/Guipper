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