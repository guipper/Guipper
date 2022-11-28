#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


uniform sampler2D iChannel0;

vec3 darkest =  vec3(0.0588235, 0.219608, 0.0588235); float lumDarkest = 0.1392156862745098;
vec3 darker = vec3( 0.188235, 0.384314, 0.188235); float lumDarker = 0.28627450980392155;
vec3 lighter = vec3( 0.545098, 0.67451, 0.0588235); float lumLighter = 0.3666666666666667;
vec3 lightest = vec3(  0.607843, 0.737255, 0.0588235); float lumLightest = 0.39803921568627454;

float CalculateHue(vec4 color, float minCol, float maxCol)
{
    float hue = 0.0;  
    hue = hue*60.0;
    
    if(hue < 0.0)
    {
        hue += 360.0;
    }
    
    if(abs(maxCol - color.r) < 0.000001)
    {
        // If Red is max, then Hue = (G-B)/(max-min)
        hue = (color.g - color.b)/(maxCol-minCol);
    }
   	else if(abs(maxCol - color.g) < 0.000001)
    {
        // If Green is max, then Hue = 2.0 + (B-R)/(max-min)
        hue = 2.0 + (color.b - color.r)/(maxCol-minCol);
    }
    else
    {
        // If Blue is max, then Hue = 4.0 + (R-G)/(max-min)
        hue = 4.0 + (color.r - color.g)/(maxCol-maxCol);
    }
    
    return hue;
}

void main()
{
	vec2 uv = fragCoord.xy / iResolution.xy;
    uv *= 100.0;
    uv = vec2(floor(uv.x), floor(uv.y));
    uv *= 0.01;   
    
    
	fragColor = texture(iChannel0, uv);
    float maxCol = max(max(fragColor.r, fragColor.g), fragColor.b);
    float minCol = min(min(fragColor.r, fragColor.g), fragColor.b);
    float lum = (minCol + maxCol)/2.0;
    
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
    
   
}
               
               
