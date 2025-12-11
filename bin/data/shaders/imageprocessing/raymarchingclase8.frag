#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
uniform float size;
uniform float extrude;
uniform float txtile;
uniform float rotate;
uniform float offsetx=.5;
uniform float offsety=.5;
//uniform sampler2D tx;

uniform sampler2D fondo;
// ESFERA Y CUBO DE DIFERENTES COLORES

// Agrego comentarios a lo que cambia con respecto
// al ejemplo anterior de una esfera y un solo color


// VARIABLES GLOBALES

float det = .001;
float maxdist = 30.;
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
   // p.xz *= rot(rotate*TWO_PI+time*.1);

	vec3 psph = p;

	vec2 pselect = mix(p.xz,p.yz,sin(rotate*4.+time+p.y)*.5+.5);
	
	vec2 uv2 = gl_FragCoord.xy/resolution.xy;
	uv2*=0.25;
	vec4 tx = texture2D(fondo,uv2);	
	
	float prom = length(tx.rgb);
	
    float sph = sphere(psph, 15.3*size+prom*.3);
	
	vec3 pbox = p;
	//pbox.x -= sin(time)*2.0;
	
	
	
	float maptile = 2.2;
	pbox.x+=time;
	pbox.x = mod(pbox.x,maptile) -maptile/2;
	pbox.y = mod(pbox.y,maptile) -maptile/2;
	pbox.z = mod(pbox.z,maptile) -maptile/2;
    float box = box(pbox, vec3(0.06));
	//box = opSmoothSubtraction(box,sphere(p,2.8),0.5);
	
	box = opSmoothIntersection(box,sphere(p,5.0),0.5);
	//box = opSmoothSubtraction(sphere(p,1.8),box,.9);
	//p.y+=4.5;
	
	vec3 pline = p;
	pline.y+=sin(time)*4.;
    float line = sdVerticalCapsule(pline,2.0,1.2);

    float d = sph;
	      d = opSmoothUnion(sph,box,1.0);
		  d = opSmoothUnion(d,line,0.5);


    //if (abs(d-sph)<1.) objcol = vec3(1.0,0.0,0.0);
	//if (abs(d-box)<1.) objcol = vec3(1.0,1.0,0.0);
	
	
	
	
	vec3 sphcol = vec3(sin(p.x*1.)*.5+.5,sin(p.y*1.)*.5+.5,sin(p.z*1.)*.5+.5);
	sphcol.rgb = tx.rgb;
	 objcol = mix(vec3(1.0,0.0,0.0),sphcol,box);
	 objcol = mix(vec3(0.0,0.0,1.0),objcol,line);
	
	//objcol = tx.rgb;
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
	vec2 uv2 = gl_FragCoord.xy/resolution.xy;
	vec4 tx = texture2D(fondo,uv2);	
    /*if (d < det)
    {
        p -= det * dir;
        col = shade(p, dir);
    }
    else 
    {
        p = from + maxdist * dir;
        col = sin(p*2.+time)*0.0;
		
		
		
		col = tx.rgb;
    }*/
	
	p -= det * dir;
	col = mix(tx.rgb,mix(shade(p, dir),tx.rgb,0.0),d<det);
	
	//col+= tx.rgb;
    return col;    
}

// MAIN

void main(void)
{
 
    vec2 uv = gl_FragCoord.xy/resolution.xy - .5; 
    uv.x *= resolution.x / resolution.y; 
    
    vec3 from = vec3(0., 0., -15.);
    vec3 dir = normalize(vec3(uv, 1.));
    vec3 col = march(from, dir);
    fragColor = vec4(col, 1.);
}