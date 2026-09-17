#pragma include "../common.frag"

// LUMA KEY - brightness becomes transparency.
//
// The plainest way to get a matte out of material that was never shot against
// a key colour: anything darker than the threshold disappears. On a black
// background - which is what every generative shader in this library renders -
// that is all it takes to make the subject stackable in the FINAL panel.
//
// Note what this does NOT do: it never touches rgb. A key that also darkens
// the edge bakes the background into the picture, and then no compositor can
// get it back. Colour out, alpha out, separately.

uniform sampler2D textura1;
// Luma below this is fully transparent, above threshold+softness fully opaque.
uniform float threshold = 0.15;
// 0 is a hard cut, which aliases badly on anything but a hard-edged graphic.
uniform float softness = 0.12;
// Lifts the whole matte back up: at 1.0 nothing is ever fully transparent, so
// the key reads as a fade rather than a cut.
uniform float floor_level = 0.0;
// Keys out the BRIGHT end instead - for dark subjects on a light background.
uniform bool invert;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 src = texture(textura1, uv);
	// Rec.601 weights, the same vector common.frag already carries: green
	// carries most of the perceived brightness, so an unweighted average keys
	// a saturated blue and a saturated green at wildly different points.
	float luma = dot(src.rgb, luminanceVector);
	if (invert) luma = 1.0 - luma;

	float low = clamp(threshold, 0.0, 1.0);
	// max() rather than a raw add: with softness at 0 smoothstep needs the two
	// edges to differ or it returns garbage on some drivers.
	float high = max(low + mapr(softness, 0.0, 1.0), low + 0.0001);
	float key = smoothstep(low, high, luma);
	key = mix(key, 1.0, clamp(floor_level, 0.0, 1.0));

	fragColor = vec4(src.rgb, src.a * key);
}
