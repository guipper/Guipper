#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float size;
 //float size = .2;
 float period = 1.5;
vec2 R;

#define rot(a)             mat2( cos(a), -sin(a), sin(a), cos(a) )
float easeInOut(float t) { return t < .5 ? 2.*t*t : -1.+ (4.-2.*t)*t; }
float angle( float t )   { return ( floor(t) + easeInOut(fract(t)) ) * PI/2.; }

float arrow( vec2 coords )
{
    float x = abs(coords.x - .5), y=coords.y, p = 1./R.y/mapr(size,0.0,1.0);
    return  smoothstep(-p, p, y < .5 || x<.25 ? y : y-.5)  // bases
          * smoothstep( p,-p, y < .5 ? x-.25 : x-1.+y);    // sides
}

float drawArrow( vec2 coords, vec2 offset, float a )
{
    coords -= offset;
    vec2 origin = vec2(.5, .25);
    coords = (coords-origin) * rot(a) + origin; 
    return arrow(coords);
}

float cell( vec2 U, float a )
{
    float v = 0.;
    for (int i=0; i<6; i++) 
    	v += drawArrow(U, vec2(i%3-1, i/3), a);
    return v;
}

void main()
{
    R = iResolution.xy;
    vec2 U = ( gl_FragCoord.xy.xy - .5*vec2(R.x,0) ) / R.y;
    
    float t = iTime/period, a = angle(t);
    int i = int(t) % 4;
    U = fract( U/mapr(size,0.0,1.0) + (i>1 ? .5 : 0.) ); 
    
	if (i%2==1){
		U.y +=.5;
		a+=PI;
	}
	
	vec4 fin = vec4( cell(U, a) );
    fragColor =  fin;
	
    if (i%2==1){ 
		fragColor = 1.-fragColor;
	}
	
}