#pragma include "../common.frag"
///Twitter: @smjtyazdi
//#define PI 3.14159265
const float scale2 = 0.1;
uniform float speed ;
vec2 trans(vec2 p){
    vec2 a = vec2(1.,0.);
    vec2 b = vec2(0.5,0.5*sqrt(3.));
    float ab = dot(a,b);
    
    float aa = (dot(p,a) - dot(p,b)*ab) / ( 1. - ab*ab);
    float bb = (dot(p,b) - dot(p,a)*ab) / ( 1. - ab*ab);
    
    return vec2(aa,bb);
}

vec2 rot2(vec2 p ,float t){
    return vec2(p.x*cos(t) + p.y*sin(t) , p.y*cos(t)-p.x*sin(t));
}

float sierp2(vec2 p,float kk){

    float col = 1.;
    
    vec2 base = vec2(0.);
    float scale2 = 1.; 
    
    for(float k=0.;k<kk;k+=1.){
        vec2 g = trans(p-base)*scale2;
        scale2*=2.;
        if(min(g.x,g.y)>0.)
            if(g.x+g.y<1.){col = 0.; continue;}
        
        g.x += -1.;

        base += vec2(1.,0.)/scale2*2.;

        
        if(min(g.x,g.y)>0.)
            if(g.x+g.y<1.){col = 0.; continue;}
        
        g.x +=1.;
        g.y += -1.;
        base += vec2(-0.5,0.5*sqrt(3.))/scale2*2.;

        if(min(g.x,g.y)>0.)
            if(g.x+g.y<1.){col = 0.; continue;}
        
       col = 1.;
        
        break;
    }
    
    
    return col;
}

float sierp_mix(vec2 p,float time){
    return mix(sierp2(p,4.),sierp2(p,5.),time);
}


float render(vec2 p,float time){
 
    float col = 1.;
    
    float time_ind = floor(time);
    time = mod(time,1.);
    
    
    time = 0.5 - 0.5*cos(time*PI);
    
    p = rot2(p,2.*PI/3.*mod(time_ind,3.)); //Delete this if you like to see the simplified version
    
    float tt = PI/3.*(-time);
    p.y += sin(PI/3.)*2./3.;
    
    p *= exp(-(time)*log(2.));
    
    
    
    float d = 1. + 2.*cos(tt+PI/3.);
    float dy= sqrt(3.)*1.0 + 2.*sin(tt+PI/3.);
    
    float ind = mod(round(p.x/d)+1.,2.)*2.-1.;
    float indy = mod(round(p.y/dy)+1.,2.)*2.-1.;
    p.x = p.x - round(p.x/d)*d;
    p.y = p.y - round(p.y/dy)*dy; //Delete this if you like to see the simplified version
    p.y *= ind*indy;
  
    col = sierp_mix(rot2(p-vec2(-d/2.,0.),tt)*2.,time)*sierp_mix(rot2(p-vec2(d/2.,0.),PI-tt-PI/3.)*2.,time)*sierp_mix((p-vec2(-0.5,sin(tt+PI/3.)))*2.,time);
  
    
    return col;
}


float rend(vec2 p ,float time){
    float d = 1./(scale2*iResolution.y)/2.;
    return (render(p,time)+render(p+vec2(d,0.),time)+render(p+vec2(0.,d),time)+render(p+vec2(d),time))/4.;
}

void main()
{
   
   vec2 p = (gl_FragCoord.xy.xy  - iResolution.xy/2.0)/(scale2*iResolution.y);
   

   float time = iTime*speed;;
   
   
   vec3 col;
   
   for(float k=0.;k<3.;k+=1.){
       float res = rend(p,time-k/3./60.);
       
       if(k==2.)col.x=1.-res;
       if(k==1.)col.y=1.-res;
       if(k==0.)col.z=1.-res;
   }
   
   fragColor = vec4(col,1.0);
}