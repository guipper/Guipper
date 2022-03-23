#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float v1;
uniform float fractxy;
void main()
{
vec2 uv = gl_FragCoord.xy / resolution;

	 vec2 p  = vec2(0.5)-uv;





    float a = atan(p.x,p.y);
    float rad  = length(p);



    uv-=0.5;

    float sp=0.5;
    //uv*=abs(sin(time*sp))*50.+20;
    uv*=100.*fractxy;

	
	float mv1 = mapr(v1,1.0,200.0);
    float def = abs(cos(a+time+sin(rad*PI*mv1)))*(abs(cos(time*sp))*0.3+0.2);

    vec2 p2 = vec2(uv.x*cos(def),uv.y*cos(def));
    vec2 ipos = floor(p2);
    vec2 fpos = fract(p2*2);

    float rand = sin(random(ipos*2)*time);


    float rand2 = cos(random(ipos)*time);

    fpos -=0.5;
    float fa = atan(fpos.x,fpos.y);
    float frad = length(fpos);
    fpos+=0.5;

    float e = rand;
    float e2 = rand2;


    float r = 1.0;
    float g = 1.0;
    float b = 1.0;


    vec3 col = hsb2rgb(vec3(r,g,b));
    col = vec3(0.7,0.3,0.5)*e+vec3(0.5,0.2,0.5)*e2;


    gl_FragColor = vec4(col, 1.0);


}
