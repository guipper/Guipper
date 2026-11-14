#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 7/10 - ADVECT VELOCITY
// Adveccion semi-Lagrangiana: cada punto traza hacia atras por el
// campo de velocidad y samplea el frame anterior. Corresponde a
// advectionShader de WebGL-Fluid-Simulation (pasada de velocidad).
//
// El sampleo conserva la codificacion del campo. Solo .rg se disipa;
// el carrier .ba (mouse previo) pasa intacto desde texture1.
//
// Sliders: velocity_dissipation
// ============================================================

uniform float velocity_dissipation = 0.1; // disipacion de velocidad (mapr 0-2)

uniform sampler2D texture1; // velocidad final proyectada (fluidpass_gradient)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	vec2 vel = (texture(texture1, uv).rg - 0.5) * 2000.0;
	vec2 coord = uv - (1.0 / 60.0) * vel * t;
	vec4 result = texture(texture1, coord);
	float decay = 1.0 + mapr(velocity_dissipation, 0.0, 2.0) * (1.0 / 60.0);

	fragColor = vec4(result.rg / decay, texture1.ba);
}
