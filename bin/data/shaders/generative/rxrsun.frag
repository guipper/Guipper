#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float rxmult ;
uniform float mcnt ;
mat2 rotate2d(float _angle);
float noise (in vec2 st,float fase);
float random (in vec2 _st);
//#define PI 3.14159265359
//#define TWO_PI 6.28318530718
//#define pi 3.14159265359
float sm(float v1,float v2,float val){return smoothstep(v1,v2,val);}


float snoise2(vec2 v) {

    // Precompute values for skewed triangular grid
    const vec4 C = vec4(0.211324865405187,
                        // (3.0-sqrt(3.0))/6.0
                        0.366025403784439,
                        // 0.5*(sqrt(3.0)-1.0)
                        -0.577350269189626,
                        // -1.0 + 2.0 * C.x
                        0.024390243902439);
                        // 1.0 / 41.0

    // First corner (x0)
    vec2 i  = floor(v + dot(v, C.yy));
    vec2 x0 = v - i + dot(i, C.xx);

    // Other two corners (x1, x2)
    vec2 i1 = vec2(0.0);
    i1 = (x0.x > x0.y)? vec2(1.0, 0.0):vec2(0.0, 1.0);
    vec2 x1 = x0.xy + C.xx - i1;
    vec2 x2 = x0.xy + C.zz;

    // Do some permutations to avoid
    // truncation effects in permutation
    i = mod289(i);
    vec3 p = permute(
            permute( i.y + vec3(0.0, i1.y, 1.0))
                + i.x + vec3(0.0, i1.x, 1.0 ));

    vec3 m = max(0.5 - vec3(
                        dot(x0,x0),
                        dot(x1,x1),
                        dot(x2,x2)
                        ), 0.0);

    m = m*m ;
    m = m*m ;
    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;
    m *= 1.79284291400159 - 0.85373472095314 * (a0*a0+h*h);

    // Compute final noise value at P
    vec3 g = vec3(0.0);
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * vec2(x1.x,x2.x) + h.yz * vec2(x1.y,x2.y);
    return 130.0 * dot(m, g);
}


float ridge2(float h, float offset) {
    h = abs(h);     // create creases
    h = offset - h; // invert so creases are at top
    h = h * h;      // sharpen creases
    return h;
}
#define OCTAVES 8
float ridgedMF2(vec2 p) {
    float lacunarity = 2.0;
    float gain = 0.5;
    float offset = 0.9;

    float sum = 0.0;
    float freq = 1.0, amp = 0.5;
    float prev = 1.0;
    for(int i=0; i < OCTAVES; i++) {
        float n = ridge2(snoise2(p*freq), offset);
        sum += n*amp;
        sum += n*amp*prev;  // scale by previous octave
        prev = n;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

float rxr2(vec2 uv){

    float e = 0;
    
    e = ridgedMF2(vec2(ridgedMF(vec2(uv.x,uv.y))));
    
    return e;
}






float poly2(vec2 uv,vec2 p, float s, float dif,int N,float a){
    // Remap the space to -1. to 1.
    vec2 st = p - uv ;
    // Angle and radius from the current pixel
    float a2 = atan(st.x,st.y)+a;
    float r = TWO_PI/float(N);
    float d = cos(floor(.5+a2/r)*r-a2)*length(st);
    float e = 1.0 - smoothstep(s,dif,d);
    return e;
}

vec2 rotate2d(vec2 uv,float _angle){
      
      float fx = resolution.x/resolution.y;
      uv-=vec2(0.5*fx,0.5);
      uv = uv*mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle));
      uv+=vec2(0.5*fx,0.5);
      
      return uv;
}
vec2 rotate2d2(vec2 uv,float _angle){
      
      float fx = resolution.x/resolution.y;
      uv-=vec2(0.5,0.5);
      uv = uv*mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle));
      uv+=vec2(0.5,0.5);
      
      return uv;
}
vec2 scale3(vec2 uv, vec2 sc){
    uv-=vec2(0.5);
    mat2 e = mat2(sc.x,0.0,
                0.0,sc.y); 
    uv = uv*e;
    uv+=vec2(0.5);
    return vec2(uv);
}

void main(void){   
    
    vec2 uv = gl_FragCoord.xy/resolution.xy;
    vec2 puv = uv;
    float fx = resolution.x/resolution.y;
    uv.x*=fx;
    vec2 uv2 = uv;
    
    vec2 p = vec2(0.5*fx,0.5) - uv;
    float r = length(p);
    float a =atan(p.x,p.y);
  
   
    
    vec3 dib = vec3(0.0);
    float e =0.;
   
     
    
    const int cnt = 10;
    
    vec2 uv3 = uv;
    
    //uv3.x*=fx;
    float mmcnt = mapr(mcnt,0.0,5.0);
    for(int i =1; i<=cnt; i++){

		float index = float(i)/float(cnt);
		float fase = index*pi*2;
		
		uv3 = rotate2d(uv3,time*0.02+fase); 
		//uv3 = scale(uv3,vec2(0.9999999999+sin(r*3.0+time)*0.05+0.05));
		uv3 = scale3(uv3,vec2(0.85+rxr2(uv*2.2+time*0.1)*rxmult));
	   
		//uv3 = scale(uv3,vec2(1.0+rxr(uv*0.2+time*0.001)*0.1));
		vec2 uv3 = fract(uv3*i);
		vec2 p2 = vec2(0.5) -uv3;
		float a2 = atan(p2.x,p2.y);
		float r2 = length(p2);

		float siz = 0.1;
		//e= cir(uv3,vec2(0.5,0.5),siz,siz);
		e=0.;
		//uv3 = fract(uv3*0.5);
		e+= poly2(uv3,vec2(0.5,0.5),siz,siz,9,pi/2+fase+time*0.5)*1.2;
		e-= poly2(uv3,vec2(0.5,0.5),siz*0.,siz*0.0,9,pi/2+fase+time*0.5)*1.0;
		
		
		vec3 col2 = vec3(1.0-r,1.0-r,0.0);                
		vec3 col1 = vec3(1.0-r,0.0,1.0-sm(0.1,0.9,r));
		dib+= vec3(e)*mix(col1,col2,sm(e,.0,index));
		if(float(i) > mmcnt){
			break;
		}
    }
    
    //dib/=cnt*0.6;
    //dib+=sin(dib*10+time*3)*0.00;

    puv = rotate2d2(puv,-time*0.000001);
    puv = scale(puv,vec2(0.999));
    //puv+=vec2(0.003);
    vec4 prev = texture(feedback,puv);
    
    
    
    
    vec3 fin = dib+prev.rgb*0.9;
         fin = mix(dib,prev.rgb,0.95);
         
     float lm = 0.8;
     if(dib.r > lm || dib.g > lm || dib.b > lm){
        fin = dib;
       
     }else{
        fin = prev.rgb*0.995;
     }
    // fin = vec3(rxr(uv*10.2+time*0.001));
    fragColor = vec4(fin,1.0);
}







