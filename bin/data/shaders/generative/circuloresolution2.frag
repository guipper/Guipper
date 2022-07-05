#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float sizedif;
uniform float posx;
uniform float posy;
uniform float lk;
uniform sampler2D miimagen;
//uniform float posyasdqwd; 
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

    float fx = resolution.x/resolution.y;
	//uv.x*=fx;
	//uv.x*=resolution.y/resolution.x;
	vec2 m = vec2(mouse.x*fx,mouse.y);

	//float e = poly(uv,m,0.4,0.4,mpuntas,0.0); // pincel
	float e = cir(uv,vec2(posx*fx,posy),size,sizedif);
	vec4 img = texture(miimagen,uv);
	vec3 fin = vec3(e);
	
	img.r = sin(uv.x*100.+time);
	//fin.r*=0.2;
	fragColor = vec4(img.rgb,1.0);
}
