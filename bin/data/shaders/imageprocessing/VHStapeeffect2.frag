#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D iChannel0;
uniform float range ;
uniform float noiseQuality ;
uniform float noiseIntensity ;
uniform float offsetIntensity ;
uniform float colorOffsetIntensity ;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float verticalBar(float pos, float uvY, float offset)
{
    float edge0 = (pos - mapr(range,0.0,0.05)); 
    float edge1 = (pos + mapr(range,0.0,0.05)); 

    float x = smoothstep(edge0, pos, uvY) * offset;
    x -= smoothstep(pos, edge1, uvY) * offset;
    return x;
}

void main()
{
	// Kept before any displacement: the alpha is read from HERE, so the effect
	// can never make the image bigger than it is. See the note further down.
	vec2 baseUv = gl_FragCoord.xy / iResolution.xy;
	vec2 uv = baseUv;
    
    for (float i = 0.0; i < 0.71; i += 0.1313)
    {
        float d = mod(iTime * i, 1.7);
        float o = sin(1.0 - tan(iTime * 0.24 * i));
    	o *= offsetIntensity;
        uv.x += verticalBar(d, uv.y, o);
    }
    
	float mnoiseQuality = mapr(noiseQuality,0.0,300.0);
    float uvY = uv.y;
    uvY *= mnoiseQuality;
    uvY = float(int(uvY)) * (1.0 / mnoiseQuality);
	
    float noise = rand(vec2(iTime * 0.00001, uvY));
    uv.x += noise * mapr(noiseIntensity,0.0,0.01);

    vec2 offsetR = vec2(0.006 * sin(iTime), 0.0) * colorOffsetIntensity;
    vec2 offsetG = vec2(0.0073 * (cos(iTime * 0.97)), 0.0) * colorOffsetIntensity;
    
    vec4 sampleR = texture(iChannel0, uv + offsetR);
    vec4 sampleG = texture(iChannel0, uv + offsetG);
    vec4 sampleB = texture(iChannel0, uv);

    // Alpha from the source instead of a hardcoded 1.0, or a PNG or GIF loses
    // its transparent background the moment it goes through this shader - the
    // box FBO is written with blending disabled, so this alpha IS the result.
    //
    // Read at baseUv - the UNDISPLACED position - so the silhouette is exactly
    // the source's. Every displacement above moves colour around INSIDE the
    // shape and none of it can spill past the edge: neither the chromatic
    // split, which lands a red and a green fringe a few pixels off to the
    // sides, nor the vertical bars, which shift whole rows far enough to draw
    // a second copy of the image out in the margin. On opaque input alpha is 1
    // everywhere and nothing about the effect changes.
    float alpha = texture(iChannel0, baseUv).a;

    fragColor = vec4(sampleR.r, sampleG.g, sampleB.b, alpha);
}


