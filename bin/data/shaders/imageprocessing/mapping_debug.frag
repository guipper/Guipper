#pragma include "../common.frag"

// Calibration source for projection mapping and multi-screen wall alignment.
// Boolean uniforms appear as inspector toggles. The original orientation
// square remains visible when every option is off.
uniform bool show_grid;
uniform bool show_square_grid;
uniform bool show_crosshair;
uniform bool show_safe_areas;
uniform bool show_circles;
uniform bool show_color_bars;

uniform float square_size = 0.55;
uniform float square_center_x = 0.5;
uniform float square_center_y = 0.5;
uniform float edge_width = 0.018;
uniform float grid_density = 0.28;
uniform float line_width = 0.18;
uniform float background_level = 0.025;

float lineMask(float coordinate, float spacing, float halfWidth)
{
	float distanceToLine = abs(mod(coordinate + spacing * 0.5, spacing) -
		spacing * 0.5);
	return 1.0 - smoothstep(halfWidth, halfWidth + 1.0, distanceToLine);
}

float rectangleMask(vec2 uv, vec2 minimum, vec2 maximum)
{
	vec2 inside = step(minimum, uv) * step(uv, maximum);
	return inside.x * inside.y;
}

float rectangleOutlinePixels(vec2 pixel, vec2 minimum, vec2 maximum,
	float halfWidth)
{
	float inside = rectangleMask(pixel, minimum, maximum);
	vec2 edgeDistance = min(pixel - minimum, maximum - pixel);
	float nearestEdge = min(edgeDistance.x, edgeDistance.y);
	return inside * (1.0 - smoothstep(halfWidth, halfWidth + 1.0, nearestEdge));
}

vec3 colorBar(float x)
{
	if (x < 1.0 / 7.0) return vec3(0.75);
	if (x < 2.0 / 7.0) return vec3(0.75, 0.75, 0.0);
	if (x < 3.0 / 7.0) return vec3(0.0, 0.75, 0.75);
	if (x < 4.0 / 7.0) return vec3(0.0, 0.75, 0.0);
	if (x < 5.0 / 7.0) return vec3(0.75, 0.0, 0.75);
	if (x < 6.0 / 7.0) return vec3(0.75, 0.0, 0.0);
	return vec3(0.0, 0.0, 0.75);
}

void main()
{
	vec2 pixel = gl_FragCoord.xy;
	vec2 uv = pixel / resolution;
	float shortestSide = max(1.0, min(resolution.x, resolution.y));
	float stroke = mix(0.75, 4.0, line_width);
	float divisions = floor(mix(4.0, 40.0, grid_density) + 0.5);
	vec3 color = vec3(background_level);

	// Original orientation square. The four edge colors identify rotation and
	// mirrored outputs immediately: top red, left green, right blue, bottom gold.
	vec2 halfSize = vec2(square_size * 0.5);
	vec2 squareMin = vec2(square_center_x, square_center_y) - halfSize;
	vec2 squareMax = vec2(square_center_x, square_center_y) + halfSize;
	float square = rectangleMask(uv, squareMin, squareMax);
	color = mix(color, vec3(0.82), square);

	float top = rectangleMask(uv, squareMin,
		vec2(squareMax.x, squareMin.y + edge_width));
	float bottom = rectangleMask(uv,
		vec2(squareMin.x, squareMax.y - edge_width), squareMax);
	float left = rectangleMask(uv, squareMin,
		vec2(squareMin.x + edge_width, squareMax.y));
	float right = rectangleMask(uv,
		vec2(squareMax.x - edge_width, squareMin.y), squareMax);
	color = mix(color, vec3(1.0, 0.12, 0.08), top);
	color = mix(color, vec3(0.10, 1.0, 0.24), left);
	color = mix(color, vec3(0.10, 0.45, 1.0), right);
	color = mix(color, vec3(1.0, 0.78, 0.06), bottom);

	if (show_grid)
	{
		// Equal UV divisions: useful for matching content regions and wall crops.
		vec2 spacing = resolution / divisions;
		float minorGrid = max(lineMask(pixel.x, spacing.x, stroke * 0.5),
			lineMask(pixel.y, spacing.y, stroke * 0.5));
		float majorGrid = max(lineMask(pixel.x, spacing.x * 5.0, stroke),
			lineMask(pixel.y, spacing.y * 5.0, stroke));
		color = mix(color, vec3(0.25), minorGrid * 0.72);
		color = mix(color, vec3(0.0, 0.78, 0.88), majorGrid * 0.92);
	}

	if (show_square_grid)
	{
		// One pixel spacing for both axes: cells remain physically square. A
		// stretched output turns these into rectangles and exposes the error.
		float spacing = shortestSide / divisions;
		vec2 centeredPixel = pixel - resolution * 0.5;
		float squareGrid = max(lineMask(centeredPixel.x, spacing, stroke * 0.65),
			lineMask(centeredPixel.y, spacing, stroke * 0.65));
		float squareMajor = max(
			lineMask(centeredPixel.x, spacing * 5.0, stroke * 1.15),
			lineMask(centeredPixel.y, spacing * 5.0, stroke * 1.15));
		color = mix(color, vec3(0.86), squareGrid * 0.68);
		color = mix(color, vec3(1.0, 0.48, 0.08), squareMajor * 0.95);
	}

	if (show_crosshair)
	{
		float crosshair = max(lineMask(pixel.x - resolution.x * 0.5,
			resolution.x * 2.0, stroke), lineMask(pixel.y - resolution.y * 0.5,
			resolution.y * 2.0, stroke));
		color = mix(color, vec3(1.0, 0.12, 0.72), crosshair);
	}

	if (show_safe_areas)
	{
		float frame5 = rectangleOutlinePixels(pixel, resolution * 0.05,
			resolution * 0.95, stroke);
		float frame10 = rectangleOutlinePixels(pixel, resolution * 0.10,
			resolution * 0.90, stroke);
		float border = rectangleOutlinePixels(pixel, vec2(0.0),
			resolution - vec2(1.0), stroke * 1.4);
		color = mix(color, vec3(1.0), frame5);
		color = mix(color, vec3(1.0, 0.72, 0.0), frame10);
		color = mix(color, vec3(0.0, 0.9, 0.95), border);
	}

	if (show_circles)
	{
		// Pixel-space circles expose non-uniform scaling and projector anamorphosis.
		float radius = length(pixel - resolution * 0.5);
		float circle1 = 1.0 - smoothstep(stroke, stroke + 1.0,
			abs(radius - shortestSide * 0.15));
		float circle2 = 1.0 - smoothstep(stroke, stroke + 1.0,
			abs(radius - shortestSide * 0.30));
		float circle3 = 1.0 - smoothstep(stroke, stroke + 1.0,
			abs(radius - shortestSide * 0.45));
		color = mix(color, vec3(0.30, 1.0, 0.55),
			max(circle1, max(circle2, circle3)));
	}

	if (show_color_bars && uv.y > 0.86)
	{
		color = colorBar(uv.x);
		float separator = 1.0 - smoothstep(1.0, 2.0,
			abs(pixel.y - resolution.y * 0.86));
		color = mix(color, vec3(1.0), separator);
	}

	fragColor = vec4(color, 1.0);
}
