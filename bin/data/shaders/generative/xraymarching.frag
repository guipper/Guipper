#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

vec3 objcol;

mat2 rot(float a){
    float s = sin(a), c = cos(a);
    return mat2(c, s, -s, c);
}

float de(vec3 pos)
{
    float t = mod(time,17.);
    float a=smoothstep(13.,17.,t)*8.-smoothstep(4.,0.,t)*4.;
    float f=sin(time*5.+sin(time*20.)*.2);
    pos.xz *= rot(time);
    pos.yz *= rot(time);
    vec3 p = pos;
    float s=1.;
    for (int i=0; i<4; i++){
        p=abs(p)*1.3-.5-f*.1-a;
        p.xy*=rot(radians(45.));
        p.xz*=rot(radians(45.));
        s*=1.3;
    }
    float fra = length(p)/s-.5;
    pos.xy *= rot(time);
    p = abs(pos) - 2. - a;
    float d = length(p) - .7;
    d = min(d, max(length(p.xz)-.1,p.y));
    d = min(d, max(length(p.yz)-.1,p.x));
    d = min(d, max(length(p.xy)-.1,p.z));
    p = abs(pos);
    p.x -= 4.+a+f*.5;
    d = min(d, length(p) - .7);
    d = min(d, max(length(p.yz)-.1,p.x));
    d = min(d, length(p.yz+abs(sin(p.x+time*10.)*.2))-.1);
    p = abs(pos);
    p.y -= 4.+a+f*.5;
    d = min(d, length(p) - .7);
    d = min(d, max(length(p.xz)-.1,p.y));
    d = min(d, fra);
    objcol = abs(p);
    if (d==fra) objcol=vec3(2.,0.,0.);
    return d;
}


vec3 normal(vec3 p) {
    vec2 d = vec2(0., .01);
    return normalize(vec3(de(p+d.yxx), de(p+d.xyx), de(p+d.xxy))-de(p));
}


vec3 march(vec3 from, vec3 dir)
{
    float d = 0., td = 0., maxdist = 20., ref = 0.;
    vec3 p = from, col = vec3(0.);
    for (int i = 0; i<100; i++)
    {
        float d2 = de(p) * (1.-random(dir.xy)*.2);
        if (d2<0.)
        {
            vec3 n = normal(p);
            dir = reflect(dir, n);
            d2 = .1;

        }
        d = max(.01, abs(d2));
        p += d * dir;
        td += d;
        if (td>maxdist) break;
        col += .01 * objcol;
    }
    return pow(col, vec3(2.));
}


void main()
{
    vec2 uv = gl_FragCoord.xy / resolution - .5;
    uv.x *= resolution.x / resolution.y;
    vec3 from = vec3(0.,0.,-10.);
    vec3 dir = normalize(vec3(uv, 1.));
    vec3 col = march(from, dir);

    gl_FragColor = vec4(col,1.);
}