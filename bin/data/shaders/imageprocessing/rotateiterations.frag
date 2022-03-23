#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;
uniform float fase;
uniform float cnt;
uniform float speed;
uniform float blendmode;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
	float fix = resolution.x/resolution.y;

	float mapfase = mapr(fase,-pi/2,pi/2);
	float mapspeed = mapr(speed,-1.0,1.0);
	int mapcnt = int(floor(mapr(cnt,1.0,20.0)));
	
	int cnt = 5;
	
	vec3 dib = vec3(0.0);
	
		vec2 uv2 = gl_FragCoord.xy/resolution;
	uv2.x *=fix;
	for(int i=1; i<mapcnt ; i++){
		uv2 = gl_FragCoord.xy/resolution;
		uv2.x *=fix;
		float index = i * pi * 2 / (mapcnt-1);
		//uv2 = fract(uv2);
		
		uv2-=vec2(0.5*fix,0.5);
		uv2*= rotate2d(index+mapfase+mapspeed*time);
		uv2+= vec2(0.5*fix,0.5);
		//uv2 = rotate2d(index*mapfase+mapspeed*time)*uv2;
		//uv2 = rotate2d(index+mapspeed*time)*uv2;
		
	//	uv2+= resolution/2;
		
		/*uv2*=resolution;
		uv = fract(uv2);
		uv2/=resolution;*/
		vec4 t1 =  texture2D(textura1, uv2);
		//dib += t1.rgb;
	  	//dib = mix(dib,t1.rgb,t1.rgb);
		
		int bm = int(mapr(blendmode,0.0,25.0));
		//vec3 fin = blendMode(bm,t1.rgb,t2.rgb,opacity);
	//	dib = mix(dib,t1.rgb,t1.rgb);
        dib = blendMode(bm,dib,t1.rgb,1.0);
	}
	//vec4 t1 =  texture2D(textura1, uv2);
	//dib += t1.rgb;
	//dib/=mapcnt;
	
	
	vec3 fin = dib;
	
	gl_FragColor = vec4(dib,1.0);
}