#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
/**

    Isometric Flat
    @byt3_m3chanic | 7/22/2021

    Continuing my isometric fun, and playing with
    mixing 2D and 3D 

*/

#define R   iResolution
#define M   iMouse
#define T   iTime
#define PI  3.14159265359
#define PI2 6.28318530718

#define MAX_DIST    100.
#define MIN_DIST    1e-4

float ga1,ga2,ga3,ga4,tmod;

mat2 turn,r45,rx,ry;
mat2 rot(float a){ return mat2(cos(a),sin(a),-sin(a),cos(a)); }
float hash21(vec2 p){ return fract(sin(dot(p,vec2(23.86,48.32)))*4374.432); }
float lsp(float begin, float end, float t) { return clamp((t - begin) / (end - begin), 0.0, 1.0); }

//@iq - all sdf's
float box(vec3 p, vec3 s) 
{
    p = abs(p)-s;
    return length(max(p, 0.))+min(max(p.x, max(p.y, p.z)), 0.);
}

float box( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

vec4 hex(vec2 uv, float scale)
{
    uv *= scale;
    const vec2 s = vec2(sqrt(3.), 1.);
    //id's and center - compacted to vec4's
    vec4 id = vec4(floor(uv/s),floor(uv/s + .5));
    vec4 cn = vec4(vec2(id.xy)+.5,vec2(id.zw)+.0)*vec4(s,s);
    vec2 p,cid;
    vec2 p1 = cn.xy - uv;
    vec2 p2 = cn.zw - uv;
    //nearest hexagon coords / id.
    if (length(p1) < length(p2)){
        p = p1;
        cid = id.xy;
    } else {
        p = p2;
        cid = id.zw + .5;
    }
    return vec4(p,cid);
}

void btn(inout vec4 p, float s, float f, float m)	
{
	p.xy = abs(p.xy + f) - abs(p.xy - f) - p.xy;
	float r = dot(p.xyz, p.xyz);
	if (r < m){
		if(m==.0) m=.000001;
		p /= m;
	}else if (r<1.){
		p /= r;
	}
	p *= s;
}

void tet(inout vec4 p, float k1, float k2, float k3, float k4) 
{
	p = abs(p);
	float k = (k1 - .5)*2.;
	p.xyz /= vec3(k2, k3, k4);

	if (p.x < p.y) p.xy = p.yx; p.x = -p.x;
	if (p.x > p.y) p.xy = p.yx; p.x = -p.x;
	if (p.x < p.z) p.xz = p.zx; p.x = -p.x;
	if (p.x > p.z) p.xz = p.zx; p.x = -p.x;

	p.xyz = p.xyz * k1 - k + 1.;
	p.xyz *= vec3(k2, k3, k4);
	p.w *= abs(k);
}

vec2 map (vec3 p) 
{
    p.yz*=r45;
    p.xz*=r45;
    p.xz*=rx;
    
    vec3 q = p;
    float orbits = .0;
    
    p.y-=ga1*5.;
    p.y=mod(p.y+2.5,5.)-2.5;
    
    vec4 P = vec4(p.xzy, 1.0); 
    for(int i = 0; i < 3; i++) {
        btn(P, 3.85, 2.5, .25);
        if(i == 1) orbits = max(length(P.xy)/PI,abs(P.z));
        tet(P, 1.5, 1., 1., 1.); 
    }
    
    float ln = .99*(abs(P.z)-05.)/P.w;
    float bx = box(q,vec3(1.25,4.,1.25));
    ln=max(ln,bx);
    
	return vec2(ln,floor(orbits*.5));
}

vec3 normal(vec3 p, float t)
{
    float e = MIN_DIST*t;
    vec2 h =vec2(1,-1)*.5773;
    vec3 n = h.xyy * map(p+h.xyy*e).x+
             h.yyx * map(p+h.yyx*e).x+
             h.yxy * map(p+h.yxy*e).x+
             h.xxx * map(p+h.xxx*e).x;
    return normalize(n);
}

vec3 hue(float t)
{ 
    const vec3 c = vec3(0.941,0.686,0.141);
    return .45 + .45*cos(time*.5+PI2*t*(c*vec3(.98,.99,.97))); 
}

void pattern(inout vec3 C, vec2 uv, float d)
{
    vec2 vuv = uv;
    vec2 uvp = uv;
    uv.y -=ga1;
    uvp.y+=ga1*2.;
    float pd = floor((uvp.y+1.)/2.);
    uvp.y=mod(uvp.y+1.,2.)-1.;
    float px = fwidth(uv.x);
    vec4 p    = hex(uv,8.);
    
    float hs  = hash21(p.zw);

    float sz =  (length(abs(p.z))*12.);
    float sy =  (length(abs(p.w))*12.);
    float sx = .18+.18*sin(sz+sy+T*2.75);//
    
    p.y*=1.5;
    p.xy*=rot(45.*PI/180.);
    
    float h2 = box(p.xy,vec2(sx));
    if(hs>.5) h2=abs(h2)-.02;
    h2=smoothstep(.01+px,-px,h2);
    
    uvp.y*=1.5;
    uvp.xy*=rot(45.*PI/180.);
    
    float h3 = box(uvp.xy+vec2(.2665),vec2(.675));
    h3 = max(h3,-box(uvp.xy+vec2(.2665),vec2(.475)));
    h3=smoothstep(px,-px,h3);
    
    vec3 h3c = hue( clamp((vuv.y+.5)*.25,0.,1.)+pd );
    C = mix(C,h3c,d>0.?h3:0.);
    
    if(p.z<-2. || p.z>1.5){
        C = mix(C,hue(hs),d>0.?h2:0.);
    }

}

void main()
{   
    // precal       
    
    r45=rot(-0.78539816339);
    turn=rot(T*5.*PI/180.);

    tmod = mod(time, 10.);
    float t1 = lsp(0.0, 5.0, tmod);
    float t2 = lsp(5.0, 10.0, tmod);
    ga1 = (t1)+floor(time*.1);
    ga2 = (t2)+floor(time*.1);
    rx=rot(-t2*PI); 
    //
    vec2 uv = (2.*fragCoord.xy-R.xy)/max(R.x,R.y);
    uv.y+=.5;
	uv.y = 1.0-uv.y;
	
    //@Flopine's isometric setup - tweaked
    //https://www.shadertoy.com/view/NtXSWS
    vec3 ro = vec3(uv*2.5,-10.);
    vec3 rd = vec3(0.,0.,1.);
    //
    
    //background
    vec3 C = mix(hue(3.),hue(32.),clamp((uv.y+1.)*.75,0.,1.));
    
    vec3 p;
    float d=0.,m;
    //marcher
    for(int i=0;i<150;i++)
    {
        p=ro+rd*d;
        vec2 ray = map(p);
        d += ray.x;
        m  = ray.y;
        if(abs(ray.x)<MIN_DIST ||d>MAX_DIST)break;
    }
    float alpha = 1.;
    if(d<MAX_DIST)
    {
        alpha = 0.;
        vec3 n = normal(p,d);
        vec3 lpos =vec3(-5.0, .25, -9.0);
        vec3 l = normalize(lpos-p);

        float diff = d*.1;//clamp(dot(n,l),0.,1.);
        
        float shdw = .95;
        for( float t=.06; t < 12.; )
        {
            float h = map(p + l*t).x;
            if( h<MIN_DIST ) { shdw = 0.; break; }
            shdw = min(shdw, 24.*h/t);
            t += h * .25;
            if( shdw<MIN_DIST || t>25. ) break;
        }

        diff = mix(diff,diff*shdw,.75);

        vec3 h = hue(m);

        C = vec3(diff)*h;
    } 
    pattern(C,uv,alpha);
    C=clamp(C,vec3(.03),vec3(1.));
    C=pow(C, vec3(.4545));
    fragColor = vec4(C,1.0);
}


