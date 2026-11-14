#pragma include "../common.frag"

// ============================================================
// FLUIDO_REAL - solver de Navier-Stokes REAL, multi-pass
// //#GUIPPER_FLUID  (marcador: activa el pipeline en C++)
//
// Portado de WebGL-Fluid-Enhanced (michaelbrusegard) y
// WebGL-Fluid-Simulation (paveldogreat). Nada de feedback
// trucho: es el solver de verdad, con adveccion semi-Lagrangiana,
// proyeccion (curl, vorticity confinement, divergencia, presion
// Jacobi, gradient subtract) y dye advectado.
//
// Como funciona con Guipper: el C++ detecta el marcador, crea los
// FBOs internos (velocity, dye, pressure, divergence, curl en
// RGBA16F) y corre este mismo .frag 10 veces por frame en orden
// (splat vel, splat dye, curl, vorticity, divergence, pressure xN,
// gradientSubtract, advect vel, advect dye, display), cambiando
// `uniform int pass` y los samplers en cada pasada (ping-pong).
// El mouse inyecta fuerza (velocity) y color (dye); el click hace
// una explosion de splats.
//
// Sliders: curl, velocity_dissipation, dye_dissipation,
// splat_radius, splat_force, dye_hue, dye_sat, dye_bright,
// audio_pulse, shading, burst, fluid_sim_res, fluid_dye_res,
// pressure_iters.
// ============================================================

uniform float curl = 0.3;                // vorticity confinement (0-100)
uniform float velocity_dissipation = 0.1;// disipacion de velocidad (0-2)
uniform float dye_dissipation = 0.5;     // disipacion del color (0-2)
uniform float splat_radius = 0.08;       // radio del splat en UV (0.001-0.02 x aspect)
uniform float splat_force = 0.5;         // fuerza del splat (0-12000)
uniform float dye_hue = 0.6;             // tono del color inyectado
uniform float dye_sat = 0.9;             // saturacion
uniform float dye_bright = 0.6;          // brillo del color inyectado
uniform float audio_pulse = 0.0;         // 1 = el brillo pulsa con audio_trigger
uniform float shading = 1.0;             // 1 = falso 3D (luz por gradiente) en display
uniform float burst = 1.0;               // 1 = explosion de splats al hacer click
uniform float fluid_sim_res = 0.25;      // escala de resolucion de la simulacion (la lee C++)
uniform float fluid_dye_res = 0.5;       // escala de resolucion del dye (la lee C++)
uniform float pressure_iters = 0.5;      // iteraciones de presion (C++: 1-40)

uniform int pass; // @internal  (lo setea C++ en cada pasada del pipeline)
uniform float u_dt; // @internal  (dt por frame, clamp 1/60)
uniform float u_aspect; // @internal  (aspecto de la simulacion)
uniform float u_pressed_edge; // @internal  (1 solo el frame en que arranca el click)
uniform vec2 u_texelSize; // @internal  (1/resolucion del target actual)
uniform vec4 u_mouse; // @internal  (xy = posicion en la caja, z = boton, w = cursor sobre la caja)
uniform vec2 u_mouse_delta; // @internal  (delta del mouse en UV)

uniform sampler2D u_velocity; // @internal
uniform sampler2D u_source;   // @internal  (fuente de adveccion: velocity o dye segun pass)
uniform sampler2D u_dye;      // @internal
uniform sampler2D u_pressure; // @internal
uniform sampler2D u_divergence; // @internal
uniform sampler2D u_curl;     // @internal

uniform float audio_trigger;  // (global: filtrado de sliders por jp_shader_globals)

// Hash 1D -> [0,1] sin textura, para el burst de splats del click.
float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void main()
{
	vec2 uv = gl_FragCoord.xy * u_texelSize;

	// ---------- PASS 0: SPLAT VELOCITY ----------
	// Inyecta el delta del mouse como fuerza en el campo de velocidad.
	if (pass == 0)
	{
		vec2 base = texture(u_velocity, uv).xy;
		float radius = mapr(splat_radius, 0.001, 0.02) * max(u_aspect, 1.0);
		vec2 d = u_mouse_delta * mapr(splat_force, 0.0, 12000.0);
		vec2 p = uv - u_mouse.xy;
		p.x *= u_aspect;
		float m = exp(-dot(p, p) / radius);
		// Se inyecta mientras el mouse se mueve (hover pinta, como la web)
		float active = length(u_mouse_delta) > 0.00001 ? 1.0 : 0.0;
		base += d * m * active * u_mouse.w;

		// Burst del click: remolinos aleatorios alrededor del cursor
		if (burst > 0.5 && u_pressed_edge > 0.5)
		{
			float frame = floor(time * 60.0);
			for (int i = 0; i < 6; i++)
			{
				vec2 rnd = vec2(hash12(vec2(float(i) * 7.13, frame)),
				                hash12(vec2(float(i) * 3.71, frame + 11.7)));
				vec2 bp = u_mouse.xy + (rnd - 0.5) * 0.25;
				vec2 bq = uv - bp;
				bq.x *= u_aspect;
				float bm = exp(-dot(bq, bq) / (radius * 3.0));
				vec2 bd = (rnd - 0.5) * 1000.0;
				base += bd * bm;
			}
		}
		fragColor = vec4(base, 0.0, 1.0);
		return;
	}

	// ---------- PASS 1: SPLAT DYE ----------
	// Inyecta color en la posicion del mouse.
	else if (pass == 1)
	{
		vec3 base = texture(u_dye, uv).rgb;
		float radius = mapr(splat_radius, 0.001, 0.02) * max(u_aspect, 1.0);
		float bright = mapr(dye_bright, 0.05, 1.0);
		if (audio_pulse > 0.5)
		{
			bright *= 0.4 + audio_trigger * 2.2;
		}
		vec3 col = hsb2rgb(vec3(mapr(dye_hue, 0.0, 1.0), mapr(dye_sat, 0.0, 1.0), bright));
		vec2 p = uv - u_mouse.xy;
		p.x *= u_aspect;
		float m = exp(-dot(p, p) / radius);
		float active = length(u_mouse_delta) > 0.00001 ? 1.0 : 0.0;
		base += col * m * active * u_mouse.w * 0.6;

		// Burst del click: 6 manchones de color con tono jittereado
		if (burst > 0.5 && u_pressed_edge > 0.5)
		{
			float frame = floor(time * 60.0);
			for (int i = 0; i < 6; i++)
			{
				vec2 rnd = vec2(hash12(vec2(float(i) * 7.13, frame)),
				                hash12(vec2(float(i) * 3.71, frame + 11.7)));
				vec2 bp = u_mouse.xy + (rnd - 0.5) * 0.25;
				vec2 bq = uv - bp;
				bq.x *= u_aspect;
				float bm = exp(-dot(bq, bq) / (radius * 3.0));
				vec3 bcol = hsb2rgb(vec3(fract(mapr(dye_hue, 0.0, 1.0) + rnd.x * 0.3),
				                         mapr(dye_sat, 0.0, 1.0), bright));
				base += bcol * bm * 4.0;
			}
		}
		fragColor = vec4(base, 1.0);
		return;
	}

	// ---------- PASS 2: CURL ----------
	// Vorticidad del campo de velocidad (para el confinement).
	else if (pass == 2)
	{
		float L = texture(u_velocity, uv - vec2(u_texelSize.x, 0.0)).y;
		float R = texture(u_velocity, uv + vec2(u_texelSize.x, 0.0)).y;
		float T = texture(u_velocity, uv + vec2(0.0, u_texelSize.y)).x;
		float B = texture(u_velocity, uv - vec2(0.0, u_texelSize.y)).x;
		float vorticity = R - L - T + B;
		fragColor = vec4(0.5 * vorticity, 0.0, 0.0, 1.0);
		return;
	}

	// ---------- PASS 3: VORTICITY CONFINEMENT ----------
	// Reinyecta la vorticidad perdida por la disipacion numerica:
	// es lo que da los remolinos vivos tipo liquido.
	else if (pass == 3)
	{
		float L = texture(u_curl, uv - vec2(u_texelSize.x, 0.0)).x;
		float R = texture(u_curl, uv + vec2(u_texelSize.x, 0.0)).x;
		float T = texture(u_curl, uv + vec2(0.0, u_texelSize.y)).x;
		float B = texture(u_curl, uv - vec2(0.0, u_texelSize.y)).x;
		float C = texture(u_curl, uv).x;

		vec2 force = 0.5 * vec2(abs(T) - abs(B), abs(R) - abs(L));
		force /= length(force) + 0.0001;
		force *= mapr(curl, 0.0, 100.0) * C;
		force.y *= -1.0;

		vec2 velocity = texture(u_velocity, uv).xy;
		velocity += force * u_dt;
		velocity = min(max(velocity, -1000.0), 1000.0);
		fragColor = vec4(velocity, 0.0, 1.0);
		return;
	}

	// ---------- PASS 4: DIVERGENCE ----------
	else if (pass == 4)
	{
		float L = texture(u_velocity, uv - vec2(u_texelSize.x, 0.0)).x;
		float R = texture(u_velocity, uv + vec2(u_texelSize.x, 0.0)).x;
		float T = texture(u_velocity, uv + vec2(0.0, u_texelSize.y)).y;
		float B = texture(u_velocity, uv - vec2(0.0, u_texelSize.y)).y;

		vec2 C = texture(u_velocity, uv).xy;
		// Condiciones de borde: reflejan la componente normal.
		if (uv.x < u_texelSize.x) { L = -C.x; }
		if (uv.x > 1.0 - u_texelSize.x) { R = -C.x; }
		if (uv.y > 1.0 - u_texelSize.y) { T = -C.y; }
		if (uv.y < u_texelSize.y) { B = -C.y; }

		float div = 0.5 * (R - L + T - B);
		fragColor = vec4(div, 0.0, 0.0, 1.0);
		return;
	}

	// ---------- PASS 5: PRESSURE (Jacobi) ----------
	// Una iteracion del solve de Poisson. C++ lo corre N veces
	// (pressure_iters) con ping-pong entre los dos buffers.
	else if (pass == 5)
	{
		float L = texture(u_pressure, uv - vec2(u_texelSize.x, 0.0)).x;
		float R = texture(u_pressure, uv + vec2(u_texelSize.x, 0.0)).x;
		float T = texture(u_pressure, uv + vec2(0.0, u_texelSize.y)).x;
		float B = texture(u_pressure, uv - vec2(0.0, u_texelSize.y)).x;
		float divergence = texture(u_divergence, uv).x;
		float pressure = (L + R + B + T - divergence) * 0.25;
		fragColor = vec4(pressure, 0.0, 0.0, 1.0);
		return;
	}

	// ---------- PASS 6: GRADIENT SUBTRACT ----------
	// Resta el gradiente de presion: deja el campo de velocidad
	// libre de divergencia (incompresible).
	else if (pass == 6)
	{
		float L = texture(u_pressure, uv - vec2(u_texelSize.x, 0.0)).x;
		float R = texture(u_pressure, uv + vec2(u_texelSize.x, 0.0)).x;
		float T = texture(u_pressure, uv + vec2(0.0, u_texelSize.y)).x;
		float B = texture(u_pressure, uv - vec2(0.0, u_texelSize.y)).x;
		vec2 velocity = texture(u_velocity, uv).xy;
		velocity.xy -= vec2(R - L, T - B);
		fragColor = vec4(velocity, 0.0, 1.0);
		return;
	}

	// ---------- PASS 7: ADVECT VELOCITY ----------
	// Adveccion semi-Lagrangiana: cada particula traza hacia atras
	// por el campo de velocidad y samplea el frame anterior.
	else if (pass == 7)
	{
		vec2 coord = uv - u_dt * texture(u_velocity, uv).xy * u_texelSize;
		vec4 result = texture(u_source, coord);
		float decay = 1.0 + mapr(velocity_dissipation, 0.0, 2.0) * u_dt;
		fragColor = result / decay;
		return;
	}

	// ---------- PASS 8: ADVECT DYE ----------
	else if (pass == 8)
	{
		vec2 coord = uv - u_dt * texture(u_velocity, uv).xy * u_texelSize;
		vec4 result = texture(u_source, coord);
		float decay = 1.0 + mapr(dye_dissipation, 0.0, 2.0) * u_dt;
		fragColor = result / decay;
		return;
	}

	// ---------- PASS 9: DISPLAY ----------
	// El dye final a la pantalla (fbo de la caja), con sombreado
	// falso 3D opcional.
	else
	{
		vec3 c = texture(u_dye, uv).rgb;
		if (shading > 0.5)
		{
			vec3 lc = texture(u_dye, uv - vec2(u_texelSize.x, 0.0)).rgb;
			vec3 rc = texture(u_dye, uv + vec2(u_texelSize.x, 0.0)).rgb;
			vec3 tc = texture(u_dye, uv + vec2(0.0, u_texelSize.y)).rgb;
			vec3 bc = texture(u_dye, uv - vec2(0.0, u_texelSize.y)).rgb;

			float dx = length(rc) - length(lc);
			float dy = length(tc) - length(bc);

			vec3 n = normalize(vec3(dx, dy, length(u_texelSize)));
			vec3 l = vec3(0.0, 0.0, 1.0);

			float diffuse = clamp(dot(n, l) + 0.7, 0.7, 1.0);
			c *= diffuse;
		}
		fragColor = vec4(c, 1.0);
		return;
	}
}
