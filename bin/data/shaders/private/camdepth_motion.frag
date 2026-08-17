#version 330

// Motion-parallax pass for JPbox_camdepth.
//
// This is the one cue in the box that is actual GEOMETRY rather than an image
// statistic. Brightness, saturation and local structure are all guesses about
// what a surface looks like; parallax measures how far a thing MOVED, and under
// any camera or subject motion the near thing sweeps more pixels than the far
// one. That relationship is real, which is why this cue survives footage where
// the others fail - a dark subject against a bright wall, for instance, which
// the brightness cue gets exactly backwards.
//
// Runs at a QUARTER of the render resolution on purpose. A frame difference is
// the single most noise-amplifying operation in the box: sensor noise is
// uncorrelated between frames, so it lands in the difference at full strength.
// Downsampling averages it away for free and costs 1/16th of the work, and
// parallax has no fine detail worth preserving anyway.
//
// Output is packed, not visual:
//   .r = this frame's luma, kept so the NEXT frame has something to difference
//   .g = accumulated motion energy, which is the cue itself

uniform sampler2D camara;
uniform sampler2D anterior;   // this same buffer from last frame (ping-ponged)
uniform vec2 resolution;

uniform float espejo;       // must match the depth pass or the cue lands mirrored
uniform float retencion;    // how long motion lingers once it stops
uniform float ganancia;     // sensitivity of the difference

out vec4 fragColor;

float luma(vec3 c)
{
	return dot(c, vec3(0.299, 0.587, 0.114));
}

void main()
{
	// Output space. The camera is mirrored on sampling instead, so this buffer
	// stays in the same orientation as the depth pass that reads it back.
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 cam = uv;
	if (espejo > 0.5) cam.x = 1.0 - cam.x;

	// Four taps averaged rather than one point sample. At quarter resolution a
	// point sample would alias a single noisy sensor pixel into the difference;
	// the box filter is what makes the cue readable at all.
	vec2 texel = 1.0 / resolution;
	float now = 0.25 * (
		luma(texture(camara, cam + vec2(-0.5, -0.5) * texel).rgb) +
		luma(texture(camara, cam + vec2( 0.5, -0.5) * texel).rgb) +
		luma(texture(camara, cam + vec2(-0.5,  0.5) * texel).rgb) +
		luma(texture(camara, cam + vec2( 0.5,  0.5) * texel).rgb));

	vec2 previous = texture(anterior, uv).rg;
	float difference = abs(now - previous.r) * max(0.0, ganancia);

	// Peak-hold with decay, NOT a running average.
	//
	// Motion is intermittent and depth is not. A dancer who holds a pose for a
	// second is still in the foreground, but an average would collapse to zero
	// the moment they stop and the subject would drop to "far" mid-phrase -
	// which is precisely when it is most visible. Holding the peak and bleeding
	// it off slowly keeps the cue steady through pauses, at the cost of a near
	// reading lingering briefly after something leaves.
	float energy = max(difference, previous.g * clamp(retencion, 0.0, 0.995));

	fragColor = vec4(now, clamp(energy, 0.0, 1.0), 0.0, 1.0);
}
