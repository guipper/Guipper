#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float cnt;
uniform float ite_scale;
uniform float speedrdm;
uniform float speedx = 0.5;
uniform float speedy = 0.5;
uniform float speedrot;
uniform float fb_force;
uniform float palette;

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	float fix = resolution.x/resolution.y;
	uv.x *=fix;
	vec2 puv = gl_FragCoord.xy ;
	vec4 fb =  texture2D(feedback, puv/resolution);

	
	vec3 dib = vec3(1.0);
	
	int mcnt = int(floor(mapr(cnt,7.0,9.0)));
	float mite_scale = mapr(ite_scale,.4,0.95);
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
		
		uv2-=vec2(0.5);
		uv2 = rotate2d(mspeedrot*time)*uv2;
		uv2+=vec2(0.5);
		
		uv2-=vec2(0.5);
		uv2 = scale(vec2(mite_scale*i))*uv2;
		uv2+=vec2(0.5);
		
		float e = random2(uv2*mite_scale*i,time*mspeedrdm+fase);
		vec3 col1 = hsb2rgb(vec3(0.8,1.0,1.0));
		vec3 col2 = hsb2rgb(vec3(0.2,1.0,1.0));
		
	float cnt_cols = 5.;
	float indexcolor= mapr(palette,0.0,float(cnt_cols));
			
		 //1
		// col1 = vec3(1.0,1.0,sin(e*10.)*1.5);
		 //col2 = vec3(0.0,sin(e*10.)*1.5,0.0);
			
		 //2
		 //col1 = vec3(sin(e*10.)*1.5,0.0,0.5);
		 //col2 = vec3(sin(e*10.+time)*0.5,sin(e*10.)*1.5,0.0);
			
			
		//3
		 //col1 = hsb2rgb(vec3(sin(e*10.),1.0,1.0));
		 //col2 = hsb2rgb(vec3(cos(e*10.),1.0,1.0));
		 
		 //3
		 //col1 = hsb2rgb(vec3(.2,sin(e*10.),1.0));
		 //col2 = hsb2rgb(vec3(sin(e*10.),1.0,sin(e*10.)));
			
				
		//	 col1 = hsb2rgb(vec3(0.49,1.0,1.0));
		//	 col2 = hsb2rgb(vec3(0.97,1.0,1.0));
		
		if(indexcolor < 1.){
			 col1 = vec3(1.0,sin(e*10.)*.5+.5,sin(e*10.)*1.5)*.8;
			 col2 = vec3(0.4,sin(e*10.)*0.5,0.0)*.8;
			
		}else if(indexcolor < 2.){
			 
			col1 = vec3(sin(e*10.)*1.5,0.0,0.5);
			col2 = vec3(sin(e*10.+time)*0.5,sin(e*10.)*1.5,0.0);
			
			
			
		}else if(indexcolor < 3.){
				
			col1 = hsb2rgb(vec3(sin(e*4.+time*.1),1.0,1.0));
			col2 = hsb2rgb(vec3(cos(e*1.+time*.1),1.0,1.0));
		}else if(indexcolor <4.){
				
			col1 = hsb2rgb(vec3(.2,sin(e*10.),1.0))*.8;
			col2 = hsb2rgb(vec3(sin(e*10.)*.5+.5,1.0,sin(e*10.)*.5+.5))*.8;
		}else if(indexcolor <5.){
				
			col1 = vec3(sin(e*10.),sin(e*10.),sin(e*10.));
			col2 = vec3(sin(e*10.),sin(e*10.),sin(e*10.));
			//obj1_col1 = vec3(0.8,.6,0.4);
		}
		
		
		dib+= vec3(e)*mix(col2,col1,e);
	}
	
	dib/=(mcnt+1.);
	
	//dib = smoothstep(sm1,sm2,dib);
	dib = smoothstep(.0,1.,dib);
	
	float mfb_force = mapr(fb_force,0.2,0.4);
	
	vec3 fin = dib*.6+
			   fb.rgb*mfb_force;
	
	fragColor = vec4(fin,1.0); 
}