#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float v1;

void main()
{
	  //REGULAR NOISE
    vec2 uv = gl_FragCoord.xy / resolution;

    vec2 uv2 = uv;
    uv2 *= 3.0;

    float rand = noise(uv2,PI *PI*100000+time*0.0001);
    float rand2= noise(uv2,PI *2+time*0.0000001);
    float rand3= noise(uv2,PI );

    vec2 pos = vec2(0.5)-uv;
    float radius = length(pos);
    float angle = atan(pos.x,pos.y);

    //float form = sin(rand*rand2*rand3*PI*3+time*2+sin(uv.y*PI+time+sin(uv.x*PI+time+sin(uv.y*PI+time))))*0.3;
    float form = rand*sin(rand*PI*10*v1+time+sin(rand*PI*5*v1
    +sin(rand*PI*15*v1+time)
    +sin(radius*PI*5*v1+time+sin(uv.x*PI*10*v1)
    +sin(uv2.x*PI*5-time+sin(uv.x*PI*10)
    +sin(uv2.y*PI*5-time+sin(uv.x*PI*10)
    +sin(uv2.x*PI*5-time+cos(uv2.y*PI*2)
    ))))));


    float r = rand2+form;
    float b = rand2+form+sin(form*PI);
    float g = sin(form*PI);




    gl_FragColor = vec4(r,g,b,1.0);


}
