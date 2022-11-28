#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

const float BPM = 130.;
uniform float ss ;
uniform float dif ; 

float getBPM(float _t){
	return fract(time/60.*BPM)/(BPM/60.0);
}
void main()
{
	vec2 uv=gl_FragCoord.xy/resolution;
	float fix = resolution.x/resolution.y;
	uv.x*=fix;
	
	float bpm1 = 1.0-getBPM(time);
	
	vec2 p  = vec2(0.5*fix,0.5) -uv;
	float r = length(p); 
		  //r = sin(r*10.+bpm1*1.);
	float e = 1.-smoothstep(bpm1*ss,
					        bpm1*ss+dif*bpm1,r);
    fragColor = vec4(vec3(e),1.);
}