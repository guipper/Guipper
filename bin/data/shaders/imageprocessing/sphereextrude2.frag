#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float extrude;
uniform float txtile;
uniform float rotate;
uniform float offsetx=.5;
uniform float offsety=.5;
uniform float tilex=.5;
uniform float tiley=.5;
uniform float tilez=.5;
uniform float speedz=.1;
uniform sampler2D tx;
// ESFERA Y CUBO DE DIFERENTES COLORES

// Agrego comentarios a lo que cambia con respecto
// al ejemplo anterior de una esfera y un solo color


// VARIABLES GLOBALES

float det = .001;
float maxdist = 50.;
int maxsteps = 100;
// variable donde se establecerá el color de cada objeto
vec3 objcol;

// FUNCION DE ROTACION

mat2 rot(float a) {
    float s=sin(a), c=cos(a);
    return mat2(c,s,-s,c);
}


// FUNCIONES DE DISTANCIA PRIMITIVAS 

float sphere(vec3 p, float rad) 
{
    return length(p) - rad;
}

// función de distancia a una "caja", en el vec3 c van las dimensiones
// alto, ancho, largo
float box(vec3 p, vec3 c)
{
    p=abs(p)-c;
    return length(max(vec3(0.), p) + min(0, max(p.z, max(p.x, p.y))));
}


// FUNCION DE ESTIMACION DE DISTANCIA

float de(vec3 p) 
{	
	
    p.xz *= rot(rotate*TWO_PI);
	
	
	float maptilex = mapr(tilex,0.0,20.0);
	float maptiley = mapr(tiley,0.0,20.0);
	float maptilez = mapr(tilez,0.0,20.0);
	
	p.x = mod(p.x,maptilex) -maptilex/2;
	p.y = mod(p.y,maptiley) -maptiley/2;
	p.z = mod(p.z,maptilez) -maptilez/2;
	 // guardamos la distancia a la esfera en sph
	//p.xy = mod(p.xy,5.0) -2.5;
	//p.z = mod(p.z,5.0) -2.5;
	
	//p.x = mod(p.x,t
	
	
	vec3 psph = p;
    // desplazamos x para ubicar la esfera en -3.
    //p.x -= 4.;

   
	

	vec2 pselect = mix(p.xz,p.yz,sin(rotate*4.+time+p.y)*.5+.5);
	
	vec4 txtx = texture2D(tx,abs(p.xy*txtile*.25));
	
	
	vec3 ptx = vec3(p.x+mapr(offsetx,-10.,10.),
					p.y+mapr(offsety,-10.,10.),p.z);
		 txtx = texture2D(tx,abs(ptx.xy*txtile*.25));
	float prom = length(txtx);
	

	float def = length(sin(p*5.+time*2.))*.1;
		  def = snoise(p.xy+time)*10.;
		  
		  
    float sph = sphere(psph, 10.3*size+prom*extrude*4.);
		  
    //desplazamos x para la posicion del cubo en +3.
    //p.x += 5.;

    // guardamos la distancia al cubo en box
    float box = box(p, vec3(2.));
	
	box = opSmoothSubtraction(sphere(p,1.8),box,.9);
	//p.y+=4.5;
    float line = sdVerticalCapsule(p,10.0,1.2);
    // la función min sirve en este caso para combinar objetos en la escena
    // aquí la usamos con sph y box para que aparezcan ambos y almacenamos
    // el resultado en d que va a ser la distancia que devolverá la función
    float d = min(sph, box);
		  d = sph;
	      //d = opSmoothUnion(sph,box,1.0);
		  //d = opSmoothUnion(d,line,0.5);
    // para darle a la esfera y el cubo diferentes colores, simplemente
    // tenemos que comparar d con sph y box
    // si d == sph, estamos más cerca de la esfera 
    // si d == box, estamos más cercanos al cubo
    // en base a eso elegimos el color de cada objeto y lo almacenamos en objcol

    if (abs(d-sph)<1.) objcol = txtx.rgb;
   // if (d == box) objcol = vec3(1.,0.2, 0.);
	//if (d == line) objcol = vec3(1., 0.0, 0.);

    return d*.7;
}

// FUNCION NORMAL

vec3 normal(vec3 p) 
{   
    vec2 d = vec2(0., det);
    
    return normalize(vec3(de(p + d.yxx), de(p + d.xyx), de(p + d.xxy)) - de(p));
}

// FUNCION SHADE

vec3 shade(vec3 p, vec3 dir) {
    
    vec3 lightdir = normalize(vec3(1.5, 1., -1.)); 
    
    // aquí definimos el color del objeto según la variable objcolor seteada en la funcion
    // de distancia. La guardamos en col antes de llamar a la funcion normal
    vec3 col = objcol;
    
    
    vec3 n = normal(p);
    
    float diff = max(0., dot(lightdir, n));
    
    vec3 refl = reflect(dir, n);
    
    float spec = pow(max(0., dot(lightdir, refl)), 20.);
    
    float amb = .1;
    
    return col*(amb + diff) + spec * .7;
    
}



// FUNCION DE RAYMARCHING

vec3 march(vec3 from, vec3 dir) 
{

    float d, td=0.;
    vec3 p, col;


    for (int i=0; i<maxsteps; i++) 
    {
        p = from + td * dir;

        d = de(p);

        if (d < det || td > maxdist) break;

        td += d;
    }

    if (d < det)
    {
        p -= det * dir;
        col = shade(p, dir);
    }
    else 
    {
        // para este background estoy ubicando la posición en el fondo de la escena
        // from + distancia máxima * dirección del rayo
        p = from + maxdist * dir;
        // usamos esta posición para dibujar un fondo
        // en este caso es un fondo simple usando la función sin
        col = sin(p*2.+time)*0.0;
    }
    return col;    
}

// MAIN

void main(void)
{
 
    vec2 uv = gl_FragCoord.xy/resolution.xy - .5; 
    uv.x *= resolution.x / resolution.y; 
    
    vec3 from = vec3(0., 0., time*5.);
    vec3 dir = normalize(vec3(uv, 1.));
    vec3 col = march(from, dir);
    fragColor = vec4(col, 1.);
}