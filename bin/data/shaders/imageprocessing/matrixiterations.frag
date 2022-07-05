#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;

uniform float fase;
uniform float cnt;
uniform float speed;
uniform float fractalize;
uniform float zoom;
uniform float freq;
uniform float zoom_index;
uniform float blendmode;
//uniform float blendmode;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float fix = resolution.x/resolution.y;

	float mapfase = mapr(fase,-pi/2,pi/2);
	float mapspeed = mapr(speed,-1.0,1.0);
	int mapcnt = int(floor(mapr(cnt,1.0,20.0)));
	
	int cnt = 5;
	
	vec3 dib = vec3(0.0);
	vec3 dib_bm2 = vec3(0.0);
		vec2 uv2 = gl_FragCoord.xy/resolution;
//	uv2.x *=fix;
	vec3 col1 = vec3(1.0,0.0,0.0);
	vec3 col2 = vec3(0.0,0.0,1.0);
	for(int i=1; i<mapcnt ; i++){
		uv2 = gl_FragCoord.xy/resolution;
		uv2.x *=fix;
		float index = i * pi * 2 *freq / (mapcnt-1);
		//uv2 = fract(uv2);
		float index2 = i/float(mapcnt)*zoom_index*2.0;
		
		
		uv2.x+=time*sin(index2*pi*2.)*.0001;
		uv2.y+=time*sin(index2*pi*2.)*.0001;
		
		
		
		uv2-=vec2(0.5*fix,0.5);
		uv2*= scale(vec2(zoom+index2));
		uv2+= vec2(0.5*fix,0.5);
		
		
		
		uv2-=vec2(0.5*fix,0.5);
		uv2*= rotate2d(index+mapfase+mapspeed*time);
		uv2+= vec2(0.5*fix,0.5);
		
		
		uv2-=vec2(0.5*fix,0.5);
		uv2*= scale(vec2(1.0+i*1.0));
		uv2+= vec2(0.5*fix,0.5);
		//uv2.y+=time;
		
		uv2 = fract(uv2*fractalize*3.+time*0.01);
		
		
		//uv2 = rotate2d(index*mapfase+mapspeed*time)*uv2;
		//uv2 = rotate2d(index+mapspeed*time)*uv2;
		
	//	uv2+= resolution/2;
		
		/*uv2*=resolution;
		uv = fract(uv2);
		uv2/=resolution;*/
		vec4 t1 =  texture2D(textura1, uv2);
		//dib += t1.rgb;
	  	//dib = mix(dib,t1.rgb,t1.rgb);
		
		//int bm = int(mapr(blendmode,0.0,25.0));
		//vec3 fin = blendMode(bm,t1.rgb,t2.rgb,opacity);
		
		vec3 colf =mix(col1,col2,i/float(mapcnt));
		     colf = mix(t1.rgb,1.-t1.rgb,i/float(mapcnt));
		dib_bm2 = mix(dib_bm2,t1.rgb*colf,t1.rgb);
		dib+=t1.rgb*colf;
     //   dib = blendMode(bm,dib,t1.rgb,1.0);
	} 
	//vec4 t1 =  texture2D(textura1, uv2);
	//dib += t1.rgb;
	dib/=mapcnt*0.5;
	
	
	vec3 fin = mix(dib,dib_bm2,blendmode);
	
	fragColor = vec4(fin,1.0); 
}