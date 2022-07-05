#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float sinfy ;
uniform float multdisplace ;
uniform float multysin ;
uniform float cnt ; 
void main(){ 


	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);
	vec2 uv2 = uv;
	float prom = (t1.r+t1.g+t1.b)/3.;
	
	
	
	int mcnt = int(floor(mapr(cnt,1.0,20.0)));
//	mcnt = 10;
	vec3 dib = t1.rgb;
	
		vec2 uv3 = uv;
	for(int i=0; i<mcnt; i++){
		
		float fase = i*pi*2/mcnt;
		t1 =  texture2D(texture1, uv3);
		prom = (t1.r+t1.g+t1.b)/3.;
		
		
	
		uv3.x+=sin(time+fase)*prom*mapr(multdisplace,0.001,0.1);		
		uv3.y+=cos(time+fase)*prom*mapr(multdisplace,0.001,0.1);
	
		vec4 tx2 =  texture2D(texture1, uv3); 
		dib+=tx2.rgb;
	} 
	
	dib/=mcnt;
	dib+=sin(dib*sinfy*100.+time)*mapr(multysin,0.0,1.0);
	fragColor = vec4(dib*1.5,1.0);
}
