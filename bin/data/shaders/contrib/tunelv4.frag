#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float speed;
float Sphere(vec3 p, vec3 o, float r){
        return length(p-o)-r;
}

float Plane(vec3 p){
	return p.y;	
}

vec3 DR(vec3 p,vec3 q){
	return mod(p,q)-q/2.;
}


///----------------------------



//Torus function
float sdTorus( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
}


//Sphere function
float sdSpherev2( vec3 p, float s1 )
{
   vec4 s = vec4(0, s1, 9, s1);
   return  length(p-s.xyz)-s.w;
}
//Box function
float sdBox( vec3 p, vec3 b )
{
  vec3 d = abs(p) - b;
  return length(max(d,0.0))
         + min(max(d.x,max(d.y,d.z)),0.0); // remove this line for an only partially signed sdf
}
//Triprism function
float sdTriPrism( vec3 p, vec2 h )
{
    vec3 q = abs(p);
    return max(q.z-h.y,max(q.x*0.866025+p.y*0.5,-p.y)-h.x*0.5);
}
//Cone function
float sdCone( vec3 p, vec2 c )
{
    // c must be normalized
    float q = length(p.xy);
    return dot(c,vec2(q,p.z));
}


///por Iq
float sdHexPrism( vec3 p, vec2 h )
{
  const vec3 k = vec3(-0.8660254, 0.5, 0.57735);
  p = abs(p);
  p.xy -= 2.0*min(dot(k.xy, p.xy), 0.0)*k.xy;
  vec2 d = vec2(
       length(p.xy-vec2(clamp(p.x,-k.z*h.x,k.z*h.x), h.x))*sign(p.y-h.x),
       p.z-h.y );
  return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

///---------------------------------------------
float intersectSDF(float distA, float distB) {
    return max(distA, distB);
}

float unionSDF(float distA, float distB) {
    return min(distA, distB);
}

float differenceSDF(float distA, float distB) {
    return max(distA, -distB);
}


vec2 opU(vec2 d1, vec2 d2 ) {
  vec2 resp;
    if (d1.x < d2.x){
        resp = d1;
    }
    else
    {
        resp = d2;
    }

   return resp;
}

// Create infinite copies of an object -  http://iquilezles.org/www/articles/distfunctions/distfunctions.htm
vec2 opRep( in vec2 p, in float s )
{
    return mod(p+s*0.5,s)-s*0.5;
}



///-------------------------------

float h(vec3 p) {



    vec2 res;
    float dif1, dif2;


    res= vec2(999.9);

     p.xz=opRep( p.xz, 15. );
   // p.z=mod( mod(p.z, 7.0),2.0)-0.5;



    float sdb1=sdBox(p-vec3(0.0,5.5,0.), vec3(8.5,5.8,8.0) );
    float sdb2=sdBox(p-vec3(0.0,6.0,0.), vec3(7,5.2,9.0) );

    float sdb3d=sdBox(p-vec3(0.0,6.0,0.), vec3(1,1.0,1.0) );



    float sdb3pisoi=sdBox(p-vec3(-5.0,3.0,0.0), vec3(3,1.1,2.) );
    float sdb4pisod=sdBox(p-vec3(5.0,3.0,0.0), vec3(3,1.1,2.) );

    float sdb5pisoi=sdBox(p-vec3(-5.0,6.0,0.0), vec3(3,1.1,4.0) );
    float sdb6pisod=sdBox(p-vec3(5.0,6.0,0.0), vec3(3,1.1,4.0) );



    dif2=differenceSDF(sdb1,sdb3pisoi);
    dif2=differenceSDF(dif2,sdb4pisod);
    dif2=differenceSDF(dif2,sdb5pisoi);
    dif2=differenceSDF(dif2,sdb6pisod);

    //dif1=differenceSDF(sdb1, sdb2);
    dif1=differenceSDF(dif2,sdb2);


    res=opU(res, vec2(dif1,8));

    return res.x;
}

vec3 GetNormal(vec3 p) {
	vec2 e=vec2(.001,0);
	return normalize(vec3(
		h(p+e.xyy)-h(p-e.xyy),
		h(p+e.yxy)-h(p-e.yxy),
		h(p+e.yyx)-h(p-e.yyx)
	));
}

float AO(vec3 p,vec3 q){			// AO at point p with normal q
	float o=0.,s=1.,r,d;
    	for(float i=0.;i<5.;i++){
		r=.01+.12*i/4.;
		vec3 a=q*r+p;
		d=h(a);
		o+=-(d-r)*s;
		s*=.95;
    	}
    	return clamp(1.-3.*o,0.,1.);
}

float SH(vec3 p, vec3 q){			// Calculate shadow amount: p=intersection point; q=light direction
	vec3 r = p + q*.01;
	float d;
	for(int i=0; i<16; i++) {
		d = h(r);
		if (d < .001)
			break;
		r += q * d;				// March along!
	}
	if (d < 0.001)
		return 0.1;
	else
		return 1.;
}

void main()
{
	vec2 uv = (fragCoord.xy * 2. - iResolution.xy) / iResolution.y;
	
	float ftime = iTime*speed*5.0;
	//vec3 ro = vec3(1.4, 1.6, -4+iTime);			// Cam pos (ray origin)
    vec3 ro = vec3(1.4, 4.6, -24.0+ftime);			// Cam pos (ray origin)


	vec3 up = vec3(0, 1, 0);			// Cam orientation vecs
	vec3 fd = vec3(0, 0, 1);
	vec3 right = -cross(fd,up);
	
	float fov=1.8;
	vec3 rd = normalize(right*uv.x + up*uv.y + fd*fov);	// Ray dir
	
	vec3 p = ro;					// Current test position
	float d;					// Distance of p from surface
	for(int i=0; i<128; i++) {
		d = h(p);
		if (d < .001)
			break;
		p += rd * d;				// March along!
	}
	




	if (d < .001) {					// We hit something!
		//vec3 lp = vec3(10,10,-10);		// Light position
        //vec3 lp = vec3(0.5,5.,-10);		// Light position
        vec3 lp = vec3(0.0,5.,ftime);		// Light position

		vec3 ld = normalize(lp-p);		// Light direction
		vec3 n = GetNormal(p); 			// Normal
		float diffuse = dot(n,ld);		// Diffuse amount
		vec3 c = vec3(.8,.9,1) * diffuse * AO(p,n) * SH(p,ld);
		
	

        vec3 lp1 = vec3(10.0,1.,ftime-5.0);		// Light position

		vec3 ld1 = normalize(lp1-p);		// Light direction
		vec3 n1 = GetNormal(p); 			// Normal
		float diffuse1 = dot(n1,ld1);		// Diffuse amount
		vec3 c1 = vec3(.3,.5,.8) * diffuse1 * AO(p,n1) * SH(p,ld1);
		
		fragColor =( vec4(c, 1)+vec4(c1, 1))/1.5;



	} else						// We fucking hit NOTHING
		fragColor = vec4(uv.x / 2., uv.y * 1.4, 1, 1);
}
