#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float v1;

#define T(x) texture(iChannel0, fract((x)/iResolution.xy))  #define T(x) texture(sTD2DInputs[0], fract((x)/uTDOutputInfo.res.zw))  
void main()
{
	  vec2 u = gl_FragCoord.xy;     
	  //c=1./u.yyyx;     c=u.yyyx/1e4;///iTime;    
	  //for(float t=1.4; t<1e2; t+=t)     //  
	  
	  c += (c.gbar-c)/3.+T(u-c.wzt);    
	  for(float t=.6; t<4e2; t+=t){      
		c += c.gbar/4.-c.3+T(u-c.wz*t);     
		c = mix(T(u+c.xy), cos(c), .07); 
	  }
}
