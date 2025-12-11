#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float altura;
uniform float offsetx;

// Simplex 2D noise
//
vec3 permute2(vec3 x) { return mod(((x*34.0)+1.0)*x, 289.0); }

float snoise2(vec2 v){
  const vec4 C = vec4(0.211324865405187, 0.366025403784439,
           -0.577350269189626, 0.024390243902439);
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  i = mod(i, 289.0);
  vec3 p = permute2( permute2( i.y + vec3(0.0, i1.y, 1.0 ))
  + i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy),
    dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

// Función para generar múltiples octavas de noise (FBM - Fractional Brownian Motion)
float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for(int i = 0; i < octaves; i++) {
        value += amplitude * snoise2(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

// Función para dibujar nubes
float clouds(vec2 uv, float y_offset) {
    vec2 cloud_uv = vec2(uv.x * 3.0 + time * 0.02, (uv.y - y_offset) * 8.0);
    float n = fbm(cloud_uv, 3);
    n = smoothstep(0.3, 0.8, n);
    return n;
}

float mountain(vec2 _uv,float _freq1,float _offsetx,float _ele,float _altura,int _picudo){
	
	// El parámetro altura controla la elevación general del paisaje
	float base_height = _altura * 0.5; // Normalizar altura entre 0 y 0.5
	
	// Generar montañas lejanas con picos (usando FBM para más detalle)
	float mountain_far_noise = fbm(vec2(_uv.x * _freq1+_offsetx, 0.0), _picudo) * _ele;
	float mountain_far_height = 0.35 + base_height + mountain_far_noise;
	float mask_far = step(mountain_far_height, _uv.y);
	return mask_far;
	
}

float nubes(vec2 _uv){

	float moffsetx = offsetx*10;
	
	float malt =mapr(altura,-1.0,1.0);
	_uv.y-=malt;
	vec2 p = vec2(0.5) - _uv;
	

	
	float ancho = 1.0;
	// Ajustar la altura máxima de las nubes según el parámetro altura
	float alto = 0.4 ; // Las nubes bajan cuando altura aumenta
	float y = -0.8;
	
	float snx = sin(_uv.x*2.+time*0.1);
	
	float m6 = mountain(_uv,2.0,20.+moffsetx+time*0.025,0.35,y+0.9,4); //GROUND2;
	float m7 = mountain(_uv,2.0,6.+moffsetx+time*0.025,1.5,y,1); //GROUND2;
	
	
	m6 = m6 * step(_uv.x,ancho);
	m6 = m6 * step(1.-_uv.x,ancho);
	
	
	
	m6 = m6 * step(_uv.y,alto);
	
	//m6 = step(1.-_uv.x-ancho,m6);
	//m6 = step(1.-_uv.y+0.13,m6);
	//m6-= step(_uv.x-ancho,m6);
	
	
	return m6;
}
void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = gl_FragCoord.xy/resolution.xy;
	uv.y-=(1.-altura)*0.001;
	// Colores del paisaje
	vec3 sky_color = vec3(121./255., 185./255., 216./255.); // Cielo azul
	vec3 cloud_color = vec3(1.0, 1.0, 1.0); // Nubes blancas
	vec3 mountain_far = vec3(64./255., 136./255., 186./255.); // Montañas lejanas azul oscuro
	vec3 mountain_mid = vec3(130./255., 130./255., 116./255.); // Montañas medias azul medio
	
	
	
	vec3 ground_color1 = vec3(181./255., 160./255., 139./255.); // Tierra/arena 1
	vec3 ground_color2 = vec3(179./255., 157./255., 120./255.); // Tierra/arena 2
	
	vec3 agua = vec3(42./255.,167./255.,204./255.);
	// Empezar con el cielo
	vec3 dib = sky_color;
	

	float malt = mapr(altura,-1.0,1.0)+0.25;
	float moffsetx = offsetx*10;
	
	float m1 = mountain(uv,5.0,500.+moffsetx,0.15,malt-0.05,8);
	float m2 = mountain(uv,2.0,10030.+moffsetx,0.2,malt+.25,3);
	float m3 = mountain(uv,2.0,40000.+moffsetx,0.00,malt+.25,4); // AGUA
	float m4 = mountain(uv,2.0,200400.+moffsetx,0.02,malt+.55,4); //GROUND1;
	
	float m5 = mountain(uv,5.0,2415.+moffsetx,0.02,malt+.95,4); //GROUND2;
	float nub1 = nubes(uv);
	
	dib = mix(dib, mountain_far, m1);
	dib = mix(dib,agua,m3);
	dib = mix(dib,mountain_mid,m2);
	
	dib = mix(dib,ground_color2,m4);
	dib = mix(dib,ground_color1,m5);
	dib = mix(dib,cloud_color,nub1);
	//dib = mix(dib, ground_color, mask_ground);
	
	
    fragColor = vec4(dib, 1.0);
}