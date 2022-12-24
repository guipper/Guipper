#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// "Fractal Cartoon" - former "DE edge detection" by Kali

// There are no lights and no AO, only color by normals and dark edges.

// update: Nyan Cat cameo, thanks to code from mu6k: https://www.shadertoy.com/view/4dXGWH


//#define SHOWONLYEDGES
#define NYAN 
#define WAVES
#define BORDER

#define RAY_STEPS 150

#define BRIGHTNESS 1.
#define GAMMA 1.
#define SATURATION 0.8


#define detail .001
#define t iTime*.5

#define color1 vec3(245,183,214)/255.
#define color2 vec3(124,76,237)/255.
#define color3 vec3(235,41,104)/255.
//#define color3 vec3(255,127,0)/255.
#define color4 vec3(128,100,230)/255.
#define color5 vec3(39,255,82)/255.
#define color6 vec3(247,215,59)/255.

uniform sampler2D tx;


const vec3 origin=vec3(-1.,.7,0.);
vec3 sbcolor;
float det=0.0;
float rails=0., sberry=0.;

// 2D rotation function
mat2 rot(float a) {
	return mat2(cos(a),sin(a),-sin(a),cos(a));	
}

// "Amazing Surface" fractal
vec4 formula(vec4 p) {
		p.xz = abs(p.xz+1.)-abs(p.xz-1.)-p.xz;
		p.y-=.25;
		p.xy*=rot(radians(35.));
		p=p*2./clamp(dot(p.xyz,p.xyz),.2,1.);
	return p;
}

// Camera path
vec3 path(float ti) {
	ti*=1.5;
	vec3  p=vec3(sin(ti),(1.-sin(ti*2.))*.5,-ti*5.)*.5;
	return p;
}

float ring(vec3 p)
{
    p.xz *= rot(t*10.);
    p.xy *= rot(sin(t*10.)*.3);
    float cyl1=length(p.xz)-1.2;
    float cyl2=length(p.xz)-2.1;
    float d=max(cyl2,-cyl1);
    d=max(d,abs(p.y)-.01);
    return d;
}

float corona(vec3 p) 
{
    p.y-=.75;
    p.y*=1.+smoothstep(0.,1.5,abs(p.x));
    p*=1.+smoothstep(1.,0.,p.y)*.3;
    p.y*=1.5;
    p.y-=abs(.5-mod(p.x*2.,1.));
    float cyl=length(p.xz)-.5;
    cyl=max(cyl,abs(p.y)-.5);
    return cyl*.3;

}

float strawberry(vec3 p)
{
    p.x+=1.;
    p.y-=2.;
    //p.yz*=rot(t);
    vec3 p2=p;
    p.xz*=1.-p.y*.3;
    sbcolor=color3;
    float body=length(p)-1.;
    p=p2;
    float ri=ring(p);
    float d=min(body,ri);
    if (d==ri) sbcolor=vec3(1.);
    p=p2;
    p.y-=.5;
    p.z=abs(p.z);
    p.z-=.95;
    float sph1=length(p)-.105;
    p=p2;
    p.xz*=rot(.4);
    p.y-=.5;
    p.z=abs(p.z);
    p.z-=.95;
    float sph2=length(p)-.105;
    p=p2;
    p.xz*=rot(-.4);
    p.y-=.5;
    p.z=abs(p.z);
    p.z-=.95;
    float sph3=length(p)-.105;
    p=p2;
    p.xz*=rot(-.2);
    p.y-=.15;
    p.z=abs(p.z);
    p.z-=.97;
    float sph4=length(p)-.105;
    p=p2;
    p.xz*=rot(.2);
    p.y-=.15;
    p.z=abs(p.z);
    p.z-=.97;
    float sph5=length(p)-.105;
    float sph=min(min(min(min(sph1,sph2),sph3),sph4),sph5);
    d=min(d,sph);
    if (d==sph) sbcolor=vec3(1.2);
    float cor=corona(p2);
    d=min(d,cor);
    if (d==cor) sbcolor=color4;
    return d;
}


// Distance function
float de(vec3 pos) {
    sberry=0.;
    vec3 p2=pos;
    rails=0.;
#ifdef WAVES
	pos.y+=sin(pos.z-t*6.)*.15; //waves!
#endif
	float hid=0.;
	vec3 tpos=pos;
	tpos.z=abs(3.-mod(tpos.z,6.));
	vec4 p=vec4(tpos,1.);
	float m=100.;
    for (int i=0; i<4; i++) {
        p=formula(p);
    }
	float fr=(length(max(vec2(0.),p.yz-1.5))-1.)/p.w;
	float ro=max(abs(pos.x+1.)-.3,pos.y-.35);
		  ro=max(ro,-max(abs(pos.x+1.)-.1,pos.y-.5));
	pos.z=abs(.25-mod(pos.z,.5));
		  ro=max(ro,-max(abs(pos.z)-.2,pos.y-.3));
		  ro=max(ro,-max(abs(pos.z)-.01,-pos.y+.32));
    float d=min(fr,ro);
    if (d==ro) rails=1.;
    p2.z -= path(t).z - 8.;
    p2.xy *=rot(sin(t*2.)*.7);
    p2.y -= 2.;
    p2.yz *=rot(.6);
    float sb=strawberry(p2);
    d = min(d, sb);
    if (d==sb) sberry = 1.;
	return d;
}


// Calc normals, and here is edge detection, set to variable "edge"

float edge=0.;
vec3 normal(vec3 p) { 
	vec3 e = vec3(0.0,det*5.,0.0);

	float d1=de(p-e.yxx),d2=de(p+e.yxx);
	float d3=de(p-e.xyx),d4=de(p+e.xyx);
	float d5=de(p-e.xxy),d6=de(p+e.xxy);
	float d=de(p);
	edge=abs(d-0.5*(d2+d1))+abs(d-0.5*(d4+d3))+abs(d-0.5*(d6+d5));//edge finder
	edge=min(1.,pow(edge,.55)*15.);
	return normalize(vec3(d1-d2,d3-d4,d5-d6));
}


// Used Nyan Cat code by mu6k, with some mods

vec4 rainbow(vec2 p)
{
	float q = max(p.x,-0.1);
	float s = sin(p.x*7.0+t*70.0)*0.08;
	p.y+=s;
	p.y*=1.1;
	
	vec4 c;
	if (p.x>0.0) c=vec4(0,0,0,0); else
	if (0.0/6.0<p.y&&p.y<1.0/6.0) c= vec4(255,43,14,255)/255.0; else
	if (1.0/6.0<p.y&&p.y<2.0/6.0) c= vec4(255,168,6,255)/255.0; else
	if (2.0/6.0<p.y&&p.y<3.0/6.0) c= vec4(255,244,0,255)/255.0; else
	if (3.0/6.0<p.y&&p.y<4.0/6.0) c= vec4(51,234,5,255)/255.0; else
	if (4.0/6.0<p.y&&p.y<5.0/6.0) c= vec4(8,163,255,255)/255.0; else
	if (5.0/6.0<p.y&&p.y<6.0/6.0) c= vec4(122,85,255,255)/255.0; else
	if (abs(p.y)-.05<0.0001) c=vec4(0.,0.,0.,1.); else
	if (abs(p.y-1.)-.05<0.0001) c=vec4(0.,0.,0.,1.); else
		c=vec4(0,0,0,0);
	c.a*=.8-min(.8,abs(p.x*.08));
	c.xyz=mix(c.xyz,vec3(length(c.xyz)),.15);
	return c;
}

vec4 nyan(vec2 p)
{
	vec2 uv = p*vec2(0.4,1.0);
	float ns=3.0;
	float nt = iTime*ns; nt-=mod(nt,240.0/256.0/6.0); nt = mod(nt,240.0/256.0);
	float ny = mod(iTime*ns,1.0); ny-=mod(ny,0.75); ny*=-0.05;
	vec4 color = texture(tx,vec2(uv.x/3.0+210.0/256.0-nt+0.05,.5-uv.y-ny));
	if (uv.x<-0.3) color.a = 0.0;
	if (uv.x>0.2) color.a=0.0;
	return color;
}


// Raymarching and 2D graphics

vec3 raymarch(in vec3 from, in vec3 dir) 

{
	edge=0.;
	vec3 p, norm;
	float d=100.;
	float totdist=0.;
    float g=0.;
	for (int i=0; i<RAY_STEPS; i++) {
		if (d>det && totdist<25.0) {
			p=from+totdist*dir;
			d=de(p);
			det=detail*exp(.13*totdist);
			totdist+=d; 
            g+=max(0.,.05-d);
		}
	}
	vec3 col=vec3(0.);
	p-=(det-d)*dir;
	norm=normal(p);
#ifdef SHOWONLYEDGES
	col=1.-vec3(edge); // show wireframe version
#else
    //col=color3*max(0.,norm.x);
    //col+=color2*max(0.,-norm.x);
    //col+=color1*abs(norm.y);
    //col+=rails*-norm.y*.3;
    //col+=.5*abs(norm.z);
//    col+=rails*abs(norm.y);
    //col+=color3*abs(norm.z)*.5;
    //col+=color4*max(0.,-norm.z);
    col*=max(0.,1.-edge);
  col=(1.-abs(norm))*max(0.,1.-edge*.8); // set normal as color with dark edges
#endif		

    if (sberry>0.) {
        col=sbcolor;
        float dif=max(0.,-norm.y);
        col*=.7+dif*.4;
    }
    totdist=clamp(totdist,0.,26.);
	dir.y-=.02;
	float sunsize=7.; // responsive sun size
	float an=atan(dir.x,dir.y)+iTime*1.5; // angle for drawing and rotating sun
	float s=pow(clamp(1.0-length(dir.xy)*sunsize-abs(.2-mod(an,.4)),0.,1.),.1); // sun
	float sb=pow(clamp(1.0-length(dir.xy)*(sunsize-.2)-abs(.2-mod(an,.4)),0.,1.),.1); // sun border
	float sg=pow(clamp(1.0-length(dir.xy)*(sunsize-4.)-.5*abs(.2-mod(an,.4)),0.,1.),3.); // sun rays
	float y=smoothstep(1.5,-1.5,dir.y)*(1.-sb*.5); // gradient sky
	
	// set up background with sky and sun
	vec3 backg=color4*((1.-s)+(1.2-sb))-g;
         backg*=y;
         backg+=color6*s;
		 backg+=sg*(1.-length(dir.xy))*(1.-sb);
	col=mix(color6,col,exp(-.004*totdist*totdist));// distant fading to sun color
	if (totdist>25.) col=backg; // hit background
	col=pow(col,vec3(GAMMA))*BRIGHTNESS;
	col=mix(vec3(length(col)),col,SATURATION);
#ifdef SHOWONLYEDGES
	col=1.-vec3(length(col));
#else
	//col*=vec3(1.,.9,.85);
#ifdef NYAN
	dir.yx*=rot(dir.x);
	vec2 ncatpos=(dir.xy+vec2(-3.+mod(-t,6.),-.27));
	vec4 ncat=nyan(ncatpos*5.);
	vec4 rain=rainbow(ncatpos*10.+vec2(.8,.5));
//	if (totdist>8.) col=mix(col,max(vec3(.2),rain.xyz),rain.a*.9);
//	if (totdist>8.) col=mix(col,max(vec3(.2),ncat.xyz),ncat.a*.9);
#endif
#endif
	return col;
}

// get camera position
vec3 move(inout vec3 dir) {
	vec3 go=path(t);
	vec3 adv=path(t+.7);
	float hd=de(adv);
	vec3 advec=normalize(adv-go);
	float an=adv.x-go.x; an*=min(1.,abs(adv.z-go.z))*sign(adv.z-go.z)*.7;
	dir.xy*=mat2(cos(an),sin(an),-sin(an),cos(an));
    an=advec.y*1.7;
	dir.yz*=mat2(cos(an),sin(an),-sin(an),cos(an));
	an=atan(advec.x,advec.z);
	dir.xz*=mat2(cos(an),sin(an),-sin(an),cos(an));
	return go;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / iResolution.xy*2.-1.;
	uv.y*=-1.;
	vec2 oriuv=uv;
	uv.y*=iResolution.y/iResolution.x;
	vec2 mouse=(iMouse.xy/iResolution.xy-.5)*3.;
	mouse=vec2(0.,-0.05);
	float fov=.9-max(0.,.7-iTime*.3);
	vec3 dir=normalize(vec3(uv*fov,1.));
	dir.yz*=rot(mouse.y);
	dir.xz*=rot(mouse.x);
	vec3 from=origin+move(dir);
	vec3 color=raymarch(from,dir); 
	#ifdef BORDER
	color=mix(vec3(0.),color,pow(max(0.,.95-length(oriuv*oriuv*oriuv*vec2(1.05,1.1))),.3));
	#endif
	fragColor = vec4(color,1.);
}

