#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float iterations;
uniform float corteradial;
uniform float corteradial2;

uniform float cirsize;
uniform float cntcirculos;
uniform float fase;
uniform float velocidadanimacion;
uniform float velocidadrotacion;
uniform float brightness;

void main(){	
	
	vec2 uv = gl_FragCoord.xy / resolution;
	
    float fx = resolution.x/resolution.y;
	//uv.x*=fx;
	
	vec3 dib = vec3(0.0);
	
	int mapcnt = int(floor(mapr(iterations,1.0,10.)));
	float mapcntcirculos = floor(mapr(cntcirculos,0.0,10.));
	float mapcirsize = mapr(cirsize,0.0,0.6);
	
	for(int i=0; i<mapcnt; i++){
		
		vec2 uv2 = uv;
		
		float fase = mapr(fase,0.0,pi/2);
		uv2 = rot(uv2,fase*(i+1)+mapr(velocidadrotacion,-0.5,0.5)*time);	
		uv2 = fract(uv2*(i+1));
		vec2 p2 = vec2(0.5,0.5) - uv2;
		
		float r2 = length(p2);
		float a2 = atan(p2.x,p2.y);
		
		vec3 col = vec3(0.0,1.0,0.5)*1.0;
		vec3 col2 = vec3(1.0,0.5,0.0)*1.0;
		
		float df = (sin(uv2.x*200.+time)*0.5+0.5)*
					(sin(uv2.y*200.+time)*0.5+0.5);
		float e = sin(r2*corteradial*40.+velocidadanimacion*10.*time
				 *0.5+0.5+sin(r2*40.*corteradial2+time
				 //+sin(a2*10.)
				 
				 )) * 0.5+0.5;
		
		vec3 dib2 = mix(col,col2,e);
		
		float cirl = cir(fract(uv2*mapcntcirculos),vec2(0.5),0.1,mapcirsize)*1.0;
	
		e-=cirl;
		dib+= mix(dib,dib2,e);
	}
	dib/=mapcnt;
	dib*=brightness;
	vec4 fb = texture2D(feedback, gl_FragCoord.xy/resolution);
	vec3 fin = dib;
	
	gl_FragColor = vec4(fin,1.0); 
}


