#pragma include "../common.frag"

// SLIT SCAN - one line of the input, smeared across the output over time.
//
// HOW IT WORKS, because it is not obvious: `feedback` is this box's OWN output
// from the previous frame, one frame deep. A rolling buffer of N frames is not
// available - but it is not needed. Every frame this shader copies the previous
// output shifted by one band and writes the live slit into the band it just
// vacated, so after N frames band N holds the input from N frames ago. The
// history lives in the picture itself.
//
// That makes the delay EXACT rather than a decay approximation, and it is the
// same trick a video synthesiser plays with a frame store.
//
// Two consequences worth knowing:
//   - The first second after patching it is garbage, because the buffer starts
//     black and has to fill.
//   - `speed` is in bands per frame, so the look changes with the frame rate.
//     There is no fix for that with one buffer; it is why the default is 1.

uniform sampler2D textura1;
// Which line of the SOURCE is being sampled. 0.5 is the middle of the frame.
uniform float slit_pos = 0.5;
// Thickness of the band written each frame, in output rows. Also the scroll
// distance, so these cannot be separated.
uniform float band = 0.0;
// 0 the slit scrolls DOWN the output, 1 it scrolls UP. Anything between is
// still one or the other; there is no half a row.
uniform float direction = 0.0;
// Rotates the whole thing 90 degrees: a vertical slit smeared sideways.
uniform bool horizontal;
// Fades the history as it travels, so old rows dim instead of hanging around.
uniform float decay = 0.0;
// Blends the plain input back in, for when the smear should sit on top of a
// still-readable picture rather than replace it.
uniform float dry = 0.0;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	// Everything below is written for a vertical scan; swapping the axes here
	// and again at the end is cheaper and far easier to read than branching
	// through the whole body twice.
	vec2 scanUv = horizontal ? uv.yx : uv;
	vec2 res = horizontal ? resolution.yx : resolution;

	float step_px = max(floor(mapr(band, 1.0, 24.0)), 1.0);
	float step_uv = step_px / max(res.y, 1.0);
	float down = direction < 0.5 ? 1.0 : -1.0;

	vec4 outColor;
	// The freshly vacated band: the leading edge when scrolling down, the
	// trailing one when scrolling up.
	float edge = down > 0.0 ? scanUv.y : 1.0 - scanUv.y;
	if (edge < step_uv)
	{
		// The slit itself. The whole width is kept, only the LINE is fixed -
		// that is what makes a moving subject smear into a shape rather than
		// into a single column of colour.
		vec2 sourceUv = vec2(scanUv.x, clamp(slit_pos, 0.0, 1.0));
		outColor = texture(textura1, horizontal ? sourceUv.yx : sourceUv);
	}
	else
	{
		vec2 back = vec2(scanUv.x, scanUv.y - step_uv * down);
		outColor = texture(feedback, horizontal ? back.yx : back);
		outColor.rgb *= 1.0 - mapr(decay, 0.0, 0.25);
	}

	vec4 src = texture(textura1, uv);
	fragColor = mix(outColor, src, clamp(dry, 0.0, 1.0));
}
