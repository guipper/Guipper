#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales




// RAYMARCHING BASICO DE UNA ESFERA

// nomenclaturas generales:
// p = posición en el espacio
// d = distancia
// dir = dirección

// VARIABLES GLOBALES

// distancia mínima en la que se considera que se chocó con un objeto
// también funciona como el nivel de detalle de una superficie


vec3 colorbj = vec3(1.0,1.0,1.0);

float det = .001;

// distancia máxima que recorrerá el rayo, lo que esté más allá de
// esta distancia se considera fuera de la escena

float maxdist = 50.;

// máxima cantidad de pasos que dará el raymarching
// un valor estandar puede ser 100
// pero puede que necesitemos más según la escena

int maxsteps = 100;

// FUNCIONES DE DISTANCIA PRIMITIVAS 
// son las que nos devuelven la distancia estimada para diferentes formas geométricas
// en este caso usaremos una de las más simples, una esfera, que es simplemente un length
// de la posición, restándole el radio de la misma



float opSmoothUnion( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h); }

float opSmoothSubtraction( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); }

float opSmoothIntersection( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) + k*h*(1.0-h); }
    
    

float sphere(vec3 p, float rad) 
{
    return length(p) - rad;
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float ground(vec3 p, float y) 
{
    p.y += y;
    return abs(p.y);
}
// FUNCION DE ESTIMACION DE DISTANCIA
// se va a encargar de devolvernos la distancia estimada a uno o varios objetos
// se pueden hacer aquí varias alteraciones a las formas primitivas
// algo que se verá en los próximos ejemplos

float de(vec3 p) 
{   
    
  
    
    float d = 0;
  //  vec3 p2 = p;
    
   // p2.z-=time;
    
    
    
    float ms = floor(2.0)+1.;     
    float indexy = floor(p.z/ms);
    
    p.xy*=rotate2d(indexy+time*.1);
    p.x = mod(p.x, ms) - ms/2;    
    p.z = mod(p.z, ms) - ms/2;

    

   // p.x+=3.;
   // p.y+=3.;
    
    
   // p.x+=sin(time)*5.;
    //p.y+=cos(time)*5.;
    
   
    
    vec3 p2 = p;
    
    float ms2 = floor(1.0)+1.;  //Subdivision espacial de la esfera;    
    
   // p2.z+=time;
    p2.z = mod(p2.z,ms2)-ms/2;
    
    float l = length(sin(p2*5.))*.2;
    
    float esfera =sphere(p2, 0.5+fbm(p.xy*1.1)*0.2);
    
    float caja = sdBox(p, vec3(0.1,2.1,0.1) );
    
    d = esfera;
    
    //d = opSmoothUnion( d, esfera, 0.2 );
    //d = opSmoothSubtraction( d, caja, 0.1  );
    //d = opSmoothIntersection( d, caja, 0.1 );
    
    //d = min(d,caja);
    //d = min(d,esfera);

  
    if(d == caja){
        colorbj = vec3(1.0,sin(indexy*10),0.0);
    }
    
    if(d == esfera){
        colorbj = vec3(0.5,0.1,1.0);
        
        vec3 col1 = vec3(1.0,0.0,.0);
        vec3 col2 = vec3(1.0,1.0,.0);
        
        colorbj = mix(col1,col2, smoothstep(.0,.4, l));
    }
  
  // d = min(d,sphere(p2,2.2));
    
    return d*0.2;
}








// FUNCION NORMAL
// La normal es el vector que es perpendicular a la superficie para un punto determinado
// nos sirve principalmente para calcular la iluminación en dicho punto
// la fórmula utiliza para este cálculo la diferencia entre la distancia obtenida 
// para 3 puntos apenas desplazados en los ejes x, y, z, 
// con la distancia obtenida en el punto actual.
// (esta explicación es sólo para saber lo que hace, en la práctica la podemos copiar y pegar o
// memorizarla, ya que no hay mucho para experimentar acá)

vec3 normal(vec3 p) 
{   
    vec2 d = vec2(0., det); // det es la distancia que establecimos como el nivel de detalle
    
    // usamos aquí la variable d para establecer un corrimiento de la posición original en x, y, z
    // según si ponemos x o y (de la variable d) en cada posición luego del punto, 
    // estamos estableciendo en que eje va el desplazamiento, que es igual a det
    
    return normalize(vec3(de(p + d.yxx), de(p + d.xyx), de(p + d.xxy)) - de(p));
    
    // también podríamos colocar de(p + vec3(det, 0., 0.)) etc, esto es sólo para hacerlo más fácil
}

// FUNCION SHADE
// Es la que va a establecer el color final según la iluminación, el color de la superficie, etc.

vec3 shade(vec3 p, vec3 dir) {
    
    // establecemos la dirección desde donde viene la luz
    // en este caso es una sola fuente, y no es una luz ubicada en el espacio,
    // sino sólo una dirección desde donde viene, como si fuera una fuente de luz lejana
    // por ejemplo el sol
    
    // x = derecha/izquierda - y = arriba/abajo - z = delante/detrás
    vec3 lightdir = normalize(vec3(1.5, 1., -1.)); 
    
    // como sólo hay un objeto en la escena, definimos su color aquí
    
    vec3 col = colorbj;
    
    // obtenemos la normal
    
    vec3 n = normal(p);
    
    // calculamos la luz difusa, que es la que se dispersa luego de rebotar en la superficie
    // para esto usamos la función dot (producto escalar), que en este caso la podemos ver
    // como una función que dados dos vectores normalizados (de largo 1.), nos devuelve un valor
    // entre -1. y 1. según cuán alineados están dichos vectores.
    // de esta manera obtenemos el sombreado de la superficie según hacia qué dirección apunta 
    // la misma y la diferencia con la dirección desde donde viene la luz. 
    // usamos la función max porque queremos descartar los valores negativos,
    // si no está apuntando a la luz, que sea 0.
    
    float diff = max(0., dot(lightdir, n));
    
    // calculamos la luz especular, que es la que se refleja directamente como si fuera un espejo,
    // y nos dá lo que podríamos llamar como el "brillo" en la superficie.
    
    // primero obtenemos, con la función reflect que ya trae GLSL, el vector reflejo entre
    // la dirección en la que va el rayo y la superficie
    
    vec3 refl = reflect(dir, n);
    
    // caculamos la luz especular, usando también la función dot obtenemos 
    // la diferencia entre este vector y la dirección desde que viene la luz, 
    // elevada a una potencia con la función pow, que nos va a determinar el tamaño del "brillo"
    
    float spec = pow(max(0., dot(lightdir, refl)), 20.);
    
    // la luz ambiental es la que va a iluminar uniformemente toda la superficie
    float amb = .1;

    // este es una de las formas de calcular la combinación de las luces
    // considerando una luz blanca que golpea un objeto uniformemente azul
    // y es el color multiplicado por la suma de la luz ambiental y la difusa,
    // sumandole a ese resultado la luz especular (brillo)
    
    // podemos alterar los valores de estas variables para modificar la iluminación
    // en este caso le bajé un poco el brillo a la especular
    
    return col*(amb + diff) + spec * 1.2;
    
}



// FUNCION DE RAYMARCHING

vec3 march(vec3 from, vec3 dir) 
{
    // variables que vamos a usar
    // d = distancia actual al objeto más próximo
    // td = distancia total recorrida desde la cámara
    // p = posición actual del rayo
    // col = color final

    float d, td=0.;
    vec3 p, col;

    // bucle del raymarching
    // a cada paso avanzará según la distancia obtenida 
    // en la posición actual, que nos dará la función de distancia de()

    for (int i=0; i<maxsteps; i++) 
    {
        // obtenemos la posición actual de rayo para esta iteración
        // distancia de la cámara + total distancia recorrida * dirección hacia la que va el rayo
        // en el primer paso td = 0 por lo que el rayo está en la posición de la cámara

        p = from + td * dir;

        // llamamos a la función de estimación de distancia, que devolverá
        // la distancia desde este punto al objeto más cercano

        d = de(p);

        // si la distancia es menor al umbral que definimos para determinar si se chocó con un objeto,
        // o bien el rayo sobrepasó la distancia máxima que especificamos, cortamos el for

        if (d < det || td > maxdist) break;

        // sumamos la nueva distancia obtenida en el acumulador, el rayo avanza

        td += d;
    }

    // Una vez que el for termina, se decide qué hacer según si el rayo golpeó una superficie o no

    if (d < det) // el rayo chocó con una superficie
    {
        p -= dir * det; 
        col = shade(p, dir);
    }
    else // el rayo no chocó con una superficie
    {
        // aquí podemos dibujar un fondo por ejemplo

        col = vec3(sin(p.z*2.0+time*2.),sin(p.z*2.0-time*2.),0.0)*0.5;
    }
    return col;    
}

// MAIN

void main(void)
{

 
    vec2 uv = gl_FragCoord.xy/resolution.xy - .5; 
    
    uv.x *= resolution.x / resolution.y; 

    vec3 from = vec3(0., 0., -8.);
    
    from.z+=time*1.0;
    vec3 dir = normalize(vec3(uv, 1.0));
    vec3 col = march(from, dir);

    fragColor = vec4(col, 1.);
}




