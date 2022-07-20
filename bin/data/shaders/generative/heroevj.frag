#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform float speedx;
uniform float speedy;
uniform float scalex;
uniform float scaley;
uniform float flush;
uniform float animationspeed1;
uniform float animationspeed2;
uniform float negro;

void main(){
	
	vec2 uv = gl_FragCoord.xy / resolution;


	float mapspeedx = mapr(speedx,-1.,1.0);
	float mapspeedy = mapr(speedy,-1.,1.0);
	
	float mapscalex = mapr(scalex,0.0,30.0);
	float mapscaley = mapr(scaley,0.0,30.0);
	float mapscale2 = mapr(flush,1.0,100.0);
	
	float manimationspeed1 = mapr(animationspeed1,0.0,5.0);
	float manimationspeed2 = mapr(animationspeed2,0.0,5.0);
	
	
	float e = 0;
	e = fbm(vec2(uv.x*mapscalex+time*mapspeedx*2.
				,uv.y*mapscaley+time*mapspeedy*2.),time*manimationspeed1+1.0)*1.0;
	
    // e=0.5;  
	float e2 = fbm(vec2(mapscale2,
				        mapscale2)*e,manimationspeed2*time*2.+1000000.0);
						
	float e3 = fbm(vec2(uv.x*10.,
				        uv.y*10.)*e,0.5+time+1000000.0);
	
	vec3 fin = vec3(e2);
	

	//fin = smoothstep(0.75,1.0,fin);
	//fin*=vec3(1.0,0.2,0.2);
	
		
//	fin = smoothstep(0.75,1.0,fin);
	fin =mix(vec3(1.0,0.9,0.4),vec3(0.8,.3,.9),fin);
	fin =mix(fin,mix(vec3(0.0),fin,sin(e2*10.)*.5+.5)*mapr(e3,-.5,0.5),negro);
//	fin = smoothstep(0.,.
	fragColor = vec4(fin,1.0);
	
	
}
