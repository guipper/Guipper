#pragma include "../common.frag"

// SIGNAL FAULT - the things that go wrong between the tape and the tube.
//
// Sync loss, mains hum, a ghost off a badly matched aerial, and the three
// faults that belong to tape specifically: the head-switching band along the
// bottom edge, dropouts, and tracking error.
//
// Deliberately separate from signalwarp: that one is a continuous wobble you
// leave running, these are discrete faults you punch in. Chained, warp goes
// first - the tape is unstable, and then the signal off it breaks up.

uniform sampler2D textura1;
// Vertical roll, the picture crawling up or down past a black frame bar.
uniform float roll = 0.0;
// A 50 Hz mains bar drifting slowly down the picture.
uniform float hum = 0.0;
// Multipath: the same picture arriving again, later and weaker.
uniform float ghost = 0.0;
uniform float ghost_delay = 0.3;
// The band along the BOTTOM where a VHS head hands over mid-field. Always at
// the bottom, always torn - that position is the giveaway that says "tape".
uniform float headswitch = 0.0;
// Bright horizontal dashes: the instant a head reads no signal at all.
uniform float dropout = 0.0;
// Tracking error: a noisy band of misaligned lines drifting up the picture.
uniform float tracking = 0.0;

float faultHash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	// The picture can roll; the SCREEN cannot. Head switching happens in the
	// player and mains hum is picked up on the way to the tube, so both stay
	// nailed to the glass while the image slides past them. Reading them off
	// the rolled uv made the band roll too, which reads as part of the picture
	// instead of as a fault on top of it.
	//
	// y is measured DOWN from the top - row 0 of the FBO is the top row of the
	// drawn rectangle, the same convention mapping_advanced.frag documents.
	float screenY = gl_FragCoord.y / resolution.y;
	float bar = 1.0;

	float rollAmount = mapr(roll, 0.0, 1.5);
	if (rollAmount > 0.0)
	{
		uv.y = fract(uv.y + time * rollAmount);
		// The black frame bar that rolls with it. Without it the picture just
		// scrolls, and a scrolling picture is not a roll - the seam is the
		// whole tell.
		bar *= smoothstep(0.0, 0.02, uv.y) * smoothstep(0.0, 0.02, 1.0 - uv.y);
	}

	// Tracking: a band of lines that never locked, drifting up the picture.
	float trackingAmount = clamp(tracking, 0.0, 1.0);
	float trackingBand = 0.0;
	if (trackingAmount > 0.0)
	{
		float centre = fract(time * 0.12);
		float halfHeight = mapr(tracking, 0.005, 0.12);
		trackingBand = 1.0 - smoothstep(0.0, halfHeight,
			abs(fract(uv.y - centre + 0.5) - 0.5));
		uv.x += (faultHash(vec2(floor(uv.y * resolution.y),
			floor(time * 30.0))) - 0.5) * trackingBand * 0.15;
	}

	// Head switching, on the bottom few percent only.
	float switchAmount = clamp(headswitch, 0.0, 1.0);
	float switchBand = 0.0;
	if (switchAmount > 0.0)
	{
		// Measured from the BOTTOM edge: a VHS head hands over near the end of
		// the field, so the torn band is always along the bottom of the frame.
		// That position is most of what makes it read as tape rather than as
		// generic glitch.
		switchBand = 1.0 - smoothstep(0.0, mapr(headswitch, 0.005, 0.06),
			1.0 - screenY);
		uv.x += (faultHash(vec2(floor(screenY * resolution.y),
			floor(time * 25.0))) - 0.5) * switchBand * 0.4;
	}

	vec4 src = texture(textura1, clamp(uv, 0.0, 1.0));
	vec3 col = src.rgb;

	float ghostAmount = clamp(ghost, 0.0, 1.0);
	if (ghostAmount > 0.0)
	{
		// ADDED, not mixed: a reflection arrives on top of the direct signal,
		// it does not replace part of it. Mixing dims the picture as you add
		// the ghost, which is backwards.
		vec2 echoUv = vec2(uv.x - mapr(ghost_delay, 0.005, 0.12), uv.y);
		vec3 echo = texture(textura1, clamp(echoUv, 0.0, 1.0)).rgb;
		float onScreen = step(0.0, echoUv.x);
		col += echo * ghostAmount * 0.6 * onScreen;
	}

	float humAmount = mapr(hum, 0.0, 0.5);
	if (humAmount > 0.0)
	{
		// Two cycles down the frame, drifting: mains and frame rate are never
		// quite locked, which is why a hum bar always creeps.
		col *= 1.0 + sin((screenY + time * 0.15) * TWO_PI * 2.0) * humAmount;
	}

	float dropoutAmount = clamp(dropout, 0.0, 1.0);
	if (dropoutAmount > 0.0)
	{
		// Short dashes, not full lines: a dropout is a few hundred microseconds
		// of missing signal, so it clears well before the line ends.
		float line = floor(screenY * resolution.y);
		float slot = floor(time * 24.0);
		float dash = floor(uv.x * 28.0);
		float hit = step(1.0 - dropoutAmount * 0.06,
			faultHash(vec2(line * 7.0 + dash, slot)));
		col = mix(col, vec3(1.0), hit);
	}

	// The bands that displaced the sample also carry their own noise: a
	// misaligned head reads tape, but not the picture on it.
	float noiseBand = max(trackingBand, switchBand);
	if (noiseBand > 0.0)
	{
		float grain = faultHash(gl_FragCoord.xy + floor(time * 30.0));
		col = mix(col, vec3(grain), noiseBand * 0.55);
	}

	fragColor = vec4(col * bar, src.a);
}
