#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float deformator1;
uniform float puntas;
uniform float amp;
uniform float msize;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
   
   vec2 pos = vec2(0.5) - uv;
   
   float angle = atan(pos.x,pos.y);
   float radius = length(pos)*2.0;
   
   
   float size = 0.1;
   
   float mpuntas = floor(mapr(puntas,2.0,10.0));
   float mamp = mapr(amp,0.0,5.0);
   float formator = sin(radius)+abs(sin(angle*mpuntas+time*0.9+sin(radius*PI*mpuntas)*mamp))*0.5+
                    +cos(angle*5+time*2+sin(angle*18+sin(radius*10+sin(angle*5)*0.5)*10))*deformator1;
   
   formator = smoothstep(1,-.5,formator)+mapr(msize,0.0,1.0); 

   
   float r = smoothstep(formator,formator-0.5,radius)+radius/5;
   float g = smoothstep(formator-0.6,formator-0.9,radius)*0.85
   +smoothstep(formator-0.1,formator-1.2,radius)*0.85;
   
   float b = -smoothstep(formator-0.2,formator-0.9,radius)*0.1;
   
   
   
   gl_FragColor = vec4(r,g,b,1.0);
    
}
