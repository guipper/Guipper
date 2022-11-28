#pragma include "../common.frag"

//uniform vec2 resolution;
//uniform sampler2D titulos;
//uniform sampler2D cuadrados;
//uniform float time;

uniform float speedx;
uniform float speedy;
uniform float scalex;
uniform float scaley;
uniform float flush;
uniform float animationspeed1;
uniform float animationspeed2;


float fbm2 (in vec2 uv) {
    // Initial values
    float value = 0.5;
    float amplitude = 0.5;
    float frequency = 0.;
    vec2 shift = vec2(100);
    mat2 rot2 = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));
    // Loop of octaves
    for (int i = 0; i < 16; i++) {
        value += amplitude * noise(uv,time);
        uv = rot2 * uv * 2.0 + shift;
        amplitude *= .5;
    }
    return value;
}
float fbm2 (in vec2 uv,in float _time) {
    // Initial values
    float value = 0.5;
    float amplitude = 0.5;
    float frequency = 0.;
    vec2 shift = vec2(100);
    mat2 rot2 = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));
    // Loop of octaves
    for (int i = 0; i < 16; i++) {
        value += amplitude * noise(uv,_time);
        uv = rot2 * uv * 2.0 + shift;
        amplitude *= .5;
    }
    return value;
}


void main(){
	
	vec2 uv = gl_FragCoord.xy / resolution;
	uv.y = 1.-uv.y;
	vec3 c1 = vec3(37./255.,52./255.,117./255.); //mismo azul que el logo. pero no va para el shader 
		 c1 = vec3(0.0,0.0,1.0);
	vec3 c2 = vec3(227./255.,20./255.,78./255.);
		 c2 = vec3(1.0,0.,0.);

	
	float manimationspeed1 = mapr(animationspeed1,0.0,5.0);
	float manimationspeed2 = mapr(animationspeed2,0.0,5.0);
	
	
	float e = fbm2(vec2(uv.x*5.,uv.y*5.+time*.1),time*manimationspeed1+1.0);
		 // e*=uv.y*0.4;
    // e=0.5;  
	float e2 = fbm2(vec2(uv.x*5.5,
				        uv.y*5.5),time*3.0+1000000.0);
						
	float e3 = fbm2(vec2(uv.x*10.,
				        uv.y*10.),0.5+time*1.0+1000000.0);
						
	float e4 = fbm2(vec2(uv.x*5.5,
				        uv.y*5.5),time*3.0+31232.0);
	vec3 fin = vec3(e2);
	

	//fin = smoothstep(0.75,1.0,fin);
	//fin*=vec3(1.0,0.2,0.2);
	
		
//	fin = smoothstep(0.75,1.0,fin);

	float s = 0.21;
	float linea = smoothstep(0.5-s,0.5+s,uv.x)*(1.-smoothstep(0.5-s,0.5+s,uv.x))*2.;
	fin = mix(c1,c2,uv.x);
	fin = mix(fin,mix(c1,c2,e3),linea*e2);
	//fin = mix(fin,vec3(0.85),smoothstep(0.3,.55+e*.1,uv.y)); //Mas parecida a la grafica original pero chota
	fin = mix(fin,vec3(1.0),smoothstep(0.15,.8+e*.3,uv.y)); //COn un toque de blanco
	
	
	//fin = mix(mix(c1,c2,fin),mix(c1,c2,uv.x),1.0) ;
	//fin = mix(fin,vec3(1.0),smoothstep(0.1,0.9,uv.y));
	//fin =mix(vec3(0.0),fin,sin(e2*10.)*.5+.5)*mapr(e3,-.5,0.5);
//	fin = smoothstep(0.,.

	
	
	//vec4 tx = texture2D(titulos,uv);
	//vec4 tx2 = texture2D(cuadrados,uv);
	
	
	
	vec3 textc = mix(c1,c2,sin(uv.x*4.+e2+pi-time*.1)*.5+.5);
	textc = mix(textc,vec3(1.0),vec3(e3*.1));
	//fin=mix(fin,textc,tx.rgb);
	
	//fin=mix(fin,vec3(mix(c1,c2,e3)),tx2.rgb);
	//fin=mix(fin,mix(c1,c2,e3),tx2.rgb);
	fragColor = vec4(fin,1.0);
	
	
}
