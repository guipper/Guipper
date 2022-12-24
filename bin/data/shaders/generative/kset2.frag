float kset(vec3 p) {
	p=abs(.5-fract(p*.2));
	float m=100.;
	for (int i=0; i<15; i++) {
		p=abs(p)/dot(p,p)-.9;
		m=min(m,abs(length(p)-.5));
	}
	m=pow(max(0.,1.-m),50.);
	return m*turbulence*.15*particlesbright;
}