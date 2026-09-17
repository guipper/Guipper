#pragma include "../common.frag"

// CRT TUBE - the glass, not the signal. Curvature, scanlines, phosphor mask,
// vignette.
//
// Everything OUTSIDE the curved screen comes out with alpha 0 rather than
// black, so the tube can be composited over another layer and actually read as
// a screen sitting in front of something. That is the same rule the mapping
// shaders follow: the box FBO is written with blending disabled, so the alpha
// written here is final.

uniform sampler2D textura1;
// Barrel distortion. 0 is a flat panel, high is a 1970s portable.
uniform float curvature = 0.0;
uniform float scanlines = 0.0;
// Lines down the picture. Around 0.35 lands near the 480 of NTSC.
uniform float scan_count = 0.35;
// Aperture grille: vertical R/G/B phosphor stripes. Visible only when the
// stripe is a couple of pixels wide, so at low render sizes keep it small.
uniform float mask_amount = 0.0;
uniform float mask_size = 0.2;
uniform float vignette = 0.0;
// Scanlines and the mask both remove light. 0.5 is unity gain, above that is
// the brightness you turn up to get it back.
uniform float boost = 0.5;

vec2 crtCurve(vec2 uv, float k)
{
	uv = uv * 2.0 - 1.0;
	// Unequal divisors on purpose: a tube bends more across than down, and
	// bending both the same amount reads as a fisheye lens instead of glass.
	vec2 bend = abs(uv.yx) / vec2(6.0, 5.0);
	uv += uv * bend * bend * k;
	return uv * 0.5 + 0.5;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	uv = crtCurve(uv, mapr(curvature, 0.0, 4.0));

	// Off the tube entirely. Answered before sampling so the corners cost
	// nothing, and returned transparent rather than black - see the header.
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
	{
		fragColor = vec4(0.0);
		return;
	}

	vec4 src = texture(textura1, uv);
	vec3 col = src.rgb;

	// cos rather than a hard step: a real beam has a gaussian profile, and a
	// square wave aliases into moire the moment the render size is not an exact
	// multiple of the line count.
	float depth = clamp(scanlines, 0.0, 1.0);
	if (depth > 0.0)
	{
		float beam = 0.5 + 0.5 * cos(uv.y * mapr(scan_count, 100.0, 1200.0) *
			TWO_PI);
		col *= mix(1.0, beam, depth);
	}

	float maskStrength = clamp(mask_amount, 0.0, 1.0);
	if (maskStrength > 0.0)
	{
		// Keyed off gl_FragCoord, not uv: the stripe belongs to the SCREEN, so
		// it must not stretch or drift when the curvature moves the sample.
		float cell = mapr(mask_size, 1.0, 6.0);
		float slot = mod(floor(gl_FragCoord.x / cell), 3.0);
		vec3 stripe = vec3(step(slot, 0.5),
			step(abs(slot - 1.0), 0.5),
			step(1.5, slot));
		// 1.6 because only one channel in three survives; without it the mask
		// costs two thirds of the brightness before `boost` sees it.
		col *= mix(vec3(1.0), stripe * 1.6, maskStrength);
	}

	float vignetteAmount = clamp(vignette, 0.0, 1.0);
	if (vignetteAmount > 0.0)
	{
		float falloff = 16.0 * uv.x * (1.0 - uv.x) * uv.y * (1.0 - uv.y);
		col *= mix(1.0, pow(clamp(falloff, 0.0, 1.0), 0.35), vignetteAmount);
	}

	col *= mapr(boost, 0.0, 2.0);
	fragColor = vec4(col, src.a);
}
