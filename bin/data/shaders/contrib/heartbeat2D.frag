#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float speed;

vec2 scale(vec2 st, float size)
{
    mat2 s = mat2(vec2(size, 0.0), vec2(0.0, size));
    return s * st;
}

float animate(float a, float t) 
{
    // Thump Thump... Thump Thump...
    return a * max(sin(t * 5.0), 0.0) * cos(t * 5.0);
}

float heart3(vec2 st, vec2 translate, float radius, float smoothRange)
{
    vec2 uv = st - translate;
    
    // Two partially overlapping circles for the +ve y quadrants
    float top = step(0.0, uv.y) * (smoothstep(radius + 0.025, radius + 0.025 - smoothRange, length(abs(uv) - vec2(radius - 0.025,0.0))));
        
    // Two symmetric sin curves for the -ve y quadrants
    uv.x = abs(uv.x);
    
    float bottom = step(-PI, uv.y * PI) * step(0.0, -uv.y) * 
        smoothstep(0.0, smoothRange, (radius * sin(uv.y * PI + PI / 2.0) - uv.x + 1.0 * radius));
    
    // Put them all together
    return top + bottom;
}



void main()
{
    vec2 st = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    st.y = -st.y;
    st = scale(st, 1.0 - animate(0.05, iTime*speed*5.0));
    float c = heart3(st, vec2(0.0, 0.25), 0.35, 0.01);
    
    fragColor = vec4(mix(vec3(1.0), vec3(0.9, 0.15, 0.07), c),1.0);
}