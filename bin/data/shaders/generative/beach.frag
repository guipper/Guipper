#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales






void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;

	vec2 coords = gl_FragCoord.xy ;
	
	
	vec3 colagua1 = vec3(0.2,0.2,0.4);
	vec3 colagua2 = vec3(0.2,0.5,0.8);
	
	
	float lf = sin(time*2.0+sin(uv.x*10.0))*0.1+0.1;
	
	vec3 colagua_final = mix(colagua1,colagua2,fbm(uv*0.5+lf));
	
	
	float lf2 = sin(time)*0.3+0.3;
	float e1 = smoothstep(lf2+lf*0.3,lf2+lf*0.5+0.8,uv.y);
	
	
	vec3 colarena1 = vec3(0.7,0.7,0.2);
	vec3 colarena2 = vec3(0.7,0.75,0.2);
	vec3 colarena3 = vec3(0.55,0.5,0.2);
	vec3 colarenafinal = mix(colarena1,colarena2,noise(uv*10000.,time*5.0));
	
	float e2 = smoothstep(0.4+e1*0.5,0.1,1.0-uv.y);
	colarenafinal = mix(colarena3*colarenafinal,colarenafinal,e2);
	
	
	vec3 fin = colarenafinal;
		 fin = mix(colagua_final,fin,e1);
	gl_FragColor = vec4(fin,1.0);
}









