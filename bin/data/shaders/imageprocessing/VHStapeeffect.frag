#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D iChannel0;
uniform float range = 0.05;
uniform float noiseQuality = 250.0;
uniform float noiseIntensity = 0.0088;
uniform float offsetIntensity = 0.02;
uniform float colorOffsetIntensity = 1.3;

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
	vec2 uv = fragCoord.xy / iResolution.xy;
    
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
    uv.x += noise * noiseIntensity;

    vec2 offsetR = vec2(0.006 * sin(iTime), 0.0) * colorOffsetIntensity;
    vec2 offsetG = vec2(0.0073 * (cos(iTime * 0.97)), 0.0) * colorOffsetIntensity;
    
    float r = texture(iChannel0, uv + offsetR).r;
    float g = texture(iChannel0, uv + offsetG).g;
    float b = texture(iChannel0, uv).b;

    vec4 tex = vec4(r, g, b, 1.0);
    fragColor = tex;
}


