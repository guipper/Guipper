#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D iChannel0;

// Normalized controls. Defaults preserve the original 100 x 100 green palette.
uniform float pixels_x = 0.25; // 16..352 cells; 0.25 = 100.
uniform float pixels_y = 0.25;
uniform float brightness = 0.5; // Midpoint leaves input lightness unchanged.
uniform float contrast = 0.5;   // Midpoint leaves input contrast unchanged.
uniform float palette_hue = 0.0;
uniform float palette_saturation = 0.5; // Midpoint keeps the original saturation.
uniform float effect_mix = 1.0;

vec3 darkest =  vec3(0.0588235, 0.219608, 0.0588235); float lumDarkest = 0.1392156862745098;
vec3 darker = vec3( 0.188235, 0.384314, 0.188235); float lumDarker = 0.28627450980392155;
vec3 lighter = vec3( 0.545098, 0.67451, 0.0588235); float lumLighter = 0.3666666666666667;
vec3 lightest = vec3(  0.607843, 0.737255, 0.0588235); float lumLightest = 0.39803921568627454;

void main()
{
	vec2 uv = fragCoord.xy / iResolution.xy;
    vec4 original = texture(iChannel0, uv);
    vec2 cells = floor(vec2(16.0) + vec2(pixels_x, pixels_y) * 336.0 + 0.5);
    cells = max(cells, vec2(1.0));
    vec2 pixelUV = floor(uv * cells) / cells;
    fragColor = texture(iChannel0, pixelUV);
    float maxCol = max(max(fragColor.r, fragColor.g), fragColor.b);
    float minCol = min(min(fragColor.r, fragColor.g), fragColor.b);
    float lum = (minCol + maxCol)/2.0;
    lum = clamp((lum - 0.5) * contrast * 2.0 + 0.5
        + (brightness - 0.5) * 2.0, 0.0, 1.0);
    
    float darkestDist = abs(lumDarkest - lum); //length(darkest - fragColor.rgb);
    float darkerDist = abs(lumDarker - lum); //length(darker - fragColor.rgb);
    float lighterDist = abs(lumLighter - lum); //length(lighter - fragColor.rgb);
    float lightestDist = abs(lumLightest - lum); //length(lightest - fragColor.rgb);
    
    float minDist = min(min(min(darkestDist, darkerDist), lighterDist), lightestDist);
    
    if( abs(minDist - darkestDist) < 0.000001)
    {
        fragColor = vec4(darkest, 1.0);
    }
    else if(abs(minDist - darkerDist) < 0.000001)
    {
        fragColor = vec4(darker, 1.0);
    }
    else if(abs(minDist - lighterDist) < 0.000001)
    {
        fragColor = vec4(lighter, 1.0);
    }
    else
    {
        fragColor = vec4(lightest, 1.0);
    }
    
   
    // Avoid a roundtrip at neutral settings to retain the exact original palette.
    if (palette_hue != 0.0 || palette_saturation != 0.5) {
        vec3 hsv = rgb2hsb(fragColor.rgb);
        hsv.x = fract(hsv.x + palette_hue);
        hsv.y = clamp(hsv.y * palette_saturation * 2.0, 0.0, 1.0);
        fragColor.rgb = hsb2rgb(hsv);
    }
    fragColor = vec4(mix(original.rgb, fragColor.rgb, effect_mix), 1.0);
}
               
               
