#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


#define steps 10
#define camDist 30.

uniform float rdyz;
uniform float rdxy;
uniform float royz;
uniform float speed;
uniform float kush;
//uniform float roxy;

mat2 rz2(float a){float c=cos(a),s=sin(a);return mat2(c,s,-s,c);}

//float rayDist = 1000.0;
float dist = 1000.0;
vec3 rayColor = vec3(0.0,0.0,0.0);
vec3 ro;

float fog = 0.;


float sphere(vec3 ray,vec3 pos, float radius)
{
    return length(ray-pos) - radius;
}

float cylinder(vec3 ray,vec3 pos, float radius, float height)
{
    return max(length(ray.xy-pos.xy) - radius, abs(ray.z-pos.z)-height);
}

float cube(vec3 ray, vec3 pos, float size)
{
    ray = abs(ray-pos) - size;
    return max(max(ray.x,ray.y),ray.z);
}

float line(vec3 ray, vec3 pos, float size)
{
    ray = abs(ray-pos) - size;
    return max(ray.z,ray.y);
}

float box(vec3 ray, vec3 pos, vec3 size)
{
    ray = abs(ray-pos) - size;
    return max(max(ray.x,ray.y),ray.z);
}

vec3 Spherize(vec3 pos)
{
    vec3 result = vec3(0.,0.,0.);
    result.x = atan(normalize(pos.xy)).y * 10.0;
    result.y = atan(normalize(pos.zx)).y * 10.0;
    result.z = (2.*length(pos))-15.;
    result.xy += 10.;
    return result;
}

vec3 FractalSpace(vec3 pos)
{
    pos *= 0.1;

    float height = length(pos) * 90.0;
    float s=3.;
	for(int i=0;i<steps;i++){
		pos.xy=abs(pos).xy-s;
        pos.xy *= rz2(0.1*iTime*speed);
		//pos.y *=sin(pos.x*2.0+time);
		pos.xz *= rz2(time*mapr(kush,0.0,0.1)); //PLEASE DO NOT UNCOMMENT ME
		s=s/1.3;
	}

    return pos;
}

float map(vec3 pos)
{
    float rayDist = 0.;

    //pos = Spherize(pos);

    pos = FractalSpace(pos);
    pos += .05;

    vec3 pos1 = pos;
    pos1.x = abs(pos.x);
    vec3 pos2 = pos;

    pos.xy = abs(pos.xy);


    //ground
    rayDist = box(pos,vec3(0.0,0.0,-0.1),vec3(2.,10.0,0.1));

    //roads
    rayDist = max(rayDist,-box(pos,vec3(0.0,0.0,0.0),vec3(0.02,10.0,0.002)));
    rayDist = max(rayDist,-box(pos,vec3(0.0,0.0,0.0),vec3(10.,0.02,0.002)));

    rayDist = max(rayDist,-box(pos,vec3(0.15,0.0,0.0),vec3(0.01,10.0,0.002)));
    rayDist = max(rayDist,-box(pos,vec3(0.0,0.15,0.0),vec3(10.,0.01,0.002)));



    //Paris building
    /*vec3 b1 = pos;
    b1.xy -= vec2(0.22,0.08);
    b1 = abs(b1);
    rayDist = min(rayDist,box(b1,vec3(0.,0.,0.0),vec3(0.05,0.05,0.05)));
    rayDist = max(rayDist,-box(b1,vec3(0.0,0.0,0.05),vec3(0.02,0.02,0.05)));

    vec3 bev1 = b1;
    bev1 -= vec3(0.04,0.06,0.062);
    bev1.yz *= rz2(2.);
    rayDist = max(rayDist,-box(bev1,vec3(0.0),vec3(0.02,0.02,0.02)));

    vec3 bev2 = b1;
    bev2 -= vec3(0.057,0.03,0.062);
    bev2.xz *= rz2(2.1);
    rayDist = max(rayDist,-box(bev2,vec3(0.0),vec3(0.02,0.022,0.02)));*/



    //Paris building
    vec3 b1 = pos;
    b1.xy -= vec2(0.22,0.08);
    b1 = abs(b1);
    float XYsize =  max(0.,.5*pos.z-0.02);
    float Zsize =  max(0.,0.4*max(b1.x,b1.y)-0.01);
    rayDist = min(rayDist,box(b1,vec3(0.,0.,0.0),vec3(0.05-XYsize,0.05-XYsize,0.06-Zsize)));
    rayDist = max(rayDist,-box(b1,vec3(0.0,0.0,0.05),vec3(0.02+XYsize,0.02+XYsize,0.05)));

    rayDist = min(rayDist,box(b1,vec3(0.02,0.04,0.046),vec3(0.005,0.009,0.008)));
    rayDist = min(rayDist,box(b1,vec3(0.04,0.01,0.05),vec3(0.005,0.001,0.008)));

    b1.xy *= 0.56;
    b1 = abs(b1-0.02);
    rayDist = max(rayDist,-box(b1,vec3(0.01,0.01,0.01),vec3(0.003,0.003,0.008)));

    //Garden walls
    vec3 b0 = pos1;
    b0.xy -= 0.08;
    rayDist = min(rayDist,box(b0,vec3(0.,0.,0.005),vec3(0.05,0.05,0.005)));
    rayDist = max(rayDist,-box(b0,vec3(0.,0.,0.006),vec3(0.048,0.048,0.005)));

    //Garden trees
    float noise = 1.+0.1*length(sin(pos*2000.0))+0.2*length(sin(pos*900.0));

    b0.xy = abs(b0.xy);
    b0.xy -= 0.02;
    b0.xy = abs(b0.xy);
    b0 -= vec3(0.01,0.01,0.02);
    rayDist = min(rayDist,cylinder(b0,vec3(0.,0.,-0.01),0.001,0.006));
    b0*=noise;
    rayDist = min(rayDist,sphere(b0,vec3(0.),0.008));

    //big building
    vec3 b2 = pos;
    b2.xy -= 0.22;
    //b2.xy -= vec2(0.22,-0.08);
    b2 = abs(b2);
    rayDist = min(rayDist,box(b2,vec3(0.,0.,0.0),vec3(0.05,0.05,0.1)));
    rayDist = min(rayDist,box(b2,vec3(0.,0.,0.05),vec3(0.04,0.04,0.1)));
    //rayDist = min(rayDist,box(b0,vec3(0.,0.,0.006),vec3(0.048,0.048,0.005)));

    //parking
    vec3 b3 = pos2;
    b3.xy -= vec2(0.08,0.-0.08);
    b3 = abs(b3);
    rayDist = min(rayDist,box(b3,vec3(0.,0.,0.033),vec3(0.05,0.05,0.033)));
    rayDist = max(rayDist,-box(b3,vec3(0.,0.0,0.064),vec3(0.049,0.049,0.006)));
    rayDist = min(rayDist,box(b3,vec3(0.,0.,0.02),vec3(0.051,0.051,0.002)));
    rayDist = min(rayDist,box(b3,vec3(0.,0.,0.04),vec3(0.051,0.051,0.002)));
    rayDist = max(rayDist,-box(b3,vec3(0.022,0.,0.033),vec3(0.01,0.051,0.003)));
    rayDist = max(rayDist,-box(b3,vec3(0.,0.022,0.033),vec3(0.051,0.01,0.003)));
    rayDist = max(rayDist,-box(b3,vec3(0.022,0.,0.053),vec3(0.01,0.051,0.003)));
    rayDist = max(rayDist,-box(b3,vec3(0.,0.022,0.053),vec3(0.051,0.01,0.003)));

    //square
    vec3 b4 = pos2;
    b4.xy -= vec2(0.08,0.-0.22);
    b4 = abs(b4);
    rayDist = min(rayDist,box(b4,vec3(0.,0.,0.003),vec3(0.05,0.05,0.003)));
    rayDist = max(rayDist,-box(b4,vec3(0.,0.,0.004),vec3(0.049,0.049,0.004)));
    rayDist = max(rayDist,-box(b4,vec3(0.,0.,0.004),vec3(0.051,0.01,0.004)));
    rayDist = max(rayDist,-box(b4,vec3(0.,0.,0.004),vec3(0.01,0.051,0.004)));

    vec3 cone = b4;
    vec3 stairs = b4;

    rayDist = min(rayDist,cylinder(b4,vec3(0.,0.,0.015),0.015,0.006));

    cone.xy += (cone.z-0.021);
    rayDist = min(rayDist,cylinder(cone,vec3(0.,0.,0.027),0.015,0.006));

    stairs.xy += floor(stairs.z * 800.0)/800.0;
    rayDist = min(rayDist,box(stairs,vec3(0.018,0.018,0.0),vec3(0.01,0.01,0.01)));
    rayDist = min(rayDist,box(stairs,vec3(0.00,0.0,0.01),vec3(0.01,0.01,0.02)));


    //Business building
    vec3 b5 = pos2;
    b5.xy -= vec2(-0.08,0.-0.22);
    b5.xy *= 1.+2.0*floor(b5.z * 50.0)/50.0;
    b5.xy *= rz2(b5.z);
    rayDist = min(rayDist,box(b5,vec3(0.,0.,0.15),vec3(0.05,0.05,0.15)));

    fog = pos.z;

    return rayDist;

}
vec4 GetSampleColor(vec2 uv)
{
    ro = vec3(0.,0.,-camDist);
    vec3 rd = normalize(vec3(uv.x,uv.y,1.)) * 7.;


    rd.yz *= rz2(3.14 * rdyz);
    rd.xy *= rz2(3.1*rdxy );
    ro.yz *= rz2(3.14+1.6*royz);
   // ro.xy *= rz2(3.1*roxy );

    vec3 mp=ro;

    int i;
    for (i=0;i<120;i++){
        dist = map(mp);
        //if(abs(rayDist)<mix(0.0001,0.1,(mp.z+camDist)*0.005))
        if(abs(dist)<0.0001)
            break;
        mp+=rd*dist;
    }

    float ma=1.-float(i)/120.;

    return vec4(mp,ma);
}

vec3 GetNormal(vec3 pos, float posDist)
{
    vec2 e = vec2(0.002,0.) * length(pos-ro);
    return normalize(vec3(map(pos+e.xyy) - posDist, map(pos+e.yxy) - posDist, map(pos+e.yyx) - posDist));
}

void main()
{
    vec2 uv = (gl_FragCoord.xy-iResolution.xy*0.5)/iResolution.yy;


    vec4 pos = GetSampleColor(uv);
    float height = fog;
    //vec4 posX = GetSampleColor(vec2(uv.x+dFdx(uv).x*0.8,uv.y));
    //vec4 posY = GetSampleColor(vec2(uv.x,uv.y+dFdy(uv).y*0.8));

    //vec3 normal = normalize(cross(normalize(posY.xyz-pos.xyz),normalize(posX.xyz-pos.xyz)));
    vec3 normal = GetNormal(pos.xyz,dist);

    //cam direction (screen space)
    //vec3 cp = vec3(0.,0.,-1);

    vec3 lightPOV = vec3(1.,-1.,1.);
    vec3 lightPOV2 = vec3(-1.,-1.,1.);

    pos.w *= pos.w;

    //diffuse
    vec3 finalCol = max(0.,dot(normal,lightPOV)) * vec3(0.,.5,1.);
    finalCol +=  max(0.,dot(normal,lightPOV2)) * vec3(1.,.5,0.);
    finalCol *= 0.8;

    vec3 fogCol = vec3(1.,0.8,0.7);
    float rayLgth = length(pos.xyz-ro) * 0.005;
    //finalCol *= vec3(pow(rayLgth,1.));
    finalCol = mix(finalCol*pos.w,fogCol,rayLgth);
    //finalCol += fogCol*rayLgth;
    //finalCol += (1.-finalCol)*pow(max(0.,1.-fog),20.)*0.2 * (1.-pos.z*0.01);
    if (height < 0.0 && pos.w > 0.1)
    	finalCol *= 0.0;


    fragColor = vec4(finalCol,1.0);
}