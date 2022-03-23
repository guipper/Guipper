#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;

uniform sampler2D textura2;
uniform sampler2D textura3;

uniform float cnt;


uniform float zoom;
uniform float zoom_index;
uniform float freq;
uniform float speedrot;
uniform float fractalize;
uniform float fractalize_index;
uniform float speedx;
uniform float speedy;
uniform float texturewrap;
uniform float blendmode;
uniform float colormix;
//uniform float blendmode;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 wruv = uv;
		
	float fix = resolution.x/resolution.y;

	//float mapfase = mapr(fase,-pi/2,pi/2);
	float mapspeedrot = mapr(speedrot,-1.0,1.0);
	int mapcnt = int(floor(mapr(cnt,1.0,20.0)));
	
	int cnt = 5;
	
	vec3 dib = vec3(0.0);
	vec3 dib_bm2 = vec3(0.0);
		vec2 uv2 = gl_FragCoord.xy/resolution;
//	uv2.x *=fix;
	vec3 col1 = vec3(1.0,0.0,0.0);
	vec3 col2 = vec3(0.0,0.0,1.0);
	for(int i=1; i<mapcnt ; i++){
		vec2 uv2 = gl_FragCoord.xy ;
	
	    float index = i * pi * 4 *freq / (mapcnt-1);
		
		float index2 = i/float(mapcnt)*(zoom_index*5.0);
		uv2-=resolution/2;
		uv2*= scale(vec2(zoom+index2));
		uv2+=resolution/2;
		
		
		//rotindex
	
		uv2-=resolution/2;
	    uv2*= rotate2d(0.0+index+time*mapspeedrot);
		uv2+=resolution/2;
		
		uv2/=resolution;
		float index3 = mix(1.0,i/float(mapcnt)*4.0,fractalize_index);
		
		//uv2 = fract(uv2*(10.0*fractalize+1.0)*(index3));
		uv2.y+=time*speedy*0.1;
		//uv2.y+=time*speedx;
		uv2 = fract(uv2*(10.0*fractalize+1.0)*(index3));
		
		vec4 t1 =  texture2D(textura1, uv2);	
		uv2 = abs(2.*fract(uv2*(texturewrap*4.0+1.0))-1.0);
		vec4 t2 =  texture2D(textura2, uv2);
		vec4 t3 =  texture2D(textura3, uv2);
		
	
	    vec3 colf =mix(col1,col2,i/float(mapcnt));
		     colf = mix(t2.rgb,t3.rgb,i/float(mapcnt));
		
		dib_bm2 =
		
		mix(mix(dib_bm2,t1.rgb*colf,t1.rgb),
			mix(dib_bm2,t1.rgb,t1.rgb),
			vec3(1.-colormix));
		
		
		
		
		//dib+=mix(t1.rgb*colf,t1.rgb,vec3(1.-colormix));
		dib+=t1.rgb;
		//dib_bm2 = mix(dib_bm2,t1.rgb*colf,t1.rgb);
		
	}
	//vec4 t1 =  texture2D(textura1, uv2);
	//dib += t1.rgb;
	dib/=mapcnt*0.5;
	
	
	vec3 fin = mix(dib,dib_bm2,blendmode);
	
	gl_FragColor = vec4(fin,1.0);
}