#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 3/10 - VORTICITY CONFINEMENT
// Reinyecta la vorticidad que pierde la disipacion numerica: es lo
// que da los remolinos vivos tipo liquido. Corresponde a
// vorticityShader de WebGL-Fluid-Simulation.
//
// Salida: velocidad (codificada) + carrier .ba desde texture1.
// La velocidad se clampa a ±1000 texel/seg (convencion de la web).
//
// Sliders: curl
// ============================================================

uniform float curl = 0.3; // fuerza del confinement (mapr 0-100)

uniform sampler2D texture1; // velocidad (fluidpass_splat)
uniform sampler2D texture2; // curl (fluidpass_curl)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	float L = (texture(texture2, uv - vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float R = (texture(texture2, uv + vec2(t.x, 0.0)).x - 0.5) * 2000.0;
	float T = (texture(texture2, uv + vec2(0.0, t.y)).x - 0.5) * 2000.0;
	float B = (texture(texture2, uv - vec2(0.0, t.y)).x - 0.5) * 2000.0;
	float C = (texture(texture2, uv).x - 0.5) * 2000.0;

	vec2 force = 0.5 * vec2(abs(T) - abs(B), abs(R) - abs(L));
	force /= length(force) + 0.0001;
	force *= mapr(curl, 0.0, 100.0) * C;
	force.y *= -1.0; // convencion de la web; si gira al reves, invertir

	vec2 velocity = (texture(texture1, uv).rg - 0.5) * 2000.0;
	velocity += force * (1.0 / 60.0);
	velocity = min(max(velocity, -1000.0), 1000.0);

	fragColor = vec4(velocity * 0.0005 + 0.5, texture1.ba);
}
