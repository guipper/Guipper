#pragma include "../common.frag"

// ============================================================
// FLUIDO_MOUSE - fluido de feedback con flow field + mouse
// Arrastra el mouse: inyecta energia de color y revuelve las
// trails como si estuvieras mezclando liquido.
// Sliders: decay, flow_*, self_disp, mouse_force, inject_*,
//          hue, sat, gain, audio_pulse.
// ============================================================

uniform float decay = 0.94;          // persistencia de las trails (0.85-1.0)
uniform float flow_force = 0.5;      // fuerza del flow field
uniform float flow_scale = 0.35;     // escala del noise
uniform float flow_speed = 0.3;      // velocidad de animacion del flow
uniform float self_disp = 0.5;       // autodesplazamiento por color del feedback
uniform float mouse_force = 0.6;     // remolino que empuja el cursor
uniform float inject_size = 0.35;    // radio de la inyeccion
uniform float inject_intensity = 0.4;// brillo de la inyeccion
uniform float hue = 0.6;             // tono del color inyectado
uniform float sat = 0.9;             // saturacion del color inyectado
uniform float gain = 0.5;            // brillo general (0.3-1.0; el feedback total nunca amplifica)
uniform float audio_pulse = 0.0;     // 1 = la inyeccion pulsa con audio_trigger

uniform int boxframeNum;
uniform float audio_trigger;

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float fx = resolution.x / resolution.y;
	vec2 p  = vec2(uv.x * fx, uv.y);        // espacio cuadrado
	vec2 mp = vec2(mouse.x * fx, mouse.y);  // mouse en espacio cuadrado

	float t = time * mapr(flow_speed, 0.0, 3.0);

	// --- flow field (curl-ish con snoise) ---
	vec2 fuv = p * mapr(flow_scale, 0.6, 8.0);
	float a1 = snoise(fuv + t);
	float a2 = snoise(fuv + vec2(17.3, 9.1) - t * 0.6);
	vec2 flow = vec2(a1, a2) * mapr(flow_force, 0.0, 0.02);

	// --- remolino del mouse: tangencial + radial ---
	vec2 toM = mp - p;
	float md = length(toM);
	float fall = exp(-md * md * 140.0);
	vec2 dirn = toM / (md + 0.0001);
	vec2 stir = (vec2(-dirn.y, dirn.x) + dirn * 0.4) * fall * mapr(mouse_force, 0.0, 0.02);

	// --- autodesplazamiento por el color del frame anterior ---
	vec4 fb0 = texture(feedback, uv);
	vec2 self = (fb0.rg - 0.5) * mapr(self_disp, 0.0, 0.04);

	// --- sampleo desplazado con suavizado (5 taps) ---
	vec2 suv = uv + flow + stir + self;
	vec3 fb = texture(feedback, suv).rgb;
	fb += texture(feedback, suv + vec2(0.0012, 0.0)).rgb;
	fb += texture(feedback, suv - vec2(0.0012, 0.0)).rgb;
	fb += texture(feedback, suv + vec2(0.0, 0.0012)).rgb;
	fb += texture(feedback, suv - vec2(0.0, 0.0012)).rgb;
	fb *= 0.2;

	// --- decaimiento (nunca amplifica: el multiplicador total queda < 1,
	// igual que en feedbackballs.frag, asi las trails se apagan solas) ---
	float d = mapr(decay, 0.85, 1.0) * mapr(gain, 0.3, 1.0);
	fb *= d;

	// --- inyeccion de energia en el mouse ---
	float r = mapr(inject_size, 0.02, 0.35);
	float glow = exp(-md * md / max(r * r, 0.0001)) * mapr(inject_intensity, 0.0, 3.0);
	if (audio_pulse > 0.5)
	{
		glow *= 0.35 + audio_trigger * 2.8;
	}
	vec3 col = hsb2rgb(vec3(mapr(hue, 0.0, 1.0), mapr(sat, 0.0, 1.0), 1.0));
	fb += col * glow;

	// --- clamp + piso de ruido: lo que quedo por debajo del umbral se
	// apaga del todo (auto-limpieza total, y el color deja de saturar) ---
	fb = clamp(fb, 0.0, 1.0);
	fb *= step(0.003, length(fb));

	fragColor = vec4(fb, 1.0);
}
