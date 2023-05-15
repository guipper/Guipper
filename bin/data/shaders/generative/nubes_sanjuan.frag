#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float nubesize ;
uniform float seed;
uniform float sep;
uniform float fbmsc;
mat2 scale3(vec2 _scale){
    return mat2(_scale.x,0.0,
                0.0,_scale.y);
}
float rdm(float p){
    p*=1234.56;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}
const mat2 m = mat2( 1.6,  1.2, -1.2,  1.6 );

vec2 hash( vec2 p ) {
	p = vec2(dot(p,vec2(127.1,311.7)), dot(p,vec2(269.5,183.3)));
	return -1.0 + 2.0*fract(sin(p)*43758.5453123);
}

float noise2( in vec2 p ) {
    const float K1 = 0.366025404; // (sqrt(3)-1)/2;
    const float K2 = 0.211324865; // (3-sqrt(3))/6;
	vec2 i = floor(p + (p.x+p.y)*K1);	
    vec2 a = p - i + (i.x+i.y)*K2;
    vec2 o = (a.x>a.y) ? vec2(1.0,0.0) : vec2(0.0,1.0); //vec2 of = 0.5 + 0.5*vec2(sign(a.x-a.y), sign(a.y-a.x));
    vec2 b = a - o + K2;
	vec2 c = a - 1.0 + 2.0*K2;
    vec3 h = max(0.5-vec3(dot(a,a), dot(b,b), dot(c,c) ), 0.0 );
	vec3 n = h*h*h*h*vec3( dot(a,hash(i+0.0)), dot(b,hash(i+o)), dot(c,hash(i+1.0)));
    return dot(n, vec3(70.0));	
}
float fbm2(vec2 n) {
	float total = 0.0, amplitude = 0.1;
	for (int i = 0; i < 7; i++) {
		total += noise2(n) * amplitude;
		n = m * n;
		amplitude *= 0.4;
	}
	return total;
}


float nube(vec2 uv,float _seed){
	

	float s  =mapr(nubesize,0.0,0.2);
	float d = 0.01;
	float ss = 0.0;
	float fx = resolution.x/resolution.y;
	
	for(int i=0; i<8; i++){
		vec2 uv2 = uv;
		uv2 = fract(uv2);
		uv2.x*= fx;
		
		uv2.x+= mapr(rdm(_seed+12314.),-.5,.5);
		uv2.y+= mapr(rdm(_seed+321.),-.5,.5);

		uv2.x+=mapr(rdm(_seed+4134.+i*4321.),-sep,sep);
		uv2.y+=mapr(rdm(_seed+3241.+i*2134.),-sep,sep);
		
		vec2 p = vec2(.5*fx,.5) -uv2;
		float r = length(p);
		
		ss+= 1.-smoothstep(s,s+d,r);
	}
	return ss;
	
}



void main( ) {
   // vec2 p = gl_FragCoord.xy / iResolution.xy;
	vec2 uv = gl_FragCoord.xy / iResolution.xy;
	
	vec3 c1 = vec3(145./255.,209./255.,204./255.);
	vec3 c2 = vec3(215./255.,230./255.,229./255.);
	
	vec3 fin = mix(c1,c2,uv.y);
	vec2 uvc = uv;
	
	uvc-=vec2(.5);
	uvc*=scale3(vec2(5.9));
	uvc+=vec2(.5);
	
	
	for(int i=0; i<10; i++){
		uv.x+=time*0.001;
		
		float n = 0.0;
		n = nube(uv,seed+i*410.) ;
		float ruido = 1.- smoothstep(-0.125,0.0,fbm2(uv*mapr(fbmsc,0.0,100.0))*1.);
		n*=ruido; 
		fin+= n;
	}
	
	//float ss = 0.0;
	//fin = vec3(smoothstep(0.0,0.01,fbm2(uv*5.)*1.));
	fragColor = vec4(fin, 1.0 );
}