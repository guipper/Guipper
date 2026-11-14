#pragma include "../common.frag"

// ============================================================
// FLUIDPASS 1/10 - SPLAT VELOCITY
// Inyecta el delta del mouse como fuerza en el campo de velocidad.
// Corresponde a splatShader de WebGL-Fluid-Simulation (paveldogreat).
//
// CONVENCION DEL CAMPO DE VELOCIDAD (fbo RGBA8):
//   .rg = velocidad en texel/seg (web convention), codificada:
//         encode: clamp(v,-2000,2000)*0.0005+0.5
//         decode: (c-0.5)*2000
//   .ba = "carrier": posicion del mouse del frame anterior, para
//         poder calcular el delta del mouse dentro del shader.
// La caja lee la velocidad final del frame anterior (fluidpass_advect_vel),
// le suma el impulso del mouse y escribe el resultado en su propio fbo.
//
// Sliders: splat_radius, splat_force, burst (el burst se dispara con
// audio_trigger, el kick).
// ============================================================

uniform float splat_radius = 0.08; // radio del splat (mapr 0.001-0.02 x aspect)
uniform float splat_force = 0.5;   // fuerza del splat (mapr 0-12000)
uniform float burst = 1.0;         // 1 = explosion de remolinos con el kick

uniform sampler2D texture1; // velocidad actual (fluidpass_advect_vel)

uniform int boxframeNum;    // global: contador de frames de la caja
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

	// Seed: los primeros 2 frames escriben el campo neutro (0.5 = velocidad 0)
	// para que el anillo de velocidad no arranque con el offset del negro.
	if (boxframeNum < 2)
	{
		fragColor = vec4(0.5, 0.5, mouse.xy);
		return;
	}

	// Delta del mouse entre este frame y el anterior (guardado en .ba).
	vec2 delta = mouse.xy - feedback.ba;
	delta = clamp(delta, -0.5, 0.5);

	vec2 base = (texture(texture1, uv).rg - 0.5) * 2000.0;

	float radius = mapr(splat_radius, 0.001, 0.02) * max(aspect, 1.0);
	vec2 d = delta * mapr(splat_force, 0.0, 12000.0);
	vec2 p = uv - mouse.xy;
	p.x *= aspect;
	float m = exp(-dot(p, p) / radius);
	float active = length(delta) > 0.00001 ? 1.0 : 0.0;
	base += d * m * active;

	// Burst con el kick: remolinos aleatorios alrededor del cursor.
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
			vec2 bd = (rnd - 0.5) * 1000.0;
			base += bd * bm;
		}
	}

	// Salida: velocidad codificada + carrier (mouse actual en .ba).
	fragColor = vec4(clamp(base, -2000.0, 2000.0) * 0.0005 + 0.5, mouse.xy);
}
