#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float sc;
uniform float posx;
uniform float posy;
uniform float fuerzaruido;
uniform float rotacion;
uniform float frac;

//uniform float posyasdqwd;
void main()
{   
    vec2 uv = gl_FragCoord.xy/resolution.xy;
    
    
    float n = snoise(vec2(uv.x*fuerzaruido,uv.y*fuerzaruido-time*5))*0.5+0.5;
    
    vec2 coords = uv;
    
	
    coords -= vec2(0.5);
    coords*=scale(vec2(mapr(sc,0.98,1.9)));
    coords += vec2(0.5);
    
    coords -= vec2(0.5);
    coords = rotate2d(mapr(rotacion,0.4,2.4)) * coords;
    coords += vec2(0.5);
    coords = fract(coords*mapr(frac,0.952,1.1)); 
    vec4 prev = texture(feedback,coords);
    
   vec4 e =     
   vec4(poly(uv,vec2(mapr(posx,0.25,.77),mapr(posy,0.25,.77))
        ,0.08*sin(uv.x*100+sin(uv.y*30+time*2))
        ,0.08+prev.r*0.1
        ,3
        ,0));
        
    
    e.g *= noise(uv,PI/2)*sin(uv.x*30);
    e.b *= noise(uv,PI);
    e.r *= noise(uv,PI/17)*3;
    
    
    vec4 fin = e+prev*.982;
   
    fragColor = vec4(fin.rgb,1.0);
}




