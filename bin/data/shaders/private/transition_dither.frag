#pragma include "../common.frag"

// Transition: ordered dither.
//
// Same uniform contract as mix.frag. Instead of blending the two frames, each
// pixel picks ONE of them: the incoming frame wins once the threshold for its
// screen position falls below the progress. No pixel is ever a mixture, which
// keeps both images at full contrast throughout - a plain crossfade washes the
// midpoint out to grey, and on a projector that reads as a dip in brightness.
uniform float mixst;
uniform sampler2D textura1;
uniform sampler2D textura2;

// 4x4 Bayer matrix, normalised to 0..1. Ordered rather than random so the
// pattern is stable frame to frame - random thresholds crawl and look like
// noise rather than a transition.
float bayer(vec2 pixel)
{
	int x = int(mod(pixel.x, 4.0));
	int y = int(mod(pixel.y, 4.0));
	int index = x + y * 4;
	float m[16] = float[16](
		 0.0,  8.0,  2.0, 10.0,
		12.0,  4.0, 14.0,  6.0,
		 3.0, 11.0,  1.0,  9.0,
		15.0,  7.0, 13.0,  5.0);
	return m[index] / 16.0;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 t1 = texture(textura1, uv);
	vec4 t2 = texture(textura2, uv);

	float threshold = bayer(gl_FragCoord.xy);
	// Widened slightly beyond 0..1 so the very first and very last frames are
	// fully one source - at exactly 0 or 1 a strict compare would leave a few
	// stray pixels of the other image.
	float progress = mixst * 1.02 - 0.01;

	fragColor = progress > threshold ? t2 : t1;
}
