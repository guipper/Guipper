#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float sizedif;
uniform float posx;
uniform float posy;

//uniform float posyasdqwd;
void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;

    float fx = 1920./1080.;
	uv.x*=fx;
	
	vec2 p = vec2(posx*fx,posy) - uv; 
	float r = length(p);

	float e = 1.-smoothstep(size,size+sizedif,r);;
	vec3 fin = vec3(e);
	//fin.r*=0.2;
	fragColor = vec4(fin,1.0);
}
