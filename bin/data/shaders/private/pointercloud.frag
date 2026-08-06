#version 330

// PointerCloud - point shading.
// Points are drawn as round sprites with a soft edge; colour comes either
// from the connected source or from a depth-driven palette.

uniform float colorCycle;
uniform float brightness;

in float vDepthNorm;
in vec3 vColor;
in float vHasColor;

out vec4 fragColor;

const float TAU = 6.28318530718;

void main()
{
	vec2 offset = gl_PointCoord * 2.0 - 1.0;
	float radius = length(offset);
	if (radius > 1.0) discard;

	vec3 palette = 0.55 + 0.45 * cos(
		TAU * (vDepthNorm + colorCycle + vec3(0.0, 0.33, 0.67)));
	vec3 color = mix(palette, vColor, vHasColor);

	// Near points read brighter, which gives the cloud some depth cueing even
	// when it is flattened towards orthographic.
	color *= brightness * mix(1.25, 0.55, vDepthNorm);

	float edge = 1.0 - smoothstep(0.75, 1.0, radius);
	fragColor = vec4(color, edge);
}
