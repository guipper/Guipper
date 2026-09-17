#pragma include "../common.frag"

// SHAPE MATTE - a soft rectangle or ellipse, cut out of whatever comes in.
//
// This is the "apply the effect only to one area" box. Patch it AFTER the
// effect and send both the untouched composition and this to the FINAL panel:
// the effect now only has alpha inside the shape, so it lands as a patch on top
// of the clean image. No blending box, no chroma key holding the two together.
//
// It only writes alpha, never rgb, for the same reason lumakey does not: a
// matte that darkens its own edge cannot be undone further down the chain.

uniform sampler2D textura1;
uniform float center_x = 0.5;
uniform float center_y = 0.5;
uniform float size_x = 0.4;
uniform float size_y = 0.4;
// Soft edge, as a fraction of the shorter side. 0 is a hard cut and will
// alias on a rotated rectangle.
uniform float feather = 0.05;
uniform float rotation = 0.5;
// Rectangle by default; the toggle makes it an ellipse.
uniform bool ellipse;
// Keeps everything OUTSIDE the shape instead - a hole rather than a patch.
uniform bool invert;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 src = texture(textura1, uv);

	// Corrected for the frame's aspect so a "square" is square and a rotation
	// does not shear. Everything below happens in this space.
	float aspect = resolution.x / max(resolution.y, 1.0);
	vec2 p = (uv - vec2(center_x, center_y)) * vec2(aspect, 1.0);
	p = rotate2d(mapr(rotation, -PI, PI)) * p;

	vec2 half_size = max(vec2(mapr(size_x, 0.0, 1.0) * aspect,
		mapr(size_y, 0.0, 1.0)) * 0.5, vec2(0.0001));
	float soft = max(mapr(feather, 0.0, 0.5), 0.0001);

	float shape;
	if (ellipse)
	{
		// Distance in units of the radius, so one smoothstep covers both axes
		// even when the ellipse is far from round.
		float d = length(p / half_size);
		shape = 1.0 - smoothstep(1.0 - soft, 1.0 + soft, d);
	}
	else
	{
		// Feathered per axis and multiplied: doing it on the box distance
		// instead rounds the corners, which is a rounded rectangle, not a
		// feathered one.
		vec2 edge = 1.0 - smoothstep(half_size - soft * half_size,
			half_size + soft * half_size, abs(p));
		shape = edge.x * edge.y;
	}
	if (invert) shape = 1.0 - shape;

	fragColor = vec4(src.rgb, src.a * clamp(shape, 0.0, 1.0));
}
