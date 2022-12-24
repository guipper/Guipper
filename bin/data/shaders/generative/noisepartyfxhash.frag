#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float cnt;
uniform float ite_scale;
uniform float speedrdm;
uniform float speedx = 0.5;
uniform float speedy = 0.5;
uniform float speedrot;
uniform float amp2;
uniform float f1;
uniform float f2;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	float fix = resolution.x/resolution.y;
	uv.x *=fix;
	vec2 puv = gl_FragCoord.xy ;
	vec4 fb =  texture2D(feedback, puv/resolution);

	
	vec3 dib = vec3(1.0);
	
	int mcnt = int(floor(mapr(cnt,4.0,8.0)));
	float mite_scale = mapr(ite_scale,.2,0.45);
	float mspeedx = mapr(speedx,-0.005,0.005);
	float mspeedy = mapr(speedy,-0.005,0.005);
	float mspeedrot = mapr(speedrot,-0.005,0.005);
	float mspeedrdm = mapr(speedrdm,0.0,0.1);
	
	//col1 = vec3(1.0,0.0,0.0);
	//col2 = vec3(0.0,0.0,1.0);
	for(int i=1; i<mcnt; i++){
		float fase = i*pi*2./mcnt;
		vec2 uv2 = uv;
		float indx = i/mcnt;
		uv2.x+=time*mspeedx;
		uv2.y+=time*mspeedy;
		
		uv2-=vec2(0.5*fix,0.5);
		uv2 = rotate2d(mspeedrot*+time*.2)*uv2;
		uv2+=vec2(0.5*fix,0.5);
		
		uv2-=vec2(0.5*fix,0.5);
		uv2 = scale(vec2(mite_scale*i))*uv2;
		uv2+=vec2(0.5*fix,0.5);
		
	
		float e4 = sin(uv2.y*10.+time+sin(uv2.x*2.)*.5+.5)*mapr(amp2,0.05,0.3);
		
		uv2+=random2(uv2*uv.x*800.*f1+sin(uv.y*10.*f2+time*.2)*0.1+time*.5)*e4*4.;
		uv2+=random2(uv2*uv.y*10.*f2+sin(uv.x*5.*f1+time*.2)*0.1+time*.5)*e4*4.;
		float e = random2(uv2*mite_scale*i,time*mspeedrdm*.2+fase);
		vec3 col1 = hsb2rgb(vec3(0.8,1.0,1.0));
		vec3 col2 = hsb2rgb(vec3(0.2,0.8,1.0));
		
		float cnt_cols = 5.;
		
		col1 = vec3(sin(e*10.)*1.5,0.0,0.5);
		col2 = vec3(sin(e*2.+time*.5)*0.5,sin(e*10.)*1.5,0.0);
		
		dib+= vec3(e)*mix(col2,col1,e)*2.5;
	}
	
	
	dib/=(mcnt+1.);
	
	//dib = smoothstep(sm1,sm2,dib);
	dib = smoothstep(.0,1.,dib);
	
	
	
	vec3 fin = dib;
	
	fragColor = vec4(fin,1.0); 
}