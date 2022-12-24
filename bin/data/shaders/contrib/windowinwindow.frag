#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
mat2 rot(float t)
{
 	return mat2(cos(t), sin(t), -sin(t), cos(t));
}

float sdBox( vec3 p, vec3 b )
{
  vec3 d = abs(p) - b;
  return min(max(d.x,max(d.y,d.z)),0.0) +
         length(max(d,0.0));
}
 
float sdBoxXY( vec3 p, vec3 b )
{
  vec2 d = abs(p.xy) - b.xy;
  return min(max(d.x,d.y),0.0) +
         length(max(d,0.0));
}

float mapr2 = 1.0;
float mapt = 0.0;
vec3 mapp = vec3(0.0);

float map(vec3 p)
{        
    //p.x -= 2.0;
    //p.x = (fract(p.x * 0.25) - 0.5) * 4.0;
    //p.z = abs(p.z - 1.0);
    
    float d = 1000.0;
    
    float room = mod(floor(mapr2), 4.0);
    
    vec2 scl = vec2(0.9, 0.5);
    
    if (room == 0.0) {
        scl.xy = scl.yx;
    }
    
    if (room == 1.0 || room == 3.0) {
        scl.xy = vec2(0.5);
    }
    
	float k = -sdBox(p, vec3(4.0, 1.0, 4.0));
    
    float c = sdBox(p, vec3(scl.x, scl.y, 0.05));
    
    float e = sdBox(p, vec3(scl.x+0.1, scl.y+0.1, 0.05));
    
    mapt = 3.0;
	mapp = p;
    
    if (room == 0.0)
    {
        if (k < d) {
            d = k;
            mapt = 1.0;
        }
    }
    
    if (room == 2.0)
    {
        vec3 spq = fract(p * 0.5 - 0.5) * 2.0 - 1.0;
        float spin = 1.3 - length(spq);
		float spd = spin;
        
        if (spd < d) {
            d = spd;
            mapt = 1.0;
        }
    }
    
    if (c < d) {
        d = c;
        mapt = 0.0;
		mapp = vec3(p.xy, 0.0);
    }

    if (e < d) {
        d = e;
        mapt = 1.0;
    }

    return d;
}

vec3 normal(vec3 p)
{
	vec3 o = vec3(0.01, 0.0, 0.0);
    return normalize(vec3(map(p+o.xyy) - map(p-o.xyy),
                          map(p+o.yxy) - map(p-o.yxy),
                          map(p+o.yyx) - map(p-o.yyx)));
}

float trace(vec3 o, vec3 r)
{
	float t = 0.0;
    for (int i = 0; i < 64; ++i) {
        vec3 p = o + r * t;
        float d = map(p);
        t += d * 0.8;
    }
    return t;
}

vec3 sky(vec3 r, float gt)
{
    vec3 t = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        float fi = float(i) / 3.0;
        float ft = fract(gt - fi);
        float bt = ft * (1.0 - ft) * 4.0;
        float sc = 1.0 / (1.0 + ft);
        vec2 uv = r.xy * sc * rot(iTime);
        vec3 tex = texture(iChannel0, uv).xyz;
        tex *= tex;
        t += tex * bt;
    }
    return t;
}

vec3 _texture(vec3 p)
{
	vec3 ta = texture(iChannel1, p.yz).xyz;
    vec3 tb = texture(iChannel1, p.xz).xyz;
    vec3 tc = texture(iChannel1, p.xy).xyz;
    return (ta*ta + tb*tb + tc*tc) / 3.0;
}

void main()
{
	vec2 uv = fragCoord.xy / iResolution.xy;
    uv = uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    
    float gt = iTime * 0.15;
    
    vec3 br = normalize(vec3(uv, 1.0));
    vec3 r = br;
    vec3 bo = vec3(0.0, 0.0, -2.0);
    vec3 o = bo + abs(bo) * fract(gt);
    
    mapr2 = mod(floor(gt), 4.0);
	float dir = sign(mod(floor(gt), 3.0) - 1.5);
    
    float near = fract(gt);
    mat2 mr = rot((1.0 - near) * near * 1.57);

    r.xy *= mr;
    o.xy *= mr;
    r.xz *= mr;
    o.xz *= mr;
    
    vec3 fc = vec3(0.0);
    vec3 ao = o;
    
    for (int i = 0; i < 8; ++i) {
        
        float fi = float(i);
        
        float t = trace(o, r);

        vec3 w = o + r * t;
        
        vec3 n = normal(w);

        float fd = map(w);

        vec2 puv = mapp.xy;

        float fog = 1.0 / (1.0 + t * t * 0.01);
        
        vec3 tc = vec3(fog);
        
        float first = max(sign(1.0-fi),0.0);
        
        vec3 ref = _texture(w * 0.1);
        
        ref *= 1.0 - abs(dot(r, n));
        //tc = mix(ref, ref * (1.0 - near), first);
        
        if (t > 100.0) {
            float pn = near;
            float st = mix(fract(gt)*4.0, fract(gt), near*first);
            fc = sky(br, st);
        } else {
        	fc = ref * fog;
        }
            
        if (mapt != 0.0) {
            break;
        }
        
        //fc = mix(fc, vec3(1.0, 0.0, 1.0), near);

        //fc = mix(fc, fc * vec3(1.0, 0.0, 1.0), 1.0-near);
        
        //fc = vec3(1.0);
        
        vec3 nr = normalize(vec3(puv, 1.0));
        
        float pwn = pow(near, 1.0);
        r = normalize(mix(nr, br, pwn*first));
 
        vec3 mo = bo;
        ao += mo;
        o = mix(bo, ao, pow(near,1.0)*first);
        
        if (fi < 1.0) {
            mat2 mr = rot((1.0 - near) * 4.0 * near * -3.14);
            //r.xy *= mr;
            //o.xy *= mr;
            r.xz *= mr;
            o.xz *= mr;
        }

        mapr2 += 1.0;
    }
    
	fragColor = vec4(sqrt(fc), 1.0);
}