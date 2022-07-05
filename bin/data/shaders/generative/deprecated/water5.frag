#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2DRect tex;
uniform float height;
uniform float turbina;
uniform float perspective;
uniform float brilloespuma;
uniform float brilloagua;
uniform float velocidadflujo;
uniform float transparencia;

const int ITER_GEOMETRY = 1;
const int ITER_FRAGMENT = 3;
const float SEA_HEIGHT = 0.3;
const float SEA_CHOPPY = 4.0;
const float SEA_SPEED = 0.8;
const float SEA_FREQ = 0.365;
const vec3 SEA_BASE = vec3(0.0,0.09,0.18);
const vec3 SEA_WATER_COLOR = vec3(0.2,0.55,1.)*0.5;
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




float espuma(vec2 p) {
	p.y*=.2;
	float c=0.;
	float s=1.;
	float t=(.3+velocidadflujo*2.);
	float a=3.;
	float n=(1.-smoothstep(0.,7.,abs(p.x)));
	for (int i=0; i<7; i++) {
		c+=noise(p*s)*a;
		p.y-=mod(time,1000.)*t;
		t*=.4;
		a*=.65;
		s*=1.5;
	}
	//c+=noise(p*10.);
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
	
	float tu=smoothstep(0.,5.,abs(p.x))*2.*turbina-turbina*2.;
	p.y+=tu;
	p.y-=smoothstep(0.,3.,p.z-5.)*tu;
	
    float dd = -p.y + 2. - h - espuma(-p.xz)*.15*turbina;
	p.y-=2.;
	p.z-=10.;
	return dd;
}

vec3 normal(vec3 p) {
	vec3 d=vec3(0.,.001, 0.);
	return normalize(vec3(de(p+d.yxx),de(p+d.xyx),de(p+d.xxy))-de(p));
}


vec3 shade(vec3 p, vec3 dir, vec3 n) {
	vec3 ldir=normalize(vec3(.0,2.,-1.)) ;
	float dif=max(0.,dot(ldir,-n))+.3;
	vec3 ref=reflect(ldir,dir);
	float spe=pow(max(0.,dot(ref,-n)),100.);
	return (SEA_WATER_COLOR+noise(p.xz)*.1)*dif+spe*brilloagua;
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
		g+=max(0.,.5-d)/.5;
	}
	vec3 back = texture2DRect(tex,gl_FragCoord.xy).rgb;
	float gl=g*.1*(1.-smoothstep(1.,3.,abs(p.x)))*turbina;
	if (d<det) {
		p-=det*dir;
		vec3 n = normal(p);
		col=mix(shade(p, dir, n),back,transparencia);
		float e=(espuma(-p.xz)*brilloespuma-gl*.2*brilloespuma)*(.5+.5*smoothstep(0.,5.,p.z+10.));
		col=mix(col,e*vec3(.5,.6,.7),min(1.,e*turbina));
	} else {
		col=back+gl;
	}
	return col;
}




void main()
{
   vec2 uv = gl_FragCoord.xy / resolution.xy - .5;
	uv.x*=resolution.x/resolution.y;
	uv.y+=height-.5;
	vec3 from = vec3(0.,perspective*2.,-10.);
	vec3 dir = normalize(vec3(uv, .8));
	vec3 col=march(from, dir);
	fragColor = vec4(col, 1.); 
}