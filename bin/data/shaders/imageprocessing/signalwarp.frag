#pragma include "../common.frag"

// SIGNAL WARP - the geometry half of an unstable analogue picture.
//
// Only the sampling position moves; colour is untouched, which is what keeps
// this stackable with crttube / composite / signalfault instead of each one
// fighting over the same pixels.
//
// Every knob is 0..1 because that is the only range Guipper gives a shader
// uniform, so the useful span lives in the mapr() call, not in the slider.
// Everything neutral at 0 - dropping the box in front of a chain must not
// change the image until a knob is moved.

uniform sampler2D textura1;
// Displacement amplitude, as a fraction of the frame.
uniform float wave_amount = 0.0;
// Cycles down the picture. Low is a slow bend, high is the fine ripple of a
// badly terminated cable.
uniform float wave_freq = 0.3;
uniform float wave_speed = 0.5;
// 0 rows shifted sideways (the usual one), 1 columns shifted vertically.
uniform float wave_vertical = 0.0;
// Wow and flutter: the slow, irregular drift of a tape transport, as opposed
// to the clean periodic wave above. Together they stop the motion reading as
// a sine.
uniform float flutter = 0.0;
// Hard horizontal jumps on whole bands of lines - a line that lost its sync
// and restarted in the wrong place. One knob drives how often AND how far,
// because in a real fault those two never move independently.
uniform float tear = 0.0;
uniform float tear_size = 0.4;

float warpHash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float t = time * mapr(wave_speed, 0.0, 6.0);
	float vertical = clamp(wave_vertical, 0.0, 1.0);

	// The travelling wave. Reading the phase across x instead of y is what
	// turns a horizontal wobble into a vertical one, so one branch covers both.
	float phase = mix(uv.y, uv.x, vertical) * mapr(wave_freq, 1.0, 80.0) + t;
	float shift = sin(phase * TWO_PI) * mapr(wave_amount, 0.0, 0.15);
	uv += mix(vec2(shift, 0.0), vec2(0.0, shift), vertical);

	// Band-limited noise rather than another sine: flutter that repeats is not
	// flutter, it is just a second wave.
	float flutterAmount = mapr(flutter, 0.0, 0.06);
	if (flutterAmount > 0.0)
	{
		float drift = noise(vec2(uv.y * 5.0, time * 0.8)) - 0.5;
		uv.x += drift * flutterAmount * 2.0;
	}

	float tearAmount = mapr(tear, 0.0, 0.3);
	if (tearAmount > 0.0)
	{
		// Quantised in time as well as in y: a tear holds for a few frames and
		// then jumps somewhere else, which is how a dropped sync actually
		// behaves. Per-frame randomness reads as noise, not as a fault.
		float band = floor(uv.y * mapr(tear_size, 4.0, 140.0));
		float slot = floor(t * 8.0 + 0.5);
		float fires = step(1.0 - mapr(tear, 0.0, 0.6),
			warpHash(vec2(band, slot)));
		uv.x += (warpHash(vec2(band, slot + 37.0)) - 0.5) *
			tearAmount * fires;
	}

	vec4 src = texture(textura1, uv);
	// Displacement can reach outside the frame. GL_CLAMP_TO_EDGE would smear
	// the edge row across the gap; going transparent leaves a hole instead,
	// which is both what a real dropout looks like and what lets this sit over
	// another layer in the FINAL stack.
	float inside = step(0.0, uv.x) * step(uv.x, 1.0) *
		step(0.0, uv.y) * step(uv.y, 1.0);
	fragColor = vec4(src.rgb, src.a * inside);
}
