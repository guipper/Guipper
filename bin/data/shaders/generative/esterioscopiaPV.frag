#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float sc1;
uniform float size;
uniform float diffuse;
uniform float iterations;
uniform float ite_scale;

uniform float ite_size;


uniform float fb_force;
uniform float e_force;

uniform float hue1;
uniform float hue2;
uniform float fasey;

uniform float xspeed;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;

	vec2 coords = gl_FragCoord.xy ;
	//coords.y = resolution.y -coords.y;
	vec4 fb =  texture2D(feedback, coords/resolution);
		
	float fix = resolution.x/resolution.y;
	uv.x *= fix;
	float e = 0.;
	
	float msc1 = mapr(sc1,1.,20.);
	float mcnt = floor(mapr(iterations,1.,20.));
	float mite_scale = mapr(ite_scale,0.0,1.);
	float mxspeed = mapr(xspeed,-0.5,0.5);
	//float mt = time * speed * 10.;//MAP TIME
	float msize = size;
	
	vec3 dib = vec3(0.);
	vec3 col1 = hsb2rgb(vec3(hue1,0.8,1.0));
	vec3 col2 = hsb2rgb(vec3(hue2,0.8,1.0));
	
	for(int i=1; i<mcnt; i++){
		
		float idx = float(i)/float(mcnt);
		vec2 uv3 = uv;
		
	
		vec2 uv2 = fract(vec2(uv3.x+mxspeed*time*idx,
							  uv3.y+fasey));
		uv2-=vec2(0.5,0.5);
		uv2 = scale(vec2(msc1+i*mite_scale))*uv2;
		uv2+=vec2(0.5,0.5);
		
		float siz = msize*0.1*(mix(idx,1.0,ite_size));
		e= cir(fract(uv2),vec2(0.5),
		siz,
		siz+diffuse*0.1);	
			
		dib+=vec3(e)*mix(col2,col1,i/mcnt);
	}
	
	vec3 fin = vec3(0);
	
	
	//dib = rgb2hsb(dib);
	fin = dib*e_force+fb.rgb*mapr(fb_force,0.2,1.0);
	
	gl_FragColor = vec4(fin,1.0);
}









