#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// COPIAR OBJETOS A LO LARGO DE VARIOS EJES

// utilizando la función mod, podemos repetir objetos en la escena

// VARIABLES GLOBALES
uniform float msmult;
uniform float defor;
uniform float freqx;
uniform float ampx;
float det = 0.002;
float maxdist = 100.;

int maxsteps = 500;

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

float box(vec3 p, vec3 c)
{
    p=abs(p)-c;
    return length(max(p,0.))+min(0.,max(p.z,max(p.x,p.y)));
}

float ground(vec3 p, float y) 
{
    p.y += y;
    return abs(p.y);
}





float sdCone( vec3 p, vec2 c, float h )
{
  float q = length(p.xz);
  return max(dot(c.xy,vec2(q,p.y)),-h-p.y);
}
float sdSphere( vec3 p, float s )
{
  return length(p)-s;
}
float de(vec3 p) 
{


    float box = 1;
    vec3 pos = p;
    float pz = abs(fract(sin(p.z*0.33)*1.0-1));
    
    int cnt = 2;
    vec3 col1 = vec3(0.9,0.2,0.7);
    vec3 col2 = vec3(0.4,0.5,0.8);
   
    for(int i=0; i<cnt; i++){
        float index = i/float(cnt);
        float ms = floor(msmult*10.0)+1.;
       // p.x+= i;
      //    p.z+= sin(index*pi*+time);
        p.x = mod(p.x, ms) - ms/2;
        p.z = mod(p.z, ms*2.) - ms*2/2;
        vec3 pbox = p;
        vec3 pcono = p;
        pcono.y -=2.2;
        pcono.x+= sin(pcono.y*80.*freqx+time*2)*ampx; 
        
        float mof = cos(p.z*4+index*pi*4+time)*defor*0.08;
        box = min(box,sdCone(pcono,vec2(0.1+mof,0.01+index*0.05+mof),8.1));   
        
        vec3 col11 = vec3(0.7,0.7,0.7);
        vec3 col12 = vec3(0.7,0.7,0.7);
        vec3 col21 = vec3(0.8,0.2,0.8);
        vec3 col22 = vec3(0.5,0.3,0.4);
        
        col1 = mix(col11,col12,index);
        
        col2 = mix(col21,col22,index);
    }
    vec3 pospiso = pos;
   // pospiso.z+=sin(time);
    //pospiso.y+=sin(p.x*10)*0.2+0.5;
    //pospiso.xy = vec2(sin(p.x));
    pospiso.y += cos(pospiso.x*2+pospiso.z*4)*.1;    
    pospiso.y += sin(pospiso.z*2+pospiso.y*2)*.2;    
    float pla = ground(pospiso, 0.8);
    
    float d = box;
    
    d = min(box,pla);
    // para generar el cuadriculado   
   
    float c = pow(max(max(fract(p.x),fract(p.y)), fract(p.z)),0.5);
    float b = pow(max(fract(p.x),fract(p.z)),8.);

   // length(fract(pos.xz));
  //  if (d==box) objcol=vec3(0.8-pz,sin(length(fract(pos.zz))*1)*0.7,pz);
 
  
   
   
   
   
    if (d==box) objcol=mix(col1,col2,pz);
    if (d==pla) objcol=vec3(0.7,0.7,0.7);



    return d *1.;
}

// FUNCION NORMAL

vec3 normal(vec3 p) 
{   
    vec2 d = vec2(0., det);
    
    return normalize(vec3(de(p + d.yxx), de(p + d.xyx), de(p + d.xxy)) - de(p));
}

// FUNCION SHADOW
// calcula la sombra, generando un efecto de suavizado de los bordes
// a medida que se aleja del objeto

float shadow(vec3 p, vec3 ldir) {
    float td=.001,sh=150.,d=det;
    
    for (int i=0; i<0; i++) {
        p+=ldir*d;
        d=de(p);
        td+=d;
        sh=min(sh,1.*d/td);
        if (sh<.001) break;
    }
    return clamp(sh,0.,0.9);
}


// FUNCION SHADE

vec3 shade(vec3 p, vec3 dir) {

    vec3 col = objcol;
    
    vec3 lightdir = normalize(vec3(sin(time)*0.5+0.5, 0.9, 0.9)); 

    vec3 n = normal(p);

    float sh = shadow(p, lightdir);    
    
    float diff = max(0.0, dot(lightdir, n)) * sh; // multiplicamos por sombra;
    
    vec3 refl = reflect(dir, n);
    
    float spec = pow(max(0., dot(lightdir, refl)), 0.01) * sh; // multiplicamos por sombra;
    
    float amb = .1;
    
    return col*(amb*6.0 + diff) + spec * 0.1;
    
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
    
    vec3 colfondo = vec3(1.-p.y,1.-p.y,1.-p.y);
    
    float f = sin(dir.z*100+time)*0.5+0.5;
    vec3 col1 = vec3(0.8,0.3,0.4);
    vec3 col2 = vec3(0.7,0.7,0.7);
    vec3 colf = mix(col1,col2,f);
  //  colfondo = col1;
    col = mix(colf,col, exp(-.0005*td*td));
    return col;    
}


// MAIN

void main(void)
{
 
    vec2 uv = gl_FragCoord.xy/resolution.xy - .5; 
	uv.y = 1.-uv.y-1.0;
    uv.x *= resolution.x / resolution.y; 
    	
    vec3 from = vec3(0., 0.,2.);
    //from.z-=time*0.1;
    vec3 dir = normalize(vec3(uv, 1.));

    //una forma simple de rotar la cámara
    //es rotando en los mismos ejes tanto from como dir
    from.xz *= rot(time*.2);
    
    from.z-=time*1;
    from.y+=sin(time*.001);
     from.y+=1.9;
  //  dir.xz *= rot(time*.2);
    //dir.xy *= rot(time*.02);
   // dir.z+= sin(time)*0.8;
    vec3 col = march(from, dir);

    fragColor = vec4(col, 1.);
}







