#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 8/10 - SPLAT DYE
// Inyecta color en la posicion del mouse mientras se mueve, y una
// explosion de color con el kick (audio_trigger). Corresponde a
// splatShader de WebGL-Fluid-Simulation (pasada de dye).
// El mouse previo viene en el carrier .ba del fbo de dye
// (fluidpass_advect_dye lo escribe cada frame).
//
// Sliders: dye_hue, dye_sat, dye_bright, dye_radius, audio_pulse, burst
// ============================================================

uniform float dye_hue = 0.6;     // tono del color inyectado
uniform float dye_sat = 0.9;     // saturacion
uniform float dye_bright = 0.6;  // brillo
uniform float dye_radius = 0.08; // radio del splat (mapr 0.001-0.02 x aspect)
uniform float audio_pulse = 0.0; // 1 = el brillo pulsa con audio_trigger
uniform float burst = 1.0;       // 1 = explosion de color con el kick

uniform sampler2D texture1; // dye (fluidpass_advect_dye) - .ba = mouse previo
uniform float audio_trigger; // global: kick

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float aspect = resolution.x / resolution.y;

	vec3 base = texture(texture1, uv).rgb;

	float radius = mapr(dye_radius, 0.001, 0.02) * max(aspect, 1.0);
	float bright = mapr(dye_bright, 0.05, 1.0);
	if (audio_pulse > 0.5)
	{
		bright *= 0.4 + audio_trigger * 2.2;
	}
	vec3 col = hsb2rgb(vec3(mapr(dye_hue, 0.0, 1.0),
	                        mapr(dye_sat, 0.0, 1.0), bright));

	// Delta del mouse (el previo viene en el carrier .ba del dye).
	vec2 delta = mouse.xy - texture1.ba;
	vec2 p = uv - mouse.xy;
	p.x *= aspect;
	float m = exp(-dot(p, p) / radius);
	float active = length(delta) > 0.00001 ? 1.0 : 0.0;
	base += col * m * active * 0.6;

	// Burst de color con el kick: 6 manchones con tono jittereado.
	if (burst > 0.5 && audio_trigger > 0.5)
	{
		float frame = floor(time * 60.0);
		for (int i = 0; i < 6; i++)
		{
			vec2 rnd = vec2(hash12(vec2(float(i) * 7.13, frame)),
			                hash12(vec2(float(i) * 3.71, frame + 11.7)));
			vec2 bp = mouse.xy + (rnd - 0.5) * 0.25;
			vec2 bq = uv - bp;
			bq.x *= aspect;
			float bm = exp(-dot(bq, bq) / (radius * 3.0));
			vec3 bcol = hsb2rgb(vec3(fract(mapr(dye_hue, 0.0, 1.0) + rnd.x * 0.3),
			                         mapr(dye_sat, 0.0, 1.0), bright));
			base += bcol * bm * 4.0;
		}
	}

	fragColor = vec4(base, 1.0);
}
