#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float seed;

uniform float r1;
uniform float g1;
uniform float b1;
uniform float r2;
uniform float g2;
uniform float b2;

float line( vec2 p, vec2 a, vec2 b, float r ){
  vec2 pa = p - a, ba = b - a;
  float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  return length( pa - ba*h ) - r;
}

float rdm(float p){
    p*=1234.56;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}
float line2(vec2 _uv,vec2 _p1,vec2 _p2){
	
	float e = line(_uv,_p1,_p2,0.01);
	float e2 =  smoothstep(0.06,0.00,e)*.4;
	e = smoothstep(0.001,0.00,e);
	e-=sin(e*40.+time)*.0002;
	return e + e2;
}
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec3 c1 = vec3(r1,g1,b1);
	vec3 c2 = vec3(r2,g2,b2);
	
	float e = 0.0;

	vec2 uv2 = fract(uv*4.);
	e+=line2(uv2,vec2(0.0,0.),vec2(1.0,1.0));
	e+=line2(uv2,vec2(0.0,1.0),vec2(1.0,0.0));
	e = sin(e+time)*.5+.5;
	//e+=line2(uv,vec2(0.0,0.0),vec2(1.0,1.0));
	//e+=line2(uv,vec2(0.0,0.0),vec2(1.0,1.0));
	//e = sin(uv.y*4.+time*.1)*.5+.5+e*.1;
	//e = smoothstep(0.02,0.1,e);
	vec3 dib = vec3(e);
	
	vec3 fin = mix(c1,c2,dib);
	fragColor = vec4(fin,1.0);
}
