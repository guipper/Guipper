#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define t (iTime*.03)

float sphere(vec3 p, vec3 rd, float r){
	float b = dot( -p, rd ),
	  inner = b*b - dot(p,p) + r*r;
	return inner < 0. ?  -1. : b - sqrt(inner);
}

vec3 kset(vec3 p) {
	float m=1000., mi=0., l;
	for (float i=0.; i<20.; i++) {
		p = abs(p)/dot(p,p) - 1.;
        l=length(p);
        if (l < m) m=l, mi=i;

	}
	return normalize( vec3(1.) )
           * pow( max(0.,1.-m), 2.5+.3*sin(l*25.+t*50.) );
}

void main()
{
    vec2 R = iResolution.xy,  
        mo = vec2(.1,.2);
    vec2 uv = (gl_FragCoord.xy-.5*R)/ R.y;
    
    vec3 ro = -vec3(mo, 2.5-.7*sin(t*3.7535)),
	     rd = normalize(vec3(uv,3.)),
	     v = vec3(0), p;
    float tt,c=cos(t),s=sin(t);
	mat2 rot = mat2(c,-s,s,c);
    
	for (float i=20.; i<50.; i++) {
		tt = sphere(ro, rd, i*.03);
		p = ro+rd*tt;
		p.xy *= rot;
		p.xz *= rot;
		v = .9*v + .5*kset(p+p)*step(0.,tt);
	}

	fragColor.xyz = v*v *vec3(1.2,1.05,.9);
	fragColor.w = 1.;
}