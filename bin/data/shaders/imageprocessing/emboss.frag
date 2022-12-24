#pragma include "../common.frag"

uniform sampler2D input_texture;

uniform float force ;
void main() {
	
	vec2 uv = gl_FragCoord.xy / resolution.xy;
	ivec2 ires = textureSize( input_texture, 0 );
	float ResS = float( ires.s );
	float ResT = float( ires.t );
	
	vec3 irgb = texture( input_texture, uv ).rgb;
	vec2 stp0 = vec2(1./ResS, 0. );
	vec2 stpp = vec2(1./ResS, 1./ResT);
	vec3 c00 = texture( input_texture, uv ).rgb;
	vec3 cp1p1 = texture( input_texture, uv + stpp ).rgb;
	vec3 diffs = c00 - cp1p1; // vector difference
	float max = diffs.r;
	vec3 color = vec3( diffs.r*5.*force,diffs.g*5.*force,diffs.b*5.*force);
	fragColor = vec4( color, 1. );
}
