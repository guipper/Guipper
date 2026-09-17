#pragma include "../common.frag"

// COLOR KEY - one colour becomes transparency. The green screen.
//
// There are already chromakey.frag and chromaalpha.frag in this folder and
// NEITHER produces alpha: the first keys to black, the second to white, and
// both end on a hardcoded 1.0. That is fine as a hard matte to feed into
// blending/mask.frag, and useless for the FINAL panel, which composites with
// real alpha. This is the one that outputs a matte.
//
// Distance is measured with the key colour's own brightness divided out, so a
// shadowed green and a lit green key together. Plain RGB distance keys the lit
// part of a green screen and leaves the shadowed part opaque, which is the
// single most common way this goes wrong.

uniform sampler2D textura1;
uniform float key_red = 0.0;   // @color r
uniform float key_green = 1.0; // @color g
uniform float key_blue = 0.0;  // @color b
// How far from the key colour still counts as background.
uniform float tolerance = 0.25;
uniform float softness = 0.12;
// Removes the green fringe the key leaves on hair and edges, by pulling any
// remaining key hue back toward the other two channels. Costs saturation, which
// is exactly the trade every keyer makes.
uniform float spill = 0.0;
uniform bool invert;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 src = texture(textura1, uv);
	vec3 key = vec3(key_red, key_green, key_blue);

	// Normalising by luma is what makes this shade-independent. The epsilon
	// keeps a black key colour - a legal, if odd, choice - from dividing by 0.
	float srcLuma = max(dot(src.rgb, luminanceVector), 0.0001);
	float keyLuma = max(dot(key, luminanceVector), 0.0001);
	float dist = distance(src.rgb / srcLuma, key / keyLuma);

	float low = mapr(tolerance, 0.0, 1.5);
	float high = max(low + mapr(softness, 0.0, 1.0), low + 0.0001);
	float matte = smoothstep(low, high, dist);
	if (invert) matte = 1.0 - matte;

	vec3 col = src.rgb;
	float spillAmount = clamp(spill, 0.0, 1.0);
	if (spillAmount > 0.0)
	{
		// Only where the key hue still dominates, and only down to the average
		// of the other two channels - going further inverts the fringe into the
		// complementary colour, which is worse than the fringe.
		float keyed = dot(normalize(max(key, vec3(0.0001))),
			normalize(max(src.rgb, vec3(0.0001))));
		float grey = dot(col, vec3(1.0 / 3.0));
		col = mix(col, vec3(grey), spillAmount *
			smoothstep(0.7, 1.0, keyed) * (1.0 - matte));
	}

	fragColor = vec4(col, src.a * matte);
}
