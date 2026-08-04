#pragma include "../common.frag"

// Projection mapping: four layers, each warped through a 12 control point Coons
// patch and masked with a bezier path baked into an R8 FBO by the C++ side.
//
// Coordinate spaces, all of which turn out to be the same one - do not "fix"
// this by adding a y flip, that was tried and it mirrors the output:
//   outputUv  - gl_FragCoord.xy / resolution. Already a TOP LEFT ORIGIN, y DOWN
//               display coordinate. openFrameworks suppresses its own y flip
//               while rendering into an FBO and does not mark FBO textures as
//               flipped when drawing them, so fragment row 0 is the row that
//               ends up at the TOP of the drawn rectangle.
//   displayUv - editor space, y DOWN: what layerN_surface and layerN_bounds are
//               expressed in, corner 0 being the top left handle in the panel.
//               Identical to outputUv; kept as a name so the intent is legible.
//   sourceUv  - Coons patch parameter, u along the top edge, v DOWN the left
//               edge - so it indexes the input texture directly, no flip.
// mapping.frag follows the same convention and mapping_debug.frag labels low
// gl_FragCoord.y as "top", both of which confirm it.

uniform sampler2D textura1;
uniform sampler2D textura2;
uniform sampler2D textura3;
uniform sampler2D textura4;

uniform float layer1_opacity = 1.0;
uniform float layer1_feather = 0.0;
uniform float layer2_opacity = 1.0;
uniform float layer2_feather = 0.0;
uniform float layer3_opacity = 1.0;
uniform float layer3_feather = 0.0;
uniform float layer4_opacity = 1.0;
uniform float layer4_feather = 0.0;

uniform sampler2D layer1_mask; // @internal
uniform sampler2D layer2_mask; // @internal
uniform sampler2D layer3_mask; // @internal
uniform sampler2D layer4_mask; // @internal
uniform int layer1_connected;
uniform int layer2_connected;
uniform int layer3_connected;
uniform int layer4_connected;
// 0 stretch, 1 contain, 2 cover. Ints on purpose: the uniform parser ignores
// them, so these add no inspector slider and cannot shift parameter order.
uniform int layer1_fit;
uniform int layer2_fit;
uniform int layer3_fit;
uniform int layer4_fit;
uniform vec2 layer1_surface[12];
uniform vec2 layer2_surface[12];
uniform vec2 layer3_surface[12];
uniform vec2 layer4_surface[12];
uniform vec4 layer1_bounds;
uniform vec4 layer2_bounds;
uniform vec4 layer3_bounds;
uniform vec4 layer4_bounds;

// Widest soft edge, as a fraction of the shortest output side.
#define FEATHER_MAX 0.12
// The 17 tap mask kernel undersamples past this radius, so the mask edge stops
// getting softer here while the surface edge keeps using the full range.
#define MASK_FEATHER_MAX_PIXELS 6.0

float cross2(vec2 a, vec2 b)
{
	return a.x * b.y - a.y * b.x;
}

vec2 cubicPoint(vec2 a, vec2 b, vec2 c, vec2 d, float t)
{
	float s = 1.0 - t;
	return a * s * s * s + b * 3.0 * s * s * t +
		c * 3.0 * s * t * t + d * t * t * t;
}

vec2 cubicDerivative(vec2 a, vec2 b, vec2 c, vec2 d, float t)
{
	float s = 1.0 - t;
	return (b - a) * 3.0 * s * s +
		(c - b) * 6.0 * s * t + (d - c) * 3.0 * t * t;
}

// Analytic inverse of the corner quad, same routines as mapping.frag. This is
// only a rescue seed for the Newton solve below, never the first choice: on a
// patch with bulged handles the corner quad is a poor description of the real
// surface, and seeding from it makes the solve converge to the wrong branch and
// drop around 10% of the patch. Measured, not assumed.

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
	vec2 bottomRight, vec2 bottomLeft)
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

	// best.x stays negative when the quad has no root under this point.
	return best;
}

void evaluateSurface(vec2 uv, vec2 points[12], out vec2 position,
	out vec2 derivativeU, out vec2 derivativeV)
{
	vec2 top = cubicPoint(points[0], points[4], points[5], points[1], uv.x);
	vec2 bottom = cubicPoint(points[3], points[8], points[9], points[2], uv.x);
	vec2 left = cubicPoint(points[0], points[10], points[11], points[3], uv.y);
	vec2 right = cubicPoint(points[1], points[6], points[7], points[2], uv.y);
	vec2 topDerivative = cubicDerivative(
		points[0], points[4], points[5], points[1], uv.x);
	vec2 bottomDerivative = cubicDerivative(
		points[3], points[8], points[9], points[2], uv.x);
	vec2 leftDerivative = cubicDerivative(
		points[0], points[10], points[11], points[3], uv.y);
	vec2 rightDerivative = cubicDerivative(
		points[1], points[6], points[7], points[2], uv.y);

	vec2 bilinearTop = mix(points[0], points[1], uv.x);
	vec2 bilinearBottom = mix(points[3], points[2], uv.x);
	vec2 bilinear = mix(bilinearTop, bilinearBottom, uv.y);
	vec2 bilinearDerivativeU = mix(
		points[1] - points[0], points[2] - points[3], uv.y);
	vec2 bilinearDerivativeV = mix(
		points[3] - points[0], points[2] - points[1], uv.x);

	position = mix(top, bottom, uv.y) + mix(left, right, uv.x) - bilinear;
	derivativeU = mix(topDerivative, bottomDerivative, uv.y) +
		right - left - bilinearDerivativeU;
	derivativeV = bottom - top + mix(leftDerivative, rightDerivative, uv.x) -
		bilinearDerivativeV;
}

// One Newton solve from a given seed. Reports the parameter along with the
// Jacobian evaluated AT that parameter, so the caller gets antialiasing
// gradients for free and no extra evaluateSurface call is needed. Convergence
// and acceptance are both in PIXELS, so behaviour does not drift with the
// render resolution the way an absolute uv tolerance does.
bool solveSurface(vec2 seed, vec2 displayPoint, vec2 points[12],
	out vec2 sourceUv, out vec2 derivativeU, out vec2 derivativeV)
{
	vec2 uv = seed;
	vec2 position = vec2(0.0);
	float residual = 1000000.0;
	sourceUv = seed;
	derivativeU = vec2(1.0, 0.0);
	derivativeV = vec2(0.0, 1.0);
	for (int iteration = 0; iteration < 8; iteration++)
	{
		evaluateSurface(uv, points, position, derivativeU, derivativeV);
		sourceUv = uv;
		vec2 error = position - displayPoint;
		residual = length(error * resolution);
		if (residual < 0.25)
			break;
		float determinant = cross2(derivativeU, derivativeV);
		if (abs(determinant) < 0.000001)
			break;
		uv -= vec2(
			cross2(error, derivativeV),
			cross2(derivativeU, error)) / determinant;
	}
	return residual < 0.5;
}

// Seeding order matters more than iteration count here. The bounding box guess
// is a good global estimate for bulged patches, so it goes first; the analytic
// corner quad inverse only gets a turn when that fails, where it recovers about
// a third of the remaining dropouts. Both orders were measured against a dense
// forward sampling of the patch.
bool inverseSurface(vec2 displayPoint, vec2 points[12], vec4 bounds,
	out vec2 sourceUv, out vec2 derivativeU, out vec2 derivativeV)
{
	vec2 extent = max(bounds.zw - bounds.xy, vec2(0.0001));
	vec2 boundsSeed = clamp((displayPoint - bounds.xy) / extent, 0.0, 1.0);
	if (solveSurface(boundsSeed, displayPoint, points, sourceUv,
			derivativeU, derivativeV))
		return true;

	vec2 rescue = inverseBilinear(displayPoint, points[0], points[1],
		points[2], points[3]);
	if (rescue.x < 0.0)
		return false;
	return solveSurface(rescue, displayPoint, points, sourceUv,
		derivativeU, derivativeV);
}

// Pixel coverage of the patch edge, from the Jacobian rather than fwidth: the
// neighbouring fragments across the boundary are rejected or land on another
// branch, so their derivatives are useless exactly where antialiasing matters.
float surfaceCoverage(vec2 sourceUv, vec2 derivativeU, vec2 derivativeV,
	float featherPixels)
{
	vec2 pixelU = derivativeU * resolution;
	vec2 pixelV = derivativeV * resolution;
	float determinant = abs(cross2(pixelU, pixelV));
	if (determinant < 0.000001)
		return 0.0;
	float gradientU = max(length(pixelV) / determinant, 0.000001);
	float gradientV = max(length(pixelU) / determinant, 0.000001);
	float distanceU = min(sourceUv.x, 1.0 - sourceUv.x) / gradientU;
	float distanceV = min(sourceUv.y, 1.0 - sourceUv.y) / gradientV;
	float edgePixels = min(distanceU, distanceV);
	// 0.45 rather than 0.5 so the first interior pixel saturates at exactly 1.0
	// and an untouched patch stays a bit exact pass through.
	float halfWidth = 0.45 + featherPixels;
	return clamp(edgePixels / (2.0 * halfWidth) + 0.5, 0.0, 1.0);
}

// 3x3 tent with the offsets in texels, so the footprint is square in pixels on
// any aspect ratio. Weights sum to 16 and divide by 16: a uniformly white mask
// comes back as exactly 1.0.
float tentSample(sampler2D maskTexture, vec2 outputUv, vec2 offset)
{
	float total = texture(maskTexture, outputUv).r * 4.0;
	total += (texture(maskTexture, outputUv + vec2(offset.x, 0.0)).r +
		texture(maskTexture, outputUv - vec2(offset.x, 0.0)).r +
		texture(maskTexture, outputUv + vec2(0.0, offset.y)).r +
		texture(maskTexture, outputUv - vec2(0.0, offset.y)).r) * 2.0;
	total += texture(maskTexture, outputUv + offset).r +
		texture(maskTexture, outputUv - offset).r +
		texture(maskTexture, outputUv + vec2(offset.x, -offset.y)).r +
		texture(maskTexture, outputUv + vec2(-offset.x, offset.y)).r;
	return total / 16.0;
}

// Two nested tents. Symmetric about the path, so the feather fades outward as
// well as inward instead of only eroding the inside.
float samplePathMask(sampler2D maskTexture, vec2 outputUv,
	float featherPixels)
{
	vec2 texel = 1.0 / max(vec2(textureSize(maskTexture, 0)), vec2(1.0));
	float radius = clamp(featherPixels, 1.0, MASK_FEATHER_MAX_PIXELS);
	vec2 offset = texel * radius * 0.5;
	float outer = tentSample(maskTexture, outputUv, offset);
	float inner = tentSample(maskTexture, outputUv, offset * 0.5);
	return mix(outer, inner, 0.5);
}

// The quad's proportions in output pixels, averaging opposite edges so a
// trapezoid still reports something sensible. Chord lengths, not arc lengths:
// a bent edge is not meant to stretch the artwork along it.
float surfaceAspect(vec2 points[12])
{
	float width = 0.5 * (length((points[1] - points[0]) * resolution) +
		length((points[2] - points[3]) * resolution));
	float height = 0.5 * (length((points[3] - points[0]) * resolution) +
		length((points[2] - points[1]) * resolution));
	return height > 0.0001 ? width / max(height, 0.0001) : 1.0;
}

// Rescales the patch parameter about its centre so the source keeps its own
// proportions inside the quad. Contain widens the sampled range past 0..1 so the
// letterbox falls outside the image; cover narrows it so the overflow is
// cropped. Stretch returns the parameter untouched, which is what every
// composition authored before these modes expects.
vec2 applySourceFit(vec2 sourceUv, vec2 points[12], int fitMode,
	vec2 sourceSize)
{
	if (fitMode == 0)
		return sourceUv;
	float ratio = surfaceAspect(points) /
		max(sourceSize.x / max(sourceSize.y, 1.0), 0.0001);
	vec2 scale = fitMode == 1 ?
		vec2(max(ratio, 1.0), max(1.0 / ratio, 1.0)) :
		vec2(min(ratio, 1.0), min(1.0 / ratio, 1.0));
	return (sourceUv - 0.5) * scale + 0.5;
}

vec4 renderLayer(sampler2D sourceTexture, sampler2D maskTexture,
	vec2 points[12], vec4 bounds, float opacity, float feather,
	int fitMode, vec2 outputUv)
{
	// Editor space and output space are the same space; see the file header.
	vec2 displayUv = outputUv;
	float featherPixels = mapr(clamp(feather, 0.0, 1.0), 0.0, FEATHER_MAX) *
		min(resolution.x, resolution.y);

	// Everything outside the control hull is empty, and the hull is inside the
	// uploaded box. Skipping the solve here is most of the cost of this shader.
	vec2 margin = (featherPixels + 2.0) / resolution;
	if (any(lessThan(displayUv, bounds.xy - margin)) ||
		any(greaterThan(displayUv, bounds.zw + margin)))
		return vec4(0.0);

	vec2 sourceUv = vec2(0.0);
	vec2 derivativeU = vec2(0.0);
	vec2 derivativeV = vec2(0.0);
	if (!inverseSurface(displayUv, points, bounds, sourceUv,
			derivativeU, derivativeV))
		return vec4(0.0);

	float surfaceMask = surfaceCoverage(sourceUv, derivativeU,
		derivativeV, featherPixels);
	if (surfaceMask <= 0.0)
		return vec4(0.0);

	float pathMask = samplePathMask(maskTexture, outputUv, featherPixels);
	vec2 fitted = applySourceFit(sourceUv, points, fitMode,
		vec2(textureSize(sourceTexture, 0)));
	// Contain leaves the letterbox outside 0..1, and there is no artwork there -
	// clamping would smear the edge row across it instead.
	if (fitMode != 0 && (any(lessThan(fitted, vec2(0.0))) ||
		any(greaterThan(fitted, vec2(1.0)))))
		return vec4(0.0);
	vec2 clamped = clamp(fitted, 0.0, 1.0);
	vec4 source = texture(sourceTexture, clamped);
	float alpha = clamp(opacity, 0.0, 1.0) * surfaceMask *
		pathMask * source.a;
	return vec4(source.rgb, alpha);
}

void compositeLayer(inout vec3 color, vec4 layer)
{
	color = mix(color, layer.rgb, clamp(layer.a, 0.0, 1.0));
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec3 color = vec3(0.0);
	// Back to front, so TEXTURE 1 is the top layer and TEXTURE 4 the bottom -
	// the same order the T1..T4 buttons read in.
	if (layer4_connected == 1 && layer4_opacity > 0.0001)
		compositeLayer(color, renderLayer(textura4, layer4_mask,
			layer4_surface, layer4_bounds, layer4_opacity,
			layer4_feather, layer4_fit, uv));
	if (layer3_connected == 1 && layer3_opacity > 0.0001)
		compositeLayer(color, renderLayer(textura3, layer3_mask,
			layer3_surface, layer3_bounds, layer3_opacity,
			layer3_feather, layer3_fit, uv));
	if (layer2_connected == 1 && layer2_opacity > 0.0001)
		compositeLayer(color, renderLayer(textura2, layer2_mask,
			layer2_surface, layer2_bounds, layer2_opacity,
			layer2_feather, layer2_fit, uv));
	if (layer1_connected == 1 && layer1_opacity > 0.0001)
		compositeLayer(color, renderLayer(textura1, layer1_mask,
			layer1_surface, layer1_bounds, layer1_opacity,
			layer1_feather, layer1_fit, uv));
	fragColor = vec4(color, 1.0);
}
