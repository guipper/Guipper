#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float sizedif;
uniform float posx;
uniform float posy;
uniform bool useMouse;
uniform float feedbackforce;
//uniform float posyasdqwd;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 puv = uv;
    float fx = resolution.x/resolution.y;
	uv.x*=fx;
	//uv.x*=resolution.y/resolution.x;
	vec2 m = vec2(mouse.x*fx,mouse.y);

	//float e = poly(uv,m,0.4,0.4,mpuntas,0.0); // pincel
	float e = 0;
	
	
	
	if(useMouse){
		e = cir(uv,vec2(posx*fx,posy),size,sizedif);
	}else{
		e = cir(uv,m,size,sizedif);
	
	}
	
	//fin.r*=0.2;
	
	vec4 fb = texture2D(feedback,puv);
	vec3 fin = vec3(e)+fb.rgb*feedbackforce;
	
	fragColor = vec4(fin,1.0);
}






