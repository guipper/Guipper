#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

//#define VAPORWAVE

uniform sampler2D iChannel0;
float amp(vec2 p){
    return smoothstep(1.,8.,abs(p.x));

}


float hash(vec2 uv,float p){
    float a = amp(uv);
    return a>0.?
        a*pow(random(uv*5000.),sin(time)*0.5+0.5)*(1.-.4*pow(.51+.49*sin((.02*(uv.y+.5*uv.x)-1.)*2.),500.))
        :0.;
}

vec2 trinoise(vec2 uv, float dist){
    const float sq = sqrt(3./2.);
    uv.x *= sq;
    uv.y -= .5*uv.x;
    vec2 d = fract(uv);
    uv -= d;
    if(dot(d,vec2(1))<1.){
        float n1 = hash(uv,dist);
        float n2 = hash(uv+vec2(1,0),dist);
        float n3 = hash(uv+vec2(0,1),dist);
        float nmid = mix(n2,n3,d.y);
        float ng = mix(n1,n3,d.y);
        float dx = d.x/(1.-d.y);
        return vec2(mix(ng,nmid,dx),min(min((1.-dx)*(1.-d.y),d.x),d.y));
	}else{
    	float n2 = hash(uv+vec2(1,0),dist);
        float n3 = hash(uv+vec2(0,1),dist);
        float n4 = hash(uv+1.,dist);
        float nmid = mix(n2,n3,d.y);
        float nd = mix(n2,n4,d.y);
        float dx = (1.-d.x)/(d.y);
        return vec2(mix(nd,nmid,dx),min(min((1.-dx)*d.y,1.-d.x),1.-d.y));
	}
    return vec2(0);
}


vec2 map(vec3 p){
    vec2 n = trinoise(p.xz,1.7);
    return vec2(p.y-2.*n.x,n.y);
}

vec3 grad(vec3 p){
    const vec2 e = vec2(.005,0);
    float a =map(p).x;
    return vec3(map(p+e.xyy).x-map(p-e.xyy).x
                ,map(p+e.yxy).x-map(p-e.yxy).x
                ,map(p+e.yyx).x-map(p-e.yyx).x)/2.*e.x;

}

vec2 intersect(vec3 ro,vec3 rd){
    float d =0.,h=0.;
    for(int i = 0;i<500;i++){
        vec3 p = ro+d*rd;
        vec2 s = map(p);
        h = s.x;
        d+=h*.5;
        if(abs(h)<.001*d)
            return vec2(d,s.y);
        if(d>150.|| p.y>2.5) break;
    }

    return vec2(-1);
}


vec3 sun(vec3 rd,vec3 ld,vec3 base){

	float sun = smoothstep(.21,.2,distance(rd,ld));

    if(sun>0.){
        float yd = (rd.y-ld.y);

        float a =sin(3.1*exp(-(yd)*14.));

        sun*=smoothstep(-.8,0.,a);

        base = mix(base,vec3(1.,.8,.4)*.75,sun);
    }
    return base;
}
vec3 gsky(vec3 rd,vec3 ld,bool mask){
    float haze = exp2(-5.*(abs(rd.y)-.2*dot(rd,ld)));
    float st = mask?pow(texture(iChannel0,(rd.xy+vec2(300.1,100)*rd.z)*10.).r,500.)*(1.-min(haze,1.)):0.;
    vec3 col=clamp(mix(vec3(.4,.1,.7),vec3(.7,.1,.4),haze)+st,0.,1.);
    return mask?sun(rd,ld,col):col;

}

void main()
{
    vec2 uv = (2.*fragCoord-iResolution.xy)/iResolution.x;
	uv.y = -uv.y;
 //   float dt = fract(texture(iChannel0,fragCoord/resolution.xy).r-0.8);
    vec3 ro = vec3(0.,1.0,(-2000.+iTime*0.1)*2.);
    vec3 rd = normalize(vec3(uv,.75));//vec3(uv,sqrt(1.-dot(uv,uv)));

    vec2 i = intersect(ro,rd);
    float d = i.x;

    vec3 ld = normalize(vec3(0,.125+.05*sin(.1*iTime),1));

    float fog = d>0.?exp2(-d*.14):0.;
    vec3 sky = gsky(rd,ld,d<0.);

    vec3 p = ro+d*rd;
    vec3 n = normalize(grad(p));

    float diff = dot(n,ld)+.1*n.y;
    vec3 col = vec3(.1,.11,.18)*diff;

    vec3 rfd = reflect(rd,n);
    vec3 rfcol = gsky(rfd,ld,true);

    col = mix(col,rfcol,.05+.95*pow(max(1.+dot(rd,n),0.),5.));
    #ifdef VAPORWAVE
    col = mix(col,vec3(.4,.5,1.),smoothstep(.05,.0,i.y));
    col = mix(sky,col,fog);
    col = sqrt(col);
    #else
    col = mix(col,vec3(.8,.1,.92),smoothstep(.05,.0,i.y));
    col = mix(sky,col,fog);
    //no gamma for that old cg look
    #endif
    fragColor = vec4(col,1.);
}