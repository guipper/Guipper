#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
float det=.002;
const float maxdist=9.;
vec3 fcol;
// remove to use global time

mat2 rot2D(float a) {
	float s=sin(a);
    float c=cos(a);
    return mat2(c,s,-s,c);
}


#define titi mod(iTime,20.) 

float de(vec3 p) {
    vec3 pos=p*2.;
    float sc=1.;
    for (int i=0; i<5; i++) {
        float x=fract(atan(p.y,p.x)/3.1416*5.)-.5;
        float y=length(p.xy)-2.5-step(118.,titi)*2.;
        p=vec3(x,y,p.z)*1.5;
        p.yz*=rot2D(titi*.2+titi*smoothstep(35.,36.5,titi));
        sc*=1.5;
    }
    fcol=abs(normalize(p.grb+pos))+sqrt(abs(p.y))*.5;
	fcol*=1.+sin(length(p)+titi*10.);
    p/=sc;
    return min(length(p.xz)-.05,length(pos.xy)-1.5+pos.z*.1)*.5;
}

vec3 march(vec3 from, vec3 dir) {
    if (titi>100. && titi<118.) det+=.07;
    if (titi>218. && titi<236.) det+=.05;
	vec3 p, col=vec3(0.);
    float totdist=0., d;
    for (int i=0; i<200; i++) {
    	p=from+totdist*dir;
        d=de(p);
        if (totdist>maxdist) break;
    	totdist+=max(det,abs(d));   
		col+=fcol*pow(1.-totdist/maxdist,2.);
    }
	col*=.01;
    p=dir*maxdist;
    p*=.3;
    p+=.5;
	for (int i=0; i<9; i++) p=abs(p)/dot(p,p)-.8;
    col+=p*p*.005*abs(p.x)*step(det,.05);
    return col;
}


void main()
{
    vec2 uv=(gl_FragCoord.xy-.5*iResolution.xy)/iResolution.y;
    vec3 dir=normalize(vec3(uv,.7-fract(titi*.88)*.6*step(154.,titi)));
    vec3 from=vec3(sin(titi),sin(titi*.4)*2.,-4.);
    float a=fract(titi*.22)*2.;
    from.yz*=rot2D(a);
    dir.yz*=rot2D(a);
    from.xy*=rot2D(titi);
    dir.xy*=rot2D(titi);
	vec3 col=march(from, dir)*mod(fragCoord.y,3.)*.5;
    col+=smoothstep(1.8,2.,a);
    col*=smoothstep(0.,8.,titi);
    if (titi>36. && titi<54.5) col=1.-col;
    fragColor = vec4(col,1.0);
}