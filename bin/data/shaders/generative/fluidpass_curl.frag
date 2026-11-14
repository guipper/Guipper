#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 2/10 - CURL
// Vorticidad del campo de velocidad (se la usa para el vorticity
// confinement). Corresponde a curlShader de WebGL-Fluid-Simulation.
//
// Salida: curl codificado en .r (misma escala ±2000 que la velocidad).
// Lee la velocidad ya splateada (fluidpass_splat).
// ============================================================

uniform sampler2D texture1; // velocidad (fluidpass_splat)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	float L = (texture(texture1, uv - vec2(t.x, 0.0)).y - 0.5) * 2000.0;
	float R = (texture(texture1, uv + vec2(t.x, 0.0)).y - 0.5) * 2000.0;
	float T = (texture(texture1, uv + vec2(0.0, t.y)).x - 0.5) * 2000.0;
	float B = (texture(texture1, uv - vec2(0.0, t.y)).x - 0.5) * 2000.0;

	float vorticity = R - L - T + B;
	fragColor = vec4(clamp(0.5 * vorticity, -2000.0, 2000.0) * 0.0005 + 0.5,
	                 0.0, 0.0, 1.0);
}
