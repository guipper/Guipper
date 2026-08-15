#pragma include "../common.frag"

// Transition: warp.
//
// Same uniform contract as mix.frag - textura1, textura2, mixst, resolution -
// so TransitionSR can swap between them with no call-site change.
//
// The outgoing frame is pushed OUTWARD from the centre while the incoming one
// arrives from further out and settles. Both are displaced through the SAME
// flow field, which is what makes the two look like one continuous movement
// rather than two clips dissolving - the MilkDrop trick.
uniform float mixst;
uniform sampler2D textura1;
uniform sampler2D textura2;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 centred = uv - vec2(0.5);

	// Peaks in the middle of the transition and returns to zero at both ends,
	// so the first and last frames are exactly the untouched sources.
	float bulge = sin(mixst * 3.14159265);

	// A slow rotational drift keeps the displacement from reading as a plain
	// zoom. Small: this is a transition, not an effect.
	float swirl = bulge * 0.35 * length(centred);
	float cosA = cos(swirl);
	float sinA = sin(swirl);
	mat2 rot = mat2(cosA, -sinA, sinA, cosA);

	// Outgoing expands as it leaves; incoming contracts into place. The
	// opposite signs are what makes them appear to flow through each other.
	vec2 outUv = vec2(0.5) + rot * centred * (1.0 - bulge * 0.22);
	vec2 inUv  = vec2(0.5) + rot * centred * (1.0 + bulge * 0.28);

	vec4 t1 = texture(textura1, outUv);
	vec4 t2 = texture(textura2, inUv);

	// Smoothstep on the blend as well, so the crossfade lags slightly behind
	// the displacement and the warp is visible rather than washed out.
	float blend = smoothstep(0.0, 1.0, mixst);
	fragColor = mix(t1, t2, blend);
}
