#pragma include "../common.frag"


#define BLACK_AND_WHITE
#define LINES_AND_FLICKER
#define BLOTCHES
#define GRAIN

#define FREQUENCY 10.0

vec2 uv;

uniform sampler2D iChannel0;

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float rand(float c){
	return rand(vec2(c,1.0));
}

float randomLine(float seed)
{
	float b = 0.01 * rand(seed);
	float a = rand(seed+1.0);
	float c = rand(seed+2.0) - 0.5;
	float mu = rand(seed+3.0);
	
	float l = 1.0;
	
	if ( mu > 0.2)
		l = pow(  abs(a * uv.x + b * uv.y + c ), 1.0/8.0 );
	else
		l = 2.0 - pow( abs(a * uv.x + b * uv.y + c), 1.0/8.0 );				
	
	return mix(0.0, 1.0, l);
}

// Generate some blotches.
float randomBlotch(float seed)
{
	float x = rand(seed);
	float y = rand(seed+1.0);
	float s = 0.01 * rand(seed+2.0);
	
	vec2 p = vec2(x,y) - uv;
	p.x *= iResolution.x / iResolution.y;
	float a = atan(p.y,p.x);
	float v = 1.0;
	float ss = s*s * (sin(6.2831*a*x)*0.1 + 1.0);
	
	if ( dot(p,p) < ss ) v = 0.2;
	else
		v = pow(dot(p,p) - ss, 1.0/16.0);
	
	return mix(0.1 + 0.2 * (1.0 - (s / 0.02)), 1.0, v);
}


void main()
{
	uv = gl_FragCoord.xy / iResolution.xy;
	
	// Set frequency of global effect to 15 variations per second
	float t = float(int(iTime * FREQUENCY));
		  t = int(iTime * FREQUENCY);
	// Get some image movement
	vec2 suv = uv + 0.002 * vec2( rand(t), rand(t + 23.0));
	
	// Get the image
	vec3 image = texture( iChannel0, vec2(suv.x, suv.y) ).xyz;
	// Convert it to B/W
	float luma = dot( vec3(0.2126, 0.7152, 0.0722), image );
	vec3 oldImage = image;

	
	// Create a time-varying vignetting effect
	float vI = 16.0 * (uv.x * (1.0-uv.x) * uv.y * (1.0-uv.y));
	vI *= mix( 0.7, 1.0, rand(t + 0.5));
	
	// Add additive flicker
	vI += 1.0 + 0.4 * rand(t+8.);
	
	// Add a fixed vignetting (independent of the flicker)
	vI *= pow(16.0 * uv.x * (1.0-uv.x) * uv.y * (1.0-uv.y), 0.4);
	
	// Add some random lines (and some multiplicative flicker. Oh well.)
	int l = int(8.0 * rand(t+7.0));
	
	if ( 0 < l ) vI *= randomLine( t+6.0+17.* float(0));
	if ( 1 < l ) vI *= randomLine( t+6.0+17.* float(1));
	if ( 2 < l ) vI *= randomLine( t+6.0+17.* float(2));		
	if ( 3 < l ) vI *= randomLine( t+6.0+17.* float(3));
	if ( 4 < l ) vI *= randomLine( t+6.0+17.* float(4));
	if ( 5 < l ) vI *= randomLine( t+6.0+17.* float(5));
	if ( 6 < l ) vI *= randomLine( t+6.0+17.* float(6));
	if ( 7 < l ) vI *= randomLine( t+6.0+17.* float(7));
	
	

	int s = int( max(8.0 * rand(t+18.0) -2.0, 0.0 ));

	if ( 0 < s ) vI *= randomBlotch( t+6.0+19.* float(0));
	if ( 1 < s ) vI *= randomBlotch( t+6.0+19.* float(1));
	if ( 2 < s ) vI *= randomBlotch( t+6.0+19.* float(2));
	if ( 3 < s ) vI *= randomBlotch( t+6.0+19.* float(3));
	if ( 4 < s ) vI *= randomBlotch( t+6.0+19.* float(4));
	if ( 5 < s ) vI *= randomBlotch( t+6.0+19.* float(5));


	
	fragColor = vec4(oldImage* vI,1.0);
	


}