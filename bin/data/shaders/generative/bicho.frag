#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float puntas;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
   
       vec2 pos = vec2(0.5) - uv;
    float angle = atan(pos.x,pos.y);
    float radius = length(pos);
    
	
	
   float mpuntas = mapr(puntas,1.0,10.0);
  
	
    float r = sin(abs(sin(radius*PI*mpuntas)*abs(sin(angle)*PI*2.0*radius*sin(uv.y*PI*5*sin(radius*5)+time))));
    float g =smoothstep(0.2,0.5,r)*0.5;
    float b =smoothstep(0.2,0.3,r)*2.0*sin(r);
    ;

   
   
   
   fragColor = vec4(r,g,b,1.0);
    
}
