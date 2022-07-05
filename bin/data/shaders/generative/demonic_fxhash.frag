#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float iterations;
uniform float rotationspeed;
uniform float animationspeed;
uniform float rfreq ;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float e = sin(uv.x*100.);
	
	int ite = int(floor(mapr(iterations,2.0,6.0)));
	float maprot = mapr(rotationspeed,-0.05,0.05);
	float mapani = mapr(animationspeed,-0.2,0.2);
	vec3 dib = vec3(1.);
	vec3 col1 = vec3(0.1,0.1,0.1);
	vec3 col2 = vec3(0.0,0.0,0.0);
	
	
	for (float i=0; i<ite; i++){
		vec2 uv2 = uv;
		//uv2.x+=time*0.01;
		//uv2 = fract(uv2*sin(time)*2.0+2.0);
		float idx = pi*2.*i/ite;
		
		//uv2.x +=sin(time+idx)*0.1;
		//uv2.y +=cos(time+idx)*0.1;
		
		uv2 -=vec2(0.5);
		uv2 = rotate2d(time*maprot+float(i*10.))*uv2;
		uv2 +=vec2(0.5);
		
		
		//uv2 +=time*0.1;
		//uv2 = rotate2d(time*10.0)*uv2;
		//uv2 +=vec2(0.5);
		
		
		uv2 = fract(uv2*i);
		vec2 p2 = vec2(0.5) - uv2;
		float r = length(p2);
		float a = atan(p2.x,p2.y);
		
		
		float e = poly(uv2,
		vec2(0.5),
		0.1,
		0.28,
		3	,
		0.);
		
		
		dib -= mix(col1,col2,sin(e*100.*rfreq));
		//dib*=1.2;
	
		//dib+= vec3(cir(uv,vec2(0.5),0.1,0.1));
	}
	dib = smoothstep(0.0,1.0,dib);
	vec2 puv = uv;

	vec3 fin = dib;

	fragColor = vec4(fin,1.0);
}
