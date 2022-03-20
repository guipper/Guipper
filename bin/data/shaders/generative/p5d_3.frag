#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float speed;
uniform sampler2D t1;  
uniform sampler2D t2;  
// RESTAR UN OBJETO A OTRO

// Igual al ejemplo anterior, pero reemplazamos la esfera central
// por otro objeto que resulta de restarle a un cubo la esfera


// VARIABLES GLOBALES

float det = 0.01;
float maxdist = 100.;
int maxsteps = 100;

vec3 objcol;
vec3 from = vec3(0., 0., -15.);
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

float box(vec3 p, vec3 c)
{
    p=abs(p)-c;
    return length(max(p,0.))+min(0.,max(p.z,max(p.x,p.y)));
}

// construccion de un objeto usando las primitivas combinadas con max
// para obtener su interseccion. Esto generará una forma que es igual
// al espacio donde formas combinadas se intersecten.
// en este caso intersectamos un cubo con una esfera

float obj1(vec3 p,vec3 s) 
{

    s.y+=1.-abs(p.x);
    s.y*=.2;
    float box = box(p, s);

   // p.y+=fract(p.y*.1+time);
    //p.x+=fract(time);09
    float sph = sphere(p,0.5);
    float d = box;
    return d;
}

float obj2(vec3 p) 
{
    float box = box(p, vec3(0.2,0.1,0.2));
  
    // de esta manera le restamos al cubo la forma de la esfera
    float d = box;
    return d;
}


// FUNCION DE ESTIMACION DE DISTANCIA
float opSmoothUnion( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h); }

float opSmoothSubtraction( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); }

float opSmoothIntersection( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) + k*h*(1.0-h); }
    
    
float de(vec3 p) 
{	
	vec3 p4 = p;
	float time2 = iTime*speed;
    float ms = floor(1.0)+1.;

 
    //p.z+=time2*10.;
    float indexy = floor(p.z/ms);
	 float indexz = floor(p.x/ms);
    // rotamos en dos ejes
    p.xy *= rot(time2*.0+indexy*0.0);
	p.xz *= rot(time2*.0+indexy*0.0);

    float id2 = floor(p.z/ms);
    
    p.x = mod(p.x, ms) - ms/2.;
    p.z = mod(p.z, ms) - ms/2.;
    
    //p.z+=sin(id2*4+time);
    //p.y = mod(p.y, ms*2.) - ms*2/2;
    
    
    vec3 p3 = p;
    float obj2 = obj2(p3); //Columna
	p.y+=time;
    //p.xz*=rot(time);
   //  p.xy*=rot(time);
   // p=abs(p);
    //p -= sin(time)*.1+.51;
   
   
   
    
    vec3 p2 = p;
    float ms2 = 2.0;
    
    //p2.y+=time2;
   // p2.z+=time;
    float id = floor(p2.y/ms2);
    
   
    p2.y = mod(p2.y,ms2) -ms2/2.;
    //p2.xz*=rot(time2*2.+id*5.);
	//p2.xy*=rot(time2*2.+id*5.);
	//p2.yz*=rot(time2*2.+id*50.);
	//p2.y = sin(p2.x*1.);
    
    float obj1 = obj1(p2,vec3(0.5,0.5,0.5));
   
   // obj1 = opSmoothIntersection(obj1,obj2,-0.8);
    //p.z =sin(p.x*1);
   //   obj1 = max(obj1, sphere(p2, sin(time)*.02+1.1));
    // obtenemos la distancia minima entre obj1 y obj2 para combinarlas en la escena
    float d = obj1;
      //    d = min(obj1, obj2);0
     // d = obj2;
    // coloreamos segun el objeto con el que choca el rayo
	
	
	vec4 tc1 = texture2D(t1, gl_FragCoord.xy/resolution);
	vec4 tc2 = texture2D(t2, gl_FragCoord.xy/resolution);
	vec3 col1 = tc1.rgb;
	vec3 col2 = tc2.rgb; 
	
    if (d == obj2) objcol = vec3(.0, 0.0, 0.0);
    if (d == obj1) objcol = mix(col1,col2,sin(id2*1000.+time*1));
	
	
	//from.z +=time2;
	//from.x +=time2;
    //from.x-=sin(time2);
	p4-=from;
	//d = max(d,-length(p4)+4.0);
    d = max(d,-box(p4,vec3(1.0)));
	return d*0.5;
}









// FUNCION NORMAL

vec3 normal(vec3 p) 
{   
    vec2 d = vec2(0., det);
    
    return normalize(vec3(de(p + d.yxx), de(p + d.xyx), de(p + d.xxy)) - de(p));
}

// FUNCION SHADE

vec3 shade(vec3 p, vec3 dir) {
    
    vec3 lightdir = normalize(vec3(1.5, 1.2, -1.)); 
    
    // aquí definimos el color del objeto según la variable objcolor seteada en la funcion
    // de distancia. La guardamos en col antes de llamar a la funcion normal
    vec3 col = objcol;
    
    
    vec3 n = normal(p);
    
    float diff = max(0.2, dot(lightdir, n));
    
    vec3 refl = reflect(dir, n);
    
    float spec = pow(max(0.2, dot(lightdir, refl)), 5.);
    
    float amb = .1;
    
    
    

    return (col*(amb + diff) + spec * .8);
    
}



// FUNCION DE RAYMARCHING

vec3 march(vec3 from, vec3 dir) 
{

    float d, td=0.;
    vec3 p =vec3(0);
    vec3 col = vec3(0);
    
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
       // col += vec3(.0);
    }
    
     col = mix(vec3(.0),col, exp(-.0005*td*td));
    
    
    return col;    
}

// MAIN

void main()
{
	
    vec2 uv = gl_FragCoord.xy/iResolution.xy - .5; 
	float time2 = iTime*speed;
    uv.x *= iResolution.x / iResolution.y; 
    
    from = vec3(0., 0., -15.);
 
    vec3 dir = normalize(vec3(uv, 1.));
   // dir.yx*=rot(time2);
	dir.zx*=rot(time2);
    dir.yx*=rot(time2);
	//from.z +=time2;
	//from.x +=time2;
    from.x-=sin(time2);
 	from.z +=time2*20.;
    vec3 col = march(from, dir);

    fragColor = vec4(col, 1.);
}
