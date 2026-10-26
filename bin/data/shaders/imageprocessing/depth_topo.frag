#pragma include "../common.frag"

// DEPTH TOPO - contour lines through a depth image.
//
// Feed it the DEPTH stream of a Kinect or iPhone LiDAR box (near = bright,
// black = no reading). Every line is one distance from the sensor, so they
// wrap around a body like a topographic map and, with speed, travel through
// it front to back. Works in the dark: it never looks at colour.
//
// Plain grey depth has only 256 steps, which arrive as wide flat terraces; turn
// on "16 bit depth" in the iPhone box for smooth lines. A blur about one sensor
// pixel wide (the iPhone gives 256 across) also softens what is left.

uniform sampler2D depth;
// Number of lines across the near..far range of the depth box, 4 to 60.
uniform float bands = 0.35;
// Lines moving through the volume. 0 still.
uniform float speed = 0.25;
uniform float thickness = 0.3;
// Palette position and how much it changes from near to far.
uniform float hue = 0.55;
uniform float hue_spread = 0.6;
uniform float glow = 0.5;
// Flat colour between the lines, so the figure reads as a solid.
uniform float fill = 0.12;
// Previous frames left behind as the lines move. 0 none.
uniform float trails = 0.0;

// Reads the plain grey of a DEPTH box and also its "16 bit depth" output, where
// red is the coarse value and green the fraction between two greys. On plain
// grey, green equals red and adds at most 1/255.
float depthAt(vec2 uv)
{
	vec4 c = texture(depth, uv);
	return min(1.0, c.r + c.g / 255.0);
}

float smoothedDepth(vec2 uv)
{
	vec2 texel = vec2(1.0 / 256.0);
	float sum = 0.0;
	float weight = 0.0;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			float d = depthAt(uv + vec2(x, y) * texel);
			// Holes are absence, not "very far": averaging them in would pull
			// every silhouette edge towards the back.
			if (d > 0.004) { sum += d; weight += 1.0; }
		}
	}
	return weight > 0.0 ? sum / weight : 0.0;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec3 previous = texture(feedback, uv).rgb * trails * 0.97;
	float d = smoothedDepth(uv);
	float phase = d * mix(4.0, 60.0, bands) - time * mix(0.0, 3.0, speed);
	// In band units, not pixels: on terraced input the screen-space slope is
	// zero on every terrace and the lines would vanish.
	float width = mix(0.02, 0.2, thickness);
	if (depthAt(uv) <= 0.004 || d <= 0.0)
	{
		fragColor = vec4(previous, 1.0);
		return;
	}

	float f = fract(phase);
	float toLine = min(f, 1.0 - f);
	float line = 1.0 - smoothstep(0.0, width, toLine);
	float halo = exp(-toLine / (width * mix(1.0, 8.0, glow))) * glow;

	vec3 tint = 0.5 + 0.5 * cos(6.2831853 *
		(hue + hue_spread * d + vec3(0.0, 0.333333, 0.666667)));
	vec3 color = tint * (line + halo) + tint * fill * d;
	fragColor = vec4(max(color, previous), 1.0);
}
