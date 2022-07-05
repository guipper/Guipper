#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales




uniform float freq ;
uniform float siz_x ;
uniform float siz_z ;
uniform float rotz ;
uniform float speedz ; 
uniform float msm ;
// VARIABLES GLOBALES

float det = .0018;
float maxdist = 1500.;
int maxsteps = 1000;

vec3 objcol;

// FUNCION DE ROTACION
mat2 rot(float a) {
    float s=sin(a), c=cos(a);
    return mat2(c,s,-s,c);
}
// FUNCIONES DE DISTANCIA PRIMITIVAS 
float sphere(vec3 p, float rad) {
    return length(p) - rad;
}

float box(vec3 p, vec3 c){
    p=abs(p)-c;
    return length(max(p,0.))+min(0.,max(p.z,max(p.x,p.y)));
}

float ground(vec3 p, float y) {
    p.y += y;
    return abs(p.y);
}

/*
float mapr(float _value,float _low2,float _high2) {
    float val = _low2 + (_high2 - _low2) * (_value - 0.) / (1.0 - 0.);
    return val;
}*/
float de(vec3 p) 
{

    vec3 pos = p;
    float pz = abs(fract(sin(p.z*0.2)*1.0-1));
	float py = abs(fract(sin(p.z*0.2)*1.0-1));
    // utilizando la función mod podemos "tilear" objetos
    // a lo largo de uno o varios ejes
    // en el mod, el 4 indica la distancia entre objetos
    // y siempre debemos restar esa distancia / 2.
    
    
    float ms = mapr(msm,2.2,3.0);
    
    //p.xz = mod(p.xz, ms) - ms/2;
    p.xy*=rot(pz*mapr(rotz,1.0,3.0));

	
	
	
    p.x = mod(p.x, ms) - ms/2;
    
    p.z = mod(p.z, ms*2.) - ms*2/2;
    //p.yz = mod(p.yz, ms) - ms/2;
   // p.xz = mod(p.xz, ms) - ms/2;
   // float pz = mod(p.z,4)-4/2;
    
   // p.xz*=rot(time*.4);
    //p.x+=fract(time);
    vec3 pbox = p;
    
	
	
	float mfreq = mapr(freq,2.,20.);
	float msx = mapr(siz_x,1.5,2.3);
	float msy = 80.;
	float msz = mapr(siz_z,0.07,0.18);
	
	
  //  pbox*=rot(time);
    float onda = sin(p.y*2+time*0.01+sin(p.y*mfreq+time*8.4+sin(p.y*2)*0.4)*0.4)*.4-1.2;
    
    
	
	
	
	
	//pbox.xz*= rot(p.z+time)-.;
    float box = box(pbox, vec3(msx+onda,msy,msz));

    vec3 pospiso = pos;
   // pospiso.z+=sin(time);
 //   float pla = ground(pospiso, 0);
    
    float d = box;

    // para generar el cuadriculado   
    p = abs(p * 3.);
    float c = pow(max(max(fract(p.x),fract(p.y)), fract(p.z)),0.8);
    float b = pow(max(fract(p.x),fract(p.z)),5.);

    if (d==box) objcol=vec3(1.-pz,.5,pz);
  // if (d==pla) objcol=vec3(0.0,.5,.6);
    return d*.17;
}

// FUNCION NORMAL

vec3 normal(vec3 p) {   
    vec2 d = vec2(0., det);
    return normalize(vec3(de(p + d.yxx), de(p + d.xyx), de(p + d.xxy)) - de(p));
}

// FUNCION SHADOW
// calcula la sombra, generando un efecto de suavizado de los bordes
// a medida que se aleja del objeto

float shadow(vec3 p, vec3 ldir) {
    float td=.001,sh=1.,d=det;
    for (int i=0; i<100; i++) {
        p+=ldir*d;
        d=de(p);
        td+=d;
        sh=max(sh,1.*d/td);
        if (sh<.001) break;
    }
    return clamp(sh,0.,1.0);
}


// FUNCION SHADE

vec3 shade(vec3 p, vec3 dir) {
    vec3 col = objcol;
    vec3 lightdir = normalize(vec3(0.5, 0., -1.)); 
    vec3 n = normal(p);
    float sh = shadow(p, lightdir);    
    float diff = max(0.0, dot(lightdir, n)) * sh; // multiplicamos por sombra;
    vec3 refl = reflect(dir, n);
    float spec = pow(max(0., dot(lightdir, refl)), 0.5) * sh; // multiplicamos por sombra;
    float amb = .1;
    return (col*(amb*1. + diff*4.) + spec * .9)*.2;
    
}



// FUNCION DE RAYMARCHING

vec3 march(vec3 from, vec3 dir) 
{

    float d, td=0.;
    vec3 p, col;


    for (int i=0; i<maxsteps; i++) {
        p = from + td * dir;
        d = de(p);
        if (d < det || td > maxdist) break;
        td += d;
    }

    if (d < det)
    {
        p -= det * dir;
        col = shade(p, dir);
    } else {
        // si no golpeo con ningun objeto, llevamos la distancia a la máxima
        // que se definió, o sea al fondo de la escena
        // esto sirve para el correcto cálculo de la niebla
        td = maxdist;
    }
    // efecto niebla
    // mix entre el color obtenido y un color de la niebla
    // utilizando para mezclarlos la funcion exp con la variable td
    // que es la distancia en la que quedo el rayo con respecto a la cam
    // el -.01 en la funcion exp altera la distancia de la niebla
	
	
	
	vec3 c1 = vec3(0.5,0.2,0.2);
	vec3 c2 = vec3(0.8,0.8,.7);
	
	vec3 cf = mix(c1,c2,	sin(td*.1+time*1)*.5+.5);
	
    col = mix(vec3(cf),col, exp(-.002*td*td));
    return col;    
}



// MAIN

void main(void)
{
    vec2 uv = gl_FragCoord.xy/resolution.xy - .5; 
    uv.x *= resolution.x / resolution.y; 
    vec3 from = vec3(0., 0.,2.);
    vec3 dir = normalize(vec3(uv, 1.));

    //una forma simple de rotar la cámara
    //es rotando en los mismos ejes tanto from como dir
    //from.xz *= rot(time*.3);
    
    from.z+=time*mapr(speedz,-1.,1.);
    dir.xy *= rot(time*.2);
	// dir.z *= rot(time*.2);
	// dir.yz *= rot(time*.2);
    vec3 col = march(from, dir);

    fragColor = vec4(col, 1.);
}









