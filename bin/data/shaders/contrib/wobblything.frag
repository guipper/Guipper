#pragma include "../common.frag"

#define FARCLIP    35.0

#define MARCHSTEPS 60
#define AOSTEPS    8
#define SHSTEPS    10
#define SHPOWER    3.0

#define PI2        PI*0.5    

#define AMBCOL     vec3(1.0,1.0,1.0)
#define BACCOL     vec3(1.0,1.0,1.0)
#define DIFCOL     vec3(1.0,1.0,1.0)

#define MAT1       1.0

#define FOV 1.0


/***********************************************/
float rbox(vec3 p, vec3 s, float r) {	
    return length(max(abs(p)-s+vec3(r),0.0))-r;
}
float torus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz)-t.x,p.y);
    return length(q)-t.y;
}
float cylinder(vec3 p, vec2 h) {
    return max( length(p.xz)-h.x, abs(p.y)-h.y );
}

/***********************************************/
void oprep2(inout vec2 p, float l, float s, float k) {
	float r=1./l;
	float ofs=s+s/(r*2.0);
	float a= mod( atan(p.x, p.y) + PI2*r*k, PI*r) -PI2*r;
	p.xy=vec2(sin(a),cos(a))*length(p.xy) -ofs;
	p.x+=ofs;
}

float hash(float n) { 
	return fract(sin(n)*43758.5453123); 
}

float noise3(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f*f*(3.0-2.0*f);
    float n = p.x + p.y*57.0 + p.z*113.0;
    float res = mix(mix(mix( hash(n+  0.0), hash(n+  1.0),f.x),
                        mix( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
                    mix(mix( hash(n+113.0), hash(n+114.0),f.x),
                        mix( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
    return res;
}

float sminp(float a, float b) {
    const float k=0.1;
    float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
    return mix( b, a, h ) - k*h*(1.0-h);
}


/***********************************************/

vec2 DE(vec3 p) {
    
    //distortion
    float d3=noise3(p*2.0 + iTime)*0.18;
    //shape
    float h=torus(p, vec2(3.0,1.5)) -d3;
    float h2=torus(p, vec2(3.0,1.45)) -d3;
        vec3 q=p.yzx; p.yz=q.yx;
        oprep2(p.xy,32.0,0.15, 0.0);
        oprep2(p.yz,14.0,0.15, 0.0);
        float flag=p.z;
        float k=rbox(p,vec3(0.05,0.05,1.0),0.0) ;
        if (flag>0.1) k-=flag*0.18; else k-=0.01 ;

    //pipes
    p=q.zyx;

    oprep2(p.xy,3.0,8.5, 3.0);
    oprep2(p.xz,12.0,0.25, 0.0);
        
    p.y=mod(p.y,0.3)-0.5*0.3;
    float k2=rbox(p,vec3(0.12,0.12,1.0),0.05) - 0.01;

    p=q.xzy;
    float r=p.y*0.02+sin(iTime)*0.05;
        oprep2(p.zy,3.0,8.5, 0.0);
    float g=cylinder(p,vec2(1.15+r,17.0)) - sin(p.y*1.3 - iTime*4.0)*0.1 -d3;
    float g2=cylinder(p,vec2(1.05+r,18.0)) - sin(p.y*1.3 - iTime*4.0)*0.1 -d3;

      float tot=max(h,-h2);
      float sub=max(g,-g2);
        float o=max(tot,-g);
        float i=max(sub,-h);
        
            o=max(o,-k);
            i=max(i,-k2);
      
      tot=sminp(o,i);

	return vec2( tot*0.9 , MAT1);
}
/***********************************************/
vec3 normal(vec3 p) {
	vec3 e=vec3(0.01,-0.01,0.0);
	return normalize( vec3(	e.xyy*DE(p+e.xyy).x +	e.yyx*DE(p+e.yyx).x +	e.yxy*DE(p+e.yxy).x +	e.xxx*DE(p+e.xxx).x));
}
/***********************************************/
float calcAO(vec3 p, vec3 n ){
	float ao = 0.0;
	float sca = 1.0;
	for (int i=0; i<AOSTEPS; i++) {
        	float h = 0.01 + 1.2*pow(float(i)/float(AOSTEPS),1.5);
        	float dd = DE( p+n*h ).x;
        	ao += -(dd-h)*sca;
        	sca *= 0.65;
    	}
   return clamp( 1.0 - 1.0*ao, 0.0, 1.0 );
 //  return clamp(ao,0.0,1.0);
}
/***********************************************/
float calcSh( vec3 ro, vec3 rd, float s, float e, float k ) {
	float res = 1.0;
    for( int i=0; i<SHSTEPS; i++ ) {
    	if( s>e ) break;
        float h = DE( ro + rd*s ).x;
        res = min( res, k*h/s );
    	s += 0.02*SHPOWER;
    }
    return clamp( res, 0.0, 1.0 );
}
/***********************************************/
void rot( inout vec3 p, vec3 r) {
	float sa=sin(r.y); float sb=sin(r.x); float sc=sin(r.z);
	float ca=cos(r.y); float cb=cos(r.x); float cc=cos(r.z);
	p*=mat3( cb*cc, cc*sa*sb-ca*sc, ca*cc*sb+sa*sc,	cb*sc, ca*cc+sa*sb*sc, -cc*sa+ca*sb*sc,	-sb, cb*sa, ca*cb );
}
/***********************************************/
void main() {
    vec2 p = -1.0 + 2.0 * fragCoord.xy / iResolution.xy;
    p.x *= iResolution.x/iResolution.y;	
	vec3 ta = vec3(0.0, 0.0, 0.0);
	vec3 ro =vec3(0.0, 0.0, -15.0);
	vec3 lig=normalize(vec3(2.3, 3.0, 0.0));
	
//	vec2 mp=iMouse.xy/iResolution.xy;
//	rot(ro,vec3(mp.x,mp.y,0.0));
//	rot(lig,vec3(mp.x,mp.y,0.0));
	
    float a=iTime*0.5;
    float b=sin(iTime*0.25)*0.75;
	rot(ro,vec3(a,b,0.0));
	rot(lig,vec3(a,b,0.0));

	vec3 cf = normalize( ta - ro );
    vec3 cr = normalize( cross(cf,vec3(0.0,1.0,0.0) ) );
    vec3 cu = normalize( cross(cr,cf));
	vec3 rd = normalize( p.x*cr + p.y*cu + 2.5*cf );

	vec3 col=vec3(0.0);
	/* trace */
	vec2 r=vec2(0.0);	
	float d=0.0;
	vec3 ww;
	for(int i=0; i<MARCHSTEPS; i++) {
		ww=ro+rd*d;
		r=DE(ww);		
        if( abs(r.x)<0.00 || r.x>FARCLIP ) break;
        d+=r.x;
	}
    r.x=d;
	/* draw */
	if( r.x<FARCLIP ) {
	    vec2 rs=vec2(0.2,1.0);  //rim and spec
		if (r.y==MAT1) { col=vec3(0.29,0.53,0.91);  } 

		vec3 nor=normal(ww);

    	float amb= 1.0;		
    	float dif= clamp(dot(nor, lig), 0.0,1.0);
    	float bac= clamp(dot(nor,-lig), 0.0,1.0);
    	float rim= pow(1.+dot(nor,rd), 3.0);
    	float spe= pow(clamp( dot( lig, reflect(rd,nor) ), 0.5, 1.0 ) ,16.0 );
    	float ao= calcAO(ww, nor);
    	float sh= calcSh(ww, lig, 0.01, 2.0, 4.0);

	    col *= 0.5*amb*AMBCOL*ao + 0.4*dif*DIFCOL*sh + 0.05*bac*BACCOL*ao;
	    col += 0.3*rim*amb * rs.x;
    	col += 0.5*pow(spe,1.0)*sh * rs.y;
        
	}
	
	col*=exp(.08*-r.x); col*=2.0;
	
	fragColor = vec4( col, 1.0 );
}
