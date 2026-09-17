#pragma include "../common.frag"

// BAYER DITHER - ordered dithering, the look of a 1980s home computer.
//
// randomdither.frag already exists and uses a hash, which gives film-grain
// noise. This uses a fixed 8x8 threshold matrix instead, and that is the whole
// difference: an ordered pattern is STABLE frame to frame, so the texture sits
// still on the image instead of boiling. That stillness is what reads as a
// screen mode rather than as noise.
//
// The matrix is built from the recursive definition rather than typed out as
// 64 constants, so it cannot be typed wrong and the size is one number.

uniform sampler2D textura1;
// Colours per channel: low is a 2-colour screen, high barely posterises.
uniform float levels = 0.12;
// Pattern scale in pixels. 1 is the true pixel-level screen; larger is the
// chunky look of the picture being upscaled afterwards.
uniform float pattern_scale = 0.0;
uniform float contrast = 0.5;
// Dither the luma and keep the original hue, instead of dithering each
// channel into its own pattern.
uniform bool mono;

// Bayer 8x8, normalised to 0..1. Derived by interleaving bits of x and y - the
// standard recursive construction - because a hand-typed 64 entry table is a
// transcription error waiting to happen.
float bayer8(vec2 cell)
{
	int x = int(mod(cell.x, 8.0));
	int y = int(mod(cell.y, 8.0));
	int v = 0;
	for (int i = 0; i < 3; i++)
	{
		int xb = (x >> i) & 1;
		int yb = (y >> i) & 1;
		// Each level contributes two bits: the y bit above the x-xor-y bit.
		v = v | ((yb ^ xb) << (2 * i));
		v = v | (yb << (2 * i + 1));
	}
	return float(v) / 64.0;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 src = texture(textura1, uv);

	float scale = max(floor(mapr(pattern_scale, 1.0, 8.0)), 1.0);
	float t = bayer8(floor(gl_FragCoord.xy / scale));
	// Centred on 0 so the pattern brightens and darkens equally; a raw 0..1
	// threshold shifts the whole image darker.
	float bias = t - 0.5;

	float steps = max(floor(mapr(levels, 2.0, 16.0)), 2.0);
	vec3 col = src.rgb;
	col = clamp((col - 0.5) * mapr(contrast, 0.5, 3.0) + 0.5, 0.0, 1.0);

	if (mono)
	{
		float luma = dot(col, luminanceVector);
		// The bias is divided by the step count: it only has to nudge a value
		// across ONE quantisation step. Any larger and the pattern stops being
		// a dither and becomes an overlay.
		float q = floor(luma * (steps - 1.0) + bias + 0.5) / (steps - 1.0);
		// Keeping the original hue and replacing only the brightness is what
		// makes a colour picture survive a 2-level screen.
		col = src.rgb * (q / max(luma, 0.0001));
	}
	else
	{
		col = floor(col * (steps - 1.0) + bias + 0.5) / (steps - 1.0);
	}

	fragColor = vec4(clamp(col, 0.0, 1.0), src.a);
}
