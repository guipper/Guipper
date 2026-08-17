#version 330

// Pseudo-depth from an ordinary camera frame.
//
// NOT a depth sensor. There is no geometry here - this reads monocular cues out
// of a 2D image, so a dark object close to the lens comes out "far" and a bright
// wall behind comes out "near". It is a VJ effect, and anything that needs real
// metric depth (the point cloud, for instance) must keep using the Kinect.
//
// Shaped after Depth Anything V2's OUTPUT, not its method - there is no network
// here. Three things were worth borrowing:
//
//   1. V2's headline fix over V1 was that it stopped carving depth out of
//      TEXTURE. V1 read fine detail as nearness, so a patterned wall came out
//      corrugated. The detail cue below therefore measures STRUCTURE - the
//      difference between two scales - instead of raw high-frequency energy,
//      and a flat textured surface reads flat.
//   2. V2 produces smooth regions with sharp boundaries. The smoothing here is
//      edge-aware: neighbours only pull on each other when they are similar in
//      colour, so an object edge stays an edge instead of being blurred across.
//   3. V2 outputs affine-invariant INVERSE depth (disparity), which compresses
//      the far field and spends most of the range on what is close. The `curva`
//      control applies that same bias.
//
// One thing here deliberately goes BEYOND what V2 does. V2 is strictly
// single-image: it sees one frame and has no access to time. This box runs on
// live video, so it can difference consecutive frames and read MOTION PARALLAX
// - the only cue in the whole file that MEASURES geometry instead of inferring
// it from appearance. A network that has learned what a face looks like is
// still guessing; a pixel that moved twice as far genuinely is closer. Cue 4 is
// that measurement, and it is the one to reach for when the footage defeats the
// appearance cues.
//
// This pass always writes GREY. The heat ramp is a separate pass
// (camdepth_show.frag) on purpose: the temporal smoothing below reads back this
// box's own previous frame, and if the ramp were applied here that feedback
// would be reading a colour channel instead of a depth value.
//
// Output convention matches JPbox_kinect2's own grey map: NEAR = BRIGHT, far =
// black. That is what lets this box and a Kinect DEPTH box drive the same
// downstream displacement shader interchangeably.
//
// Loaded directly by JPbox_camdepth rather than parsed as a user shader, so the
// uniform names here are a private contract with that box, not inlets.

uniform sampler2D camara;      // the live camera frame
uniform sampler2D anterior;    // this box's previous output, for smoothing
uniform sampler2D movimiento;  // quarter-res parallax buffer, .g = motion energy
uniform vec2 resolution;

// Cue weights. Summed and renormalised, so zeroing one does not darken the map.
uniform float pesoFoco;        // structure / "is this a distinct surface"
uniform float pesoBrillo;
uniform float pesoVertical;
uniform float pesoParalaje;    // motion parallax - the only geometric cue here
uniform float pesoAire;        // aerial perspective / haze

uniform float radio;        // base sampling radius, in pixels
uniform float contraste;    // gamma on the result
uniform float cerca;        // output range remap
uniform float lejos;
uniform float suavizado;    // temporal smoothing
uniform float bordes;       // edge-aware strength: 0 = plain blur, 1 = strict
uniform float curva;        // 0 = linear, 1 = full inverse-depth (disparity)
uniform float invertir;     // >0.5 flips near/far
uniform float pisoAbajo;    // >0.5 = bottom of frame is near
uniform float espejo;       // >0.5 mirrors horizontally, as a camera box does

out vec4 fragColor;

float luma(vec3 c)
{
	return dot(c, vec3(0.299, 0.587, 0.114));
}

void main()
{
	// Two spaces, deliberately kept apart.
	//
	// `uv` is OUTPUT space and is never mirrored; `cam` is where the camera is
	// sampled. Mirroring only `cam` is what lets the buffers that live in output
	// space - the previous depth frame and the parallax buffer - be read back at
	// the coordinate they were written to. Flipping one shared `uv` instead made
	// the temporal blend read its own previous frame REVERSED, ghosting a
	// mirrored copy of the subject over the map whenever espejo and suavizado
	// were both on.
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 cam = uv;
	if (espejo > 0.5) cam.x = 1.0 - cam.x;
	vec2 texel = 1.0 / resolution;

	vec3 centre = texture(camara, cam).rgb;
	float centreLuma = luma(centre);

	float r = max(1.0, radio);
	// The eight offsets are reused for the fine ring, the coarse ring and the
	// edge-aware average, so the whole shader costs 16 taps rather than 24.
	vec2 ring[8] = vec2[8](
		vec2( 1.0,  0.0), vec2(-1.0,  0.0),
		vec2( 0.0,  1.0), vec2( 0.0, -1.0),
		vec2( 0.7,  0.7), vec2(-0.7,  0.7),
		vec2( 0.7, -0.7), vec2(-0.7, -0.7));

	// --- Two scales -------------------------------------------------------
	// Fine ring = texture. Coarse ring = structure. Reading them separately is
	// what lets the cue below ignore the first and keep the second.
	float fine = 0.0;
	float coarse = 0.0;
	// Edge-aware accumulation: a neighbour contributes in proportion to how
	// similar its COLOUR is to the centre, so the average never reaches across
	// an object boundary. This is the "sharp edges" half of V2's look.
	float edgeSum = 0.0;
	float edgeWeight = 0.0;
	for (int i = 0; i < 8; i++)
	{
		vec3 nearSample = texture(camara, cam + ring[i] * r * texel).rgb;
		vec3 farSample = texture(camara, cam + ring[i] * r * 4.0 * texel).rgb;
		float nearLuma = luma(nearSample);
		fine += nearLuma;
		coarse += luma(farSample);

		float colourDistance = length(nearSample - centre);
		// bordes = 0 -> every neighbour counts equally (a plain blur)
		// bordes = 1 -> only near-identical colours count (edges preserved)
		float w = exp(-colourDistance * mix(0.5, 24.0, clamp(bordes, 0.0, 1.0)));
		edgeSum += nearLuma * w;
		edgeWeight += w;
	}
	fine *= 0.125;
	coarse *= 0.125;
	float smoothedLuma = edgeWeight > 0.0001 ? edgeSum / edgeWeight : centreLuma;

	// --- Cue 1: structure, not texture ------------------------------------
	// The gap between the two scales. A patterned but flat wall has lots of
	// fine detail and almost no scale gap, so it reads FLAT - which is exactly
	// the V1 failure V2 fixed. A real object boundary changes both scales
	// differently and survives.
	float structureCue = clamp(abs(smoothedLuma - coarse) * 6.0, 0.0, 1.0);

	// --- Cue 2: brightness ------------------------------------------------
	// Light falls off with distance, so on a dark stage the lit subject is the
	// near one. The edge-aware average is used rather than the raw pixel, so
	// sensor noise does not become depth noise.
	float brightCue = smoothedLuma;

	// --- Cue 3: vertical position ----------------------------------------
	// The floor recedes upward, so low in frame reads as near. Noise-free by
	// construction, which makes it a good stabiliser under the other two.
	float verticalCue = pisoAbajo > 0.5 ? (1.0 - uv.y) : uv.y;

	// --- Cue 4: motion parallax -------------------------------------------
	// Under camera or subject motion a near surface sweeps more pixels than a
	// far one, and that holds regardless of how the scene is lit - which is why
	// this cue rescues the footage the others get wrong. A dark subject against
	// a bright wall reads backwards on brightness and correctly here.
	//
	// Read in OUTPUT space with a plain bilinear fetch: the buffer is quarter
	// resolution, so the hardware filter is already averaging four pixels at a
	// time, which is exactly the spatial blur this cue wants.
	//
	// Reports zero on a still scene, and the renormalisation below then hands
	// its share to the other cues rather than darkening the map.
	float parallaxCue = clamp(texture(movimiento, uv).g, 0.0, 1.0);
	// Square root: motion energy bunches up near zero, so a linear mapping
	// leaves everything but the fastest edge looking equally far.
	parallaxCue = sqrt(parallaxCue);

	// --- Cue 5: aerial perspective ----------------------------------------
	// Haze. Distance scatters light into the line of sight and washes colour
	// out, so the far field is DESATURATED. The one classical cue here that is
	// about colour rather than luminance, which keeps it informative on a flatly
	// lit scene where brightness and structure have nothing to say.
	float maxChannel = max(centre.r, max(centre.g, centre.b));
	float minChannel = min(centre.r, min(centre.g, centre.b));
	float aerialCue = maxChannel > 0.001 ?
		(maxChannel - minChannel) / maxChannel : 0.0;

	// --- Combine ----------------------------------------------------------
	float total = pesoFoco + pesoBrillo + pesoVertical + pesoParalaje + pesoAire;
	float depth;
	if (total < 0.0001)
	{
		// All weights at zero would divide by zero and flash. Flat mid-grey
		// reads as "no depth information" rather than as a glitch.
		depth = 0.5;
	}
	else
	{
		depth = (structureCue * pesoFoco +
				 brightCue * pesoBrillo +
				 verticalCue * pesoVertical +
				 parallaxCue * pesoParalaje +
				 aerialCue * pesoAire) / total;
	}

	// --- Disparity bias ---------------------------------------------------
	// V2 emits inverse depth, so most of its range describes what is close and
	// the far field is compressed into the last few values. Mixing toward
	// d/(2-d) reproduces that distribution; at curva = 0 this is linear.
	depth = clamp(depth, 0.0, 1.0);
	float disparity = depth / max(0.001, 2.0 - depth);
	depth = mix(depth, clamp(disparity * 2.0, 0.0, 1.0), clamp(curva, 0.0, 1.0));

	// --- Shaping ----------------------------------------------------------
	depth = pow(clamp(depth, 0.0, 1.0), max(0.05, contraste));
	depth = mix(clamp(cerca, 0.0, 1.0), clamp(lejos, 0.0, 1.0), depth);
	if (invertir > 0.5) depth = 1.0 - depth;

	// --- Temporal smoothing ----------------------------------------------
	// A per-frame estimate off a noisy sensor flickers, and every displacement
	// shader downstream amplifies it. Blending against the previous output is
	// what makes this usable rather than a novelty. Costs one texture read.
	float previous = texture(anterior, uv).r;
	depth = mix(depth, previous, clamp(suavizado, 0.0, 0.98));

	fragColor = vec4(vec3(depth), 1.0);
}
