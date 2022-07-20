#pragma include "../common.frag"

uniform float scx1_lines ;
uniform float scy1_lines ;
uniform float rot_lines ; 
uniform float rxr_sc ;
uniform float palette ;
float sm(float v1,float v2,float val){return smoothstep(v1,v2,val);}


mat2 scale2(vec2 _scale){
    mat2 e = mat2(_scale.x,0.0,
                0.0,_scale.y); 
    return e;
}

mat2 rotate2d2(float _angle){
    return mat2(cos(_angle),-sin(_angle),
                sin(_angle),cos(_angle));
}


// Some useful functions
vec3 mod2892(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2 mod2892(vec2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permute2(vec3 x) { return mod289(((x*34.0)+1.0)*x); }

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
    vec3 p = permute2(
            permute2( i.y + vec3(0.0, i1.y, 1.0))
                + i.x + vec3(0.0, i1.x, 1.0 ));

    vec3 m = max(0.5 - vec3(
                        dot(x0,x0),
                        dot(x1,x1),
                        dot(x2,x2)
                        ), 0.0);

    m = m*m ;
    m = m*m ;

    // Gradients:
    //  41 pts uniformly over a line, mapped onto a diamond
    //  The ring size 17*17 = 289 is close to a multiple
    //      of 41 (41*7 = 287)

    vec3 x = 2.0 * fract(p * C.www) - 1.0;
    vec3 h = abs(x) - 0.5;
    vec3 ox = floor(x + 0.5);
    vec3 a0 = x - ox;

    // Normalise gradients implicitly by scaling m
    // Approximation of: m *= inversesqrt(a0*a0 + h*h);
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
    e = ridgedMF2(vec2(ridgedMF2(vec2(uv.x,uv.y))));    
    return e;
}

float bcir(vec2 uv,float s,float bs,float bd){
    //BS = BORDER SIZE
    //Only border
    float fx = resolution.x/resolution.y;
    vec2 p = vec2(0.5*fx,0.5) -uv;
    float r = length(p);
    float e = 1.0-smoothstep(s+bs,s+bs+bd,r);
    e-=1.0-smoothstep(s*0.9,s*1.0,r);
    return e;
}
void main(void){   
    
     
    vec2 uv = gl_FragCoord.xy/resolution.xy;
    
    
    
    float fx = resolution.x/resolution.y;
    uv.x=1.0-uv.x;
	uv.x*=fx;
    
	vec2 uv2 = uv;
    
	uv2-=vec2(0.5);
	uv2*=rotate2d2(mapr(rot_lines,0.0,PI));
	uv2+=vec2(0.5);
	
	
	
    vec2 p = vec2(0.5*fx,0.5) - uv;
    float r = length(p);
    float a =atan(p.x,p.y);
    //uv = rotate2d(uv,time*0.9);
    float e = bcir(uv,0.02,0.001,0.02);

   
    //uv = rotate2d2(fract(uv*3),time*0.3);
    
    
    
    float ps = 0.35;
   
    
   
    float cir1 = (1.0-sm(ps,ps*1.03,r))*0.05+(0.85-sm(0.0,0.5,r))*0.9;
    float cir2 = (1.0-sm(ps,ps*1.03,r))*1.0;
    
    float lines = sin(uv2.y*50.*scx1_lines-uv2.x*20*scy1_lines)*4.5*cir1+0.5;
    //lines+=sin(lines*10+time)*0.2;

	float mrxr_sc = mapr(rxr_sc,0.5,2.0);
    float rx = rxr2(vec2(uv2.x*0.5*mrxr_sc+time*0.01,uv2.y*0.5*mrxr_sc+time*0.01))*lines;
    
    rx+=sin(rx*10+time*2)*0.2;
    e = cir1;
    e*=rx;
    
    //e+=lines*cir1;
    
	vec3 col1 = vec3(1.0,0.6,0.0)*cir1*1.2;
    vec3 col2 = vec3(1.0,1.0,1.0);                
    vec3 col3 = vec3(0.6,0.2,0.3);
	
	
	vec3 col1_2 = vec3(0.0,0.8,0.5)*cir1*1.2;
	vec3 col2_2 = vec3(0.8,1.0,1.0);   
	vec3 col3_2 = vec3(0.4,0.8,0.8);
	
	vec3 col1_3 = vec3(0.0,0.7,1.0)*cir1*1.2;
	vec3 col2_3 = vec3(1.0,1.0,1.0);   
	vec3 col3_3 = vec3(0.5,0.5,1.0);
	
	float patron =mapr(palette,0.0,2.0);
	
	//vec3 fin = vec3(0.0);
     if(patron <= 1.0){
		col1 = mix(col1,col1_2,patron);
		col2 = mix(col2,col2_2,patron);
		col3= mix(col3,col3_2,patron);
	 }
	 if(patron > 1.0 && patron <=2.0){
		col1 = mix(col1_2,col1_3,1.-patron);
		col2 = mix(col2_2,col2_3,1.-patron);
		col3 = mix(col2_2,col3_3,1.-patron);
	 }
	 


	col1 = mix(col1,col1_2,mapr(palette,0.0,0.33));
	
    vec3 dib = mix(col1,col2,e)*0.9;
    

    vec3 dib2 = vec3(0.0);
    
    
    dib2 = col3 * vec3(sin(rx*2)*.9)*sm(0.1,1.0,lines);
    
    dib2+=sm(0.2,0.8,rx)*vec3(1.0,0.5,(1.0-sm(0.0,0.7,r))*2);
    
    dib2*=rxr(uv2*0.05)*0.9;
    vec3 fin = mix(dib2,dib,cir2);
    //fin = dib2;
    
    fragColor = vec4(fin,1.0);
}