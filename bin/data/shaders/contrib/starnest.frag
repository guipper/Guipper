#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// Star Nest by Pablo Roman Andrioli

// This content is under the MIT License.

uniform float iterations;
uniform float formuparam;

uniform float volsteps;
uniform float stepsize;

uniform float zoom;

uniform float tile;
uniform float speedx;
uniform float speedy;
uniform float brightness;
//#define brightness 0.0015
uniform float darkmatter;
//#define darkmatter 0.300
uniform float distfading;
uniform float saturation;
uniform float ma1;
uniform float ma2;


void main()
{
	//get coords and direction
	vec2 uv=gl_FragCoord.xy.xy/iResolution.xy-.5;
	uv.y*=iResolution.y/iResolution.x;
	vec3 dir=vec3(uv*zoom*5.0,1.);
	//float time=iTime*speed+.25;

	//mouse rotation
	float a1=.5+1.0/iResolution.x*2.;
	float a2=.8+1.0/iResolution.y*2.;
	mat2 rot1=mat2(cos(a1),sin(a1),-sin(a1),cos(a1));
	mat2 rot2=mat2(cos(a2),sin(a2),-sin(a2),cos(a2));
	dir.xz*=rot1;
	dir.xy*=rot2;
	vec3 from=vec3(1.,.5,0.5);
	from+=vec3(time*mapr(speedx,-0.05,0.05),time*mapr(speedy,-0.05,0.05),-2.);
	//from.xz*=rot1;
	//from.xy*=rot2;
	
	//volumetric rendering
	float s=0.1,fade=1.;
	vec3 v=vec3(0.);
	
	
	int mite = int(floor(mapr(iterations,10.0,25.0)));
	int mvolsteps = int(floor(mapr(volsteps,0.0,20.0)));
	float mbri = mapr(brightness,0.0,0.0030);
	float mdarkmatter = mapr(darkmatter,0.0,10.0);
	for (int r=0; r<mvolsteps; r++) {
		vec3 p=from+s*dir*.5;
		p = abs(vec3(tile)-mod(p,vec3(tile*2.))); // tiling fold
		float pa,a=pa=0.;
		for (int i=0; i<mite; i++) { 
			p=abs(p)/dot(p,p)-formuparam; // the magic formula
			a+=abs(length(p)-pa); // absolute sum of average change
			pa=length(p);
		}
		float dm=max(0.,mdarkmatter-a*a*.001); //dark matter
		a*=a*a; // add contrast
		if (r>6) fade*=1.-dm; // dark matter, don't render near
		//v+=vec3(dm,dm*.5,0.);
		v+=fade;
		v+=vec3(s,s*s,s*s*s*s)*a*mbri*fade; // coloring based on distance
		fade*=distfading; // distance fading
		s+=stepsize;
	}
	v=mix(vec3(length(v)),v,saturation); //color adjust
	fragColor = vec4(v*.01,1.);	
	
}