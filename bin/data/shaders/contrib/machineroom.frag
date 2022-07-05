#pragma include "../common.frag"
uniform sampler2D iChannel0;
mat3 xrot(float t)
{
    return mat3(1.0, 0.0, 0.0,
                0.0, cos(t), -sin(t),
                0.0, sin(t), cos(t));
}

mat3 yrot(float t)
{
    return mat3(cos(t), 0.0, -sin(t),
                0.0, 1.0, 0.0,
                sin(t), 0.0, cos(t));
}

mat3 zrot(float t)
{
    return mat3(cos(t), -sin(t), 0.0,
                sin(t), cos(t), 0.0,
                0.0, 0.0, 1.0);
}

vec3 paxis(vec3 p)
{ /* thanks to eiffie */
    vec3 a=abs(p),r = vec3(1.0,0.0,0.0);
    if(a.z>=max(a.x,a.y))r=r.yzx;
    else if(a.y>=a.x)r=r.zxy;
    return r*sign(p);
}

float udBox( vec3 p, vec3 b )
{ /* thanks to iq */
  return length(max(abs(p)-b,0.0));
}

float sdBoxInfinite(vec3 p, vec3 b)
{
  vec2 d = abs(p.xy) - b.xy;
  return min(max(d.x,d.y),0.0) +
         length(max(d,0.0));
}

float sdCylinderInfinite(vec3 p, float r)
{
    return length(p.xz) - r;
}

vec3 func(vec3 p, float s)
{
    vec3 off = paxis(p) * s * 1.5;
    p -= off;
    p -= sign(p) * s * 2.75;
    return p;
}

vec2 map(vec3 p)
{
    p.x += sin(p.z);
    
    vec3 op = p;
    
    float k = 16.0;
    p.z = (fract(p.z/k) * 2.0 - 1.0) * k * 0.5;
    
    vec3 ip = p;
    
    float bs = 1.0;
    float r = 0.0;
    float d = 1000.0;

    for (int i = 0; i < 5; ++i) {
        
        ip = func(ip, bs);

        float fd = udBox(ip, vec3(bs));
        if (fd < d) {
            d = fd;
            r = float(i);
        }
        
        bs *= 0.5;
	}
    
    d = max(d, -sdBoxInfinite(p,vec3(1.0)));
    
    float ck = 8.0;
    vec3 pc = vec3(p.x, p.y, (fract(op.z/ck)*2.0-1.0)*ck*0.5);
    d = max(d, -sdCylinderInfinite(pc, 2.0));
    
    float ground = p.y + 0.9;
    if (ground < d) {
        d = ground;
        r = 6.0;
    }

    return vec2(d,r);
}

vec3 normal(vec3 p)
{
	vec3 o = vec3(0.01, 0.0, 0.0);
    return normalize(vec3(map(p+o.xyy).x - map(p-o.xyy).x,
                          map(p+o.yxy).x - map(p-o.yxy).x,
                          map(p+o.yyx).x - map(p-o.yyx).x));
}

float trace(vec3 o, vec3 r)
{
 	float t = 0.0;
    for (int i = 0; i < 32; ++i) {
        vec3 p = o + r * t;
        float d = map(p).x;
        t += d * 0.5;
    }
    return t;
}

vec3 _texture(vec3 p)
{
    vec3 ta = texture(iChannel0, vec2(p.y,p.z)).xyz;
    vec3 tb = texture(iChannel0, vec2(p.x,p.z)).xyz;
    vec3 tc = texture(iChannel0, vec2(p.x,p.y)).xyz;
    return (ta + tb + tc) / 3.0;
}

float aoc(vec3 origin, vec3 ray) {
    float delta = 0.1;
    const int samples = 6;
    float r = 0.0;
    for (int i = 1; i <= samples; ++i) {
        float t = delta * float(i);
     	vec3 pos = origin + ray * t;
        float dist = map(pos).x;
        float len = abs(t - dist);
        r += len * pow(2.0, -float(i));
    }
    return r;
}

void main()
{
	vec2 uv = gl_FragCoord.xy.xy / iResolution.xy;
    uv = uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;
    
	vec3 o = vec3(0.0, 0.0, 0.0);
    o.z += iTime * 0.5;
    o.x = sin(-o.z);
    vec3 r = normalize(vec3(uv, 1.3));
    r *= yrot(o.x);
    
    float t = trace(o, r);
    vec3 w = o + r * t;
    vec2 mp = map(w);
    float fd = mp.x;
    float it = mp.y;
    vec3 sn = normal(w);

	float fog = 1.0 / (1.0 + t * t * 0.1 + fd * 100.0);
    
    vec3 diff = _texture(w);
    
    if (it == 3.0) {
        diff *= 0.5;
    } else if (it == 2.0) {
        diff = diff.xxx * 1.5;
    } else if (it == 1.0) {
        diff *= vec3(1.0, 1.0, 0.0);
    }
    
    float sz = w.x + sin(w.z);
    
    if (it == 6.0) {
        float m = 0.5+0.5*sign(fract(w.z*10.0+abs(sz)*10.0)-0.5);
        float k = 0.5+0.5*sign(abs(sz)-0.8);
        float ik = 0.5+0.5*sign(abs(sz)-0.9);
        float cm = k*(1.0-ik);
        vec3 tape = vec2(m*k,0.0).xxy;
        diff = mix(diff*0.5, tape, cm);
    }
    
    vec3 lighting = vec3(0.6);
    for (int i = -2; i <= 2; ++i) {
        float lz = floor(w.z+float(i)+0.5);
        vec3 lpos = vec3(-sin(lz), 0.0, lz);
        vec3 lcol = vec3(1.0);
        float lmod = mod(lz,3.0);
        if (lmod == 0.0) {
            lcol = vec3(0.0,0.0,1.0) * (0.5+0.5*sin(iTime));
            lpos.y = 1.0;
        } else if (lmod == 2.0) {
            lcol = vec3(0.0, 1.0, 0.0);
            lpos.y = 0.25;
            lpos.x += cos(lz);
        } else {
         	lcol = vec3(1.0, 0.0, 0.0);
            lpos.y = -0.25;
            lpos.x -= cos(lz);
        }
        vec3 ldel = lpos - w;
        float ldist = length(ldel);
        ldel /= ldist;
        float lprod = max(dot(sn,ldel),0.0);
        float latten = 1.0 / (1.0 + ldist * ldist);
        lighting += lprod * latten * lcol;
    }
    
    diff *= lighting * fog;

	fragColor = vec4(diff,1.0);
}