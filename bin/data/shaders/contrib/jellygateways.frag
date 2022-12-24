#pragma include "../common.frag"

uniform float speed ;


#define MDIST 100.0
#define STEPS 128.0
#define rot2(a) mat2(cos(a),sin(a),-sin(a),cos(a))
#define pmod(p,x) (mod(p,x)-0.5*(x))

vec3 rdg = vec3(0);
vec3 hsv(vec3 c){
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
//iq box sdf
float ebox(vec3 p, vec3 b){
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}
float ebox(vec2 p, vec2 b){
  vec2 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,q.y),0.0);
}

float lim(float p, float s, float lima, float limb){
    return p-s*clamp(round(p/s),lima,limb);
}
float idlim(float p, float s, float lima, float limb){
    return clamp(round(p/s),lima,limb);
}

float dibox(vec3 p,vec3 b,vec3 rd){
    vec3 dir = sign(rd)*b;   
    vec3 rc = (dir-p)/rd;
    float dc = min(rc.y,rc.z)+0.01;
    return dc;
}
float easeOutBounce(float x) {
    float n1 = 7.5625;
    float d1 = 2.75;
    if (x < 1. / d1) {
        return n1 * x * x;
    } 
    else if (x < 2. / d1) {
        return n1 * (x -= 1.5 / d1) * x + 0.75;
    } 
    else if (x < 2.5 / d1) {
        return n1 * (x -= 2.25 / d1) * x + 0.9375;
    } 
    else {
        return n1 * (x -= 2.625 / d1) * x + 0.984375;
    }
}
vec3 map(vec3 p){
    float t = -iTime*speed*2.0*0.8;
    vec3 rd2 = rdg;
    vec2 a = vec2(1);
    vec2 b = vec2(2);
    p.xz*=rot2(t*0.3*pi/3.);
    rd2.xz*=rot2(t*0.3*pi/3.);
    //p.xz*=rot2(pi/4.);
    //rd2.xz*=rot2(pi/4.); 
    vec3 po = p;
    float dsz = 0.45;
    float m = 2.42-dsz;
    float bs = 1.-dsz*0.5;
    
    //VERTIAL TRANSLATION
    p.y+=t*m;
    
    //VERTIAL REP
    float id1 = floor(p.y/m);
    p.y = pmod(p.y,m);
    
    //ROTATE EACH LAYER
    p.xz*=rot2(id1*pi/2.);
    rd2.xz*=rot2(id1*pi/2.);

    vec3 p2 = p; //dibox p1
    
    //Auxillary boxes positions
    vec3 p3 = p;
    vec3 rd3 = rd2;
     
    p3.xz*=rot2(pi/2.);
    rd3.xz*=rot2(pi/2.);
    vec3 p4 = p3; 
    
    
    //HORIZONTAL REP
    p2.z = pmod(p2.z-m*0.5,m);
    p4.z = pmod(p4.z-m*0.5,m);
    
    float cnt = 100.;
    float id2 = idlim(p.z,m,-cnt,cnt);
    float id3 = idlim(p3.z,m,-cnt,cnt);
    p.z = lim(p.z,m,-cnt,cnt);
    p3.z = lim(p3.z,m,-cnt,cnt);
    
    
    //CLOSING ANIMATION 
    float close = max((id1-t)*1.,-2.);
    float close2 = clamp(max((id1-t-0.3)*1.,-2.)*1.4,0.,1.);
    close+=id2*0.025;
    close = clamp(close*1.4,0.,1.);
    close = easeOutBounce(close);
    //close = 1.0-easeOutBounce(1.-close);

    
    
    //CLOSING OFFSET
    p.x = abs(p.x)-34.5*0.5-0.25*7.;
    p.x-=close*34.5*0.52-0.055;
    
    p3.x = abs(p3.x)-36.5;

    p.x-=((id1-t)*0.55)*close*2.4;
    p3.x-=((id1-t)*0.55)*close2*2.4;
    //WAVEY
    p.x+=(sin(id1+id2-t*6.0)*0.18+4.)*close*2.4;
    p3.x+=(sin(id1+id3-t*6.0)*0.18+4.)*smoothstep(0.,1.,close2)*2.4;
    
    
    //BOX SDF
    a = vec2(ebox(p,vec3(7.5*2.5,bs,bs))-0.2,id2);
    
    //AUXILLARY BOX
    b = vec2(ebox(p3,vec3(7.5*2.5,bs,bs))-0.2,id3);
    
    a=(a.x<b.x)?a:b;
    //ARTIFACT REMOVAL
    float c = dibox(p2,vec3(1,1,1)*m*0.5,rd2)+.1;
    //ARTIFACT REMOVAL 2
    c = min(c,dibox(p4,vec3(1,1,1)*m*0.5,rd3)+.1);
    

    float nsdf = a.x;
    
    a.x = min(a.x,c); //Combine artifact removal
    a.y = id1;
    return vec3(a,nsdf);
}
vec3 norm(vec3 p){
    vec2 e = vec2(0.005,0);
    return normalize(map(p).x-vec3(
    map(p-e.xyy).x,
    map(p-e.yxy).x,
    map(p-e.yyx).x));
}

void main()
{
    vec2 uv = (fragCoord-0.5*iResolution.xy)/iResolution.y;
    vec3 col = vec3(0);
    vec3 ro = vec3(0,13,-5)*1.5;
    if(iMouse.z>0.){
    ro.yz*=rot2(1.0*(iMouse.y/iResolution.y-0.2));
    ro.zx*=rot2(-7.0*(iMouse.x/iResolution.x-0.5));
    }
    vec3 lk = vec3(0,0,0);
    vec3 f = normalize(lk-ro);
    vec3 r = normalize(cross(vec3(0,1,0),f));
    vec3 rd = normalize(f*(0.5)+uv.x*r+uv.y*cross(f,r));  
    rdg = rd;
    vec3 p = ro;
    float dO = 0.;

    vec3 d= vec3(0);
    for(float i = 0.; i<STEPS; i++){
        p = ro+rd*dO;
        d = map(p);
        dO+=d.x;
        if(abs(d.x)<0.005){
            break;
        }
        if(dO>MDIST){
            dO = MDIST;
            break;
        }
    }

    {
        vec3 ld = normalize(vec3(0,45,0)-p);
      
        //sss from nusan
        float sss=0.01;
        for(float i=1.; i<20.; ++i){
            float dist = i*0.09;
            sss += smoothstep(0.,1.,map(p+ld*dist).z/dist)*0.023;
        }
        vec3 al = vec3(0.204,0.267,0.373);
        vec3 n = norm(p);
        vec3 r = reflect(rd,n);
        float diff = max(0.,dot(n,ld));
        float amb = dot(n,ld)*0.45+0.55;
        float spec = pow(max(0.,dot(r,ld)),40.0);
        float fres = pow(abs(.7+dot(rd,n)),3.0);     
        //ao from blackle 
        #define AO(a,n,p) smoothstep(-a,a,map(p+n*a).z)
        float ao = AO(.3,n,p)*AO(.5,n,p)*AO(.9,n,p);

        col = al*
        mix(vec3(0.169,0.000,0.169),vec3(0.984,0.996,0.804),mix(amb,diff,0.75))
        +spec*0.3+fres*mix(al,vec3(1),0.7)*0.4;
        col+=sss*hsv(vec3(fract(d.y*0.5+d.y*0.1+0.001)*0.45+0.5,0.9,1.35));
        col*=mix(ao,1.,0.85);
        col = pow(col,vec3(0.75));
    }
    col = clamp(col,0.,1.);
    //col = smoothstep(0.,1.,col);
    fragColor = vec4(col,1.0);
}