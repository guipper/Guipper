#pragma include "../common.frag"

// Minimal calibration source for the corner-pin mapping shader.
uniform float square_size = 0.55;
uniform float square_center_x = 0.5;
uniform float square_center_y = 0.5;
uniform float edge_width = 0.018;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 halfSize = vec2(square_size * 0.5);
	vec2 squareMin = vec2(square_center_x, square_center_y) - halfSize;
	vec2 squareMax = vec2(square_center_x, square_center_y) + halfSize;
	vec2 inside = step(squareMin, uv) * step(uv, squareMax);
	float square = inside.x * inside.y;

	vec2 edge = step(squareMin, uv) * step(uv, squareMax);
	float top = step(uv.y, squareMin.y + edge_width) * edge.x * edge.y;
	float bottom = step(squareMax.y - edge_width, uv.y) * edge.x * edge.y;
	float left = step(squareMin.x, uv.x) *
		step(uv.x, squareMin.x + edge_width) * edge.x * edge.y;
	float right = step(squareMax.x - edge_width, uv.x) *
		step(uv.x, squareMax.x) * edge.x * edge.y;

	vec3 color = vec3(square);
	color = mix(color, vec3(1.0, 0.15, 0.1), top);
	color = mix(color, vec3(0.15, 1.0, 0.25), left);
	color = mix(color, vec3(0.15, 0.55, 1.0), right);
	color = mix(color, vec3(1.0, 0.85, 0.1), bottom);
	fragColor = vec4(color, 1.0);
}
