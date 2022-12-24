#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define fxrand floor(iTime*.1)
#define hash1 rnd(fxrand)
#define hash2 rnd(fxrand+.11)
#define hash3 rnd(fxrand+.22)
#define hash4 rnd(fxrand+.33)
#define hash5 rnd(fxrand+.44)
#define hash6 rnd(fxrand+.55)
#define hash7 rnd(fxrand+.66)

mat2 rota(float a)
{
    float s=sin(a);
    float c=cos(a);
    return mat2(c,s,-s,c);
}

float rnd(float p)
{
    p*=1234.5678;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}

vec3 fractal(vec2 uv) {
    float t=iTime*.05;
	vec2 p=uv*1.;
    float m=100.;
    for (int i=0; i<12; i++) {
        if (float(i+2)>iTime*5.) break;
        p*=rota(.5+hash1*2.+t);
    	p=sin(p+hash2*.15)/max(.3+hash3*.2,dot(p,p))-0.15;
    	m=min(m,abs(length(p)-1.));
    }
    m=1.-exp(-10.*m);
    vec3 col = vec3(m*2.,abs(p))*m;
    //col.rb*=rota(hash3*2.);
    return col;
}


void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = gl_FragCoord.xy/iResolution.xy-.5;
	uv.y*=-.7;
    // Time varying pixel color
    vec3 col = fractal(uv);
    //col=mix(vec3(length(col))*.7,col,.7)*step(abs(uv.x),.3);
    // Output to screen
    fragColor = vec4(col,1.0)-rnd(uv.x+sin(uv.y*123.321))*.15;
}