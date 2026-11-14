#pragma include "../common.frag"

// ============================================================
// REACTION_DIFFUSION - Gray-Scott simulado dentro del feedback
// Estado: U en canal R, V en canal G (el canal B es libre y se
// usa como tint decorativo). La simulacion corre sola frame a
// frame. El mouse inyecta quimico: arrastra para sembrar.
// Sliders: feed, kill, diff_u, diff_v, dt, inject, inject_size,
//          blue, audio_pulse.
// Regimenes clasicos (feed/kill): corales 0.037/0.06,
// manchas 0.03/0.062, ondas 0.014/0.045.
// ============================================================

uniform float feed = 0.37;       // F (0-0.1)
uniform float kill = 0.5;        // K (0-0.12)
uniform float diff_u = 0.53;     // Du (0-0.3)
uniform float diff_v = 0.53;     // Dv (0-0.15)
uniform float dt = 0.5;          // paso temporal (0-1.5)
uniform float inject = 0.5;      // cuanto U tira el mouse
uniform float inject_size = 0.12;// radio de la inyeccion
uniform float blue = 0.5;        // tint azul decorativo (canal B)
uniform float audio_pulse = 0.0; // 1 = el audio perturba la V

uniform int boxframeNum;
uniform float audio_trigger;

float rnd(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	float fx = resolution.x / resolution.y;
	vec2 p  = vec2(uv.x * fx, uv.y);        // espacio cuadrado
	vec2 mp = vec2(mouse.x * fx, mouse.y);  // mouse en espacio cuadrado

	vec2 st = texture(feedback, uv).rg;

	// Seed: los primeros 2 frames (o si el buffer quedo vacio)
	// inicializan U lleno y V con ruido, que es el estado clasico
	// de arranque de Gray-Scott.
	if (boxframeNum < 2 || (st.x + st.y) < 0.01)
	{
		float v = rnd(floor(p * 256.0)) * 0.12;
		float tint = snoise(p * 8.0 + 7.0) * 0.5 + 0.5;
		float b = mapr(blue, 0.0, 1.0);
		fragColor = vec4(1.0, v, max(tint * b, v * b * 1.8), 1.0);
		return;
	}

	// Laplaciano con celda isotropica (correccion de aspect).
	// Paso de texel en espacio cuadrado:
	float e = 1.0 / resolution.y;
	// Mismo paso expresado en uv:
	float ex = e / fx;

	vec2 c  = texture(feedback, uv).rg;
	vec2 r1 = texture(feedback, uv + vec2( ex, 0.0)).rg;
	vec2 l1 = texture(feedback, uv + vec2(-ex, 0.0)).rg;
	vec2 u1 = texture(feedback, uv + vec2(0.0,  e)).rg;
	vec2 d1 = texture(feedback, uv + vec2(0.0, -e)).rg;
	vec2 lap = r1 + l1 + u1 + d1 - 4.0 * c;

	float u = c.x;
	float v = c.y;

	float Du = mapr(diff_u, 0.0, 0.3);
	float Dv = mapr(diff_v, 0.0, 0.15);
	float F  = mapr(feed, 0.0, 0.1);
	float K  = mapr(kill, 0.0, 0.12);
	float h  = mapr(dt, 0.0, 1.5);

	// Reaccion Gray-Scott
	float uvv = u * v * v;
	u += (Du * lap.x - uvv + F * (1.0 - u)) * h;
	v += (Dv * lap.y + uvv - (F + K) * v) * h;

	// Inyeccion del mouse: U sube, V baja (sembra un "hueco")
	float r = mapr(inject_size, 0.01, 0.08);
	float fall = exp(-dot(p - mp, p - mp) / max(r * r, 0.0001));
	u += mapr(inject, 0.0, 0.6) * fall;
	v -= mapr(inject, 0.0, 0.3) * fall;

	if (audio_pulse > 0.5)
	{
		v += audio_trigger * 0.03;
	}

	u = clamp(u, 0.0, 1.0);
	v = clamp(v, 0.0, 1.0);

	// Canal B libre: tint azul decorativo que SI responde al slider.
	// Con blue en 0 el canal queda negro (look clasico rojo/verde);
	// subiendo el slider, el fondo se tiñe y los patrones V brillan
	// en azul (v * b * 1.8), asi el efecto es visible al toque.
	float tint = snoise(p * 8.0 + 7.0) * 0.5 + 0.5;
	float b = mapr(blue, 0.0, 1.0);

	fragColor = vec4(u, v, max(tint * b, v * b * 1.8), 1.0);
}
