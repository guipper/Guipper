#pragma include "../common.frag"


uniform float speedx;
uniform float speedy;

uniform float animationspeed1 = 0.16;

uniform float iterations = 0.5;
uniform float formuparam = 0.87;
uniform float volsteps = 0.48;
uniform float stepsize = 0.24;
uniform float zoom = 0.52;
uniform float tile = 0.8;

uniform float darkmatter = 0.62;
uniform float distfading = 0.31;
uniform float saturation = 0.42;
uniform float displacey = 0.1;
uniform float fbmsc;
uniform float fbmseed;


float fbm2 (in vec2 uv) {
    // Initial values
    float value = 0.5;
    float amplitude = 0.5;
    float frequency = 0.;
    vec2 shift = vec2(100);
    mat2 rot2 = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));
    // Loop of octaves
    for (int i = 0; i < 16; i++) {
        value += amplitude * noise(uv,time);
        uv = rot2 * uv * 2.0 + shift;
        amplitude *= .5;
    }
    return value;
}
float fbm2 (in vec2 uv,in float _time) {
    // Initial values
    float value = 0.5;
    float amplitude = 0.5;
    float frequency = 0.;
    vec2 shift = vec2(100);
    mat2 rot2 = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));
    // Loop of octaves
    for (int i = 0; i < 16; i++) {
        value += amplitude * noise(uv,_time);
        uv = rot2 * uv * 2.0 + shift;
        amplitude *= .5;
    }
    return value;
}

vec3 generateRedbullBackground(vec2 uv){
	
	vec3 c1 = vec3(37./255.,52./255.,117./255.); //mismo azul que el logo. pero no va para el shader 
		 c1 = vec3(0.0,0.0,1.0);
		 //c1 = vec3(r1,g1,b1);
	vec3 c2 = vec3(227./255.,20./255.,78./255.);
		 c2 = vec3(1.0,0.,0.);
		// c2 = vec3(r2,g2,b2);
	
	//float mapspeedx = mapr(speedx,-.1,.1);
	//float mapspeedy = mapr(speedy,-.1,.1);
	
	//float mapscalex = mapr(scalex,0.0,30.0);
	//float mapscaley = mapr(scaley,0.0,30.0);
	//float mapscale2 = mapr(flush,1.0,100.0);
	
	float manimationspeed1 = mapr(animationspeed1,0.0,5.0);
	//float manimationspeed2 = mapr(animationspeed2,0.0,5.0);
	
	
	float e = fbm2(vec2(uv.x*5.,uv.y*5.+time*.1),time*manimationspeed1+1.0);
		 // e*=uv.y*0.4;
    // e=0.5;  
	float e2 = fbm2(vec2(uv.x*5.5,
				        uv.y*5.5),time*3.0+3213.0+fbmseed*10.);
						
	float e3 = fbm2(vec2(uv.x*10.,
				        uv.y*10.),0.5+time*1.0+4123.0+fbmseed*10.);
						
	float e4 = fbm2(vec2(uv.x*mapr(fbmsc,4.0,22.0),
				        uv.y*mapr(fbmsc,4.0,22.0)),time*0.1+32.0+fbmseed*10.);
						
	float e5 = fbm2(vec2(uv.x*2.,
				        uv.y*2.),0.5+time*0.2+31234.0);
						
	float e6 = fbm2(vec2(uv.x*5.,uv.y*5.+time*.001),time*0.01+1.0);
	vec3 fin = vec3(e2);
	

	//fin = smoothstep(0.75,1.0,fin);
	//fin*=vec3(1.0,0.2,0.2);
	
		
//	fin = smoothstep(0.75,1.0,fin);

	float s = 0.21;
	float linea = smoothstep(0.5-s,0.5+s,uv.x)*(1.-smoothstep(0.5-s,0.5+s,uv.x))*2.;
	fin = mix(c1,c2,uv.x);
	fin = mix(fin,mix(c1,c2,e3),linea*e2);
	//fin = mix(fin,vec3(0.85),smoothstep(0.3,.55+e*.1,uv.y)); //Mas parecida a la grafica original pero chota
	
	
	vec3 c3 = vec3(0.0,0.0,.0);
	
	vec3 c4_1 = mix(vec3(1.0,0.0,0.0),vec3(0.0,0.0,1.0),uv.x);
	vec3 c4 = mix(c4_1,vec3(1.0,1.0,1.0),e6);
	
	
	fin = mix(fin,mix(c3,
					c4,vec3(e4)),smoothstep(0.25,.99+e*0.1,1.-uv.y)); //COn un toque de blanco
	
	
	//fin = mix(mix(c1,c2,fin),mix(c1,c2,uv.x),1.0) ;
	//fin = mix(fin,vec3(1.0),smoothstep(0.1,0.9,uv.y));
	//fin =mix(vec3(0.0),fin,sin(e2*10.)*.5+.5)*mapr(e3,-.5,0.5);
//	fin = smoothstep(0.,.

	
	
	//vec4 tx = texture2D(titulos,uv);
	//vec4 tx2 = texture2D(cuadrados,uv);
	
	
	
	vec3 textc = mix(c1,c2,sin(uv.x*4.+e2+pi-time*.1)*.5+.5);
	textc = mix(textc,vec3(1.0),vec3(e3*.1));
	//fin=mix(fin,textc,tx.rgb);
	
	//fin=mix(fin,vec3(mix(c1,c2,e3)),tx2.rgb);
	//fin=mix(fin,mix(c1,c2,e3),tx2.rgb);
	return fin;
}
vec3 generateStarnest(){
	//get coords and direction
	vec2 uv = gl_FragCoord.xy/resolution.xy-vec2(.5,.5);
	
	uv.y = 1.0-uv.y;
	uv.y*=resolution.y/resolution.x;
	vec3 dir=vec3(uv*mapr(zoom,3.0,35.0),1.);

	float a1=.5+1.0/resolution.x*2.;
	float a2=.8+1.0/resolution.y*2.;
	mat2 rot1=mat2(cos(a1),sin(a1),-sin(a1),cos(a1));
	mat2 rot2=mat2(cos(a2),sin(a2),-sin(a2),cos(a2));
	dir.xz*=rot1;
	dir.xy*=rot2;
	vec3 from=vec3(1.,.5,0.5);
	from+=vec3(time*mapr(speedx,-0.005,0.005),time*mapr(speedy,-0.005,0.005),-2.);
	
	
	
	//from.xz*=rot1;
	//from.xy*=rot2;
	
	//volumetric rendering
	float s=0.1,fade=1.;
	vec3 v=vec3(0.);
	
	
	int mite = int(floor(mapr(iterations,10.0,25.0)));
	int mvolsteps = int(floor(mapr(volsteps,4.0,30.0)));
	float mbri =0.001;
	float mdarkmatter = mapr(darkmatter,7.0,8.0);
	for (int r=0; r<mvolsteps; r++) {
		vec3 p=from+s*dir*.5;
		float mtile = mapr(tile,0.2,1.0);
		p = abs(vec3(mtile)-mod(p,vec3(mtile*2.))); // tiling fold
		float pa,a=pa=0.;
		for (int i=0; i<mite; i++) { 
			//p=abs(p)/dot(p,p)-formuparam; // the magic formula
			
			
			float mfp1 = mapr(formuparam,0.25,0.35);
			float mfp2 = mapr(formuparam,0.54,0.67);
				  
				  
			float mfp3_2 = mix(mfp1,mfp2,formuparam);
			float mfp3_1 = 0.;
				  
				  if(formuparam > .5){
					  mfp3_1 = mfp1;
				  }else{
					  mfp3_1 = mfp2;
				  }
				  
				float fm = mix(  mfp3_1,mfp3_2,sin(time*.025)*.5+.5);
				  
			p=abs(p)/dot(p,p)-fm;
			a+=abs(length(p)-pa); // absolute sum of average change
			pa=length(p);
		}
		float dm=max(0.,mdarkmatter-a*a*.001); //dark matter
		a*=a*a; // add contrast
		if (r>6) fade*=1.-dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
		v+=fade;
		v+=vec3(s,s*s,s*s*s*s)*a*mbri*fade; // coloring based on distance
		//fade*=mapr(distfading,0.2,.5); // distance fading
		fade*=.24; 
		s+=mapr(stepsize,0.0,0.025);
	}
	v=mix(vec3(length(v)),v,saturation); //color adjust
	
	
	
	vec3 c1 = vec3(28./255., 128./255., 183./255.);
	vec3 c2 = vec3(1.);
	
	return v*0.01;	
}
void main(){
	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	vec3 starnest = generateStarnest();
	float stprom = (starnest.r+starnest.g+starnest.b)/3.;
	uv.y-=mapr(displacey,1.0,10.0)*stprom;
	
	vec3 fin = generateRedbullBackground(uv);
	
		
		 
	vec2 uv3 = gl_FragCoord.xy / resolution;
	uv3.x+=stprom*.05;
	uv3.y-=stprom*.05;
	//vec4 tx2 = texture2D(cuadrados,uv3);
	vec2 uv4 = gl_FragCoord.xy / resolution;
	uv4.x-=stprom*.05;
	uv4.y+=stprom*.05;
	//vec4 tx3 = texture2D(cuadrados,uv4);

	
	//fin-=tx2.rgb*.5;
	//fin-=tx3.rgb*.5;
	
	
	vec2 uv5= gl_FragCoord.xy / resolution;
	//uv5.x-=stprom*.05;
	uv5.y+=stprom*.05;
	//vec4 tx4 = texture2D(botones,uv5);
	
	
	float e = fbm2(vec2(uv.x*5.,uv.y*5.+time*.1),time+1.0);
	float e2 = fbm2(vec2(uv.x*20.,uv.y*20.+time*.1),time+1.0);
	
	fragColor = vec4(fin,1.0);
	
	
}
