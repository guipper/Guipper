#pragma include "../common.frag"

// DEPTH PARALLAX - a flat picture turned into a moving 3D scene.
//
// Two inputs from the same device: `color` (COLOR stream) and `depth`
// (DEPTH stream, near = bright). Each pixel is shifted by how far its surface
// is from the `focus` distance, so the camera seems to orbit the room: near
// things swing one way, the background the other - the "wiggle stereo" look.
//
// For the iPhone the two streams cover the same field of view; turn on
// `stretch` in both boxes so they also cover the same pixels.
//
// Occlusion is resolved by searching from the nearest layer back, so a person
// passes IN FRONT of the wall instead of smearing into it.

uniform sampler2D color;
uniform sampler2D depth;
// Strength of the shift.
uniform float amount = 0.4;
uniform float speed = 0.3;
// The distance that stays still, 0 far .. 1 near.
uniform float focus = 0.5;
// 0 side to side, 1 a full circle.
uniform float orbit = 1.0;
// Cuts the scene into flat cards, like a pop-up book. 0 smooth.
uniform float layers = 0.0;
// Fades the background into black.
uniform float fog = 0.0;

const int STEPS = 32;

float sceneDepth(vec2 uv)
{
	float d = texture(depth, clamp(uv, 0.0, 1.0)).r;
	if (layers > 0.0)
	{
		float cards = mix(12.0, 3.0, layers);
		d = floor(d * cards + 0.5) / cards;
	}
	return d;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float t = time * mix(0.0, 4.0, speed);
	vec2 shift = vec2(cos(t), sin(t) * orbit) * mix(0.0, 0.15, amount);
	shift.x *= resolution.y / resolution.x;

	// Find the surface that lands on this pixel: a surface at depth z seen
	// from the shifted camera came from uv - shift * (z - focus).
	vec2 source = uv + shift * focus;
	float found = 0.0;
	for (int i = 0; i <= STEPS; ++i)
	{
		float z = 1.0 - float(i) / float(STEPS);
		vec2 candidate = uv - shift * (z - focus);
		if (sceneDepth(candidate) >= z - 1.0 / float(STEPS))
		{
			source = candidate;
			found = z;
			break;
		}
	}

	vec3 rgb = texture(color, clamp(source, 0.0, 1.0)).rgb;
	rgb *= mix(1.0, smoothstep(0.0, 0.6, found), fog);
	fragColor = vec4(rgb, 1.0);
}
