#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float f1 ;
uniform float f2 ;
uniform float f3 ;

void main()
{
  
    vec2 uv = gl_FragCoord.xy / resolution;
   
    
    
    float mf1 = mapr(f1,1.0,20.0);
	
    float mf2 = mapr(f2,1.0,20.0);
	
    float mf3 = mapr(f3,1.0,20.0);
    
    float r=  step(sin(distance(vec2(0.8),uv)*PI*3.+time+sin(uv.x*PI*mf1)+cos(uv.y*PI*5.)),0.1)*0.7;
	
    float g = step(cos(distance(vec2(0.5),uv)*PI*3.+time+sin(uv.x*PI*mf2)+cos(uv.y*PI*15.)),0.1)*0.7;
	
    float b = cos(distance(vec2(.2),uv)*PI*3.+time+sin(uv.x*PI*mf3)+cos(uv.y*PI*5.))*1500.+65.;
    fragColor = vec4(r,g,b,1.0);
}