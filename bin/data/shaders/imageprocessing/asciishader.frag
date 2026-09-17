#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
// Bitmap to ASCII (not really) fragment shader by movAX13h, September 2013
// This is the original shader that is now used in PixiJs, FL Studio and various other products.

// Here's a little tool for new characters: thrill-project.com/archiv/coding/bitmap/

// update 2018-12-14: values for characters are integer now (were float)
//                    since bit operations are available now, making use of them
//                    instead of int(mod(n/exp2(p.x + 5.0*p.y), 2.0))


uniform sampler2D iChannel0;
uniform float tile;
float character(int n, vec2 p)
{
	p = floor(p*vec2(4.0, -4.0) + 2.5);
    if (clamp(p.x, 0.0, 4.0) == p.x)
	{
        if (clamp(p.y, 0.0, 4.0) == p.y)	
		{
        	int a = int(round(p.x) + 5.0 * round(p.y));
			if (((n >> a) & 1) == 1) return 1.0;
		}	
    }
	return 0.0;
}

void main()
{
	vec2 pix = gl_FragCoord.xy;
	// Sampled ONCE, alpha included, and at the same blocky coordinate the
	// colour uses: reading the alpha at the exact pixel instead would give a
	// smooth silhouette edge inside a chunky 8px mosaic.
	vec4 src = texture(iChannel0, floor(pix/8.0)*8.0/iResolution.xy);
	vec3 col = src.rgb;
	
	float gray = 0.3 * col.r + 0.59 * col.g + 0.11 * col.b;
	
	int n =  4096;                // .
	if (gray > 0.2) n = 65600;    // :
	if (gray > 0.3) n = 332772;   // *
	if (gray > 0.4) n = 15255086; // o 
	if (gray > 0.5) n = 23385164; // &
	if (gray > 0.6) n = 15252014; // 8
	if (gray > 0.7) n = 13199452; // @
	if (gray > 0.8) n = 11512810; // #
	
	vec2 p = mod(pix/int(tile*800.0), 2.0) - vec2(1.0);
    

	float ch = character(n, p);
    col = col*ch;
	
	// Alpha carries the glyph, not just the source. The gaps between characters
	// were already painted black by the multiply above; writing 1.0 there made
	// that black opaque, so as an overlay this shader dropped a black rectangle
	// over the output instead of letting the characters float on top of it.
	// Drawn as the active render - over a cleared black frame - the result is
	// the same picture it always was.
	fragColor = vec4(col, src.a*ch);
}