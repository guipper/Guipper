#pragma include "../common.frag"

// rainbow spaghetti by mattz
// Refactored by FabriceNeyret2
void main() {
	vec2 p = gl_FragCoord.xy;
    vec2 R = iResolution.xy;
    float t = iTime;
    
	vec4 o = vec4(0.0);
    for(int x=0; x<20; x++) 
    	for(int y=0; y<20; y++)
        {
            vec4 S = 1. + sin(t*vec4(1,3,5,11));
            length( p - vec2(x,y)*R/20. ) / R.y < S.w/66.  
			?  o = S/2. : o ;                       
            
			t += .1;
        }
	fragColor = o;
}