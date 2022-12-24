#pragma include "../common.frag"
#define QUALITY 12.0


uniform float zoomspeed ;
uniform float SCALE ; 
float r(vec2 n)
{
    return fract(cos(dot(fract(n/142.0),vec2(36.26,73.12)))*354.63);
}
/*float noise(vec2 n)
{
    vec2 fn = floor(n);
    vec2 sn = smoothstep(vec2(0),vec2(1),fract(n));
    
    float h1 = mix(r(fn),r(fn+vec2(1,0)),sn.x);
    float h2 = mix(r(fn+vec2(0,1)),r(fn+vec2(1)),sn.x);
    return mix(h1,h2,sn.y);
}*/
float fractal(vec3 n)
{	
    float total = 0.5;
    for(float i = 0.0;i<QUALITY;i++)
    {
        total = mix(noise(n.xy/exp2(i-fract(n.z))+i+floor(n.z)),
                    total,pow((i-fract(n.z))/(QUALITY-1.0),2.0));
    }
 	return total;
}
void main()
{
    vec3 n = vec3((fragCoord.xy-iResolution.xy*0.5)/SCALE,-iTime*zoomspeed);
    float p1 = fractal(n);
    float p2 = fractal(n+vec3(0.5,1.0,0.0)*8.0*SCALE);
    
    float terrain = smoothstep(0.5,0.6,p1);
    
    vec3 col = mix(vec3(0.1,0.3,0.4),vec3(0.4,0.5,0.3),terrain);
	fragColor = vec4(col+vec3(p2-p1),1.0);
}