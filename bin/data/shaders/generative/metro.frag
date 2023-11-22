#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

const float BPM = 133.;
uniform bool invertbpm;
uniform float ss ;
uniform float dif ; 
uniform float shape ;

float getBPM(float _t,float _bpm){
	return fract(time/60.*_bpm)/(_bpm/60.0);
}


float sdRoundedBox( in vec2 p, in vec2 b, in vec4 r )
{
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    vec2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

float sdEquilateralTriangle( in vec2 p )
{
    const float k = sqrt(3.0);
    p.x = abs(p.x) - 1.0;
    p.y = p.y + 1.0/k;
    if( p.x+k*p.y>0.0 ) p = vec2(p.x-k*p.y,-k*p.x-p.y)/2.0;
    p.x -= clamp( p.x, -2.0, 0.0 );
    return -length(p)*sign(p.y);
}

float sdHexagram( in vec2 p, in float r )
{
    const vec4 k = vec4(-0.5,0.8660254038,0.5773502692,1.7320508076);
    p = abs(p);
    p -= 2.0*min(dot(k.xy,p),0.0)*k.xy;
    p -= 2.0*min(dot(k.yx,p),0.0)*k.yx;
    p -= vec2(clamp(p.x,r*k.z,r*k.w),r);
    return length(p)*sign(p.y);
}
void main()
{
	vec2 uv=gl_FragCoord.xy/resolution;
	float fix = resolution.x/resolution.y;
	uv.x*=fix;
	
	
	//CALCULAMOS BPM : 
	float bpm1 = 1.0-getBPM(time,BPM*1.0);
	if(invertbpm){
		bpm1 = getBPM(time,BPM*1.0);
	}else{
		bpm1 = 1.- getBPM(time,BPM*1.0);
	}
	
	

	float e = 0.0;
	
	
	//
	float mapshape = floor(shape*2.99);
	if(mapshape == 0.0){
		//CIRCULO
		vec2 p = vec2(0.5*fix,0.5) -uv;
		float r = length(p); 
		float siz = bpm1*ss;
		float dif1 = bpm1*ss+dif*bpm1;
		e = 1.-smoothstep(siz,
						  dif1,r);
	}
	if(mapshape == 1.0){
		//CUADRADO 
		vec2 uv2=gl_FragCoord.xy/resolution;
		float siz = dif*bpm1;
		float dif1 = ss*bpm1;
		uv2-=.5;
		e = sdBox(uv2,vec2(dif1));
		e = 1.-smoothstep(0.0,0.0+siz,e);
	} 
	if(mapshape == 2.0){
		vec2 uv3=gl_FragCoord.xy/resolution;
		
		float siz = dif*bpm1;
		float dif1 = ss*bpm1;
		
		/*uv3-=vec2(.5);
		uv3*=scale(vec2(mapr(ss,1.1,100.0)));
		uv3+=vec2(.5);
		*/
		uv3-=.5;
		e = sdHexagram(uv3,dif1);
		
		e = 1.-smoothstep(0.0,0.0+siz,e);
		
		e = abs(e)-siz;
	}
    fragColor = vec4(vec3(e),1.);
}