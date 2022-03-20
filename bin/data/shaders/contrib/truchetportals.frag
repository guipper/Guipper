#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define HASHSCALE1 .1031
#define HASHSCALE3 vec3(.1031, .1030, .0973)
#define HASHSCALE4 vec4(1031, .1030, .0973, .1099)

#define dir3(num) vec3(equal(abs(dir),vec3(num)))

#define dot2(p) dot(p,p)

#define rot(spin) mat2(cos(spin),sin(spin),-sin(spin),cos(spin))

//#define pi acos(-1.0) //3.14
#define FAR 20.0      //raymarching and fog max distance
#define portals 0.22   //portal amount, from 0.0 to 1.0

//hash function by Dave_Hoskins https://www.shadertoy.com/view/4djSRW
vec3 hash33(vec3 p3)
{
	p3 = fract(p3 * HASHSCALE3);
    p3 += dot(p3, p3.yxz+19.19);
    return fract((p3.xxy + p3.yxx)*p3.zyx);
}

//hash function by Dave_Hoskins https://www.shadertoy.com/view/4djSRW
float hash31(vec3 p3)
{
	p3 = fract(p3 * HASHSCALE3);
    p3 += dot(p3, p3.yxz+19.19);
    return fract((p3.x + p3.y)*p3.z);
}

float mid(vec3 p) {
    //if(var3)
    p = min(p,p.yzx);
    return max(max(p.x,p.y),p.z);
}

float torus(vec3 p, vec2 r) {//creates 4 toruses
    return length(vec2(abs(abs(length(p.xy)-r.x)-0.05),abs(p.z)-0.05))-r.y;
}

float map(vec3 p) {
    
    vec3 p2 = mod(p,2.0)-1.0;
    vec3 floorpos = floor(p*0.5);
    float len = 1e10;
    
    //the truchet flipping
    vec3 flipping = floor(hash33(floorpos)+0.5)*2.0-1.0;
    
    //actually flipping the truchet
    vec3 p3 = p2*flipping;
    
    //positions relative to truchet centers
    mat3 truchet = mat3(
        vec3(+p3.yz+vec2(-1.0, 1.0),p3.x),
        vec3(+p3.zx+vec2(-1.0, 1.0),p3.y),
        vec3(+p3.yx+vec2( 1.0,-1.0),p3.z)
    );
    
    //finding distance to truchet
    len = min(min(
        torus(truchet[0],vec2(1.0,0.01)),
        torus(truchet[1],vec2(1.0,0.01))),
        torus(truchet[2],vec2(1.0,0.01)));
    
    return len;
}

//normal calculation
vec3 findnormal(vec3 p, float len) {
    const vec2 eps = vec2(0.01,0.0);
    
    return normalize(vec3(
        len-map(p-eps.xyy),
        len-map(p-eps.yxy),
        len-map(p-eps.yyx)));
}

void main()
{
	vec2 uv = (fragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
    
    vec3 floorpos = vec3(0.0,0.0,0.0);
    vec3 pos = vec3(1.0,1.0,0.0);
    vec3 dir = vec3(3.0,2.0,1.0);
    int num = 2;
    float time = abs(mod(iTime*0.4+50.0,100.0)-50.0);
    for (float i = 0.0; i <= floor(time); i++) {
        
        pos += dir*dir3(1);
        
    	vec3 flipping = floor(hash33(floorpos)+0.5)*2.0-1.0; //the truchet flipping
        
        dir *= flipping;
        
        int num2 = (num-int(dot(dir,dir3(1)))+3)%3;
        float back = dir[num2];
        dir[num2] = dir[num];
        dir[num] = -back;
        num = num2;
        
        dir *= flipping;
        
        vec3 portalpos = floorpos+dir*dir3(1)*0.5;
        if (hash31(portalpos) >= portals) {
            portalpos += dir*dir3(1)*2.0;
        }
        if (hash31(portalpos) < portals) {
            floorpos += dir*dir3(1)*2.0;
        	pos += dir*dir3(1)*4.0;
        }
        floorpos += dir*dir3(1);
        pos += dir*dir3(1);
    }
	
    vec3 flipping = floor(hash33(floorpos)+0.5)*2.0-1.0; //the truchet flipping
    vec3 dir2 = dir;
    
    dir *= flipping;
    
    int num2 = (num-int(dot(dir,dir3(1)))+3)%3;
    float back = dir[num2];
    dir[num2] = dir[num];
    dir[num] = -back;
    num = num2;
    
    dir *= flipping;
	
    //animation
    pos += dir2*vec3(equal(abs(dir2),vec3(1.0)))*(sin(fract(time)*3.14*0.5));
    pos += dir*dir3(1)*(1.0-cos(fract(time)*3.14*0.5));
    
    //normal pointing towards where the camera moves, would be nice if the camera was looking in that direction
    vec3 forward = dir2*vec3(equal(abs(dir2),vec3(1.0)))*cos(fract(time)*3.14*0.5)+dir*dir3(1)*sin(fract(time)*3.14*0.5);
    
    /*
    mat3 rotation = mat3(
        vec3(0.0),
        vec3(0.0),
        vec3(0.0));
    rotation[2] = forward;
    rotation[1] = normalize(cross(forward,vec3(1)));
    rotation[0] = cross(rotation[1],forward);
    */
    
    mat3 rotation = mat3(
        vec3(0.0),
        vec3(0.0),
        vec3(0.0));
    
    vec2 t = vec2(cos(fract(time)*3.14*0.5),sin(fract(time)*3.14*0.5));
	rotation[2] = normalize(dir2*vec3(equal(abs(dir2),vec3(1)))*t.x+dir*dir3(1)*t.y);
    rotation[1] = normalize(dir2*vec3(equal(abs(dir2),vec3(2)))*t.x+dir*dir3(2)*t.y);
    rotation[0] = normalize(dir2*vec3(equal(abs(dir2),vec3(3)))*t.x+dir*dir3(3)*t.y);
    
    
    
    vec3 ro = pos+rotation[1]*0.125;
    vec3 rd = normalize(vec3(uv,1.0));
    
    if (length(iMouse.xy) > 40.0) {
    	rd.yz *= rot(iMouse.y/iResolution.y*3.14-3.14*0.5);
    	rd.xz *= rot(iMouse.x/iResolution.x*3.14*2.0-3.14);
    }
    
    rd = rd*transpose(rotation);
    
    
    bool hit = true;
    float len;
    float dist = 0.0;
    
    //floorpos = floor(ro*0.5);
    vec3 signdir = sign(rd);
    vec3 invdir = 2.0/(abs(rd)+0.0001);
    vec3 dists = abs(-signdir*0.5-0.5+ro*0.5-floorpos)*invdir;
    
    for (int i = 0; i < 100; i++) {
        float smallest = min(min(dists.x,dists.y),dists.z);
        len = map(ro);
        if (len < 1.0/iResolution.y*(dist+1.0)||dist>FAR) {
            hit = dist < FAR;
            break;
        }
        
        len = min(len,smallest);
        
        ro += rd*len;
        dist += len;
        
        smallest -= len;
        dists -= len;
        if (smallest <= 0.0) {
            vec3 mask = step(dists,vec3(smallest));
            vec3 localpos = (ro+dot(mask*dists,vec3(1))*rd) - floorpos*2.0-1.0;
            
            vec2 q = vec2(dot(mask.yzx,localpos),dot(mask.zxy,localpos));
            
            vec3 portalpos = floorpos+mask*signdir*0.5;
            vec4 random = vec4(hash31(portalpos),hash33(portalpos));
            random.x = random.x/portals;
            //vec3 randompos = floor(hash33(portalpos)*100.0-50.0);
            
            float portalsize = 0.0;
            
            if (random.x > 1.0) {
                
            	portalpos += mask*signdir*2.0;
                random = vec4(hash31(portalpos),hash33(portalpos));
                random.x = random.x/portals;
                
            }
            if (random.x > 1.0) {
                portalsize=100.0;
        	} else {
                float t = iTime*random.x+random.y;

                portalsize =
                    length(q)
                    +sin(atan(q.x,q.y)* 8.0+t    )*0.04*sin(t*2.0)
                    +sin(atan(q.x,q.y)*16.0+t*1.5)*0.02*sin(t*4.0);
            }
            
            
            if (portalsize < 0.4) {
                //vec3 randdirection = vec3(equal(vec3(1,2,3),vec3(clamp(floor(random.x*3.0),0.0,3.0))));
                
                floorpos += mask*signdir;
                vec3 localpos = ro-floorpos*2.0;
                floorpos += mask*signdir*2.0;
                ro = floorpos*2.0+localpos;
                
                //randdirection *= floor(0.5+random.yzw)*2.0-1.0;
                
            } else if (portalsize < 0.45) {
                fragColor = vec4(-mask*signdir*0.5+0.5,1);
                fragColor = vec4(random.xzwy);
                return;
            } else {
                floorpos += mask*signdir;
            }
            dists = (1.0-mask)*dists+mask*invdir;
        }
        
    }
    if (hit) {
        
		fragColor = vec4(findnormal(ro,len)*0.5+0.5,1.0);
        fragColor /= (dist*dist*0.0005*FAR+1.0);
        //if (all(equal(floor(ro*0.5),floorpos))) fragColor = 1.0-fragColor;
    }
}