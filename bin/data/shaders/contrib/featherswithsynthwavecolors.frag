#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define S smoothstep
#define T (iTime * .5)

mat2 Rot(float a)
{
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

float Feather(vec2 uv)
{
    // Vec2 offsets the center, x, y coordinates on the screen
   	float d = length(uv - vec2(0, clamp(uv.y, -.3, .3)));
    
    // The usage of smoothstep here is smoothstep(blur, 0, compareValue - thickness)
    // This makes it way less cumbersome to use and tidy.
    // I have a value r for radius of the line, 
    // this gets smaller along the lenght of the fether
    // Visualize 'radius' by returning it;
    float r = mix(.1, .01, S(-.3, .3, uv.y));
    float m = S(.01, .0, d - r);
    
    
    // Returns the value of the sign of uv.x, 0 is the center of the screen,
    // -1 when negative, +1 when positive
    float side = sign(uv.x);
    
    // X goes from 0 to 1, but our feather is from 0 to r, so we normalize it
    // The .9 multiplication just makes it not go entirely to 1,
    // so we don't get jagged edges
    float x = .9 * abs(uv.x) / r;
    float wave = (1.0 -x) * sqrt(x) + (1.0 - sqrt(1.0 - x)) * x;
    float y = (uv.y - wave * .2) * 80.0 + side;
    
    float id = floor(y + 20.0);
    float n = fract(sin(id * 650.2) * 795.); // semi-random number between 0 - 1
    float shade = mix(.5, 1., n);
    float strandLength = mix(.6, 1., fract(n * 2.));
    
    // fract creates repetition.
    float strand = S(.6, .0, abs(fract(y) - 0.5) - .3);
    strand *= S(.3, .0, x - strandLength);
    
    d = length(uv - vec2(0, clamp(uv.y, -.45, .2)));
	float stem = S(.01, 0.0, d + uv.y * .02);
    
    // Taking the max of 2 values is better as it will not have hidden information
    return max(strand*m*shade, stem);
}

vec3 Transform(vec3 p, float angle)
{
    p.xz *= Rot(angle);
    p.xy *= Rot(angle *.7);
    return p;
}

vec4 RayTracedFeather(vec3 rayOrigin, vec3 rayDirection, vec3 position, float angle)
{
    
    vec4 col = vec4(0);
    float t = dot(position - rayOrigin, rayDirection);
    vec3 p = rayOrigin + rayDirection * t;
    
    float y = length(position - p);
    
    // Ray traced sphere
    if (y < 1.0)
    {
    	float x = sqrt(1.0 - y);
        vec3 pF = rayOrigin + rayDirection * (t - x) - position; // Front intersection
        
        pF = Transform(pF, angle);
        // Cylindrical projection using polar coordinates.
        vec2 uvF = vec2(atan(pF.x, pF.z), pF.y); // -pi to pi, -1 to 1;
        uvF *= vec2(.25, .5);
        float f = Feather(uvF);
        vec4 front = vec4(vec3(f), S(0., .1, f));
        
        vec3 pB = rayOrigin + rayDirection * (t + x) - position; // Back intersection
        pB = Transform(pB, angle);
        vec2 uvB = vec2(atan(pB.x, pB.z), pB.y); // -pi to pi, -1 to 1;
        uvB *= vec2(.25, .5);
        float b = Feather(uvB);
        vec4 back = vec4(vec3(b), S(.0, .1, b));

        col = mix(back, front, front.a);
    }
    
    return col;
}

void main()
{
    // Center the uvs and take in account screen x size
    vec2 uv = (fragCoord-.5*iResolution.xy)/iResolution.y;
    vec2 M = iMouse.xy / iResolution.xy;
    vec3 bg = vec3(1, .4, .1) * (uv.y + .5);
    bg += vec3(1, .16, .45) * (-uv.y + .5);
    vec4 col = vec4(bg, 0);
    //col += Feather(uv);
    
    // ray tracer
    vec3 rayOrigin = vec3(0, 0, -3);
    vec3 rayDir = normalize(vec3(uv, 1));
    
    for(float i= 0.0; i < 1.0; i+= 1.0/51.0)
    {
        // M.component *i*i creates the parallax effect
        // The closest they are the stronger the effect,
        // this happens because of the Z axis being from back to front rendering.
        float x = mix(-8. , 8., fract(i + T*.1)) + M.x * i * i;
        float y = mix(-2., 2.,fract(sin(i * 324.5) * 7864.4)) + (M.y * i*i);
        float z = mix(5., 0., i);
        float a = T + i *530.35;
        vec4 feather = RayTracedFeather(rayOrigin, rayDir, vec3(x, y, z), a);
    	
        feather.rg *= i;
        feather.b += i;
        feather.rgb = mix(bg.rgb, feather.rgb, mix(.3, 1., i));
        feather.rgb = sqrt(feather.rgb);
        col = mix(col, feather, feather.a);
    }
    
    col = pow(col, vec4(.4545)); // gamma correction
    
    fragColor = vec4(col.rgb,1.0);
}