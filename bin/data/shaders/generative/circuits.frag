#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// shadertoy begin ----------------------------------------------------

// This content is under the MIT License.

#define ti iTime*.02
#define width .0025

#define fxrand floor(iTime*.1)
#define hash1 rnd(fxrand)
#define hash2 rnd(fxrand+.1)
#define hash3 rnd(fxrand+.2)
#define hash4 rnd(fxrand+.3)
#define hash5 rnd(fxrand+.4)
#define hash6 rnd(fxrand+.5)
#define hash7 rnd(fxrand+.6)

mat2 rot(float a)
{
    float s=sin(a);
    float c=cos(a);
    return mat2(c,s,-s,c);
}

float rnd(float p)
{
    p*=1234.5678;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}


float zoom = .08;

float shape=0.;
vec3 color=vec3(0.),randcol;

void formula(vec2 z, float c) {
	float minit=0.;
	float o,ot2,ot=ot2=1000.,it=0.;;
	for (int i=0; i<6; i++) {
        z=abs(z)/clamp(dot(z,z),.1,.5)-c;
        z*=rot(radians(90.));
        float l=length(z);
		o=min(max(abs(min(z.x,z.y)),-l+.25),abs(l-.25));
		ot=min(ot,o);
        if(ot==o) {
            it=float(i);
        }
		ot2=min(l*.1,ot2);
		minit=max(minit,float(i)*(1.-abs(sign(ot-o))));
	}
	minit+=1.;
	float w=width*minit*2.;
	float circ=pow(max(0.,w-ot2)/w,2.)*step(5.,it);
	shape+=max(pow(max(0.,w-ot)/w,.25),circ);
	vec3 col=vec3(1.,.9,.8);
	color+=col*(.4+mod(-ti*5.+ot2*5.+it*.1,.5)*1.6);
	color+=vec3(1.,.5,.05)*circ*(10.-minit)*1.5*smoothstep(0.,1.,.3+.3*fract(it*.15+sin(iTime*3.123)*10.6542));
    color*=clamp(iTime-it*.2+1.,0.,1.);
}


void main()
{
	vec2 pos = gl_FragCoord.xy / iResolution.xy - .5;
	pos.x*=iResolution.x/iResolution.y;
	vec2 uv=pos;
//	float sph = length(uv); sph = sqrt(1. - sph*sph)*1.5; 
//	uv=normalize(vec3(uv,sph)).xy;
	float a=ti+mod(ti,1.)*.5;
	vec2 luv=uv;
	float b=radians(45.*floor(hash4*4.));
	uv*=mat2(cos(b),sin(b),-sin(b),cos(b));
	uv+=vec2(hash1,hash2)*2.;
	uv*=zoom;
	float pix=.5/iResolution.x*zoom;
	float dof=1.;
	float c=1.;
	for (int aa=0; aa<36; aa++) {
		vec2 aauv=floor(vec2(float(aa)/6.,mod(float(aa),6.)));
		formula(uv+aauv*pix*dof,c);
	}
	shape/=36.; color/=36.;
	vec3 colo=mix(vec3(.15),color,shape);	
    if (hash5<.3) colo+=vec3(0.0,.05,0.);
    else if (hash5>.7) colo+=vec3(0.0,.0,0.1);
    else colo+=vec3(0.05,.0,0.);
	//colo*=vec3(1.2,1.1,1.0);
    colo+=(rnd(fragCoord.x+sin(fragCoord.y*432.)*1234.)-.5)*.1;
	fragColor = vec4(colo,1.0);
}

// shadertoy end --------------------------------------------------------

