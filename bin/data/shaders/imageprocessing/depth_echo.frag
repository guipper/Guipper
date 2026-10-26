#pragma include "../common.frag"

// DEPTH ECHO - copies of whoever is near the sensor, flying off into a tunnel.
//
// Feed it a DEPTH stream (near = bright). Everything closer than `threshold`
// is stamped as a coloured silhouette; every frame the previous picture is
// zoomed, twisted and faded, so each movement leaves a trail of echoes that
// changes colour as it goes. Depth, not brightness, picks the person, so it
// keeps working in a dark room and ignores lights and projections behind.

uniform sampler2D depth;
// Depth cut: higher keeps only what is nearer. Match it to the DEPTH box's
// near/far range.
uniform float threshold = 0.4;
// How long the echoes last.
uniform float decay = 0.6;
// 0.5 still; above flies outward, below falls inward.
uniform float zoom = 0.7;
// 0.5 none; either side spins one way or the other.
uniform float twist = 0.55;
// How fast new silhouettes change colour.
uniform float hue_speed = 0.3;
// 0 solid silhouettes, 1 only their outline.
uniform float outline = 0.0;
// Live figure brightness on top of the trails.
uniform float live = 0.8;

// Reads the plain grey of a DEPTH box and also its "16 bit depth" output, where
// red is the coarse value and green the fraction between two greys. On plain
// grey, green equals red and adds at most 1/255.
float depthAt(vec2 uv)
{
	vec4 c = texture(depth, uv);
	return min(1.0, c.r + c.g / 255.0);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 aspect = vec2(resolution.x / resolution.y, 1.0);

	// Where this pixel was last frame: scaled and rotated about the centre.
	vec2 p = (uv - 0.5) * aspect;
	float scale = 1.0 + (zoom - 0.5) * 0.08;
	float angle = (twist - 0.5) * 0.08;
	p = mat2(cos(angle), -sin(angle), sin(angle), cos(angle)) * p / scale;
	vec2 back = p / aspect + 0.5;
	vec3 previous = vec3(0.0);
	if (all(greaterThanEqual(back, vec2(0.0))) && all(lessThanEqual(back, vec2(1.0))))
	{
		// A slow hue drift on the way out gives the tunnel its colour bands.
		vec3 old = texture(feedback, back).rgb;
		previous = mix(old, old.gbr, 0.04) * mix(0.8, 0.995, decay);
	}

	float d = depthAt(uv);
	float figure = smoothstep(threshold - 0.015, threshold + 0.015, d);
	float edge = clamp(fwidth(figure) * 3.0, 0.0, 1.0);
	float stamp = mix(figure, edge, outline);

	vec3 tint = 0.5 + 0.5 * cos(6.2831853 *
		(time * mix(0.0, 0.5, hue_speed) + vec3(0.0, 0.333333, 0.666667)));
	vec3 color = max(previous, tint * stamp);
	// The figure itself, brighter and nearer-is-whiter, over its own trail.
	color = mix(color, mix(tint, vec3(1.0), d * 0.6), stamp * live * 0.6);
	fragColor = vec4(color, 1.0);
}
