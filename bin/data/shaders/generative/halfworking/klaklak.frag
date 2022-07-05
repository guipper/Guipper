#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float iterations;
uniform float rotationspeed;
uniform float animationspeed;
uniform float angles;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float e = sin(uv.x*100.);
	
	int ite = int(floor(mapr(iterations,0.0,100.0)));
	float maprot = mapr(rotationspeed,-0.05,0.05);
	float mapani = mapr(animationspeed,-0.2,0.2);
	
	float mapangles = floor(angles*5.0+1.0);
	vec3 dib = vec3(1.);
	vec3 col1 = vec3(0.02,0.1,0.1);
	vec3 col2 = vec3(1.0,1.0,0.9);
	
	vec2 uv2 = uv;
	for (float i=0; i<ite; i++){
		uv2 = fract(uv*2.0*i);
		
		float idx = pi*2.*i/ite;
		
		vec2 p2 = vec2(0.5) - uv2;
		float r = length(p2);
		float a = atan(p2.x,p2.y);

		float e = sin(a*mapangles*10.0+pi/2);
		dib+= vec3(e);
	}
	
	dib/=ite;
	//dib*=0.3;
	vec2 puv = uv;
	vec4 fb =  texture2DRect(feedback, puv);

	vec3 fin = dib;

	fragColor = vec4(fin,1.0);
}
