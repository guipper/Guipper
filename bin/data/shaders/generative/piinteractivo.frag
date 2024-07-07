#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
// Example Pixel Shader

// uniform float exampleUniform;

/*********************************************/

float speedx = 0.88;
float speedy = 0.5 ;
float iterations = 0.8;
float formuparam = 0.33;
float volsteps = 0.55;
float stepsize = 0.92;
float zoom = 0.96;
float tile = 0.72;
float darkmatter = 0.99;
float saturation = 0.35;

uniform float startnestfactor = 0.1;



mat2 scale(vec2 _scale);
mat2 rotate2d(float _angle);
float atan2(float x,float y);
float random (in vec2 _st);
float cir(vec2 uv,vec2 p, float s, float d);
float noise (in vec2 st,float fase);
mat2 makem2(in float theta){float c = cos(theta);float s = sin(theta);return mat2(c,-s,s,c);}

float circ(vec2 p);
float dualfbm(in vec2 p);
float snoise(vec2 v);
float cir(vec2 uv, vec2 p, float s, float d);
float poly(vec2 uv,vec2 p, float s, float d,int N,float a);
float ridge_pi(float h, float offset);
float ridge_pidMF(vec2 p);
#define time iTime*0.15
#define tau 6.2831853


float dualfbm(in vec2 p)
{	
	
    //get two rotated fbm calls and displace the domain
	vec2 p2 = p*.7;
	vec2 basis = vec2(fbm(p2-time*1.6),fbm(p2+time*1.7));
	basis = (basis-.5)*.2;
	p += basis;
	
	//coloring
	return fbm(p*makem2(time*0.2));
}
float circ(vec2 p) 
{
	float r = length(p);
	r = log(sqrt(r));
	return abs(mod(r*4.,tau)-3.14)*3.+.2;

}
// ridge_pid multifractal
// See "Texturing & Modeling, A Procedural Approach", Chapter 12
float ridge_pi(float h, float offset) {
    h = abs(h);     // create creases
    h = offset - h; // invert so creases are at top
    h = h * h;      // sharpen creases
    return h;
}

float ridge_pidMF(vec2 p) {
    float lacunarity = 2.0;
    float gain = 0.5;
    float offset = 0.9;

    float sum = 0.0;
    float freq = 1.0, amp = 0.5;
    float prev = 1.0;
    for(int i=0; i < OCTAVES; i++) {
        float n = ridge_pi(snoise(p*freq), offset);
		
		n = sin(n*10.0);
        sum += n*amp;
        sum += n*amp*prev;  // scale by previous octave
        prev = n;
        freq *= lacunarity;
        amp *= gain;
    }
    return (sum - 1.0)*2.0;
}


// SNOISE function from: https://www.shadertoy.com/view/lsf3RH

vec3 generateStarnest(){
	//get coords and direction
	vec2 uv = gl_FragCoord.xy/iResolution.xy-vec2(.5,.5);
	//uv*=2.0;
	//uv = vTexCoord;
	
	//uv.y = 1.0-uv.y;
	uv.y+=0.5;
	uv.x*=iResolution.x/iResolution.y;
	vec3 dir=vec3(uv*mapr(zoom,3.0,35.0),1.);

	float a1=.5+1.0/iResolution.x*2.;
	float a2=.8+1.0/iResolution.y*2.;
	mat2 rot1=mat2(cos(a1),sin(a1),-sin(a1),cos(a1));
	mat2 rot2=mat2(cos(a2),sin(a2),-sin(a2),cos(a2));
	dir.xz*=rot1;
	dir.xy*=rot2;
	vec3 from=vec3(1.,.5,0.5);
	from+=vec3(time*mapr(speedx,-0.005,0.005),time*mapr(speedy,-0.005,0.005),-2.);
	
	
	
	//from.xz*=rot1;
	//from.xy*=rot2;
	
	//volumetric rendering
	float s=0.1,fade=1.;
	vec3 v=vec3(0.);
	
	
	int mite = int(floor(mapr(iterations,10.0,25.0)));
	
	int mvolsteps = int(floor(mapr(volsteps,4.0,30.0)));
	const int maxvolsteps = 30;
	const int maxite = 25;
	float mbri =0.001;
	float mdarkmatter = mapr(darkmatter,7.0,8.0);
	for (int r=0; r<maxvolsteps; r++) {
		vec3 p=from+s*dir*.5;
		float mtile = mapr(tile,0.2,1.0);
		p = abs(vec3(mtile)-mod(p,vec3(mtile*2.))); // tiling fold
		float pa,a=pa=0.;
		for (int i=0; i<maxite; i++) { 
			//p=abs(p)/dot(p,p)-formuparam; // the magic formula
			
			
			float mfp1 = mapr(formuparam,0.2,0.8);
			float mfp2 = mapr(formuparam,0.54,0.67);
				  
				  
			float mfp3_2 = mix(mfp1,mfp2,formuparam);
			float mfp3_1 = 0.;
				  
				  if(formuparam > .5){
					  mfp3_1 = mfp1;
				  }else{
					  mfp3_1 = mfp2;
				  }
				  
				float fm = mix(  mfp3_1,mfp3_2,sin(time*.025)*.5+.5);
				  
			p=abs(p)/dot(p,p)-mfp1;
			a+=abs(length(p)-pa); // absolute sum of average change
			pa=length(p);
			if(i > mite){
				break;
			}
		}
		float dm=max(0.,mdarkmatter-a*a*.001); //dark matter
		a*=a*a; // add contrast
		if (r>6) fade*=1.-dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
		v+=fade;
		v+=vec3(s,s*s,s*s*s*s)*a*mbri*fade; // coloring based on distance
		//fade*=mapr(distfading,0.2,.5); // distance fading
		fade*=.24; 
		s+=mapr(stepsize,0.0,0.025);
		if(r > mvolsteps){
				break;
		}
	}
	v=mix(vec3(length(v)),v,saturation); //color adjust
	
	
	
	vec3 c1 = vec3(28./255., 128./255., 183./255.);
	vec3 c2 = vec3(1.);
	
	return v*0.01;	
}
void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

	vec2 p = vec2(0.5) - uv;
	float r = length(p);
	float a = atan(p.x,p.y);
	
	vec3 col1 = vec3(255,255,255)/255;
	vec3 col2 = vec3(28,34,34)/255;
	vec3 col3 = vec3(24,4,215)/255;
	vec3 col4 = vec3(117,239,164)/255;
	
	
	 /*col1 = texture(sTD2DInputs[1],uv).rgb;
	 col2 = texture(sTD2DInputs[3],uv).rgb;
	 col3 = texture(sTD2DInputs[2],uv).rgb;
*/
	float n = snoise(vec2(uv.x*5.0,uv.y*5.0-time*5))*0.5+0.5;
    
	//vec4 whet = texture(sTD2DInputs[0],uv);
	
	/*float wprom = (whet.r+whet.g+whet.b)/3;
	float rr = sin(n*20-iTime)*0.5+0.5;
	col1 = mix(col1,col2,rr);
	wprom = wprom*1.5-0.2;
	wprom = clamp(wprom,0.0,1.0);
	vec3 fin = mix(col1,col3,wprom);
	fin = clamp(fin,0.0,1.0);*/
	
	vec3 dib = generateStarnest();
	float dprom = (dib.r+dib.g+dib.b)/3. *startnestfactor;
	vec2 uv2 = uv;
	
	uv2.x -= sin(dprom*.5*20.0+time)*.1;
	uv2.y -= dprom*.5;
	float e = snoise(uv*10.);
	

	vec3 fin = mix(col4,col3,uv2.x);
	
	fin = mix(fin,col1,sin(uv2.y*10.+time*10.+sin(uv.x*20.-time*2.))*.35-0.3);
	
	
	
	
	//fin = mix(col2,fin,e);
	fragColor = vec4(fin,1.);
	
	
}
