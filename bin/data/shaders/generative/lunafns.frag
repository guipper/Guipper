#pragma include "../common.frag"

#define t time*.5
#define det .01

uniform float moonspeed;
uniform float darksideofthemoon;
uniform float size;

float luncol;
vec3 moon_pos;

mat2 rot(float a) {
    float si = sin(a);
    float co = cos(a);
    return mat2(co,si,-si,co);
}

float sphere(vec3 p, vec3 rd, float r){
	float b = dot( -p, rd ), i = b*b - dot(p,p) + r*r;
	return i < 0. ?  -1. : b - sqrt(i);
}

float hash(vec2 p)
{
   return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float ksetmoon(vec3 p) {
	p=(p*.4+vec3(.14,.31,.51))*1.5;
	float l=1.;
	float ln=0.;
	float lnprev=0.;
	float expsmooth=0.;
	for (int i=0; i<13; i++) {
		p.xyz=abs(p.xyz);
		p=p/dot(p,p);
		p=p*2.-vec3(1.);
		if (mod(float(i),2.)>0.) {
			lnprev=ln;
			ln=length(p);
			expsmooth+=exp(-1./abs(lnprev-ln));
		}
	}
	return expsmooth;
}

float de(vec3 p) {
    p += moon_pos;
    p.xz *= rot(t*moonspeed);
    float k = ksetmoon(p);
    p -= moon_pos;
    luncol = 1.-k * .15;
    float sph1 = length(p + moon_pos) - .9 + k * .003;
    return sph1;
}

float shadow(vec3 p, vec3 ldir) {
    float sh = 1.;
    p -= det * ldir;
    for (int i = 0; i < 50; i++) {
        p -= det * ldir;
        float d = de(p);
        if (d < det) {
            sh = 0.;
            break;
        }
    }
    return sh;
}

vec3 nor(vec3 p) {
    vec3 d = vec3(0., det, 0.);
    return normalize(vec3(de(p + d.yxx), de(p + d.xyx), de(p + d.xxy)) - de(p));
}

vec3 shade(vec3 p, vec3 dir, vec3 ldir) {
    float sh = shadow(p, ldir);
    vec3 n = nor(p);
    float amb = .05 + max(0., dot(dir, -n)) * darksideofthemoon;
    float dif = max(0., dot(ldir, -n)) * .8 * sh;
	vec3 r = reflect(ldir,n);
	float spe=pow(max(0.,dot(dir, -r)),5.) * .7 * sh;
    
    // Color luna (gris plateado)
    vec3 color_moon = vec3(0.8, 0.85, 0.9);
	vec3 col = color_moon * luncol;
    
    return (amb + dif) * col + spe * col;
}

vec3 march(vec3 from, vec3 dir) {
	vec3 p;
    vec3 col = vec3(0.); // Fondo negro
    float d, td = 0., md = 100., g=0.;
    
    for (int i=0; i<80; i++) {
    	p = from + dir * td;
    	d = de(p);
        td += d * .8;
        if (d < det || td > md) break;
        g++;
    }
    
    if (d < det) {
        p -= dir * det * 2.;
        // Luz viene desde arriba-derecha ligeramente
        vec3 light_dir = normalize(vec3(0.3, 0.5, -1.0));
        col = shade(p, dir, light_dir);
    }
    // Si no se golpea la luna, col permanece negro (fondo negro)
    
	return col;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution.xy - .5;
    uv.x *= resolution.x/resolution.y;
    
    // Aplicar size al uv para controlar el tamaño de la luna
    uv /= (size * 1.0 + 0.5);
    
    // Luna siempre en el centro
    moon_pos = vec3(0., 0., 0.);
    
    vec3 from = vec3(0., 0., -50.);
    vec3 dir = normalize(vec3(uv, 10.));
    vec3 col = march(from, dir);

    // Calcular alpha basado en la intensidad del color
    float alpha = length(col) / sqrt(3.0); // Normalizar la intensidad del color
    alpha = clamp(alpha, 0.0, 1.0);

	fragColor = vec4(col, alpha); 
}
