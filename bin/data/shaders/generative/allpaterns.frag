#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float scalex;
uniform float scaley;
uniform float speedx=.5;
uniform float speedy=.5;
uniform float pattern;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	float mapspeedx = mapr(speedx,-1.,1.0);
	float mapspeedy = mapr(speedy,-1.,1.0);
	float mapscalex = mapr(scalex,0.0,20.0);
	float mapscaley = mapr(scaley,0.0,20.0);
	
	float patron =mapr(pattern,0.0,6.0);
	
	float e = uv.y;
			//e = snoise(uv*20.)*5.0;
			//e = ridgedMF(uv*10.)*0.5;
	//e =  ridgedMF(vec2(ridgedMF(vec2(uv.x,uv.y))));
			//e = ridgedMF(vec2(ridgedMF(vec2(uv.x,uv.y))));
			
	 float e2 = rxr(vec2(uv.x*mapscalex+time*mapspeedx,
				  uv.y*mapscaley+time*mapspeedy))*0.5-.5;
	 e2= smoothstep(0.1,0.8,e2)*8.0;		  
	 float e3 = ridgedMF(vec2(uv.x*mapscalex+time*mapspeedx
					 ,uv.y*mapscaley+time*mapspeedy))*0.2;
	 e3= smoothstep(0.1,0.8,e3)*5.0;	
	 float e4 = snoise(vec2(uv.x*mapscalex+time*mapspeedx
					 ,uv.y*mapscaley+time*mapspeedy))*1.2;
	e4*=2.0;
	float e5 = noise(vec2(uv.x*mapscalex+time*mapspeedx
				  ,uv.y*mapscaley+time*mapspeedy))*1.1;
	float e6 = random2(vec2(uv.x*mapscalex+time*mapspeedx
					 ,uv.y*mapscaley+time*mapspeedy))*1.0;
	float e7 = voronoi(vec2(uv.x*mapscalex+time*mapspeedx
					,uv.y*mapscaley+time*mapspeedy),0.1)*1.0;
	float e8 = fbm(vec2(uv.x*mapscalex+time*mapspeedx
					,uv.y*mapscaley+time*mapspeedy))*1.0;
	vec3 fin = vec3(0.0);
     if(patron <= 1.0){
		fin = vec3(mix(e2,e3,patron));
	 }
	 if(patron > 1.0 && patron <=2.0){
		fin = vec3(mix(e3,e4,patron-1.0));
	 }
	 if(patron > 2.0 && patron <=3.0){
		fin = vec3(mix(e4,e5,patron-2.0));
	 }
	 if(patron > 3.0 && patron <=4.0){
		fin = vec3(mix(e5,e6,patron-3.0));
	 }
	 if(patron > 4.0 && patron <=5.0){
		fin = vec3(mix(e6,e7,patron-4.0));
	 }
	 if(patron > 5.0 && patron <=6.0){
		fin = vec3(mix(e7,e8,patron- 5.0));
	 }
	gl_FragColor = vec4(fin,1.0);
}
