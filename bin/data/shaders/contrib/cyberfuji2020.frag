#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float gridx ;
uniform float mseed ;
uniform float offsetxm ;
uniform float bloom_f ;
uniform float suns ;
uniform float fugiid1 ;
float sun(vec2 uv, float battery)
{	
	float msun = mapr(suns,0.1,0.5);
 	float val = smoothstep(msun, msun-0.01, length(uv));
 	float bloom = smoothstep(mapr(bloom_f,0.2,7.0), 0.0, length(uv));
    float cut = 3.0 * sin((uv.y + iTime * 0.03 * (battery + 0.02)) * 100.0) 
				+ clamp(uv.y * 14.0 + 1.0, -6.0, 6.0);
    cut = clamp(cut, 0.0, 1.0);
    return clamp(val * cut, 0.0, 1.0) + bloom * 0.6;
}

float grid(vec2 uv, float battery)
{
    vec2 size = vec2(uv.y, uv.y * uv.y * 0.2) * 0.01;
    uv += vec2(0.0, iTime * 1.0 * (battery + 0.05));
	uv.x+=0.5;
	float x = abs(uv.x-.5);
    uv = abs(fract(uv) - 0.5);
 	vec2 lines = smoothstep(size, vec2(0.0), uv);
 	lines += smoothstep(size * 5.0, vec2(0.0), uv) * 0.4 * battery;
	float linea = step(x,.1)*smoothstep(.18,.23,mod(uv.y,.5));
	
	
    return clamp(lines.x + lines.y, 0.0, 3.0)*step(1.97,x)+linea;
	
}

float dot2(in vec2 v ) { return dot(v,v); }

float sdTrapezoid( in vec2 p, in float r1, float r2, float he )
{
    vec2 k1 = vec2(r2,he);
    vec2 k2 = vec2(r2-r1,2.0*he);
    p.x = abs(p.x);
    vec2 ca = vec2(p.x-min(p.x,(p.y<0.0)?r1:r2), abs(p.y)-he);
    vec2 cb = p - k1 + k2*clamp( dot(k1-p,k2)/dot2(k2), 0.0, 1.0 );
    float s = (cb.x<0.0 && ca.y<0.0) ? -1.0 : 1.0;
    return s*sqrt( min(dot2(ca),dot2(cb)) );
}

float sdLine( in vec2 p, in vec2 a, in vec2 b )
{
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,vec2(0))) + min(max(d.x,d.y),0.0);
}

float opSmoothUnion(float d1, float d2, float k){
	float h = clamp(0.5 + 0.5 * (d2 - d1) /k,0.0,1.0);
    return mix(d2, d1 , h) - k * h * ( 1.0 - h);
}
float noise2 (in vec2 st,float fase) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    
    float fase2 = fase;
    // Four corners in 2D of a tile
    float a = sin(random(i)*fase2);
    float b =  sin(random(i + vec2(1.0, 0.0))*fase2);
    float c =  sin(random(i + vec2(0.0, 1.0))*fase2);
    float d =  sin(random(i + vec2(1.0, 1.0))*fase2);

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    vec2 u = f*f*(3.0-2.0*f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

float sdCloud(in vec2 p, in vec2 a1, in vec2 b1, in vec2 a2, in vec2 b2, float w)
{
	//float lineVal1 = smoothstep(w - 0.0001, w, sdLine(p, a1, b1));
    float lineVal1 = sdLine(p, a1, b1);
    float lineVal2 = sdLine(p, a2, b2);
    vec2 ww = vec2(w*1.5, 0.0);
    vec2 left = max(a1 + ww, a2 + ww);
    vec2 right = min(b1 - ww, b2 - ww);
    vec2 boxCenter = (left + right) * 0.5;
    //float boxW = right.x - left.x;
    float boxH = abs(a2.y - a1.y) * 0.5;
    //float boxVal = sdBox(p - boxCenter, vec2(boxW, boxH)) + w;
    float boxVal = sdBox(p - boxCenter, vec2(0.04, boxH)) + w;
    
    float uniVal1 = opSmoothUnion(lineVal1, boxVal, 0.05);
    float uniVal2 = opSmoothUnion(lineVal2, boxVal, 0.05);
    
    return min(uniVal1, uniVal2);
}
float ridge2(float h, float offset) {
    h = abs(h);     // create creases
    h = offset - h; // invert so creases are at top
    h = h * h;      // sharpen creases
    return h;
}
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

void main()
{
    vec2 uv = (2.0 * gl_FragCoord.xy.xy - iResolution.xy)/iResolution.y;
	uv.y = 1-uv.y;
	uv.y -=1.0;
    float battery = 1.0;
   
    {
        // Grid
        float fog = smoothstep(0.1, -0.02, abs(uv.y + 0.2));
        vec3 col = vec3(0.0, 0.1, 0.2);
        if (uv.y < -0.2)
        {
            uv.y = mapr(gridx,1.5,3.0) / (abs(uv.y + 0.0) + 0.05);
            uv.x *= uv.y * 1.0;
            float gridVal = grid(uv, battery);
            col = mix(col, vec3(1.0, 0.5, 1.0), gridVal);
        }
        else
        {
            float fujiD = min(uv.y *  mapr(fugiid1,0.0,6.0)- 0.1, 1.0);
            uv.y -= battery * 1.1 - 0.51;
            
            vec2 sunUV = uv;
            vec2 fujiUV = uv;
         
            sunUV += vec2(0.0, 0.0);
            //uv.y -= 1.1 - 0.51;
            col = vec3(1.0, 0.0, 1.0);
            float sunVal = sun(sunUV, battery);
            
            col = mix(col, vec3(1.0, 0.4, 0.1), sunUV.y * 1.0 + 0.8);
            col = mix(vec3(0.0, 0.0, 0.0), col, sunVal*1.);
            
			vec2 fjuv = vec2(uv.x,uv.y+0.2) ; 
				 fjuv.x = abs(fract(uv.x)*2.-1.);
				 fjuv.x = abs(fract(fjuv.x*0.2-1.));

			float ofssetx = uv.x +mapr(offsetxm,-0.2,0.2);
			
			ofssetx = uv.x;

			float sf = sin(uv.x*2.+time)*(sin(uv.x*10.)*.6+.6)+.8;
            float fujiVal = sdTrapezoid( fjuv  + vec2(sunUV.y , 0.4), 0.75 + pow(fjuv.y * fjuv.y, 3.1), 0.05, 0.2);
			
			float n = noise2(vec2(uv.x*2.,uv.x*2.),1626562324.*mseed);
				//  n = 1.-ridgedMF3(vec2(uv.x*0.69));;
				  n+= noise2(vec2(uv.x*4.,uv.x*4.),8287242.*mseed)*.5;
				  
				  
				  float msepx = 0.3;
				  n += smoothstep(0.5+msepx,0.5-msepx,abs(uv.x))*2.08;
				  
				  
			fujiVal = abs(uv.y+n*.19+.78)-.27;
            float waveVal = uv.y + sin(uv.x * 20.0 + iTime * 1.0) * 0.2 + 0.2;
            float wave_width = smoothstep(0.0,0.05,(waveVal));
            
            col = mix( col, mix(vec3(0.0, 0.0, 0.25), vec3(1.0, 0.0, 0.5), fujiD), step(fujiVal, 0.0));
			
           // col = mix( col, vec3(1.0, 0.5, 1.0), wave_width * step(fujiVal, 0.0));
			
            col = mix( col, vec3(1.0, 0.5, 1.0), 1.0-smoothstep(0.0,0.01,abs(fujiVal)) );
         
            col += mix( col, 
					mix(vec3(1.0, 0.12, 0.8), 
					vec3(0.0, 0.0, 0.2), 
					clamp(uv.y * 3.5 + 3.0, 0.0, 1.0)), step(0.0, fujiVal) );
            
           
			
        }

        col += fog * fog * fog;
        col = mix(vec3(col.r, col.r, col.r) * 0.5, col, battery * 0.7);

        fragColor = vec4(col,1.0);
    }
    //else fragColor = vec4(0.0);

    
}