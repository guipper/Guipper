#version 330

// Heat-ramp visualisation for JPbox_camdepth: white near, through yellow and
// red at middle distance, to violet far.
//
// WHY THIS IS A SEPARATE PASS
//
// The depth pass smooths each frame against its own previous output. If the
// ramp were applied inside that pass, the feedback would read a COLOUR CHANNEL
// where it expects a depth value - and the red channel of this ramp saturates
// at 1.0 for everything nearer than the middle, so half the depth range would
// collapse into the smoothing and the map would drift. Keeping the depth pass
// grey means the history buffer stays a real depth map and this pass is a pure
// display transform on top, reading it once.
//
// It also means the ramp costs nothing when it is switched off: the box simply
// skips this pass and leaves the grey output the downstream shaders expect.
//
// A NOTE ON THE CONTRACT
//
// Grey output is what makes this box interchangeable with a Kinect DEPTH box
// upstream of a displacement shader. Turning the ramp on breaks that: a
// displacement shader will read the ramp's LUMINANCE. That degrades gracefully
// rather than scrambling - luminance still falls from near to far across these
// stops - but the mapping is no longer linear in depth. The ramp is for looking
// at, and for using as an image in its own right; leave it off when this box is
// feeding geometry.

uniform sampler2D profundidad;   // the grey depth map
uniform vec2 resolution;

out vec4 fragColor;

// Five explicit stops rather than a polynomial fit of a Matplotlib colormap.
// The ramp is meant to be tuned by eye here, and named stops can be moved
// without refitting anything.
//
// This is roughly an inverted `inferno`. Note that `turbo`, the map most depth
// demos reach for, runs blue-green-yellow-red: it has no white end, and its
// ordering reads near-to-far backwards for this box's near = bright convention.
vec3 heatRamp(float d)
{
	const vec3 farthest = vec3(0.04, 0.02, 0.14);   // near-black violet
	const vec3 violet   = vec3(0.42, 0.07, 0.52);
	const vec3 red      = vec3(0.89, 0.18, 0.15);   // middle distance
	const vec3 yellow   = vec3(1.00, 0.82, 0.16);
	const vec3 nearest  = vec3(1.00, 1.00, 0.97);   // white

	float t = clamp(d, 0.0, 1.0) * 4.0;
	if (t < 1.0) return mix(farthest, violet, t);
	if (t < 2.0) return mix(violet, red, t - 1.0);
	if (t < 3.0) return mix(red, yellow, t - 2.0);
	return mix(yellow, nearest, t - 3.0);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	fragColor = vec4(heatRamp(texture(profundidad, uv).r), 1.0);
}
