#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

//precision mediump float;
//vec3 verdejpupper(){return vec3(0.0,1.0,0.8);}

//varying vec2 vTexCoord ;
uniform float startRandom ;

#define fx resolution.x/resolution.y
//#define PI 3.14159235659
//#define TWO_PI PI*2.
float sr; 
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

	float red =mapr(rdm(h+201.),0.0,1.);
	 float g =mapr(rdm(h+431.),0.0,1.);
	 float b =mapr(rdm(h+3023.),0.0,1.);

	 float spr =1.;
	 vec2 sp = vec2(mapr(rdm(h+21.),-spr,spr),
	 	         mapr(rdm(h+4031.),-spr,spr));
	 vec3 cf = vec3(red,g,b);

 	
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
	 
	 vec3 dib = cf +sin(e*10.+time);
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

	uv_mk.x+= mapr(rdm(h*4254.+idx*2154.),-.5,.5);
	uv_mk.y+= mapr(rdm(h*4653.+idx*157.),-.5,.5);
	float s = mapr(rdm(h+idx*325.),0.1,0.8);

	fin = mix(fin,d2,1.-mk(uv_mk,s));

	return fin;
}
void main(void) {
 vec2 uv = gl_FragCoord.xy / resolution.xy;
	vec2 puv = uv;
	
	
	float t  = floor(time*1.0)*0.01;
	sr = t;
 vec3 d1 = l2(uv,sr);
 

 const int cnt = 5;

 vec3 d1aux = d1;
 vec3 fin = d1;
	for(int i =0; i<cnt;i++){
		
		vec3 d2 = l2(uv,sr+120.+float(i)*4.);
		float idx = float(i)/float(cnt)*PI*2.;
		//MASK CIRCLE : 
		vec2 uv_c = uv;
		uv_c.x*=fx;

		float fase = rdm(sr*465.+float(i))*PI*2.;
		
		float fase2 = rdm(sr*465.+float(i))*PI*2.;

		float ampx = sin(rdm(sr*4587.+float(i)*535.)+time*.02)*.15+.05;
		float ampy = cos(rdm(sr*4587.+float(i)*535.)+time*.02)*.15+.05;
		float s = mapr(rdm(sr+float(i)*325.),0.1,0.2);
		vec2 p = vec2(0.5*fx,.5) - uv_c;
		float r = length(p);
		float a = atan(p.x,p.y);


		float amp_mof = mapr(rdm(sr*6384.+float(i)*5341.+t*10.),0.01,0.08);
		float mof = sin(a*5.+time)*amp_mof;
		float e = 0.0;
		
		float ridx = floor(mapr(rdm(sr+4685.+float(i)*579.*10.),0.0,4.0));
		
		if(i == cnt){
			ridx = 0.0;
		}
		
		e = 1.-sm(s,s+0.8,sin(r*10.+idx*40.+sin(time+idx))-mof);


		if(fin != d1aux && e > 0.001){
			fin = mix(fin,d2,e);
		}else{
			vec3 d3 = l2(uv,sr+620.+float(i)*57.);
			fin = mix(fin,d3,e);
		}
	}

	float prom = length(fin);
	
	
	vec4 fb2 = texture2D(feedback,uv);
	float fb2_prom = (fb2.r+fb2.g+fb2.b)/3.;
	
	puv-=vec2(0.5+fb2_prom*.001);
	puv*= scale(vec2(0.995+fb2_prom*.0001));
	puv+=vec2(0.5-fb2_prom*.001);
	
	vec4 fb = texture2D(feedback,puv);
		
	if(prom > 0.995){
		fin = vec3(0.0);
	}
	
	float prom2 = (fin.r+fin.g+fin.b)/3.;
	
	fin = smoothstep(0.1,0.9,fin);
	if( prom2 < 0.2){
		fin = fb.rgb*.992;
	}
	
	fragColor = vec4(fin, 1.0);
}