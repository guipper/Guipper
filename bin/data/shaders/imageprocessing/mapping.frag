#pragma include "../common.frag"

uniform sampler2D textura1;

// Coordinates use a top-left origin and are normalized to the output surface.
uniform float top_left_x = 0.0;
uniform float top_left_y = 0.0;
uniform float top_right_x = 1.0;
uniform float top_right_y = 0.0;
uniform float bottom_right_x = 1.0;
uniform float bottom_right_y = 1.0;
uniform float bottom_left_x = 0.0;
uniform float bottom_left_y = 1.0;
uniform float feather = 0.0;

float cross2(vec2 a, vec2 b)
{
	return a.x * b.y - a.y * b.x;
}

bool isConvexQuad(vec2 topLeft, vec2 topRight, vec2 bottomRight, vec2 bottomLeft)
{
	float c0 = cross2(topRight - topLeft, bottomRight - topRight);
	float c1 = cross2(bottomRight - topRight, bottomLeft - bottomRight);
	float c2 = cross2(bottomLeft - bottomRight, topLeft - bottomLeft);
	float c3 = cross2(topLeft - bottomLeft, topRight - topLeft);
	float minCross = min(min(c0, c1), min(c2, c3));
	float maxCross = max(max(c0, c1), max(c2, c3));
	return minCross > 0.00001 || maxCross < -0.00001;
}

float solveBilinearV(vec2 point, vec2 b, vec2 c, vec2 d, float u)
{
	vec2 denominator = c + d * u;
	if (abs(denominator.x) > abs(denominator.y) &&
		abs(denominator.x) > 0.00001)
	{
		return (point.x - b.x * u) / denominator.x;
	}
	if (abs(denominator.y) > 0.00001)
	{
		return (point.y - b.y * u) / denominator.y;
	}
	return -1.0;
}

void considerBilinearCandidate(float u, vec2 point, vec2 b, vec2 c,
	vec2 d, inout vec2 best, inout float bestError)
{
	if (u < -0.00001 || u > 1.00001)
	{
		return;
	}

	float v = solveBilinearV(point, b, c, d, u);
	if (v < -0.00001 || v > 1.00001)
	{
		return;
	}

	vec2 mapped = b * u + c * v + d * u * v;
	float error = length(mapped - point);
	if (error < bestError)
	{
		best = vec2(clamp(u, 0.0, 1.0), clamp(v, 0.0, 1.0));
		bestError = error;
	}
}

vec2 inverseBilinear(vec2 point, vec2 topLeft, vec2 topRight,
	vec2 bottomRight, vec2 bottomLeft, out bool valid)
{
	vec2 b = topRight - topLeft;
	vec2 c = bottomLeft - topLeft;
	vec2 d = topLeft - topRight + bottomRight - bottomLeft;
	vec2 offset = point - topLeft;

	float quadraticA = cross2(b, d);
	float quadraticB = cross2(b, c) - cross2(offset, d);
	float quadraticC = -cross2(offset, c);
	vec2 best = vec2(-1.0);
	float bestError = 1000000.0;

	if (abs(quadraticA) <= 0.00001)
	{
		if (abs(quadraticB) > 0.00001)
		{
			considerBilinearCandidate(-quadraticC / quadraticB,
				offset, b, c, d, best, bestError);
		}
	}
	else
	{
		float discriminant = quadraticB * quadraticB -
			4.0 * quadraticA * quadraticC;
		if (discriminant >= 0.0)
		{
			float root = sqrt(discriminant);
			considerBilinearCandidate(
				(-quadraticB - root) / (2.0 * quadraticA),
				offset, b, c, d, best, bestError);
			considerBilinearCandidate(
				(-quadraticB + root) / (2.0 * quadraticA),
				offset, b, c, d, best, bestError);
		}
	}

	valid = best.x >= 0.0 && bestError <= 0.0005;
	return best;
}

void main()
{
	// The inspector, mapping overlay, and Guipper FBO path share the same
	// normalized screen-space convention. Keep the corner order unchanged so
	// the shader and the draggable overlay describe the same surface.
	vec2 topLeft = vec2(top_left_x, top_left_y);
	vec2 topRight = vec2(top_right_x, top_right_y);
	vec2 bottomRight = vec2(bottom_right_x, bottom_right_y);
	vec2 bottomLeft = vec2(bottom_left_x, bottom_left_y);

	bool validQuad = isConvexQuad(topLeft, topRight, bottomRight, bottomLeft);
	if (!validQuad)
	{
		fragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	vec2 outputUv = gl_FragCoord.xy / resolution;
	bool sourceValid = false;
	vec2 sourceUv = inverseBilinear(outputUv, topLeft, topRight,
		bottomRight, bottomLeft, sourceValid);
	if (!sourceValid)
	{
		fragColor = vec4(0.0, 0.0, 0.0, 1.0);
		return;
	}

	float edgeDistance = min(min(sourceUv.x, 1.0 - sourceUv.x),
		min(sourceUv.y, 1.0 - sourceUv.y));
	float featherWidth = feather * 0.25;
	float mask = featherWidth > 0.00001 ?
		smoothstep(0.0, featherWidth, edgeDistance) : 1.0;

	vec4 sourceColor = texture(textura1, sourceUv);
	fragColor = vec4(sourceColor.rgb * mask, 1.0);
}
