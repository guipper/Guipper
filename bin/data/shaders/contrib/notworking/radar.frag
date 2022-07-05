#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define AA 1
const float tau =atan(1.)*8.;

highp float hash(float x) {
    return fract(sin(x)*43758.5453);
}
highp float rand(vec2 co) {
    return fract(sin(mod(dot(co.xy ,vec2(12.9898,78.233)),3.14))*43758.5453);
}
float mark(int n,float ang2,vec2 uv,float mn,float mx){
	float aa=float(AA)/iResolution.y;
    ang2-=tau/float(n)/2.;
	return smoothstep(mn,mx,distance(uv,vec2(0)))*smoothstep(aa*2.,0.,abs(fract(ang2/tau*float(n))-0.5)/float(n)*tau*distance(uv,vec2(0)));
}
float circle(float dist,vec2 uv){
	float aa=float(AA)/iResolution.y;
	return (smoothstep(dist-aa*2.,dist-aa,distance(uv,vec2(0)))-smoothstep(dist,dist+aa,distance(uv,vec2(0))))/10.;
}
float point(vec2 coord,vec2 uv,float ang){
	float aa=float(AA)/iResolution.y;
	return smoothstep(0.004+aa,0.002,distance(uv,coord))*smoothstep(tau*0.7,0.,ang)/2.;
}

void main(){
    vec2 uv = (gl_FragCoord.xy-iResolution.xy/2.)/iResolution.y;
	float aa=float(AA)/iResolution.y;
	
    //uv.x+=(rand(uv*iTime)-0.5)/100.*smoothstep(0.7,1.,hash(floor(iTime)));
    
    float dist=distance(uv,vec2(0));
	float sdist=smoothstep(0.5+aa,0.5,dist);
    
    float ang=iTime*tau/5.;
	float ang2=atan(uv.x,uv.y);
    ang=mod(ang2+ang,tau);
    
    float col=smoothstep(1.,0.,ang);
    col/=2.;
    col+=smoothstep(tau-aa/distance(uv,vec2(0)),tau,ang)/2.;
	col+=sdist/10.;
    col+=smoothstep(aa,0.,abs(uv.x))/4.;
    col+=smoothstep(aa,0.,abs(uv.y))/4.;
	
    col+=mark(9*4,ang2,uv,0.45,0.5)/2.;
    col+=mark(9*4*5,ang2,uv,0.475,0.5)/5.;
    col+=circle(0.1,uv);
    col+=circle(0.2,uv);
    col+=circle(0.3,uv);
    col+=circle(0.4,uv);
   	col+=point(vec2(0.3,0.1),uv,ang);
    col+=point(vec2(-0.2,-0.3),uv,ang);
    col+=point(vec2(0.2,-0.1),uv,ang);
    
    fragColor = mix(vec4(0,col,0,0),vec4(0.055,0.089,0.0,0)/1.3,1.-sdist);
	//o=o;
    
    float l=cos(gl_FragCoord.xy.y);
    l*=l;
    l/=3.;
    l+=0.6+rand(uv*iTime);
    
    fragColor*=l;
}