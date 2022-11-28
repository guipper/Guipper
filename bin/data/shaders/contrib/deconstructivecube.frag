#pragma include "../common.frag"
//Building on ideas from 
//https://www.shadertoy.com/view/NsKGDy
//https://www.shadertoy.com/view/7sKGRy
//https://www.shadertoy.com/view/fsyGD3
//https://www.shadertoy.com/view/fdyGDt
//https://www.shadertoy.com/view/7dVGDd

//heavily inspired by
//https://twitter.com/adamswaab/status/1437498093797212165

//Toggle Shadows
//#define USE_SHADOWS 




#define MDIST 350.0
#define STEPS 128.0
#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
#define pmod(p,x) (mod(p,x)-0.5*(x))


//iq palette
vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d ){
    return a + b*cos(2.*pi*(c*t+d));
}
float h21 (vec2 a) {
    return fract(sin(dot(a.xy,vec2(12.9898,78.233)))*43758.5453123);
}
float h11 (float a) {
    return fract(sin((a)*12.9898)*43758.5453123);
}
float box(vec3 p, vec3 b){
    vec3 d = abs(p)-b;
    return max(d.x,max(d.y,d.z));
}
//iq box sdf
float ebox( vec3 p, vec3 b ){
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float swave(float x, float a){
    return (sin(x*pi/3.-pi/2.)/sqrt(a*a+sin(x*pi/3.-pi/2.)*sin(x*pi/3.-pi/2.))+1./sqrt(a*a+1.))*0.5;
}
vec3 rdg = vec3(0);
float nsdf = 0.;
vec2 blocks(vec3 p, vec3 scl, vec3 rd){
    vec3 po = p;
    float t = iTime;
    
    bvec3 isEdge = bvec3(true);
    vec3 dMin = vec3(-0.5) * scl;
    vec3 dMax = vec3(0.5) * scl;
    vec3 dMini = dMin;
    vec3 dMaxi = dMax;
    
    float id = 0.;
    float seed = floor(t/4.);
    
    float MIN_SIZE = 0.5;
    float ITERS = 5.;
    float PAD_FACTOR = 1.01;
    float BREAK_CHANCE = 0.2;
   
    vec3 dim = dMax - dMin;
    //Big thanks for @0b5vr for cleaner version of subdiv algo
    for (float i = 0.; i < ITERS; i++) {

        vec3 divHash = vec3(
            h21( vec2( i + id, seed )),
            h21( vec2( i + id + 2.44, seed )),
            h21( vec2( i + id + 7.83, seed ))
        );
        if(i==0.0){
        divHash = vec3(0.49,0.5,.51);
        }
        if(i>0.){
            vec3 center = -(dMin + dMax)/2.0;

            vec3 cs = vec3(0.3);
            divHash = clamp(divHash,vec3(cs*sign(center)),vec3(1.0-cs*sign(-center)));

        }
        vec3 divide = divHash * dim + dMin;
        divide = clamp(divide, dMin + MIN_SIZE * PAD_FACTOR , dMax - MIN_SIZE * PAD_FACTOR );
        vec3 minAxis = min(abs(dMin - divide), abs(dMax - divide));
        
        float minSize = min( minAxis.x, min( minAxis.y, minAxis.z ) );
        bool smallEnough = minSize < MIN_SIZE;

        bool willBreak = false;
        if (i  > 0. && h11( id ) < BREAK_CHANCE ) { willBreak = true; }
        if (smallEnough && i  > 0.) { willBreak = true; }
        if( willBreak ) {
            break;
        }

        dMax = mix( dMax, divide, step( p, divide ));
        dMin = mix( divide, dMin, step( p, divide ));

        float pad = 0.01;
        if(dMaxi.x>dMax.x+pad&&dMini.x<dMin.x-pad)isEdge.x=false;
        if(dMaxi.y>dMax.y+pad&&dMini.y<dMin.y-pad)isEdge.y=false;
        if(dMaxi.z>dMax.z+pad&&dMini.z<dMin.z-pad)isEdge.z=false;
        
        
        vec3 diff = mix( -divide, divide, step( p, divide));
        id = length(diff + 1.0);
    
        dim = dMax - dMin;
    }
    float volume = dim.x*dim.y*dim.z;
    vec3 center = (dMin + dMax)/2.0;
    float b = 0.;

    
    if(any(isEdge)) {
        float expand = 1.0+(3.0-h11(id)*3.)*swave(t*3.0+h11(id)*1.5,0.17);
        if(isEdge.x){
        center.x*=expand;
        }
        else if(isEdge.y){
        center.y*=expand;
        }
        else if(isEdge.z){
        center.z*=expand;
        }
    }
    vec3 edgeAxis = mix(dMin, dMax, step(0.0, rd));
    vec3 dAxis = abs(p - edgeAxis) / (abs(rd) + 1E-4);
    float dEdge = min(dAxis.x,min(dAxis.y,dAxis.z));
    b= dEdge;

    vec3 d = abs(center);
    dim-=0.4;
    float a = ebox(p-center,dim*0.5)-0.2;

    if(!any(isEdge)){
        a=b;

        nsdf =5.;
    }
    else nsdf = a;
    a = min(a, b);

    
    id = h11(id)*1000.0;

    return vec2(a,id);
}

vec3 map(vec3 p){
    float t = iTime;
    vec3 po = p;
    vec2 a = vec2(1);

    vec3 scl = vec3(10.);
    vec3 rd2 = rdg;

    p.xz*=rot(t);
    rd2.xz*=rot(t);
    p.xy*=rot(pi/4.);
    rd2.xy*=rot(pi/4.);
    a = blocks(p,scl,rd2)+0.01;
    
   
    a.x = max(box(p,vec3(scl*2.0)),a.x);
    

    return vec3(a,nsdf);
}
vec3 norm(vec3 p){
    vec2 e = vec2(0.01,0.);
    return normalize(map(p).x-vec3(
    map(p-e.xyy).x,
    map(p-e.yxy).x,
    map(p-e.yyx).x));
}
void main()
{
    vec2 uv = (fragCoord-0.5*iResolution.xy)/iResolution.y;
    float t = iTime;
    vec3 col = vec3(0);
    vec3 ro = vec3(0,3.5,-20)*2.;
    if(iMouse.z>0.){
    ro.yz*=rot(2.0*(iMouse.y/iResolution.y-0.5));
    ro.zx*=rot(-7.0*(iMouse.x/iResolution.x-0.5));
    }
    ro.xz*=rot(-pi/4.);
    vec3 lk = vec3(0,0.,0);
    vec3 f = normalize(lk-ro);
    vec3 r = normalize(cross(vec3(0,1,0),f));
    vec3 rd = normalize(f*(0.99)+uv.x*r+uv.y*cross(f,r));    
    rdg = rd;
    vec3 p = ro;
    float dO = 0.;
    vec3 d = vec3(0);
    bool hit = false;
    for(float i = 0.; i<STEPS; i++){
        p = ro+rd*dO;
        d = map(p);
        dO+=d.x;
        if(abs(d.x)<0.0001||i==STEPS-1.){
            hit = true;
            break;
        }
        if(d.x>MDIST){
            dO=MDIST;
            break;
        }
    }
    if(hit){
        vec3 ld = normalize(vec3(0.,1.,0));
        vec3 n = norm(p);
        vec3 r = reflect(rd,n);
        vec3 e = vec3(0.5);
        vec3 al = pal(fract(d.y)*0.8-0.15,e*1.4,e,e*2.0,vec3(0,0.33,0.66));
        col = al;
        
        
        float shadow = 1.;
        
#ifdef USE_SHADOWS
        rdg = ld;
        for(float h = 0.09; h<10.0;){
            vec3 dd = map(p+ld*h+n*0.2);
            if(dd.x<0.001){shadow = 0.3; break;}
            //shadow = min(shadow,dd.z*20.0);
            h+=dd.x;
        }
#endif
        
        //lighting EQs from @blackle
        float spec = length(sin(r*5.)*.5+.5)/sqrt(3.);
        float fres = 1.-abs(dot(rd,n))*0.9;
        
        float diff = length(sin(n*2.)*.5+.65)/sqrt(3.);
        
        #define AO(a,n,p) smoothstep(-a,a,map(p+n*a).z)
        float ao = AO(0.3,n,p)*AO(.5,n,p)*AO(.9,n,p);
        col = al*diff+pow(spec,5.0)*fres*shadow;
        col*=pow(ao,0.2);
        col*=max(shadow,0.7);

    }
    col = pow(col,vec3(0.9));
    vec3 bg = vec3(0.698,0.710,0.878)*(1.0-length(uv)*0.5);
    col = mix(col,bg,pow(clamp(dO/MDIST,0.,1.),2.0));
    fragColor = vec4(col,1.0);
}
