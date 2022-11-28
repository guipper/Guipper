#pragma include "../common.frag"

const float PI2 = acos(-1.) * 2.;
uniform float scale2 = 0.3 ;

float hash1D(in float p)
{
    return fract(sin(p) * 6124.7621);
}

vec2 hash21(in float p)
{
    return fract(sin(vec2(p, p*1.4217)) * 6124.7621);
}

float hash2D(in vec2 p)
{
    vec2 v = vec2(162.1732, 116.1734);
    return fract(sin(dot(p, v)) * 6124.7621);
}

float hash3D(in vec3 p)
{
    vec3 v = vec3(123.4116, 162.3271, 137.1618);
    return fract(sin(dot(p, v)) * 6124.7621);
}

float noise3D(in vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3. - 2. * f);
    vec3 b = vec3(156, 12, 5);
    float n = dot(i, b);
	return mix(	mix(	mix(hash1D(n), hash1D(n + b.x), f.x),
		   				mix(hash1D(n + b.y), hash1D(n + b.x + b.y), f.x),
		   				f.y),
		  	 	mix(	mix(hash1D(n + b.z), hash1D(n + b.x + b.z), f.x),
		   				mix(hash1D(n + b.y + b.z), hash1D(n + b.x + b.y + b.z), f.x),
		   				f.y),
		  	 	f.z);
}

float sdSeg(in vec2 p)
{
    p = abs(p);
    return max(p.x - scale2 / 3., p.x + p.y - scale2);
}

float sd0(in vec2 p)
{
    p = abs(p);
    p.y -= scale2;
    if(p.y > p.x) {
        p = p.yx;
    }
    p.x -= scale2;
    return sdSeg(p);
}

float sd1(in vec2 p)
{
    p.y = abs(p.y);
    p -= scale2;
    return sdSeg(p);
}
    
float sd2(in vec2 p)
{
    if(p.y < 0.) {
        p *= -1.;
    }
    p.y = abs(p.y - scale2);
    if(p.y > p.x) {
        p = p.yx;
    }
    p.x -= scale2;
    return sdSeg(p);
}

float sd3(in vec2 p)
{
    p.y = abs(abs(p.y) - scale2);
    if(p.y > p.x) {
        p = p.yx;
    }
    p.x -= scale2;
    return sdSeg(p);
}

float sd4(in vec2 p)
{
    float d = sdSeg(p - vec2(-scale2, scale2));
    p.y = abs(p.y) - scale2;
    if(p.y < -p.x) {
        p = -p.yx;
    }
    p.x -= scale2;
    return min(d, sdSeg(p));
}

float sd5(in vec2 p)
{
    if(p.y < 0.) {
        p *= -1.;
    }
    p.y = abs(p.y - scale2);
    if(p.y > -p.x) {
        p = -p.yx;
    }
    p.x += scale2;
    return sdSeg(p);
}

float sd6(in vec2 p)
{
    float d = sdSeg(p - vec2(scale2, scale2));
    p = abs(p);
    p.y = abs(p.y - scale2);
    if(p.y > p.x) {
        p = p.yx;
    }
    p.x -= scale2;
    return max(-d, sdSeg(p));
}

float sd7(in vec2 p)
{
    float d = sdSeg(p - vec2(scale2, -scale2));
    p.y -= scale2;
    if(p.y > p.x) {
        p = p.yx;
    }
    p.x -= scale2;
    return min(d, sdSeg(p));
}

float sd8(in vec2 p)
{
    p = abs(p);
    p.y = abs(p.y - scale2);
    if(p.y > p.x) {
        p = p.yx;
    }
    p.x -= scale2;
    return sdSeg(p);
}

float sd9(in vec2 p)
{
    float d = sdSeg(p - vec2(-scale2, -scale2));
    p = abs(p);
    p.y = abs(p.y - scale2);
    if(p.y > p.x) {
        p = p.yx;
    }
    p.x -= scale2;
    return max(-d, sdSeg(p));
}

void main()
{
    vec2 p = (fragCoord * 2. - iResolution.xy)/min(iResolution.x, iResolution.y);
    vec3 col = vec3(0);
    vec2 interval = vec2(3., 1.7);
    
    for(float i=0.; i<20.; i++) {
        float L = 1. - fract(iTime) + i;
        vec2 q = p / atan(.001, L) / 500.;
        
        L = dot(q,q) * 2. + L * L;
        float I = ceil(iTime) + i;
        q += hash21(I) * interval;
        
        vec3 ID = vec3(ceil(q / interval), I);
        q = mod(q, interval) - interval * .5;
        float n = hash3D(ID + ceil(iTime * 3.) * 1.3834);
        ID.y += ceil(iTime * 10. + hash2D(vec2(ID.x, I)));
        float n2 = noise3D(ID * .316572);
        
        float d = 0.;
        float N = 10.;
        if(n < 1. / N) {
            d = sd0(q);
        } else if(n < 2. / N) {
            d = sd1(q);
        } else if(n < 3. / N) {
            d = sd2(q);
        } else if(n < 4. / N) {
            d = sd3(q);
        } else if(n < 5. / N) {
            d = sd4(q);
        } else if(n < 6. / N) {
            d = sd5(q);
        } else if(n < 7. / N) {
            d = sd6(q);
        } else if(n < 8. / N) {
            d = sd7(q);
        } else if(n < 9. / N) {
            d = sd8(q);
        } else {
            d = sd9(q);
        }
        
        float temp = col.g;
        if(-scale2 * .27 < d && d < -scale2 * .067 && n2 < .5) {
            if(temp == 0.) {
                col += vec3(.3, 1, .4) * exp(-L * .002);
            }
            col += step(temp, .7) * pow(hash1D(n2), 32.);
        }
    }

    fragColor = vec4(col, 1.0);
}