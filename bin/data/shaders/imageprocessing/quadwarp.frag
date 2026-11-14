#pragma include "../common.frag" // Contains global uniforms like time, resolution, mouse, etc.

uniform sampler2D texture1;

// Corner coordinate uniforms, defaulted to map the corners of the normalized screen space [0, 1]
uniform float bottomLeftX = 0.0;
uniform float bottomLeftY = 0.0;
uniform float bottomRightX = 1.0;
uniform float bottomRightY = 0.0;
uniform float topRightX = 1.0;
uniform float topRightY = 1.0;
uniform float topLeftX = 0.0;
uniform float topLeftY = 1.0;

// Toggle button to display or hide the interactive corner markers and bounds
uniform bool showReferences;

// 2D cross product equivalent
float cross2d(vec2 a, vec2 b) {
    return a.x * b.y - a.y * b.x;
}

// Distance from point p to line segment ab
float distToSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float denom = dot(ba, ba);
    float h = clamp(dot(pa, ba) / (denom + 0.000001), 0.0, 1.0);
    return length(pa - ba * h);
}

// Analytical Inverse Bilinear mapping of a point p in quadrilateral (a, b, c, d)
// Returns the local (u, v) normalized coordinates in the unit square [0, 1]^2
vec2 invBilinear(vec2 p, vec2 a, vec2 b, vec2 c, vec2 d) {
    vec2 e = b - a;
    vec2 f = d - a;
    vec2 g = a - b + c - d;
    vec2 h = p - a;

    float k2 = cross2d(g, f);
    float k1 = cross2d(e, f) + cross2d(h, g);
    float k0 = cross2d(h, e);

    vec2 res = vec2(-1.0);

    // If edges are parallel (parallelogram or rectangle), solve as a linear equation
    if (abs(k2) < 0.0001) {
        float k1_safe = k1 + (k1 >= 0.0 ? 0.000001 : -0.000001);
        float v = -k0 / k1_safe;
        float denomX = e.x + g.x * v;
        float denomY = e.y + g.y * v;
        float u;
        if (abs(denomX) > abs(denomY)) {
            u = (h.x - f.x * v) / (denomX + (denomX >= 0.0 ? 0.000001 : -0.000001));
        } else {
            u = (h.y - f.y * v) / (denomY + (denomY >= 0.0 ? 0.000001 : -0.000001));
        }
        res = vec2(u, v);
    } 
    // Otherwise, solve the general quadratic equation
    else {
        float w = k1 * k1 - 4.0 * k0 * k2;
        if (w >= 0.0) {
            w = sqrt(w);
            
            // Try first root
            float v1 = (-k1 - w) / (2.0 * k2);
            float denom1X = e.x + g.x * v1;
            float denom1Y = e.y + g.y * v1;
            float u1 = (abs(denom1X) > abs(denom1Y)) ? (h.x - f.x * v1) / (denom1X + (denom1X >= 0.0 ? 0.000001 : -0.000001)) : (h.y - f.y * v1) / (denom1Y + (denom1Y >= 0.0 ? 0.000001 : -0.000001));

            // Try second root
            float v2 = (-k1 + w) / (2.0 * k2);
            float denom2X = e.x + g.x * v2;
            float denom2Y = e.y + g.y * v2;
            float u2 = (abs(denom2X) > abs(denom2Y)) ? (h.x - f.x * v2) / (denom2X + (denom2X >= 0.0 ? 0.000001 : -0.000001)) : (h.y - f.y * v2) / (denom2Y + (denom2Y >= 0.0 ? 0.000001 : -0.000001));

            float u = u1;
            float v = v1;

            // If the first root falls outside the unit square, pick the second root
            if (v < 0.0 || v > 1.0 || u < 0.0 || u > 1.0) {
                u = u2;
                v = v2;
            }
            res = vec2(u, v);
        }
    }
    return res;
}

void main() {
    // Current pixel coordinate in [0, 1] range
    vec2 uv = gl_FragCoord.xy / resolution;

    // Define quadrilateral corners from uniforms
    vec2 a = vec2(bottomLeftX, bottomLeftY);
    vec2 b = vec2(bottomRightX, bottomRightY);
    vec2 c = vec2(topRightX, topRightY);
    vec2 d = vec2(topLeftX, topLeftY);

    // Map current coordinate back to the input texture space
    vec2 texUV = invBilinear(uv, a, b, c, d);

    // Sample the texture if the mapped coordinate is within the quadrilateral bounds
    vec4 finalColor = vec4(0.0);
    if (texUV.x >= 0.0 && texUV.x <= 1.0 && texUV.y >= 0.0 && texUV.y <= 1.0) {
        finalColor = texture2D(texture1, texUV);
    }

    // Overlay interactive references (lines and dots) if showReferences is toggled
    if (showReferences) {
        float aspect = resolution.y > 0.0 ? resolution.x / resolution.y : 1.0;
        vec2 scaleVec = vec2(aspect, 1.0);
        
        // 1. Calculate distances to the boundary segments of the quad
        float dAB = distToSegment(uv * scaleVec, a * scaleVec, b * scaleVec);
        float dBC = distToSegment(uv * scaleVec, b * scaleVec, c * scaleVec);
        float dCD = distToSegment(uv * scaleVec, c * scaleVec, d * scaleVec);
        float dDA = distToSegment(uv * scaleVec, d * scaleVec, a * scaleVec);
        
        float minDist = min(min(dAB, dBC), min(dCD, dDA));
        
        // solid border mask + soft glowing halo
        float lineMask = 1.0 - smoothstep(0.0015, 0.003, minDist);
        float lineGlow = 1.0 - smoothstep(0.0015, 0.012, minDist);
        vec3 lineColor = verdejpupper() * (lineMask + lineGlow * 0.45);

        // 2. Calculate distances to the interactive corner points
        float dA = length((uv - a) * scaleVec);
        float dB = length((uv - b) * scaleVec);
        float dC = length((uv - c) * scaleVec);
        float dD = length((uv - d) * scaleVec);
        
        float minDotDist = min(min(dA, dB), min(dC, dD));
        
        // Inner white core dot + outer colored glow
        float dotCore = 1.0 - smoothstep(0.003, 0.005, minDotDist);
        float dotHalo = 1.0 - smoothstep(0.005, 0.015, minDotDist);
        vec3 dotColor = verdejpupper() * dotCore + vec3(1.0) * dotCore + verdejpupper() * dotHalo * 0.6;

        // Combine lines and dots
        float refMask = max(lineMask, dotHalo);
        vec3 refColor = mix(lineColor, dotColor, dotHalo);

        // Blend onto the final fragment color
        finalColor.rgb = mix(finalColor.rgb, refColor, refMask * 0.85);
        finalColor.a = max(finalColor.a, refMask * 0.85);
    }

    fragColor = finalColor;
}
