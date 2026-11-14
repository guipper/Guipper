#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 9/10 - ADVECT DYE
// Adveccion semi-Lagrangiana del color por el campo de velocidad.
// Corresponde a advectionShader de WebGL-Fluid-Simulation (pasada de dye).
// Ademas escribe el carrier .ba = mouse actual: como esta caja corre
// DESPUES de fluidpass_splat_dye en el frame, su fbo con el mouse de
// este frame queda como "mouse previo" para el splat del frame siguiente.
//
// Sliders: dye_dissipation
// ============================================================

uniform float dye_dissipation = 0.5; // disipacion del color (mapr 0-2)

uniform sampler2D texture1; // dye (fluidpass_splat_dye)
uniform sampler2D texture2; // velocidad (fluidpass_advect_vel)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	vec2 vel = (texture(texture2, uv).rg - 0.5) * 2000.0;
	vec2 coord = uv - (1.0 / 60.0) * vel * t;
	vec4 result = texture(texture1, coord);
	float decay = 1.0 + mapr(dye_dissipation, 0.0, 2.0) * (1.0 / 60.0);

	fragColor = vec4(result.rgb / decay, mouse.xy);
}
