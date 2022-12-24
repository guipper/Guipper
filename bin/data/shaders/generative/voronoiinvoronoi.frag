#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float speed ;
uniform float frac1 ;
uniform float seed ;
vec2 scale(vec2 uv, float s);

mat2 scale(vec2 _scale);
mat2 rotate2d(float _angle); 

float voronoi(vec2 uv);
float voronoi(vec2 uv,float circle);

float t = 0.0;

vec2 random3( vec2 p ) {
    return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}

vec3 voronoi2( in vec2 x ) {
    vec2 n = floor(x);
    vec2 f = fract(x);

    // first pass: regular voronoi
    vec2 mg, mr;
    float md = 8.0;
    for (int j= -1; j <= 1; j++) {
        for (int i= -1; i <= 1; i++) {
            vec2 g = vec2(float(i),float(j));
            vec2 o = random3( n + g +seed);
            o = 0.5 + 0.5*sin( time* speed+ 6.2831*o );

            vec2 r = g + o - f;
            float d = dot(r,r);

            if( d<md ) {
                md = d;
                mr = r;
                mg = g;
            }
        }
    }

    // second pass: distance to borders
    md = 8.0;
    for (int j= -2; j <= 2; j++) {
        for (int i= -2; i <= 2; i++) {
            vec2 g = mg + vec2(float(i),float(j));
            vec2 o = random3( n + g +seed );
            o = 0.5 + 0.5*sin( time* speed + 6.2831*o ); 

            vec2 r = g + o - f;

            if ( dot(mr-r,mr-r)>0.00001 ) {
                md = min(md, dot( 0.99995*(mr+r), normalize(r-mr) ));
            }
        }
    }
    return vec3(md, mr);
}

float voronoi_v2(vec2 uv){
 // Scale
   // uv *= 10.;

    // Tile the space
    vec2 i_st = floor(uv);
    vec2 f_st = fract(uv);

    float m_dist = uv.x;  // minimun distance
    vec2 m_point;        // minimum point

    for (int j=-1; j<=1; j++ ) {
        for (int i=-1; i<=1; i++ ) {
            vec2 neighbor = vec2(float(i),float(j));
            vec2 point = random3(i_st + neighbor+seed);
            point = 0.5 + 0.5*sin(time* speed + 6.2831*point);
            vec2 diff = neighbor + point - f_st;
            float dist = length(diff);

            if( dist < m_dist ) {
                m_dist = dist;
                m_point = point;
            }
        }
    }   
    return dot(m_point,vec2(.1,1.0));
}
vec3 rgb2hsb2( in vec3 c ){
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz),
                 vec4(c.gb, K.xy),
                 step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r),
                 vec4(c.r, p.yzx),
                 step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)),
                d / (q.x + e),
                q.x);
}

void main(void)
{   
    
    vec2 uv = gl_FragCoord.xy / resolution.xy;
	
	t+=floor(length(uv)*3.2);
	
    float fx = resolution.x/resolution.y;
    uv.x*=fx;
    vec2 p = vec2(0.5*fx,0.5)- uv;
    float r =length(p);
    float a = atan(p.x,p.y);
 
	float mfrac1 = mapr(frac1,0.2,1.4);
	
    vec3 dib1 = vec3(0.0);
    float voroborde3 = smoothstep(0.00,0.01,voronoi2(uv*20.*mfrac1).r);
    
	
			voroborde3*= voronoi_v2(uv*20.*mfrac1)*1.+0.2;
    
    float estrellita =voroborde3*0.45;
    
    dib1 += 1.0-estrellita;
  
    vec3 colfondo = vec3(0.0,0.5,0.3); 
    //vec3 verde = vec3(0.0,0.5,0.3);
	vec3 colsup = vec3(1.0,1.0,1.0);

  
    float vorocolor = voronoi_v2(vec2(uv*10.*mfrac1));
    float voroborde2 = smoothstep(0.08,0.078,voronoi2(uv*10.*mfrac1).r);
    
    dib1 *=1.0-voroborde2;

    dib1 = mix( (step(0.0,dib1)*1.0-voroborde2)*colsup,
                dib1*colfondo,vec3(smoothstep(0.1,0.8,voronoi_v2(uv*10.*mfrac1))));
        
 
    vec3 fin = dib1;
 
    
    fragColor = vec4(fin,1.0);
   
}

