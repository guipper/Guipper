#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float diffuse;
uniform float offsetx;
uniform float offsety;
uniform float cnt1;
uniform float cnt2;
uniform float zoom;
uniform float scale1;
uniform sampler2DRect col1;
uniform sampler2DRect col2;
uniform float freq;
uniform float amp;
uniform float fase;
uniform float speed;
//uniform float asdwd;
uniform float fuerzafeedback;
void main()
{

	vec2 uv = gl_FragCoord.xy / resolution.xy;
	vec2 uv2 = uv;

	float fx = resolution.x/resolution.y;
	uv.x*= resolution.x/resolution.y;
	vec3 dib = vec3(0.0);
	float sc1 = 90.8;

	vec2 p = vec2(0.5) - uv2;
	p.x *=fx;
	float a =atan(p.x,p.y);
	float r =length(p);



	float e = 0.;
	
	
	float cnt = floor(mapr(cnt1,1.0,10.0));
	float mcnt2 = floor(mapr(cnt2,1.0,10.0));;
	float mkmof = mapr(scale1,0.0,1.0);
	float mffreq = mapr(freq,0.0,120.0);
	float mamp = mapr(amp,0.0,2.0);
	float mfase = mapr(fase,0.0,PI*2);
	float mspeed = mapr(speed,0.0,5.0);
	vec3 c1 = texture2DRect(col1,uv2).rgb;
	vec3 c2 = texture2DRect(col2,uv2).rgb;
	vec2 m = vec2( mapr(offsetx,0.0,2.0)*fx,mapr(offsety,0.0,2.0));

	float msize = mapr(size,0.99,0.7);
	float mfuerzafeedback = mapr(fuerzafeedback,0.6,1.0);
	float mdiffuse = mapr(diffuse,0.1,0.2);
	for(float k=0.0; k<mcnt2; k++){
		vec3 cf= mix(c2,c1,k/mcnt2);
		for(int i=0; i<cnt; i++){
		vec2 uv3 = uv;
		//uv3 =fract(uv3*4.);
		float idx = pi * 2.0 *float(i)/float(cnt)*fase;
		
		//m.x-=time*0.02; 
      
		float def =smoothstep(0.3
		,0.69,sin(uv.y*5.-mspeed*999.)*0.1*sin(uv.x*5.+time));
		uv3-=vec2(m.x,m.y);
		uv3= rotate2d(idx+k*0.8+def)*uv3;
		uv3+=vec2(m.x*fx,m.y);

		uv3 -=vec2(m.x,m.y);
		uv3= scale(vec2(k*mkmof+zoom*2.0))*uv3;
		uv3+= m;
		uv3.x+= sin(mspeed+sin(mspeed*.25*mspeed))*0.08;
		uv3.y+= cos(mspeed+sin(mspeed*0.5*mspeed)*0.9)*0.2;
		//uv3*=sin(time*0.02)*10.;
		//uv3.x+=time*0.2;
		float e2 = sin(uv3.x*5.+time*mspeed
		+sin(uv3.y*mffreq*mamp)+cos(uv3.x*mffreq*mamp)
		);

		e2*= sin(uv3.y*10.+time*2.*mspeed
		//+sin(uv3.x*20.+time)
		);
		
	   e2=smoothstep(msize,msize+0.0,e2);

	   e2 = clamp(e2,0.,1.);
	   e+= e2;
	   dib= mix(dib+vec3(e2)*cf,
	   dib,
	   dib*e2*cf);
	  // dib+= vec3(e2)*cf;
	  }
	}
	
	
	vec2 puv = gl_FragCoord.xy;
	
	
	vec4 fb =  texture2DRect(feedback, puv);
	
	vec3 fin = mix(fb.rgb*mfuerzafeedback,dib,dib);
	  gl_FragColor = vec4(fin,
					  1.0);

}
