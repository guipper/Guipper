#version 330
#pragma include "../common.frag"

uniform float currentIndex;
uniform float transition; // 0-1 crossfade between current and next slot
uniform sampler2D slot0;
uniform sampler2D slot1;
uniform sampler2D slot2;
uniform sampler2D slot3;
uniform sampler2D slot4;
uniform sampler2D slot5;
uniform sampler2D slot6;
uniform sampler2D slot7;
uniform int numSlots;

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    vec4 fin = vec4(0.0);
    
    if (numSlots > 0)
    {
        int idx0 = int(floor(currentIndex));
        int idx1 = int(min(idx0 + 1, numSlots - 1));
        float mixVal = transition;
        
        // Clamp indices to valid range
        idx0 = clamp(idx0, 0, numSlots - 1);
        idx1 = clamp(idx1, 0, numSlots - 1);
        
        vec4 t0, t1;
        
        // Sample from the correct slot texture
        if (idx0 == 0) t0 = texture(slot0, uv);
        else if (idx0 == 1) t0 = texture(slot1, uv);
        else if (idx0 == 2) t0 = texture(slot2, uv);
        else if (idx0 == 3) t0 = texture(slot3, uv);
        else if (idx0 == 4) t0 = texture(slot4, uv);
        else if (idx0 == 5) t0 = texture(slot5, uv);
        else if (idx0 == 6) t0 = texture(slot6, uv);
        else t0 = texture(slot7, uv);
        
        if (idx1 == 0) t1 = texture(slot0, uv);
        else if (idx1 == 1) t1 = texture(slot1, uv);
        else if (idx1 == 2) t1 = texture(slot2, uv);
        else if (idx1 == 3) t1 = texture(slot3, uv);
        else if (idx1 == 4) t1 = texture(slot4, uv);
        else if (idx1 == 5) t1 = texture(slot5, uv);
        else if (idx1 == 6) t1 = texture(slot6, uv);
        else t1 = texture(slot7, uv);
        
        fin = mix(t0, t1, mixVal);
    }
    
    fragColor = fin;
}
