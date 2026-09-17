#pragma include "../common.frag"

// HALFTONE - the newsprint dot screen.
//
// A grid of dots whose SIZE tracks the brightness underneath. The screen is
// rotated because an unrotated one lines up with the pixel grid and produces
// moire the moment the image scales; 15 degrees is roughly what a printer
// uses for the black plate, and for the same reason.
//
// Colour mode rotates each channel's screen to a different angle, which is
// exactly how CMYK printing avoids the three plates beating against each
// other into a rosette.

uniform sampler2D textura1;
// Dot pitch in pixels. Small enough and it disappears into the image; the
// useful range is bigger than you expect on a 1080p render.
uniform float dot_size = 0.25;
uniform float angle = 0.15;
// 0 is a hard printed dot, higher lets the edge fall off.
uniform float softness = 0.15;
// Per-channel screens at different angles instead of one on the luma.
uniform bool color;
// White dots on black rather than black on white.
uniform bool invert;

// Dot coverage at one point, for one screen angle. Returns 1 inside the dot.
float screenDot(vec2 pixel, float level, float pitch, float theta, float soft)
{
	vec2 rotated = mat2(cos(theta), -sin(theta), sin(theta), cos(theta)) * pixel;
	// Distance from the centre of this cell, in cell units.
	vec2 cell = fract(rotated / pitch) - 0.5;
	float d = length(cell) * 2.0;
	// sqrt because dot AREA is what reads as brightness, and area goes with the
	// square of the radius. Without it the midtones come out far too dark - the
	// classic mistake in every halftone shader on the internet.
	float radius = sqrt(clamp(level, 0.0, 1.0));
	return 1.0 - smoothstep(radius - soft, radius + soft, d);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 src = texture(textura1, uv);
	float pitch = max(mapr(dot_size, 2.0, 40.0), 1.0);
	float theta = mapr(angle, 0.0, PI * 0.5);
	float soft = max(mapr(softness, 0.0, 0.6), 0.001);
	vec2 pixel = gl_FragCoord.xy;

	vec3 col;
	if (color)
	{
		// 15 / 75 / 45 degrees apart, the standard plate angles.
		col = vec3(
			screenDot(pixel, src.r, pitch, theta, soft),
			screenDot(pixel, src.g, pitch, theta + PI / 3.0, soft),
			screenDot(pixel, src.b, pitch, theta + PI / 6.0, soft));
	}
	else
	{
		col = vec3(screenDot(pixel, dot(src.rgb, luminanceVector),
			pitch, theta, soft));
	}
	if (invert) col = 1.0 - col;

	fragColor = vec4(col, src.a);
}
