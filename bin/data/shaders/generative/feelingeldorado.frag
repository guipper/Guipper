#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float v1;
uniform float v2;
uniform float ca2;
float def(vec2 uv,float an){


    float ca = floor(mapr(ca2,1.0,10.0)) ;
    float cb =2.;

    float e=0.;

    vec2 pp = vec2(0.5)-uv;
    float ap = atan(pp.x,pp.y);
    float rp = length(pp);


    for (int i = 0; i<=int(ca); i++){
        vec2 p = vec2(0.5,float(i)/ca)-uv;
        float r = length(p);
        float a = atan(p.x,p.y);

        vec2 p2 = vec2(sin(p.x+time),cos(p.y+time));
        float r2 = length(p2);
        float a2 = atan(p2.x,p2.y);

		float mv1 = mapr(v1,0.0,100.0);
		float mv2 = floor(mapr(v2,0.0,100.0));
        float e2 = sin(an*mv1+sin(r*mv1+sin(an*PI*mv1-time*3.)+sin(a*mv2)*0.05+sin(r*PI*50.+sin(a*mv2+time)*2.))+time);
        e+= sin(sin(r*PI*5.+sin(e2)));
    }
    e*= 0.4-rp+sin(e*PI);
    return abs(e);
}
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution.xy;
    vec2 pos = vec2(0.5) - uv;
    float ra = length(pos);
    float an = atan(pos.x,pos.y);

    float e=0.;


    float r = def(uv,PI-PI/2)*0.5+0.2+def(uv,0.)*0.2;

    r = abs(sin(r*sin(ra*PI*5+sin(an*5)*0.8+time)));
    float g = def(uv,PI)*0.3+0.1+def(uv,PI-PI/2)*0.2;
    float b = def(uv,PI)*0.2+0.2;

    b = b;

    vec3 col = vec3(r,g,b);
    gl_FragColor = vec4(col, 1.0);


}
