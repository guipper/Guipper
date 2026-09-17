#pragma include "../common.frag"

// Controls use Guipper's normalized 0..1 range.
// Defaults reproduce the original sin(uv.x * 10.0 + time) pattern.
uniform float frequency = 0.25; // 0..40 radians across the image.
uniform float speed = 0.6;      // -5..5 radians/second; 0.5 pauses.
uniform float angle = 0.0;      // One full turn.
uniform float phase = 0.0;      // One full wave cycle.
uniform float contrast = 0.5;   // 0: flat; 0.5: sine; 1: clipped bands.
uniform float red = 1.0;        // Red-channel intensity; the sine wave modulates it.
uniform float green = 0.5;
uniform float blue = 1.0;

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    float rotation = angle * 6.28318530718;
    vec2 direction = vec2(cos(rotation), sin(rotation));
    float position = dot(uv - 0.5, direction) + 0.5;
    float wave = sin(position * frequency * 40.0
        + time * (speed - 0.5) * 10.0 + phase * 6.28318530718);
    float sweep = clamp(0.5 + wave * contrast, 0.0, 1.0);
    fragColor = vec4(sweep * red, green, blue, 1.0);
}
