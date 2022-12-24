#pragma include "../common.frag"

#define hash(x) fract(sin(x)*1e3)

vec3 ld=normalize(vec3(-1,2,5));
vec2 l=vec2(1,0);

vec3 rot3d(vec3 v,float a,vec3 ax){
    ax=normalize(ax);
    return cos(a)*v+(1.-cos(a))*dot(ax,v)*ax-sin(a)*cross(ax,v);
}

float hash3(vec3 p){
    float s=dot(p,vec3(1.2134,1.1623,1.7232));
    return hash(s);
}

vec2 hash22(vec2 p){
    vec2 s=vec2(dot(p,vec2(1.6823,1.2362)),dot(p,vec2(1.1631,1.7223)));
    return hash(s)*2.-1.;
}

float sphere(vec3 i,vec3 f,vec3 c){
    float r=hash3(i+c);
    
    if(r>.95)r=hash(r)*.5;
    else r=-.5;
    
    return length(f-c)-r;
}

// Reference:
// https://iquilezles.org/articles/fbmsdf
float sphereL(vec3 p){
    p.y-=iTime*.5;
    float d=1e5;
    vec3 i=floor(p);
    vec3 f=fract(p);
    
    d=min(d,sphere(i,f,l.yyy));
    d=min(d,sphere(i,f,l.yyx));
    d=min(d,sphere(i,f,l.yxy));
    d=min(d,sphere(i,f,l.yxx));
    
    d=min(d,sphere(i,f,l.xyy));
    d=min(d,sphere(i,f,l.xyx));
    d=min(d,sphere(i,f,l.xxy));
    d=min(d,sphere(i,f,l.xxx));
    
    return d;
}

float perlin2d(vec2 p){
    vec2 i=floor(p);
    vec2 f=fract(p);
    vec2 u=f*f*f*(6.*f*f-15.*f+10.);
    
    return mix(mix(dot(f-l.yy,hash22(i+l.yy)),dot(f-l.xy,hash22(i+l.xy)),u.x),
               mix(dot(f-l.yx,hash22(i+l.yx)),dot(f-l.xx,hash22(i+l.xx)),u.x),
               u.y);
}

float smin(float a,float b,float k){
    float h=max(k-abs(a-b),0.);
    return min(a,b)-h*h*.25/k;
}

float map(vec3 p){
    float d;
    d=sphereL(p);
    d=smin(d,p.y-perlin2d(p.zx)*.5,.4);
    return d;
}

vec3 calcN(vec3 p){
    vec2 e=vec2(1e-3,0);
    return normalize(vec3(map(p+e.xyy)-map(p-e.xyy),
                          map(p+e.yxy)-map(p-e.yxy),
                          map(p+e.yyx)-map(p-e.yyx)));
}

float fog(float d,float den){
    float s=d*den;
    return exp(-s*s);
}

vec3 hsv(float h,float s,float v){
    return ((clamp(abs(fract(h+vec3(0,2,1)/3.)*6.-3.)-1.,0.,1.)-1.)*s+1.)*v;
}

vec3 getC(vec3 p){
    vec3 col;
    col=hsv(perlin2d(p.zx*.2),.8,1.);
    return col;
}

void main()
{
    vec2 uv = vec2(gl_FragCoord.x / iResolution.x, gl_FragCoord.y / iResolution.y);
    uv -= 0.5;
    uv /= vec2(iResolution.y / iResolution.x, 1)*.5;
    uv.y = 1.0-uv.y;
	vec3 col=vec3(0);
    
    vec3 cp=vec3(.5,5,-iTime);
    vec3 cd=vec3(0,0,-1);
    vec3 cs=normalize(cross(cd,vec3(0,1,0)));
    vec3 cu=cross(cs,cd);
    
    vec3 rd=normalize(uv.x*cs+uv.y*cu+cd*2.);
    rd=rot3d(rd,iTime*.1,vec3(1,7,2));
    vec3 rp=cp;
    
    float d;
    for(int i=0;i<100;i++){
        d=map(rp);
        if(abs(d)<1e-4){
            break;
        }
        rp+=rd*d;
    }
    
    vec3 n=calcN(rp);
    vec3 al=getC(rp);
    float diff=max(dot(ld,n),0.);
    float spec=pow(max(dot(reflect(ld,n),rd),0.),20.);
    col+=al*diff+spec;
    float f=fog(length(rp-cp),.03);
    col=mix(vec3(0.0),col,f);
    
    col=pow(col,vec3(1./2.2));
    fragColor = vec4(col,1.);
}