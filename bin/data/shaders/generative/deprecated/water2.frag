#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2DRect tex;
uniform float height;
uniform float turbina;
uniform float perspective;
uniform float brilloespuma;

const int ITER_GEOMETRY = 1;
const int ITER_FRAGMENT = 3;
const float SEA_HEIGHT = 0.3;
const float SEA_CHOPPY = 4.0;
const float SEA_SPEED = 0.8;
const float SEA_FREQ = 0.365;
const vec3 SEA_BASE = vec3(0.0,0.09,0.18);
const vec3 SEA_WATER_COLOR = vec3(0.3,0.5,1.)*0.5;
#define SEA_TIME (1.0 + mod(iTime * SEA_SPEED,100.))
const mat2 octave_m = mat2(1.6,1.2,-1.2,1.6);


float hashW( vec2 p ) {
	float h = dot(p,vec2(127.1,311.7));	
    return fract(sin(h)*43758.5453123);
}
float noiseW( in vec2 p ) {
    vec2 i = floor( p );
    vec2 f = fract( p );	
	vec2 u = f*f*(3.0-2.0*f);
    return -1.0+2.0*mix( mix( hashW( i + vec2(0.0,0.0) ), 
                     hashW( i + vec2(1.0,0.0) ), u.x),
                mix( hashW( i + vec2(0.0,1.0) ), 
                     hashW( i + vec2(1.0,1.0) ), u.x), u.y);
}

float kset(vec3 p) {
	p=abs(.5-fract(p*.2));
	float m=100.;
	for (int i=0; i<15; i++) {
		p=abs(p)/dot(p,p)-.9;
		m=min(m,abs(length(p)-.5));
	}
	m=pow(max(0.,1.-m),50.);
	return m;
}

mat2 rot(float a) {
	float s=sin(a);
	float c=cos(a);
	return mat2(c,s,-s,c);
}

float sea_octave(vec2 uv, float choppy) {
    uv += noiseW(uv);        
    vec2 wv = 1.0-abs(sin(uv));
    vec2 swv = abs(cos(uv));    
    wv = mix(wv,swv,wv);
    return pow(1.0-pow(wv.x * wv.y,0.65),choppy);
}




float part(vec2 p) {
	float y=smoothstep(.0,.5,p.y+height-.3);
	p.x*=1.-p.y*1.5;
	float c=0.;
	for (int i=0; i<10; i++) {
		vec3 p3=vec3(p,float(i)*.1+time*.05)+vec3(.3,.4,.5);
		p3.y-=time*(1.+float(i)*.01)*.3;
		p3.xz*=rot(.8);
		c+=kset(p3);
	}
	return c*y;
}


float espuma(vec2 p) {
	p.y*=.5;
	float c=0.;
	float s=1.;
	float t=.8;
	float a=3.;
	float n=(1.-smoothstep(.5,5.,abs(p.x)));
	for (int i=0; i<7; i++) {
		c+=noise(p*s)*a;
		p.y-=time*t;
		t*=.4;
		a*=.7;
		s*=1.5;
	}
	return c*.5*n;
}

float smin( float a, float b, float k )
{
    float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
    return mix( b, a, h ) - k*h*(1.0-h);
}


float de(vec3 p) {
    float freq = SEA_FREQ;
    float amp = SEA_HEIGHT + turbina*.3 * (1.-smoothstep(0.,3.,abs(p.x)));
    float choppy = SEA_CHOPPY;
    vec2 uv = p.xz; uv.x *= 0.5;
    
    float d, h = 0.0;    
    for(int i = 0; i < ITER_FRAGMENT; i++) {        
    	d = sea_octave((uv+SEA_TIME)*freq,choppy);
    	d += sea_octave((uv-SEA_TIME)*freq,choppy);
        h += d * amp;        
    	uv *= octave_m; freq *= 1.9; amp *= 0.22;
        choppy = mix(choppy,1.0,0.2);
    }
	
	float tu=smoothstep(0.,4.,abs(p.x))*2.*turbina-turbina*2.;
	p.y+=tu;
	p.y-=smoothstep(0.,3.,p.z-5.)*tu;
	
    float dd = -p.y + 2. - h - espuma(-p.xz)*.3*turbina;
	p.y-=2.;
	p.z-=10.;
	return dd;
}

vec3 normal(vec3 p) {
	vec3 d=vec3(0.,.001, 0.);
	return normalize(vec3(de(p+d.yxx),de(p+d.xyx),de(p+d.xxy))-de(p));
}


vec3 shade(vec3 p, vec3 dir) {
	vec3 ldir=normalize(vec3(.0,2.,-1.)) ;
	vec3 n=normal(p);
	float dif=max(0.,dot(ldir,-n))+.3;
	vec3 ref=reflect(ldir,dir);
	float spe=pow(max(0.,dot(ref,-n)),100.);
	return SEA_WATER_COLOR*dif+spe;
}


vec3 march(vec3 from, vec3 dir) {
	float td=0,d,det=.001;
	vec3 p, col=vec3(0.);
	float g=0.;
	for (int i=0; i<150; i++) {
		p=from+dir*td;
		d=de(p);
		if (d<det || td>25.) break;
		td+=d;
		g++;
	}
	vec3 back = texture2DRect(tex,gl_FragCoord.xy).rgb;
	if (d<det) {
		p-=det*dir;
		col=mix(shade(p, dir),back,.4);
		float e=espuma(-p.xz)*brilloespuma*smoothstep(0.,5.,p.z+11.);
		col=mix(col,e*vec3(.5,.6,.7),min(1.,e*turbina));
//	col+=e*turbina*vec3(.7);
	} else {
		col=back;
	}
	return col+g*g*.001*(1.-smoothstep(1.,3.,abs(p.x)))*turbina*smoothstep(0.,1.,p.y);
}




void main()
{
   vec2 uv = gl_FragCoord.xy / resolution.xy - .5;
	uv.x*=resolution.x/resolution.y;
	uv.y+=height-.5;
	vec3 from = vec3(0.,perspective*2.,-10.);
	vec3 dir = normalize(vec3(uv, .5));
	vec3 col=march(from, dir);
	gl_FragColor = vec4(col, 1.); 
}