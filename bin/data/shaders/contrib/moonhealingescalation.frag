#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define NUM_LAYERS 5.0

vec4 firstColor = vec4(0.2,1.0,0.4,1.0);//green
vec4 endColor = vec4(0.0,0.3,1.0,1.0);//blue

//rotation matrix
mat2 rot(float a){
	float s=sin(a);
	float c=cos(a);
	return mat2(c,-s,s,c);
}

//gradient
vec4 gradient(vec2 uv){
    //uv*=rot(iTime*0.1);
	vec4 col = mix(mix(firstColor*0.9, endColor*1.2, abs(uv.x)),mix(firstColor*0.9, endColor*1.2, abs(uv.x)),uv.x);	
	return col;
}

//draw 1 circle
float Circle(vec2 uv){
	float d=length(uv);
	float m=0.05/d;
	m*=smoothstep(1.0,0.2,d);
	return m;
}

//draw 1 star
float Star(vec2 uv,float flare){

	//center of star
	float d=length(uv);
	float m=0.05/d;

	//flare
	float rays=max(0.0,1.0-abs(uv.x*uv.y*1000.0));
	m+=rays*flare;
	uv*=rot(PI/4.0);
	rays=max(0.0,1.0-abs(uv.x*uv.y*1000.0));
	m+=rays*flare*0.3;
	m*=smoothstep(1.0,0.2,d);
	return m;
}

//make random value
float Hash21(vec2 p){
	p=fract(p*vec2(123.34,456.21));
	p+=dot(p,p+45.32);
	return fract(p.x*p.y);
}

//repetition circles 
vec3 CircleLayer(vec2 uv){
	vec2 gv=fract(uv)-0.5;
	vec2 id=floor(uv);

	vec3 col=vec3(0.0);

	for(int y=-1;y<=1;y++){
		for(int x=-1;x<=1;x++){
			vec2 offset=vec2(x,y);
			float n=Hash21(id+offset);
			float size=fract(n*345.32);
			float circle=Circle(gv-offset-vec2(n,fract(n*34.0))+0.5);
			col+=circle*n*1.5;
		}
	}
	return col;
}

//repetition stars
vec3 StarLayer(vec2 uv){
	vec3 col=vec3(0.0);
	
	vec2 gv=fract(uv)-0.5;
	vec2 id=floor(uv);
	
	for(int y=-1;y<=1;y++){
		for(int x=-1;x<=1;x++){
			vec2 offset=vec2(x,y);
			float n=Hash21(id+offset);
			float size=fract(n*345.32);
			float star=Star(gv-offset-vec2(n,fract(n*34.0))+0.5,smoothstep(0.5,0.9,size));
			
			//blink
			star*=sin(iTime*3.0+n*PI*2.0)*0.5+1.0;
			col+=star*size;
		}
	}
	return col;
}


void main(){
	vec2 uv=(gl_FragCoord.xy-iResolution.xy*0.5)/iResolution.y;
	uv.y = 1.-uv.y;
	uv*=2.0;
	uv.y-=iTime;

	vec4 background=gradient(uv);
    vec4 circle=vec4(CircleLayer(uv),1.0);


	vec3 col=vec3(0.0);

	for(float i=0.0;i<1.0;i+=1.0/NUM_LAYERS){
		float depth=fract(i);
		float scale=mix(20.0,0.5,depth);
		float fade=depth*smoothstep(1.0,0.9,depth);
		col=StarLayer(uv*scale+i*453.2*fade);	
	}
	
	fragColor=vec4(col,1.0)+background+circle;
}