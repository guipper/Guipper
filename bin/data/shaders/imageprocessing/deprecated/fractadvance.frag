#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float sc1;
uniform float size;
uniform float iterations;
uniform float ite_scale;

uniform float xspeed;
uniform float yspeed;
uniform float rot;
uniform float rotspeed;


void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

	vec2 coords = gl_FragCoord.xy ;
	//coords.y = resolution.y -coords.y;
	vec4 fb =  texture2DRect(feedback, coords);

	float fix = resolution.x/resolution.y;
	uv.x *= fix;
	float e = 0.;

	float msc1 = mapr(sc1,1.,20.);
	float mcnt = floor(mapr(iterations,1.,20.));
	float mite_scale = mapr(ite_scale,0.0,1.);
	float mrot = mapr(rot,0.0,pi/4.);
	float mxspeed = mapr(xspeed,-1.0,1.0);
	float myspeed = mapr(yspeed,-1.0,1.0);
	float mrotspeed = mapr(rotspeed,-0.5,0.5);
	float msize = size;

	vec3 dib = vec3(0.);

	for(int i=1; i<mcnt; i++){

		vec2 uv3 = gl_FragCoord.xy;

    /*uv3/=resolution;
    uv3 = fract(uv3*i);
    uv3*=resolution;
*/
		uv3-=vec2(resolution.x/2*fix,resolution.y);
		uv3 = rotate2d(1.)*uv3;
		uv3+=vec2(resolution.x/2*fix,resolution.y);

		vec2 uv2 = fract(vec2(uv3.x+mxspeed*time,
							  uv3.y+myspeed*time));

    uv2 = uv3;
    //uv2/=resolution;
    //uv2 = fract(uv3*20.0);
    //uv2*=resolution;
  	/*uv2-=resolution;
		uv2 = scale(vec2(msc1+i*mite_scale))*uv2;
		uv2+=resolution;
    */
    vec4 t1 =  texture(texture1, uv2/resolution);
		e= cir(fract(uv2),vec2(0.5),msize*0.1,msize*0.08);

		dib+=t1.rgb;
	}
  //vec4 t1 =  texture(texture1, gl_FragCoord.xy/resolution);
	vec3 fin = vec3(0);


	//dib = rgb2hsb(dib);
	fin = dib;

	gl_FragColor = vec4(fin,1.0);
}
