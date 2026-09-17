#pragma include "../common.frag"

// COMPOSITE - what a single RCA cable does to a picture.
//
// The whole family comes from one fact: composite video carries brightness at
// full bandwidth and colour at about a tenth of it, modulated onto a subcarrier
// that shares the same wire. So colour smears sideways, fine luma detail leaks
// into the colour decoder as rainbows, and the carrier itself shows up as a
// crawling dot pattern along edges.
//
// Work happens in YIQ - luma plus two colour difference axes - because that is
// literally the signal being damaged. Doing it in RGB smears brightness too and
// just looks like a blur.

uniform sampler2D textura1;
// How far colour runs to the right of its edge.
uniform float bleed = 0.0;
uniform float bleed_len = 0.4;
// The crawling checkerboard the subcarrier leaves along sharp edges.
uniform float dotcrawl = 0.0;
// Fine luma detail decoded as colour: the shimmer on a striped shirt.
uniform float rainbow = 0.0;
// The overshoot an analogue amplifier leaves next to a hard edge - the bright
// line just after a dark-to-light transition.
uniform float ringing = 0.0;
// 0.5 is the colour that came in; below it the picture washes out the way a
// long cable run does.
uniform float chroma_sat = 0.5;

vec3 rgb2yiq(vec3 c)
{
	return vec3(
		dot(c, vec3(0.299, 0.587, 0.114)),
		dot(c, vec3(0.596, -0.274, -0.322)),
		dot(c, vec3(0.211, -0.523, 0.312)));
}

vec3 yiq2rgb(vec3 c)
{
	return vec3(
		c.x + 0.956 * c.y + 0.621 * c.z,
		c.x - 0.272 * c.y - 0.647 * c.z,
		c.x - 1.106 * c.y + 1.703 * c.z);
}

// Taps fixed at compile time: a loop bound on a uniform cannot be unrolled and
// costs far more than the samples it saves.
#define BLEED_TAPS 12

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 src = texture(textura1, uv);
	vec3 yiq = rgb2yiq(src.rgb);
	float luma = yiq.x;
	vec2 chroma = yiq.yz;

	float bleedAmount = clamp(bleed, 0.0, 1.0);
	if (bleedAmount > 0.0)
	{
		// Averaged from samples to the LEFT, so the colour of an edge trails to
		// the RIGHT of it - the direction the beam scans. Reversing this is the
		// single most common way this effect is done wrong.
		float span = mapr(bleed_len, 1.0, 40.0) / resolution.x;
		vec2 sum = vec2(0.0);
		float weight = 0.0;
		for (int k = 0; k < BLEED_TAPS; k++)
		{
			float f = float(k) / float(BLEED_TAPS - 1);
			vec3 tap = rgb2yiq(texture(textura1,
				vec2(uv.x - f * span, uv.y)).rgb);
			// Triangular window: a box filter leaves a visible hard end to the
			// smear, which reads as a second edge.
			float w = 1.0 - f;
			sum += tap.yz * w;
			weight += w;
		}
		chroma = mix(chroma, sum / weight, bleedAmount);
	}

	// One texel each way, which is the scale the artefacts below live at.
	float dx = 1.0 / resolution.x;
	float lumaLeft = rgb2yiq(texture(textura1, vec2(uv.x - dx, uv.y)).rgb).x;
	float lumaRight = rgb2yiq(texture(textura1, vec2(uv.x + dx, uv.y)).rgb).x;
	// Detail the colour decoder cannot tell apart from a subcarrier.
	float detail = luma * 2.0 - lumaLeft - lumaRight;

	float crawl = clamp(dotcrawl, 0.0, 1.0);
	if (crawl > 0.0)
	{
		// The carrier is diagonal and advances one step per field, which is
		// exactly why the pattern appears to crawl along an edge rather than
		// sit still on it.
		float field = floor(time * 60.0);
		float carrier = cos((gl_FragCoord.x + gl_FragCoord.y + field) * PI);
		chroma += vec2(carrier, -carrier) * detail * crawl * 1.5;
	}

	float rainbowAmount = clamp(rainbow, 0.0, 1.0);
	if (rainbowAmount > 0.0)
	{
		// Same leak, but phase-rotated over time so the false colour cycles
		// through the wheel instead of picking one hue and staying there.
		float ph = time * 3.0 + gl_FragCoord.x * 0.35;
		chroma += vec2(cos(ph), sin(ph)) * abs(detail) * rainbowAmount * 2.0;
	}

	float ringingAmount = clamp(ringing, 0.0, 1.0);
	if (ringingAmount > 0.0)
	{
		// Asymmetric on purpose: the overshoot follows the edge, it does not
		// straddle it. A symmetric unsharp mask is a sharpen filter, not a
		// bandwidth artefact.
		luma += (luma - lumaLeft) * ringingAmount * 1.8;
	}

	chroma *= mapr(chroma_sat, 0.0, 2.0);
	fragColor = vec4(yiq2rgb(vec3(luma, chroma)), src.a);
}
