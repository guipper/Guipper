#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float sinfy ;
uniform float multdisplace ;
uniform float multysin ;
uniform float cnt ; 
void main()
{ 
	vec2 uv = gl_FragCoord.xy / resolution;

	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	
	
	vec2 uv2 = uv;
	
	
	//int cnt = 4;
	
	int mcnt = int(floor(mapr(cnt,1.0,10.0)));
	mcnt = 10;
	
	//uv2.x += prom*0.2;
	//uv2.y += prom*0.2;
	
	
	vec3 dib = vec3(0.0);
	for(int i=0; i<mcnt; i++){
		float prom = (t1.r+t1.g+t1.b)/3.;
		float fase = i*pi*2./mcnt;
	
		uv2.x+=sin(time+fase*10.)*prom*mapr(multdisplace,0.001,0.005);		
		uv2.y+=cos(time+fase*10.)*prom*mapr(multdisplace,0.001,0.005);

	} 
	
	
	vec4 t2 =  texture2D(texture1,vec2(uv2.x,uv2.y));
	t2 += sin(t2*sinfy*100.+time)*mapr(multysin,0.0,1.0);
	
	
	
	
	float prom2 = (t2.r+t2.g+t2.b)/3.;
	vec2 uv3 = uv;
	uv3.x+=sin(prom2*3.+time)*.008;
	uv3.y+=cos(prom2*3.+time)*.008;
	
	vec4 t3 =  texture2D(texture1, uv3);
	
	t3 += sin(t3*sinfy*100.+time)*mapr(multysin,0.0,1.0);
	fragColor = vec4(t3.rgb,1.0);
}
