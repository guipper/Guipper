#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 6/10 - GRADIENT SUBTRACT
// Resta el gradiente de presion: proyecta la velocidad a un campo
// libre de divergencia (incompresible). Corresponde a
// gradientSubtractShader de WebGL-Fluid-Simulation.
//
// Salida: velocidad (codificada) + carrier .ba desde texture1.
// ============================================================

uniform sampler2D texture1; // velocidad (fluidpass_vorticity)
uniform sampler2D texture2; // presion (fluidpass_pressure_5)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	float L = (texture(texture2, uv - vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float R = (texture(texture2, uv + vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float T = (texture(texture2, uv + vec2(0.0, t.y)).x - 0.5) * 2000.0;
	float B = (texture(texture2, uv - vec2(0.0, t.y)).x - 0.5) * 2000.0;

	vec2 velocity = (texture(texture1, uv).rg - 0.5) * 2000.0;
	velocity.xy -= vec2(R - L, T - B);

	fragColor = vec4(clamp(velocity, -2000.0, 2000.0) * 0.0005 + 0.5,
	                 texture1.ba);
}
