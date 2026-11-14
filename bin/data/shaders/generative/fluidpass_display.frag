#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 10/10 - DISPLAY
// El dye final a la pantalla, con sombreado falso 3D opcional.
// Corresponde a displayShader de WebGL-Fluid-Simulation (sin bloom).
//
// Sliders: shading, boost, fondo
// ============================================================

uniform float shading = 1.0; // 1 = falso 3D (luz por gradiente)
uniform float boost = 0.5;   // ganancia del dye (mapr 0.1-3)
uniform float fondo = 1.0;   // base oscura para no ver negro muerto

uniform sampler2D texture1; // dye (fluidpass_advect_dye)

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 t = 1.0 / resolution;

	vec3 c = texture(texture1, uv).rgb * mapr(boost, 0.1, 3.0);

	if (shading > 0.5)
	{
		vec3 lc = texture(texture1, uv - vec2(t.x, 0.0)).rgb;
		vec3 rc = texture(texture1, uv + vec2(t.x, 0.0)).rgb;
		vec3 tc = texture(texture1, uv + vec2(0.0, t.y)).rgb;
		vec3 bc = texture(texture1, uv - vec2(0.0, t.y)).rgb;

		float dx = length(rc) - length(lc);
		float dy = length(tc) - length(bc);

		vec3 n = normalize(vec3(dx, dy, length(t)));
		vec3 l = vec3(0.0, 0.0, 1.0);

		float diffuse = clamp(dot(n, l) + 0.7, 0.7, 1.0);
		c *= diffuse;
	}

	vec3 bg = vec3(0.02, 0.02, 0.05) * fondo;
	c = max(c, bg);
	fragColor = vec4(c, 1.0);
}
