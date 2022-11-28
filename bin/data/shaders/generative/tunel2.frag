#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

vec3 objcol;
uniform float movspeed ;

uniform float r1;
uniform float g1;
uniform float b1;

uniform float r2;
uniform float g2; 
uniform float b2;

uniform float radial2;

uniform float sx ;
uniform float sy ; 
uniform float sz ;

float det = .008;
vec3 lightpos1, lightpos2;
float light1, light2;
vec3 light1color = vec3(r1,g1,b1);
vec3 light2color = vec3(r2,g2,b2);

mat2 rot(float a) 
{
    float s=sin(a), c=cos(a);
    return mat2(c,s,-s,c);
}

// copiar las coordenadas radialmente, cant = cantidad de veces, offset = distancia desde el centro (aquí definiría el radio del túnel)
void radialCopy(inout vec2 p, float cant, float offset) 
{
    float d = 3.1416 / cant * 2.;
    float at = atan(p.y, p.x);
    float a = mod(at, d) - d *.5;
    p = vec2(cos(a), sin(a)) * length(p) - vec2(offset,0.);
}

// distancia a una caja
float sdRoundBox( vec3 p, vec3 b, float r )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0) - r;
}
float sdEllipsoid( vec3 p, vec3 r )
{
  float k0 = length(p/r);
  float k1 = length(p/(r*r));
  return k0*(k0-1.0)/k1;
}
float sdCapsule( vec3 p, vec3 a, vec3 b, float r )
{
  vec3 pa = p - a, ba = b - a;
  float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  return length( pa - ba*h ) - r;
}

// función path, devuelve una posición que define el "camino" que sigue el túnel y la cámara para una posición t
// t = time para la cámara, coordenada z para la obtención de distancia (ver más abajo)
vec3 path(float t) 
{
    vec3 p = vec3(sin(t * .1), cos(t * .2), t);
    p.xy += cos(t*.1) * 5.;
    return p;
}


// función de estimación de distancia
float de(vec3 p) 
{
    light1 = length(p - lightpos1) - .1;
    light2 = length(p - lightpos2) - .1;
    p.xy -= path(p.z).xy; // desplazar el tunel en xy según el camino que recorre la cámara y la posición en z (profundidad)
    vec3 p2 = p;
    // obtengo el id de cada aro del túnel antes de usar fract más abajo para repetir la coordenada z
    float id = floor(p2.z);
    p2.xy *= rot(sin(id + time*movspeed*.1)*5.); // roto en xy según sin de time + el id que genera el desfasaje
    radialCopy(p2.xy, 12.+sin(time*.1)*5., mapr(radial2,4.0,10.)); // copiar radialmente
    p2.z = fract(p2.z) - .5; // copiar en z
    //float ring1 = sdRoundBox(p2, vec3(0.01,.32,0.01), 0.2); // un sólo cálculo de la distancia a la caja genera todo el tunel
    
	//p2.z =;
	float ring1 = sdEllipsoid(p2, vec3(mapr(sx,0.5,2.0),mapr(sy,1.0,2.0),mapr(sz,0.1,.2))); // un sólo cálculo de la distancia a la caja genera todo el tunel
    
	float d = min(ring1, min(light1, light2)); // combinación del túnel con la distancia a las luces
    return d;
}

vec3 normal(vec3 p) 
{
    vec2 d = vec2(0., det);
    return normalize(vec3(de(p+d.yxx), de(p+d.xyx), de(p+d.xxy)) - de(p));
}

vec3 shade(vec3 p, vec3 dir)
{
    if (light1<det) return light1color;
    if (light2<det) return light2color;
    vec3 lightdir1 = normalize(lightpos1 - p);
    vec3 lightdir2 = normalize(lightpos2 - p);
    float fade1 = exp(-.2 * distance(p, lightpos1));
    float fade2 = exp(-.2 * distance(p, lightpos2));
    vec3 n = normal(p);
    vec3 dif1 = max(0., dot(lightdir1, n)) * light1color * fade1 * 0.7;
    vec3 dif2 = max(0., dot(lightdir2, n)) * light2color * fade2 * 0.7;
    vec3 ref1 = reflect(lightdir1, n);
    vec3 ref2 = reflect(lightdir2, n);
    vec3 spe1 = pow(max(0., dot(ref1, dir)),10.) * light1color * fade1;
    vec3 spe2 = pow(max(0., dot(ref2, dir)),10.) * light2color * fade2;
    return dif1 + spe1 + dif2 + spe2;
}


vec3 march(vec3 from, vec3 dir) 
{
    float maxdist = 100.;
    float totdist = 0.;
    float steps = 200.;
    float d;
    vec3 p;
    vec3 col = vec3(0.);
    float glow1 = 0., glow2 = 0.;
    for (float i=0.; i<steps; i++)
    {
        p = from + totdist * dir;
        d = de(p);
        if (d < det || totdist > maxdist) break;
        totdist += d;
        glow1 = max(glow1, 1. - light1);
        glow2 = max(glow2, 1. - light2);
    }
    if (d < det) 
    {
        col = shade(p, dir);
    }
    col += pow(glow1, 5.) * light1color;
    col += pow(glow2, 5.) * light2color;
    return col;
}

// devuelve un mat3 para alinear un vector con el vector dir, especificando la dirección que se tomaría como "arriba"
mat3 lookat(vec3 dir, vec3 up) 
{
    dir = normalize(dir);
    vec3 rt = normalize(cross(dir, up));
    return mat3(rt, cross(rt, dir), dir);
}


void main(void)
{
    vec2 uv = (gl_FragCoord.xy - resolution / 2.) / resolution.y;
    float t = time * 1.*movspeed; 
    vec3 from = path(t); // posición de la camara según t
    vec3 adv = path(t + 1.); // posición de la cámara en t + 1 (un poco después), para obtener vector donde apunta
    vec3 look = normalize(adv - from); // vector hacia donde mira la cámara
    vec3 dir = normalize(vec3(uv, 1.)); // obtencion de la dir del rayo
    dir = lookat(look, vec3(0., 1., 0.)) * dir; // alineación de la dir con el vector hacia donde apunta la cámara
    // las luces siguen también el camino, aunque se le agregan otros movimiento
    lightpos1 = path(t + 5. * (1. + sin(time*movspeed / 2.)*.5)) + vec3(sin(time*movspeed) * 0.5, 0.0, 0.); 
    lightpos2 = path(t + 15.) + vec3(-sin(time*movspeed) * 1., 0., 0.);
     vec3 col = march(from, dir);
    fragColor = vec4(col*5., 1.);
}