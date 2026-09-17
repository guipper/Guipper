#pragma include "../common.frag"

// ALPHA OPS - boolean arithmetic on two mattes.
//
// The colour always comes from TEXTURA1; textura2 is read for its alpha only.
// That asymmetry is the point: it lets a shape from one branch cut into the
// picture of another without dragging that branch's pixels along.
//
// Chain two of these and you have the same union / intersect / subtract that
// the advanced mapping masks already offer, but on any box in the graph.

uniform sampler2D textura1;
uniform sampler2D textura2;
// Four zones on one knob, because a shader uniform can only be a 0..1 float
// here - an `int` is never turned into a control at all:
//   0.00 - 0.25  UNION       either matte
//   0.25 - 0.50  INTERSECT   both mattes
//   0.50 - 0.75  SUBTRACT    t1 minus t2
//   0.75 - 1.00  REPLACE     t2's matte, t1's colour
uniform float operation = 0.0;
// Blends back toward t1's own alpha, so the operation can be faded in.
uniform float amount = 1.0;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 base = texture(textura1, uv);
	float other = texture(textura2, uv).a;

	// 3.999 rather than 4.0: at exactly 1.0 a plain *4 lands on index 4 and
	// falls through every branch, leaving the top of the knob doing nothing.
	int op = int(clamp(operation, 0.0, 1.0) * 3.999);
	float result = base.a;
	if (op == 0) result = max(base.a, other);
	else if (op == 1) result = min(base.a, other);
	else if (op == 2) result = clamp(base.a - other, 0.0, 1.0);
	else result = other;

	fragColor = vec4(base.rgb,
		mix(base.a, result, clamp(amount, 0.0, 1.0)));
}
