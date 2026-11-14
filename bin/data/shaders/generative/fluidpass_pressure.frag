#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 5/10 - PRESSURE (Jacobi)
// UNA iteracion del solve de Poisson. El preset fluido_webgl.xml carga
// ESTE MISMO shader en 5 cajas encadenadas (fluidpass_pressure_1..5):
// 5 iteraciones por frame con warm start:
//   - pressure_1 lee el resultado del frame anterior (el fbo de
//     fluidpass_pressure_5, que aun tiene el frame viejo cuando corre)
//   - pressure_i lee la salida de pressure_{i-1} (mismo frame)
// Para mas iteraciones, duplica cajas en el XML y ajusta el link de
// fluidpass_gradient al ultimo pressure_N.
// Corresponde a pressureShader de WebGL-Fluid-Simulation.
//
// Salida: presion codificada en .r (escala ±2000).
// ============================================================

uniform sampler2D texture1; // presion previa (pressure_5 para la caja 1, o pressure_{i-1})
uniform sampler2D texture2; // divergencia (fluidpass_divergence)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	float L = (texture(texture1, uv - vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float R = (texture(texture1, uv + vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float T = (texture(texture1, uv + vec2(0.0, t.y)).x - 0.5) * 2000.0;
	float B = (texture(texture1, uv - vec2(0.0, t.y)).x - 0.5) * 2000.0;
	float divergence = (texture(texture2, uv).x - 0.5) * 2000.0;

	float pressure = (L + R + B + T - divergence) * 0.25;
	fragColor = vec4(clamp(pressure, -2000.0, 2000.0) * 0.0005 + 0.5,
	                 0.0, 0.0, 1.0);
}
