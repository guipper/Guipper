#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D textura1;

uniform bool flipx;
uniform bool mirrorx;
uniform float suavizadox;

uniform bool flipy;
uniform bool mirrory;
uniform float suavizadoy;
void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;	
	vec2 uv2 = gl_FragCoord.xy / resolution;

	float flip_x =0 ; 
	
	float msuavizadox = mapr(suavizadox,0.0,0.1);
	if(flipx){
		flip_x = smoothstep(1.-uv.x-msuavizadox,1.-uv.x+msuavizadox,0.5);
		//flip_x = step(1.-uv.x,0.5);
	}else{
		//flip_x = smoothstep(uv.x-msuavizadoy,uv.x+msuavizadox,0.5);
		flip_x = step(uv.x,0.5);
	}
	
	float flip_y =0 ; 
	float msuavizadoy = mapr(suavizadoy,0.0,0.1);
	if(flipy){
		//flip_y = step(1.-uv.y,0.5);
		flip_y = smoothstep(1.-uv.y-msuavizadoy,1.-uv.y+msuavizadoy,0.5);
	}else{
		//flip_y = step(uv.y,0.5);
		flip_y = smoothstep(uv.y-msuavizadoy,uv.y+msuavizadoy,0.5);
	}
	
	
	if(mirrorx){
		uv2.x = mix(uv2.x,1.-uv2.x, flip_x);
	}
	
	if(mirrory){
	uv2.y = mix(uv2.y,1.-uv2.y, flip_y);
	}
	
	uv2*=resolution;
	vec4 t1 =  texture2D(textura1, uv2/resolution);
	
	
	
	vec3 fin = t1.rgb;
	
	fragColor = vec4(fin,1.0); 
}