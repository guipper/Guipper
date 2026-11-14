#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 4/10 - DIVERGENCE
// Divergencia del campo de velocidad: la fuente del solve de presion.
// Corresponde a divergenceShader de WebGL-Fluid-Simulation.
// Borde: refleja la componente normal (paredes que no dejan escapar).
//
// Salida: divergencia codificada en .r (escala ±2000).
// ============================================================

uniform sampler2D texture1; // velocidad (fluidpass_vorticity)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	float L = (texture(texture1, uv - vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float R = (texture(texture1, uv + vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float T = (texture(texture1, uv + vec2(0.0, t.y)).y - 0.5) * 2000.0;
	float B = (texture(texture1, uv - vec2(0.0, t.y)).y - 0.5) * 2000.0;

	vec2 C = (texture(texture1, uv).rg - 0.5) * 2000.0;
	if (uv.x < t.x) { L = -C.x; }
	if (uv.x > 1.0 - t.x) { R = -C.x; }
	if (uv.y > 1.0 - t.y) { T = -C.y; }
	if (uv.y < t.y) { B = -C.y; }

	float div = 0.5 * (R - L + T - B);
	fragColor = vec4(clamp(div, -2000.0, 2000.0) * 0.0005 + 0.5,
	                 0.0, 0.0, 1.0);
}
