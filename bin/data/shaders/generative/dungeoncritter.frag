#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

vec2 translate(vec2 _st, vec2 _pos){
    return _st - _pos;
}
vec2 rotate(vec2 _st, float _angle){
    return mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle)) 
           *_st;
}
float s(float speed, float range){
    return (sin(iTime*speed)*0.5+0.5) * range;
}
float poly(vec2 uv,vec2 p, float s, float dif,float N,float a){
    // Remap the space to -1. to 1.
    vec2 st = p - uv ;
    // Angle and radius from the current pixel
    float a2 = atan(st.x,st.y)+a;
    float r = TWO_PI/N;
    float d = cos(floor(.5+a2/r)*r-a2)*length(st);
    float e = 1.0 - smoothstep(s,s+dif,d);
    return e;
}
float flame(vec2 uv, vec2 pos, float size, float tall, float phase){
    vec2 st = uv;
    st.y += -0.01;
    st.x += sin(iTime*10.+phase)*((uv.y - pos.y) * 0.04) + sin(iTime*3.+phase)*((uv.y - pos.y) * 0.03);
    st = translate(st, pos+vec2(0, -0.001));
    st.y *= sin(iTime*5.6+phase)*0.08 + (0.3 / tall); 
    st *= 2.;
    st = translate(st, -pos-vec2(0, -0.001));
    return poly(st,
        pos,
        sin(iTime*10.)*.0005 + (0.01*size),
        0.004, 3., 0.);
}
float eye(vec2 uv, vec2 pos, vec2 lookAt, float o, float scared){
    vec2 st = uv;
    float circleSides = 12.;
    lookAt -= pos;
    float angle = atan(lookAt.y, lookAt.x);
    vec2 dir = vec2(cos(angle), sin(angle));
    float blink = smoothstep(0.94, 0.99, sin(iTime*2. + o*.1));
    
    st = translate(st, vec2(.5));
    st = rotate(st, sin(iTime*2.4)*.01);
    st.y *= 1. + blink*5.; 
    st = translate(st, -vec2(.5));    
    
    float eyeCompo =  poly(st, pos                                 , 0.020 * (1.+ 0.001*o) * (1.+0.4*scared)                  , 0.002, circleSides, 0.)                
         - poly(st, pos+(.007*dir) +(0.002*o)      , (0.0033+s(10.,0.001*scared) *(-0.002*o))* (5.*(1.-scared)) , 0.002,     circleSides, 0.)
         + poly(st, pos+(.014*dir) +(0.002*o)      ,
            0.0005 * (5.*(1.-scared))
                * (1. + s(3., .8)
                * s(5., .8)), //este ultimo s() necesita tener el mismo timming qye lightspot
            0.002, circleSides, 0.)
            * (scared*2. + 0.3);
    
    eyeCompo *= 1. - smoothstep(0.94, 0.99, sin(iTime*2. + o*.1));
    
    return eyeCompo;
}

void main( )
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    
	uv.y = 1.0-uv.y;
    // hack para el aspect ratio. todo: Pero fuckea un toque la coord de iMouse tambien...
    uv = translate(uv, vec2(.5));
    uv.x *= (iResolution.x/iResolution.y);
    uv *= 0.57;
    uv = translate(uv, -vec2(.5));
    
    vec2 st;
    // true for control it with mouse
    vec2 pos = false ? vec2(iMouse.x, 1.-iMouse.y) : vec2(sin(iTime)*0.2, sin(iTime*0.25)*0.1)+.5;
    float closeToTorch = 1. - smoothstep(0., 0.15, distance(vec2(.5), pos));

//  mist
    float mist = sin(uv.y*8. + sin(uv.x*2.+iTime*.5 + sin(uv.y*30.5+iTime*0.001 + sin(uv.x*10.+iTime*0.5))));
    mist = mist * mist;
        
//  wall
    st = uv;
    st.x += (1. - step(1./11., mod(st.y, 1./11.*2.))) * 0.1;
    st.x = fract(st.x*5.);
    st.y = fract(st.y*11.);       
    st.x += sin(uv.x*10.+uv.y*4.) * 0.1;
    st.y += sin(uv.y*100.+uv.x*4.) * 0.1;    
    float wall = poly(st, vec2(.5), 0.45, 0.02, 4., 0.) * .5;
    
//  hole
    st = uv;
    st.x += sin(uv.y*20.+7.1) * 0.01;
    st.y += sin(uv.x*20.+10.2) * 0.001;
    st = translate(st, vec2(.5));
    st = rotate(st, -.01);
    st.y *= 2.3; 
    st = translate(st, -vec2(.5));
    float hole = poly(st, vec2(.5), 0.08, 0.05, 4., 0.);
     
//  eyes
    st = uv;
    st = translate(st, vec2(.5));
    st = rotate(st, sin(iTime*40.4)*(.04*closeToTorch));
    st *= 1. + (0.6*closeToTorch); 
    st = translate(st, -vec2(.5));
    vec2 face_pos = vec2(.5) + vec2(sin(iTime*40.)*(0.007*closeToTorch), sin(iTime*12.)*(0.001*closeToTorch));
    float face = eye(st, face_pos+vec2(0.03,0.), pos, 1., closeToTorch)
               + eye(st, face_pos+vec2(-0.03,0.), pos, 0., closeToTorch);
    
//  lightSpot
    st = uv;
    st.y += -0.02;
    float lightSpot = poly(st, pos, 
        0.02,
        s(3., .02) + s(5.,.02) + .27,
        20.,
        s(5., .2));
        
//  torch
    st = uv;  
    st = translate(st, pos);
    st.x *= 18.;
    st.x *= 1. + 4. * ((1.-pos.y) - (1.-uv.y));
    st = translate(st, -pos);
    st = translate(st, vec2(0,-0.06));
    float torch_stick = poly(st, pos+vec2(0.,-0.1), 0.15, 0.002, 4., 0.);
    st = uv;
    st.y += -0.04;
    st = translate(st, pos);
    st.y *= 2.5; 
    st = translate(st, -pos);
    st = translate(st, vec2(0, -.12));
    float torch_top = poly(st, pos, 0.02, 0.01, 3., PI);
    st = uv;
    st = translate(st, pos);
    st.y *= 2.; 
    st.x *= 0.42; 
    st = translate(st, -pos);
    st.y -= 0.01;
    float torch_top_back = poly(st, pos, .02, .0002, 20., 0.);
    st = uv;
    st = translate(st, pos);
    st.y *= 2.5; 
    st.x *= 0.49; 
    st = translate(st, -pos);
    st.y -= 0.01;
    float torch_reflexion = poly(st, pos, .02, .002, 20., 0.);
    
    float torch = torch_stick + torch_top;
        
    
//  fire      
    st = uv;
    st = translate(st, pos);
    st *= 0.9;
    st = translate(st, -pos);
    float fire =
        flame(st, pos, 1.4, .73, 0.)
        + flame(st, pos, 2.8, .3, 4.)
        + flame(st, pos+vec2(0.013,0), 1., .55, 10.)
        + flame(st, pos+vec2(-0.013,0), 1., .5, 3.5)
        + flame(st, pos+vec2(-0.02,0), .4, .5, 20.5)
        + flame(st, pos+vec2(0.02,0), .4, .5, 20.5)
        ;    

//  ajustes y mergeo de todo lo que va a ser pasado a la grilla
    float lightmap;
    mist = clamp(mist, -0.1, 1.) * 0.5;
    wall = wall+wall/2.;
    lightmap = (wall + mist - hole*2.) * lightSpot + (lightSpot/2.) + fire; 
    
//  dibujar grilla
    st = uv;
    st = fract(st*200.);    
    float endCompo = poly(st, vec2(.5,.5), lightmap*0.5, 0.01, 10., 0.);    
    
//  adjustments
    endCompo *= 0.6;
    endCompo += (face*0.8)*lightSpot;
    endCompo -= torch_top_back;
    endCompo += fire*2.; //just reburn fire again without grid - multiply to cut top_back
    endCompo -= torch*7.; // multiply to cut flames
    endCompo += torch_reflexion*10.;
 
 
    st = gl_FragCoord.xy / iResolution.xy; //todo: por alguna razom toque restaurar las uv, o el feedback se distorciona
    fragColor = vec4(vec3(endCompo),1.0);
}