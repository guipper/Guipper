#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float cnt;
uniform float ite_scale;
uniform float speedrdm;
uniform float speedx = 0.5;
uniform float speedy = 0.5;
uniform float speedrot;
uniform float sm1;
uniform float sm2;
uniform float fb_force;
uniform float e_force;
uniform float hue1;
uniform float hue2;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	float fix = resolution.x/resolution.y;
	uv.x *=fix;
	vec2 puv = gl_FragCoord.xy ;
	vec4 fb =  texture2D(feedback, puv/resolution);

	
	vec3 dib = vec3(1.0);
	
	int mcnt = int(floor(mapr(cnt,1.0,20.0)));
	float mite_scale = mapr(ite_scale,1.0,10.0);
	float mspeedx = mapr(speedx,-0.2,0.2);
	float mspeedy = mapr(speedy,-0.2,0.2);
	float mspeedrot = mapr(speedrot,-0.01,0.01);
	float mspeedrdm = mapr(speedrdm,0.0,1.0);
	
	//col1 = vec3(1.0,0.0,0.0);
	//col2 = vec3(0.0,0.0,1.0);
	for(int i=1; i<mcnt; i++){
		float fase = i*pi*2./mcnt;
		vec2 uv2 = uv;
		float indx = i/mcnt;
		uv2.x+=time*mspeedx;
		uv2.y+=time*mspeedy;
		
		uv2-=vec2(0.5);
		uv2 = rotate2d(mspeedrot*time)*uv2;
		uv2+=vec2(0.5);
		
		uv2-=vec2(0.5);
		uv2 = scale(vec2(mite_scale*i))*uv2;
		uv2+=vec2(0.5);
		
		float e = random2(uv2*mite_scale*i,time*mspeedrdm+fase);
		vec3 col1 = hsb2rgb(vec3(hue2,1.0,1.0));
		vec3 col2 = hsb2rgb(vec3(hue1,1.0,1.0));
		dib+= vec3(e)*mix(col2,col1,e);
	}
	
	dib/=(mcnt+1.);
	
	dib = smoothstep(sm1,sm2,dib);
	
	float mfb_force = mapr(fb_force,0.0,1.0);
	
	vec3 fin = dib*e_force+
			   fb.rgb*mfb_force;
	
	gl_FragColor = vec4(fin,1.0); 
}