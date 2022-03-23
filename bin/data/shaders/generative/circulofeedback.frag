#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float fb_scale;
uniform float fb_rotate;
uniform float fb_fuerza;
uniform float e_force;
uniform float puntas;
uniform float hue1;
uniform float hue2;
uniform float polyrotatespeed;
uniform bool automatic;
uniform float movamp;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
	
    float fx = resolution.x/resolution.y;
	uv.x*=fx;
	vec2 m ;
	
	if(automatic){
		m = vec2(mouse.x*fx,mouse.y);
	}else{
		m = vec2(0.5*fx+sin(time)*movamp,0.5+cos(time)*movamp);
	}
	int mpuntas = int(floor(mapr(puntas,0.0,10.0)));
	float msize = mapr(size,0.0,0.2);
	
	
	float mpolyrotatespeed = time*polyrotatespeed*3.;
	float mof = sin(uv.x*100+mpolyrotatespeed)*0.00;
	
	float e = poly(uv,m,msize+mof,msize*1.4+mof,mpuntas,time*mpolyrotatespeed); // pincel
	float e2 = poly(uv,m,0.0,msize,mpuntas,time*polyrotatespeed); // pincel

	e*=e_force;

	vec3 col1 = hsb2rgb(vec3(hue1,1.0,1.0));
	vec3 col2 = hsb2rgb(vec3(hue2,1.0,1.0));
	
	vec3 dib = vec3(e)*mix(col1,col2,e2);
	vec2 puv = gl_FragCoord.xy/resolution;
	
	puv-=resolution/2;
	//puv = scale(vec2(mapr(fb_scale,0.95,1.05)))*puv;
	puv+=resolution/2;
	puv-=(resolution/2);
	//puv = rotate2d(mapr(fb_rotate,-pi/4,pi/4))*puv;
	puv+=(resolution/2);
	vec4 fb = texture2D(feedback, puv);
	vec3 fin = dib+fb.rgb*mapr(fb_fuerza,0.9,1.0);
	
	gl_FragColor = vec4(fin,1.0);
}


