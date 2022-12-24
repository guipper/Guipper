#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform float fxrand ;

uniform float subdiv ;
uniform float width ;
uniform float height ;
uniform float huecos ;
uniform float modo ;

#define hash1 rnd(fxrand)
#define hash2 rnd(fxrand+.11)
#define hash3 rnd(fxrand+.22)
#define hash4 rnd(fxrand+.33)
#define hash5 rnd(fxrand+.44)
#define hash6 rnd(fxrand+.55)
#define hash7 rnd(fxrand+.66)
#define hash8 rnd(fxrand+.77)
#define hash9 rnd(fxrand+.88)
#define hash10 rnd(fxrand+.99)



mat2 rot2(float a)
{
    float s=sin(a);
    float c=cos(a);
    return mat2(c,s,-s,c);
}

float rnd(float p)
{
    p*=123.5678;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}


float st=.07, maxdist=15., det=.01;
vec3 ldir=vec3(0.,-1.,-1.),col=vec3(0.),lightpos;
float ll;
vec3 pal;
float mat=0.;
float eye=0., pupi=0.;
float abre=10.;

vec3 fractal2(vec2 p) {
    vec2 pos=p;
    float d, ml=100.;
    vec2 mc=vec2(100.);
    p=abs(fract(p*.1)-.5);
    vec2 c=p;
    for(int i=0;i<6;i++) {
        p*=rot2(1.6*hash1);
        d=dot(p,p);
        p=abs(p+.5)-abs(p-.5)-p;
    	p=p*-1.5/clamp(d,.5,1.)-c;
        mc=min(mc,abs(p));
        ml=min(ml,abs(p.y-.0));
    }
    mc=max(vec2(0.),1.-mc);
    mc=normalize(mc)*.8;
    ml=smoothstep(.2,.0,ml);
    vec3 cc = vec3(1.,.7,.5)*ml;
    //cc.xz*=rot2(mc.x*10.);
//    cc=fract(pos.y*5.)*vec3(.5);
return cc;
}

float hash(vec2 p)
{
    p*=130.;
	vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float rnd(vec2 n) { 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 48.5453);
}

float hash( float p ) 
{
    p*=1020.876554;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}

float noise2( in float p )
{
    float i = floor(p);
    float f = fract(p);
	float u = f*f*(3.0-2.0*f);

    float g0 = hash(i+0.)*2.0-1.0;
    float g1 = hash(i+1.)*2.0-1.0;
    return mix( g0*(f-0.0), g1*(f-1.0), u)-.5;
}

float noise2( in vec2 p )
{
    vec2 i = floor( p );
    vec2 f = fract( p );
	
	vec2 u = f*f*(3.0-2.0*f);

    float no= mix( mix( hash( i + vec2(0.0,0.0) ), 
                     hash( i + vec2(1.0,0.0) ), u.x),
                mix( hash( i + vec2(0.0,1.0) ), 
                     hash( i + vec2(1.0,1.0) ), u.x), u.y);
    return pow(no,.5);
}

float lit;

float fractal(vec2 p) 
{
   	//p=vec2(p.x/p.y,1./p.y);
	//p.y+=iTime*sign(p.y);
    p=fract(p*.1);
    float ot1=1000., ot2=ot1, it=0.;
	for (float i=0.; i<8.; i++) {
    	p=abs(p);
        p=p/clamp(p.x*p.y,0.15,5.)-vec2(1.5,1.);
        float m=abs(p.x);
        if (m<ot1) {
        	ot1=m+step(fract(iTime*.7+float(i)*.1),.5*abs(p.y));
            it=i;
        }
        ot2=min(ot2,length(p));
    }
    ot1=exp(-20.*ot1);
    ot2=exp(-5.*ot2);
    return ot1*.8;
}

float round(float x)
{
    if (fract(x)>.5) return floor(x)+1.;
    else return floor(x);
}

float map(vec2 p) {
    float sub=floor(subdiv*6.);
    float wid=3.+floor(width*5.);
    //sub=0.;
    //wid=3.;
    float f=sub+2.;
    p*=rot2(.5*iTime/max(.5,floor(f*length(p))/f));
    p=vec2(atan(p.x,p.y)/3.1416*wid,length(p));
    vec2 pos=p;
    float x=p.x;
    //p.x-=p.y*.7;
    //p*=4.;
    vec2 z=fract(p*f)-.5;
    p=floor(p*f)/f;
    float h=0.;
    //h-=step(abs(fract(x*f)-.5),.1)*.2;
    //h+=sin(p.x*3.);
    //h+=fract(p.y)*.5;
    h+=p.y*(1.+p.y)*.1; //.05
    float hu=floor(huecos*5.);
    float mo=floor(modo*2.);
    //h*=step(hu,mod(p.x,hu*2.)); //alterna espacios vacios
    h+=noise2(p*1100.)*1.5+step(8.,p.y)*100.;
    //h+=hash(p)*1.5+step(7.,p.y)*1000.-pow(abs(z.x),3.);
    float r=1.;
    h=h*step(r+.1,p.y)+step(p.y,r);
    h*=min(1.,iTime*10.-p.y*5.-35.-p.x*10.*step(1.,p.y)+lit*0.);
    h-=smoothstep(.45,.5,max(abs(z.x),abs(z.y)))*1.5;
    h-=step(pos.y,smoothstep(abre,abre+2.,iTime)*1.25-.15)*10.;
    //h=max(-2.,h);
	//lit=step(.9,fract(p.x*.1+p.y-iTime*.5));
    lit=step(.85,noise2(p*508.123))+.05;
    col=pow(noise2(p*1230.132),2.5)*step(lit,.5)+vec3(.0);
    //col=hash(p+2.1235)*step(lit,.5)+vec3(.0);
    //p.y-=floor(iTime);
    p.x+=floor(iTime*2.);
    col+=fractal(p)*step(15.,iTime)*.8;
    //h+=sin(p.x*2.);
    //h*=.5+height*.5;
    return h;
}

float box(vec3 p, vec3 c) {
    return length(max(vec3(0.),abs(p)-c));
}

float bounceOut(float t) {
  const float a = 4.0 / 11.0;
  const float b = 8.0 / 11.0;
  const float c = 9.0 / 10.0;

  const float ca = 4356.0 / 361.0;
  const float cb = 35442.0 / 1805.0;
  const float cc = 16061.0 / 1805.0;

  float t2 = t * t;

  return t < a
    ? 7.5625 * t2
    : t < b
      ? 9.075 * t2 - 9.9 * t + 3.4
      : t < c
        ? ca * t2 - cb * t + cc
        : 10.8 * t * t - 20.52 * t + 10.72;
}

float circularInOut(float t) {
  return t < 0.5
    ? 0.5 * (1.0 - sqrt(1.0 - 4.0 * t * t))
    : 0.5 * (sqrt((3.0 - 2.0 * t) * (2.0 * t - 1.0)) + 1.0);
}


float de(vec3 p) {
    //p+=sin(p*3.+iTime)*.05;
    p.y+=pow(smoothstep(abre+2.,abre,iTime-1.)*10.,2.);
    p.y-=sin(iTime*2.)*.1;
    //p.y-=2.5+noise2(vec2(iTime*.5))*.5;
    float s=sin(iTime*.5);
    p.xz*=rot2(s);
    p.z-=p.y*p.y*(sin(iTime*.25))*.05*min(1.,iTime-12.);
    //p.xy*=rot2(sin(iTime*.5)*.5);
    //p.yz*=rot2(iTime*.5);
    //p.x+=noise2(p.yz*6.+iTime*1.5)*.03;
    //p.y+=noise2(p.xz*6.-iTime*1.5)*.03;
    //p.z+=noise2(p.xy*6.+iTime*1.5)*.03;
    // p.x+=noise2(p.yz*5.+iTime*1.5)*.03;
    // p.y-=noise2(p.xz*5.+iTime*1.5)*.03;
    // p.z+=noise2(p.xy*5.+iTime*1.5)*.03;
    float t=iTime*.2;
    float d=100.;
    float r=1.;
    //d=sph(p,r);
    //d=box(p,r);
    //d=torus(p,r);
    //d=octahedron(p,r);
    //d=prism(p,r);
    //d=polygon(p,r,4.);
    d=box(p,vec3(2.5,5.,0.1));
    float f=fractal(p.xy);
    //mat=max(step(fract(p.x*2.+.25),.5),step(max(abs(p.x),abs(p.y-3.)),.6)*0.)-0.*step(max(abs(p.x),abs(p.y-3.)),.4);
    mat=step(fract((abs(p.x*.7)*.5)*(10.-p.y*1.5)),.5)+step(length(p+vec3(0.,-3.,0.)),.5);
    //mat=step(fract(.1+1.5*length((p-vec3(0.,3.,0.)))),.5)+step(fract(p.x*.5),.5)*0.;
    //mat=step(fract(abs(p.x*1.5))-step(length(p-vec3(0.,3.,0.)),.6)+step(length(p-vec3(0.,3.,0.)),.3)*2.,.5);
    //d=max(p.y-5.,length(p.xz)-.8);
    //mat=step(.5,fract(p.x*3.));
    //mat=step(.5,fract(atan(p.x,p.z)*3.+p.y));
    //mat=step(.5,fract(p.x*5.+floor(atan(p.y,p.z)*5.)*.5));
    //mat=step(1.3,length(sin(p*10.)));
    p.y-=3.;
    mat=max(mat,step(length(p.xy),.35));
    p.z-=.05;
    eye=max(abs(p.z)-.05,length(p.xy)-.2);
    pupi=length(p.xy);
    //p.z-=.5;
    //d=max(d,-length(p.xy)+.35);
    d=min(d,eye);
    return d*.8;
}


vec3 normal(vec2 p) {
	vec2 eps=vec2(0.,.001);
    return normalize(vec3(map(p+eps.yx)-map(p-eps.yx),2.*eps.y,map(p+eps.xy)-map(p-eps.xy)));
}

vec3 normal3(vec3 p) {
    vec2 e=vec2(0.,.001);
    return normalize(vec3(de(p+e.yxx),de(p+e.xyx),de(p+e.xxy))-de(p));
}
vec4 hit(vec3 p) {
    float h=map(p.xz);
    float d=de(p);
    return vec4(step(p.y,h),step(d,det*2.),h,d);
}

vec3 bsearch(vec3 p,vec3 dir) {
    //vec3 p;
    float ste=st;
    ste*=-.5;
    //td+=ste;
    float h2=1.;
    for (int i=0;i<20;i++) {
        p+=dir*ste;
        vec4 hi=hit(p);
        float h=max(hi.x,hi.y);
        if (abs(h-h2)>.001) {
            ste*=-.5;
	        h2=h;
        }
        //td+=ste;
    }
	return p;
}

vec3 desearch(vec3 p,vec3 dir) {
    p-=.2*dir;
    for (int i=0;i<20;i++) {
        float d=de(p);
        p+=d*dir;
        if (d<det) break;
    }
	return p;
}


vec3 shade(vec3 p,vec3 dir,float h,float td) {
    ldir=normalize(p-lightpos);
	col=vec3(0.);
    vec3 n=normal(p.xz);
    n.y*=-1.;
    float cam=max(.2,dot(dir,n));
    float dif=max(cam*.3,dot(ldir,n));
    vec3 ref=reflect(ldir,n);
    float spe=pow(max(0.,dot(dir,-ref)),20.);
    vec3 lig=lit*vec3(3.,.7,.2)*.8;
    return max(dif*.8,-n.y*.5)*(col*1.+lig*ll+pal)*.65+spe*.0;
}
float ddd;

vec3 march(vec3 from,vec3 dir) {
	vec3 p, col=vec3(0.), pr=p;
    float td=0.+hash(dir.xy+iTime)*.3, k=0.,g=0.,d=0.,eg=0.;
    p=from+td*dir;
    vec4 h;
    float ref=0.;
    float refindex=.9;
    float ey=0.;
    for (int i=0;i<180;i++) {
    	p+=dir*st;
        h=hit(p);
        if (h.y>.5 && ref==0.) {
            pr=p;
            ref=1.;
            p=desearch(p, dir);
            vec3 n=normal3(p);
   			dir=reflect(dir,n);
            p+=hash(dir.xy+iTime)*.1*dir;
            //dir.yz*=rot2(-2.);
        }
        float dd=abs(h.z-p.y);
        g=max(g,max(0.,.1-dd)/.1);
        ey=max(ey,max(0.,.1-eye)/.1);
        //ey+=.01/(.01+eye*eye)*.1;
        eg+=.01/(.01+h.w*h.w)*(1.-ref)*.0015;
        if (h.x>.5||td>maxdist||(h.y>.5&&(mat==1.||eye<det||eye<det))) break;
        td+=st;
    }
    vec3 back=pal*ref*.3+smoothstep(7.,0.,length(p.xz))*.13;
    if (h.x>.9) {
        p-=dir*det;
        p=bsearch(p,dir);
    	col=shade(p,dir,h.y,td);
        float dl=distance(p.xz,lightpos.xz);
        col*=min(.8,exp(-.023*dl*dl));
        //col=mix(back,col,exp(-.1*smoothstep(0.,5.,td*td)));
         mat=0.;
    } else if(eye>det) {
        col=back;
    } 
    col=mix(col,ref*.5*vec3(1.+mat*.3),.35*ref)*step(det,eye)+step(eye,det)*.15;
    col+=step(.3,pupi)*step(eye,det);
    ddd=0.;
    vec3 n=normal(p.xz);
    g*=step(0.05,dot(normalize(p),n));
    return max(col-g*.15-eg*(1.-ref),vec3(0.));
}

mat3 lookat(vec3 dir,vec3 up) {
	dir=normalize(dir);vec3 rt=normalize(cross(dir,normalize(up)));
    return mat3(rt,cross(rt,dir),dir);
}

vec3 path(float t) {
	return vec3(cos(t)*5.5,1.5-cos(t)*.0,sin(t*2.))*2.5;
}


void main(void)
{   
    pal=vec3(0.,.9,1.); ll=1.;
    if (hash9<.6) pal=vec3(1.,.5,.5),ll=.75;
    if (hash9<.3) pal=vec3(.7,1.,.4)*.8,ll=.75;

//    pal=vec3(0.,.9,1.); ll=1.;
    //pal=vec3(1.,.5,.5),ll=.75;
//    pal=vec3(.6,1.,.5)*.75,ll=.85;


    //pal=vec3(.8);
	
	vec2 fc2 = gl_FragCoord.xy;
	
	
    vec2 uv = (gl_FragCoord.xy-resolution.xy*.5)/resolution.y;
	
	uv.y+=1.0;
	uv.y = 1.0-uv.y;
   // vec2 uv = vTexCoord-.5;
    uv.x*=resolution.x/resolution.y;
   // vec2 uv = (fragCoord-iResolution.xy*.5)/iResolution.y;
	
    
    float t=time*.2;
    vec3 from=vec3(0.,6.,4.);
    from.yz*=rot2(mix(.5,hash1*.35-.24,smoothstep(0.,1.,iTime*.1)));
    //from.yz*=rot2(-.24);
    lightpos=vec3(0.,6.,0.);
    vec3 dir=normalize(vec3(uv,.5));
    vec3 adv=path(t+.1)-from;
    //dir.yz*=rot2(-.8);
    dir=lookat(-from+vec3(0.,1.,0.),vec3(0.,1.,0.))*dir;
    //dir.xz*=rot2(hash1*3.);
    //dir=lookat(adv+vec3(0.,-.2-(1.+sin(t*2.)),0.),vec3(adv.x*.1,1.,0.))*dir;
    vec3 col=march(from, dir)*min(1.,iTime*.5);
    //gl_FragColor = vec4(col,ddd)*step(abs(uv.x),1.5);//*mod(gl_FragCoord.y,4.)*.4;

    //col.rb*=rot2(-.5);

    //fragColor = vec4(col,1.0)*step(abs(uv.x),1.5);//*mod(gl_FragCoord.y,4.)*.4;
    fragColor = vec4(col,1.0);//*mod(gl_FragCoord.y,4.)*.4;
   
	//fragColor.a = 1.0;
	//gl_FragColor = vec4(uv.x,uv.y,.0,1.0);


   
}

