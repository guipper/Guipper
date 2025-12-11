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
float maxdist = 150.;
int maxsteps = 100;
// variable donde se establecerá el color de cada objeto
vec3 objcol;

// FUNCION DE ROTACION
float rdm(float p){
    p*=1234.56;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}

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



float obj1(vec3 p){
	
	
	float d = 1.0;
	int cnt = 10;
	//p.x-=0.0;	
	float amp = sin(time)*2.+2.;
	//p2.y+=amp;
	for(int i=0; i<cnt;i++){
		vec3 p2 = p;
		float idx = float(i)/float(cnt);
		
		float rdmf = 5.;
		//p2.x+=mapr(rdm(1.+idx*201.),-rdmf,rdmf);
		//p2.y+=mapr(rdm(13.+idx*231.),-rdmf,rdmf);
	
		float amp = 7;
		float t1 = mapr(rdm(321.+idx*1523.),-time,time);
		float t2 = mapr(rdm(36.+idx*1234.),-time,time);
		p2.x+=sin(rdm(1.+idx*201.)+ t1)*amp;
		p2.y+=cos(rdm(13.+idx*231.)+t2)*amp;
	
		d = opSmoothUnion(d,sphere(p2, 1.),0.1);
	}
	
	return d;
}

void radialCopy(inout vec2 p, float cant, float offset) 
{
    float d = 3.1416 / cant * 2.;
    float at = atan(p.y, p.x);
    float a = mod(at, d) - d *.5 ;
    p = vec2(cos(a), sin(a)) * length(p) - vec2(offset,0.);
}



float de(vec3 p) 
{
	//p.xz *= rot(rotate*TWO_PI+time*.1);
	//vec4 tx = texture2D(fondo,uv2);	
    
	vec3 psph = p;	
	vec3 psph2 = p;
	
	//psph2.x +=sin(time)*5.0;
	//psph.x+=sin(time)*5.5;
	float fz = 10.0;
	psph.z+=time*10.;
	
	
	float indexy = floor(psph.z/fz);
	
	psph.z = abs(mod(psph.z,fz))-fz/2.;
	
	psph.xy*=rotate2d(time);
	
	radialCopy(psph.xy,20.0,19.);
	
	//psph.x-=1.;
	//radialCopy(psph.xy,10.0,cos(time)*2.+2.);
	float sph = sphere(psph, 4.3*size);
	float sph2 = obj1(psph2);
	
	
	float d2 = 1.0;
	int cnt = 10;
	//p.x-=0.0;	
	float amp = sin(time)*2.+2.;
	//p2.y+=amp;
	objcol = vec3(0.0,0.0,0.0);
	
    float d = sph;
	


		
	objcol = mix(vec3(1.0,0.0,0.0),vec3(1.0,1.0,.0),sin(indexy*1.+time)*.5+.5);
	//objcol = mix(vec3(1.0,0.0,0.0),sph,de);
	//objcol = mix(vec3(1.0,0.0,0.0),sphcol,box);
	//objcol = mix(vec3(0.0,0.0,1.0),objcol,line);
	
	//objcol = tx.rgb;
    //if (d == box) objcol = vec3(1.,0.2, 0.);
	//if (d == line) objcol = vec3(1., 0.0, 0.);

    return d*.9;
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
	
	p -= det * dir;
	col = mix(vec3(0.0),shade(p, dir),d<det);
	
	//col+= tx.rgb;
    return col;    
}

// MAIN

void main(void)
{
 
    vec2 uv = gl_FragCoord.xy/resolution.xy - .5; 
    uv.x *= resolution.x / resolution.y; 
    
    vec3 from = vec3(0., 0., -30.);
    vec3 dir = normalize(vec3(uv, 1.));
    vec3 col = march(from, dir);
    fragColor = vec4(col, 1.);
}