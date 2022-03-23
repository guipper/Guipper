#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float sc1;
uniform float size;
uniform float iterations;
uniform float ite_scale;

uniform float xspeed;
uniform float yspeed;
uniform float rotangle;
uniform float rotspeed;

uniform float fb_force;
uniform float e_force;

uniform float hue1;
uniform float hue2;
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
	float mrot = mapr(rotangle,-pi/8.,pi/8.);
	float mxspeed = mapr(xspeed,-0.5,0.5);
	float myspeed = mapr(yspeed,-0.5,0.5);
	//float mt = time * speed * 10.;//MAP TIME
	float mrotspeed = mapr(rotspeed,-0.5,0.5);
	float msize = size;
	
	vec3 dib = vec3(0.);
	vec3 col1 = hsb2rgb(vec3(hue1,0.8,1.0));
	vec3 col2 = hsb2rgb(vec3(hue2,0.8,1.0));
	
	for(int i=1; i<mcnt; i++){
		
		vec2 uv3 = uv;
		
		uv3-=vec2(0.5*fix,time*0.0);
		uv3 = rotate2d(mrot*i+time*mrotspeed)*uv3;
		uv3+=vec2(0.5*fix,0.5);
		
		vec2 uv2 = fract(vec2(uv3.x+mxspeed*time,
							  uv3.y+myspeed*time));
		uv2-=vec2(0.5,0.5);
		uv2 = scale(vec2(msc1+i*mite_scale))*uv2;
		uv2+=vec2(0.5,0.5);
		
		e= cir(fract(uv2),vec2(0.5),msize*0.1,msize*0.08);	
			
		dib+=vec3(e)*mix(col2,col1,i/mcnt);
	}
	
	vec3 fin = vec3(0);
	
	
	//dib = rgb2hsb(dib);
	fin = dib*e_force+fb.rgb*mapr(fb_force,0.2,1.0);
	
	gl_FragColor = vec4(fin,1.0);
}









