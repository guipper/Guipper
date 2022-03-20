#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define t time*.5
#define det .01

uniform float luna;
uniform float sol;
uniform sampler2DRect tex;
uniform bool estrellas;
uniform float lunaspeed;
uniform float color_r;
uniform float color_g;
uniform float color_b;

float obj, td, luncol, eclipse;
vec3 v1, v2;

//vec3 color_sun = vec3(1.2, 1., .8);
vec3 color_sun = vec3(color_r, color_g, color_b)*1.2;

mat2 rot(float a) {
    float si = sin(a);
    float co = cos(a);
    return mat2(co,si,-si,co);
}

float sphere(vec3 p, vec3 rd, float r){
	float b = dot( -p, rd ), i = b*b - dot(p,p) + r*r;
	return i < 0. ?  -1. : b - sqrt(i);
}

float hash(vec2 p)
{
   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float snois(vec3 uv, float res) //by trisomie21
{
    const vec3 s = vec3(1e0, 1e2, 1e4);	
	uv *= res;	
	vec3 uv0 = floor(mod(uv, res))*s;
	vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;	
	vec3 f = fract(uv); f = f*f*(3.0-2.0*f);	
	vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,
		      	  uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);	
	vec4 r = fract(sin(v*1e-3)*1e5);
	float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);	
	r = fract(sin((v + uv1.z - uv0.z)*1e-3)*1e5);
	float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);	
	return mix(r0, r1, f.z)*2.-1.;
}
float ksetmoon(vec3 p) {
	p=(p*.4+vec3(.14,.31,.51))*1.5;
	float l=1.;
	float ln=0.;
	float lnprev=0.;
	float expsmooth=0.;
	for (int i=0; i<13; i++) {
		p.xyz=abs(p.xyz);
		p=p/dot(p,p);
		p=p*2.-vec3(1.);
		if (mod(float(i),2.)>0.) {
			lnprev=ln;
			ln=length(p);
			expsmooth+=exp(-1./abs(lnprev-ln));
		}
	}
	return expsmooth;
}

float ksetsun(vec3 p) {
    float m = 1000.;
    for (int i = 0; i < 8; i++) {
        float d=dot(p,p);
        p = abs(p) / dot(p,p) - .7;
        m = min(m, abs(d - .5));
    }
    m = pow(max(0., 1. - m), 8.);
    return m;
}

float sun(vec3 from, vec3 dir) {
	from.xz*=rot(t*.5);
    dir.xz*=rot(t*.5);
    float c = 0.;
    float d = sphere(from, dir, 1.);
	float s = step(0.,d);
    for (float i = 0.; i < 10.; i++) {
        d = sphere(from, dir, 1. + i * .015);
        vec3 p = from + dir * d;
        float a = .3 * i + t*.05;
        p.yz*=rot(a);
        p.xy*=rot(a);
        c += ksetsun(p) * .15 * s;            
    }
    return c*.8*(1.15-hash(dir.xy+mod(t,10.))*.3);
}


float cor(vec2 p) {
	float ti=t*.02+200.;
    float d=length(p);
	float fad = exp((-4.)*d) * (1. - smoothstep(.2, .1, d));
    float v1 = fad;
	float v2 = fad;
	float angle = atan( p.x, p.y )/3.1416;
	float dist = length(p)*.24;
	vec3 crd = vec3( angle, dist, ti * .1 );
    float ti2=ti+fad*.8*.24;
    float t1=abs(snois(crd+vec3(0.,-ti2*1.,ti2*.1),15.));
	float t2=abs(snois(crd+vec3(0.,-ti2*.5,ti2*.2),45.));	
    float it=6.;
 	for( int i=1; i<=int(it); i++ ){
		ti*=1.5;
        float pw = pow(1.5,float(i));
		v1+=snois(crd+vec3(0.,-ti,ti*.02),(pw*50.*(t1+1.)))/it*.13;
		v2+=snois(crd+vec3(0.,-ti,ti*.02),(pw*50.*(t2+1.)))/it*.13;
    }
	float co = pow(v1 * fad, 1.5); 
	co += pow(v2 * fad, 1.5)*.5;
	co *= 1. - t1 * .3 * (1. - fad * .3);
    return clamp(co * 6.2, 0., 1.)*(1.2-hash(p+mod(t,10.))*.3);
}


float de(vec3 p) {
    p += v1;
    p.xz *= rot(t*lunaspeed+eclipse*.1*-sign(v1.x));
    float k = ksetmoon(p);
    p -= v1;
    luncol = 1.-k * .15;
    float sph1 = length(p + v1) - .9 + k * .003;
    return sph1;
}

float shadow(vec3 p, vec3 ldir) {
    float sh = 1.;
    p -= det * ldir;
    for (int i = 0; i < 50; i++) {
        p -= det * ldir;
        float d = de(p);
        if (d < det && obj < 1.) {
            sh = 0.;
            break;
        }
    }
    return sh;
}

vec3 nor(vec3 p) {
    vec3 d = vec3(0., det, 0.);
    return normalize(vec3(de(p + d.yxx), de(p + d.xyx), de(p + d.xxy)) - de(p));
}

vec3 shade(vec3 p, vec3 dir, vec3 ldir) {
    float sh = shadow(p, ldir);
    vec3 n = nor(p);
    float amb = .05 + max(0., dot(dir, -n)) * .2;
    float dif = max(0., dot(ldir, -n)) * .8 * sh;
	vec3 r = reflect(ldir,n);
	float spe=pow(max(0.,dot(dir, -r)),5.) * .7 * sh;
    vec3 col = mix(color_sun*vec3(1.,.5,.4), color_sun, smoothstep(0.,2.,eclipse)) * luncol;
    return (amb + dif) * col + spe * color_sun;
}


vec3 march(vec3 from, vec3 dir, vec2 uv) {
	vec3 p;
    vec3 backtex = texture2DRect(tex, gl_FragCoord.xy).rgb;
    vec3 col = vec3(0.);
    float d, td = 0., sh = 1., md = 100., g=0.;
    for (int i=0; i<80; i++) {
    	p = from + dir * td;
    	d = de(p);
        td += d * .8;
        if (d < det || td > md) break;
        g++;
    }
    if (d < det) {
        p -= dir * det * 2.;
        col = shade(p, dir, normalize(vec3(v2.xy,v1.z-3.)-v1)) * (1.-obj);
        sh = max(obj,step(v1.z, v2.z));
    } else {
        float e = smoothstep(0.,2.,eclipse);
        float s = sun(from + v2, dir) * max(.9,e);
        col += vec3(s, s * s, s * s * s) * color_sun;
        uv *= 1.-v2.z*.02;
		uv += v2.xy * .2;
        float c = cor(uv.xy*.95);
        col = max(col, vec3(c, c * c, c * c * c)) * color_sun;
        float at = atan(uv.y, uv.x);
        float a = abs(.5-fract(at/3.14*5.))*length(uv)*.3*sin(at*10.-t*5.);
		col += pow(pow(max(0.,1. - length(uv) + a), 4.7 - e * 3.) * .7 * color_sun * e, vec3(2.));
        float m = step(sphere(from + v2, dir, 1.),0.);
        if (estrellas) {
            vec2 q = dir.xy*5.+vec2(.4,.3);
            for (int i=0; i<6; i++) {
                q=abs(q)/dot(q,q)-1.;
            }
            col += length(q)*.005*m;
        } else {
            col += backtex*m;
        }
    }
    col += g*g*.0005*(.15+col);
	return col;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution.xy - .5;
    uv.x *= resolution.x/resolution.y;
    float d1 = smoothstep(0.,1.,luna)*1.;
    float d2 = smoothstep(0.,1.,sol)*1.4;
    v1 = vec3(d1*2., -abs(d1)+.3, 0.) ;
    v2 = vec3(-d2 * 3.5, -abs(d2)+.3, 0.);
    eclipse = distance(v1.xy, v2.xy);
    v1.z += 1. + smoothstep(3.,7.,eclipse)*30.;
    v2.z -= eclipse * 2.;
    uv *= 1.+eclipse*.05;
    vec3 from = vec3(0. ,0. , -50.);
    vec3 dir = normalize(vec3(uv, 10.));
    vec3 col = march(from, dir, uv);

	gl_FragColor = vec4(col, 1.); 
}