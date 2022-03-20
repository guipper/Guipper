#pragma include "../common.frag"

// "Magic Pencil" by Kali

// Testing the use of the webcam input as a controller

uniform sampler2DRect iChannel0;
uniform sampler2DRect iChannel1;


#define red vec3(1.,0.,0.)
#define green vec3(0.,1.,0.)
#define blue vec3(0.,0.,1.)


// Use a pencil, pen or some stick-like thing
// the most pure red, green or blue you can find.

// Choose the color here:

#define target_color blue


// Make sure there aren't other objects the same color as the pencil
// in the scene, and adjust this settings so the webcam window is b&w
// and only the pencil is in color, with a red dot at both ends.

#define treshold .64
#define min_luma .35


// Put the pencil horizontal and close to the camera, then rotate it.
// The texture should follow it's rotation.

// It works better with a front light and a dark background.



// Other settings:

// Resolution for scanning the webcam image.
// You can use lower values for more fps but less precision,
// but too low values could make the scan miss the pencil.

#define scan_resolution 30.


// Proportional size of webcam window

#define webcam_window_size .5



// get two coords from the pencil, by finding the first and last match.
vec4 getcoords(void) {
	vec2 coord1=vec2(0.);
	vec2 coord2=vec2(0.);
	float res=1./scan_resolution; //scanning step
	for (float x=0.; x<scan_resolution; x++)
	{
		for (float y=0.; y<scan_resolution; y++)
		{
			vec2 pos=res*vec2(x,y);
			vec3 col=texture(iChannel0,pos).rgb;
			if (length(target_color*normalize(col))>treshold && length(target_color*col)>min_luma){
				//save first match
				coord1=coord1+pos*(1.-sign(coord1.x)); //just wanted to avoid another if :)
				//last match coords will stay saved in coord2
				coord2=pos;
			}
		}
	}
	return vec4(coord1,coord2);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / iResolution.xy*1./webcam_window_size; //webcam window
	vec2 uvrot = gl_FragCoord.xy / iResolution.xy; //texture window
	uv.x=1.-uv.x; //webcam x mirror
	uvrot.y=1.-uvrot.y; //texture y mirror
	vec4 gpos=getcoords(); //get pencil coords
	vec3 col=texture(iChannel0,uv).rgb; //get pix from webcam
	vec2 vec=gpos.xy-gpos.zw; //vector defined by the pencil
	vec2 aspect=vec2(iResolution.x/iResolution.y,1.);
	uv*=aspect; uvrot*=aspect; gpos.xy*=aspect; gpos.zw*=aspect;

	//make all monochrome but the pencil (also darkening)
	if (length(target_color*normalize(col))<treshold || length(target_color*col)<min_luma)
	    col=vec3(length(col))*.5;

	//red points
	float c=0.;
	if (length(gpos.xy)>0.) c+=smoothstep(0.,1.,max(0.,.05-distance(uv,gpos.xy))*20.);
	if (length(gpos.zw)>0.) c+=smoothstep(0.,1.,max(0.,.05-distance(uv,gpos.zw))*20.);
	col=mix(col,vec3(1.,.4,.4),c);

	if (uv.y>.9 && uv.x<.1) col=target_color; //little square showing the color choosed

	// rotate texture
	if (uv.y>1. || uv.x<0.) {
		float ang=atan(vec.x,vec.y)+3.1416*.5; //get angle from pencil
		mat2 rot=mat2(cos(ang),-sin(ang),sin(ang),cos(ang)); //2D rot matrix
		//rotate only if pencil is present
		if (length(gpos.xy)>0.) uvrot=(uvrot-aspect*.5)*rot+aspect*.5;
		col=texture(iChannel1,uvrot).rgb;
	}


	gl_FragColor = vec4(col, 1.0);
}
