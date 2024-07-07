#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

//precision mediump float;
//vec3 verdejpupper(){return vec3(0.0,1.0,0.8);}

//varying vec2 vTexCoord ;
uniform float startRandom ;
uniform float offset;
uniform float escala;
uniform float xx;
uniform float yy;

#define fx resolution.x/resolution.y
#define h1 (rdm(startRandom))
#define h2 (rdm(startRandom+2.))
//#define PI 3.14159235659
//#define TWO_PI PI*2.
#define sr startRandom 
float rdm(float p){
    p*=1234.56;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}

/*float mapr(float value,float low2,float high2) {
	 return low2 + (high2 - low2) * (value - 0.) / (1. - 0.);
}*/
/*
mat2 rotate2d(float _angle){
    return mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle));
}
mat2 scale(vec2 _scale){
    return mat2(_scale.x,0.0,
                0.0,_scale.y);
}*/

float sm(float m1,float m2, float e){
	return smoothstep(m1,m2,e);
}
/*float poly(vec2 uv,vec2 p, float s, float dif,int N,float a){
    // Remap the space to -1. to 1.
    vec2 st = p - uv ;
    // Angle and radius from the current pixel
    float a2 = atan(st.x,st.y)+a;
    float r = TWO_PI/float(N);
    float d = cos(floor(.5+a2/r)*r-a2)*length(st);
    float e = 1.0 - smoothstep(s,dif,d);
    return e;
}*/


vec3 l2(vec2 uv,float h){

 	float red =mapr(rdm(h+201.),0.6,1.);
	 float g =mapr(rdm(h+431.),0.6,1.);
	 float b =mapr(rdm(h+3023.),0.6,1.);

	 float spr =1.;
	 vec2 sp = vec2(mapr(rdm(h+21.),-spr,spr),
	 	         mapr(rdm(h+4031.),-spr,spr));
	 vec3 cf = vec3(red,g,b);
		
	vec3 colors[11];
	colors[0] = vec3(239./255.,71./255.,32./255.);
	colors[1] = vec3(255./255.,214./255.,44./255.);
	colors[2] = vec3(34./255.,71./156.,255./255.);
	colors[3] = vec3(245./255.,171./255.,208./255.);
	colors[4] = vec3(4./255.,186./255.,101./255.);
	colors[5] = vec3(197./255.,97./255.,45./255.);
	colors[6] = vec3(15./255.,117./255.,253./255.);
	colors[7] = vec3(0./255.,0./255.,0./255.);
	colors[8] = vec3(154./255.,83./255.,241./255.);
	colors[9] = vec3(108./255.,254./255.,181./255.);
	colors[10] = vec3(254./255.,172./255.,0./255.);
	
	
	int idx = int(mapr(rdm(sr*4254.+uv.x*0.001),0.0,11.));
	  
	 cf = colors[idx];
 	
	 float fr = mapr(rdm(h+453.),5.0,30.);
	 uv.x*=fx;
	 uv-=vec2(.5);
	 uv*=rotate2d(rdm(h+324.)*PI*2.);
	 uv+=vec2(.5);
	 uv =fract(uv*fr+vec2(time)*sp);
	 
	 vec2 p =vec2(0.5*fx,.5)-uv;
	 float r = length(p);

	float ridx = floor(mapr(rdm(h+4685.),0.0,3.0));

	float e = 0.0; 

	if(ridx == 0.0){

		e = 1.-sm(0.1,0.2,uv.x);
	}else if(ridx == 1.0 ){

 		e = 1.-sm(0.1,0.2,uv.x);
 		e+= 1.-sm(0.1,0.2,uv.y);
	}else if(ridx == 2.0){
		vec2 p =vec2(0.5,.5)-uv;
	 	float r = length(p);
	 	e = 1.-sm(0.1,0.2,r);

	}
	 
	 vec3 dib = cf +e;
	 return dib;
}

vec3 mk(vec2 uv,float s1){

	uv.x*=fx;
	vec2 p = vec2(.5*fx,.5) -uv;
	float r = length(p);
	float e = sm(s1,s1+0.01,r);
	return vec3(e);
}
vec3 dibf(vec2 uv,vec3 fin,float idx,float h){
	vec3 d2 = l2(uv,h*float(idx));
	vec2 uv_mk = uv;

	uv_mk.x+= mapr(rdm(h*4254.+idx*2154.),-.35,.35);
	uv_mk.y+= mapr(rdm(h*4653.+idx*157.),-.35,.35);
	float s = mapr(rdm(h+idx*325.),0.1,0.2);

	fin = mix(fin,d2,1.-mk(uv_mk,s));

	return fin;
}

vec3 getPamiColor(float c) {
		vec3 colors[11];
	colors[0] = vec3(239./255.,71./255.,32./255.);
	colors[1] = vec3(255./255.,214./255.,44./255.);
	colors[2] = vec3(34./255.,71./156.,255./255.);
	colors[3] = vec3(245./255.,171./255.,208./255.);
	colors[4] = vec3(4./255.,186./255.,101./255.);
	colors[5] = vec3(197./255.,97./255.,45./255.);
	colors[6] = vec3(15./255.,117./255.,253./255.);
	colors[7] = vec3(0./255.,0./255.,0./255.);
	colors[8] = vec3(154./255.,83./255.,241./255.);
	colors[9] = vec3(108./255.,254./255.,181./255.);
	colors[10] = vec3(254./255.,172./255.,0./255.);
	
	
	int idx1=int(mod(floor(c*escala*50+offset*50.+floor(sr*10)*234),11.));  
	int idx2=int(mod(floor(c*escala*50+offset*50.+floor(sr*10)*543),11.));  
	vec3 cf = mix(colors[idx1],colors[idx2], fract(sr*10));
	return cf;
}

void main(void) {
 vec2 uv = gl_FragCoord.xy / resolution.xy;
	
	vec3 fin = getPamiColor(length(uv-1.+vec2(xx, yy)));
	fragColor = vec4(fin, 1.0);
}
