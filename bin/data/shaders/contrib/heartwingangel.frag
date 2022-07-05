#pragma include "../common.frag"
// @christinacoffin
// 2015-05-07: 1st version, need to optim and do some proper AA

vec3 JuliaFractal(vec2 c, vec2 c2, float animparam, float anim2 ) {	
	vec2 z = c;
    
	float ci = 0.0;
	float mean = 0.0;
    
	for(int i = 0;i < 64; i++)
    {
		vec2 a = vec2(z.x,abs(z.y));
		
        float b = atan(a.y*(0.99+animparam*9.0), a.x+.110765432+animparam);
		
        if(b > 0.0) b -= 6.303431307+(animparam*3.1513);
		
        z = vec2(log(length(a*(0.98899-(animparam*2.70*anim2)))),b) + c2;

        if (i>0) mean+=length(z/a*b);

        mean+=a.x-(b*77.0/length(a*b));

        mean = clamp(mean, 111.0, 99999.0);
	}
    
	mean/=131.21;
	ci =  1.0 - fract(log2(.5*log2(mean/(0.57891895-abs(animparam*141.0)))));

	return vec3( .5+.5*cos(6.*ci+0.0),.5+.75*cos(6.*ci + 0.14),.5+.5*cos(6.*ci +0.7) );
}


void main()
{
    float animWings = 0.004 * cos(iTime*0.5);
    float animFlap = 0.011 * sin(iTime*1.0);    
    float timeVal = 56.48-20.1601;
	vec2 uv = gl_FragCoord.xy.xy - iResolution.xy*.5;
	uv /= iResolution.x*1.5113*abs(sin(timeVal));
    uv.y -= animWings*5.0; 
	vec2 tuv = uv*125.0;
	float rot=3.141592654*0.5;
  
	uv.x = tuv.x*cos(rot)-tuv.y*sin(rot);
	uv.y =1.05* tuv.x*sin(rot)+tuv.y*cos(rot);
	float juliax = tan(timeVal) * 0.011 + 0.02/(gl_FragCoord.xy.y*0.19531*(1.0-animFlap));
	float juliay = cos(timeVal * 0.213) * (0.022+animFlap) + 5.66752-(juliax*1.5101);//+(gl_FragCoord.xy.y*0.0001);// or 5.7
    
 
    float tapU = (1.0/ float(iResolution.x))*25.5;//*cos(animFlap);
    float tapV = (1.0/ float(iResolution.y))*25.5;//*cos(animFlap);
    
  
	fragColor = vec4( JuliaFractal(uv+vec2(0.0,0.0), vec2(juliax, juliay), animWings, animFlap ) ,1.0);
    
    fragColor += vec4( JuliaFractal(uv+vec2(tapU,tapV), vec2(juliax, juliay), animWings, animFlap ) ,1.0);
//    fragColor += vec4( JuliaFractal(uv+vec2(tapU,-tapV), vec2(juliax, juliay), animWings, animFlap ) ,1.0);
//    fragColor += vec4( JuliaFractal(uv+vec2(-tapU,tapV), vec2(juliax, juliay), animWings, animFlap ) ,1.0);
    fragColor += vec4( JuliaFractal(uv+vec2(-tapU,-tapV), vec2(juliax, juliay), animWings, animFlap ) ,1.0);  
    fragColor *= 0.3333;
    
    fragColor.xyz = fragColor.zyx;
	fragColor.xyz = vec3(1)-fragColor.xyz;

}