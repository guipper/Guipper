#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
// RAYMARCHEANDO UNA ESFERA CON DESPLAZAMIENTO DE SUPERFICIE + ANILLOS CON GLOW + FOG + BACKGROUND

float maxdist=50.; // MAXIMA DISTANCIA A RAYMARCHEAR
float det=.002; // THRESHOLD DE DISTANCIA DE CHOQUE DEL RAYO
vec3 luzdir = vec3(1.,.5,1.); // DIRECCION DE LA FUENTE DE LUZ

// COLORES

vec3 color_esfera = vec3(1.5,.2,.1); 
vec3 color_anillo = vec3(.3,.2,1.);
vec3 color_piso = vec3(.5,.3,.4);
vec3 color_niebla = vec3(1.,.9,.95);


// VARIABLE GLOBAL PARA DISTANCIA A LOS ANILLOS QUE USA EL CALCULO DE GLOW

float dist_ani=0.;




// FUNCION DE ROTACION EN 2D

mat2 rot2D(float a) {
	a=radians(a);
    return mat2(cos(a),sin(a),-sin(a),cos(a));    
}



// FUNCION PARA ALINEAR UN VECTOR CON RESPECTO A OTRO (SE USA ACA PARA ALINEAR LA DIRECCION DE LA CAMARA HACIA EL OBJETO)


mat3 lookat(vec3 fw,vec3 up){ 
	fw=normalize(fw);vec3 rt=normalize(cross(fw,normalize(up)));return mat3(rt,cross(rt,fw),fw);
}



// ESTIMACION DE LA DISTANCIA A UN CUBO (b = DIMENSIONES)

float cubo( vec3 p, vec3 b )
{
  return length(max(vec3(0.),abs(p)-b));
}  


// ESTIMACION DE LA DISTANCIA A UN CILINDRO 

float cilindro( vec3 p, vec2 b )
{
  b.y*=.5;  
  p.z+=b.y;
  return max(abs(p.z)-b.y,length(p.xy)-b.x);
}


// ESTIMACION DE LA DISTANCIA A UN CONO 

float cono (vec3 p, vec2 b) {
  b.y*=.5;
  p.z+=b.y;
  float d = max(0.,-p.z+b.y);
  return max(abs(p.z)-b.y,length(p.xy)-b.x+d*b.x/b.y*.5);

}



// ESTIMACION DE LA DISTANCIA A UN TORO (t = RADIO,ANCHO)

float toro( vec3 p, vec2 t )
{
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
}




// ESTIMACION DE LA DISTANCIA A UNA ESFERA (r = RADIO)

float esfera(vec3 p, float r) {
	return length(p) - r;
}



// FUNCION PARA TILEAR EL ESPACIO SIN DISCONTINUIDADES (abs & mod)


vec3 tile(vec3 p, float t) {
	return abs(t - mod(p, t*2.));
}



// SUPERFICIE DE LA ESFERA (usada para desplazamiento y fake AO)

float superficie(vec3 p) {
    p.xz*=rot2D(iTime*40.); // Rota la superficie
    p.xy*=rot2D(iTime*40.); // Idem
    p=tile(p,.1); // Tilea el espacio
	return max(max(p.x,p.y),max(p.y,p.z));
}



// ESTIMACION DE LA DISTANCIA AL PISO

float piso(vec3 p) {
    float ciclo = smoothstep(0.5,1.,abs(sin(iTime*.05)));
	float d = -p.x+2.5; // distancia al plano x 
    d-=sin(length(p.yz*4.)-iTime*10.)*smoothstep(0.,10.,10.-length(p.yz))*.1;
    return d;
}


// ESTIMACION DE LA DISTANCIA LOS ANILLOS

float anillos(vec3 p) {
   	float d = toro(p, vec2(1.2,.07)); // Toro xyz
   	d = min(d, toro(p.yxz, vec2(1.2,.07))); // Combina con Toro yxz
   	d = min(d, toro(p.xzy, vec2(1.2,.07))); // Combina con Toro xzy
    vec3 t=vec3(0.,0.,1.2);
    vec3 s=vec3(0.,.07, 1.);
    p=abs(p);
    //d = min(d, cilindro(p-t.xxz, s.yz));
    d = min(d, cilindro(-p+t.xxz, s.yz));
    //d = min(d, cilindro(p.xzy+t.xxz, s.yz));
    d = min(d, cilindro(-p.xzy+t.xxz, s.yz));
    //d = min(d, cilindro(p.zyx+t.xxz, s.yz));
    d = min(d, cilindro(-p.zyx+t.xxz, s.yz));
    //d = min(d, cono(p+t.xxz+s.xxz, vec2(.2,.5)));
    d = min(d, cono(-p+t.xxz+s.xxz, vec2(.2,.5)));
    //d = min(d, cono(p.xzy+t.xxz+s.xxz, vec2(.2,.5)));
    d = min(d, cono(-p.xzy+t.xxz+s.xxz, vec2(.2,.5)));
    //d = min(d, cono(p.zyx+t.xxz+s.xxz, vec2(.2,.5)));
    d = min(d, cono(-p.zyx+t.xxz+s.xxz, vec2(.2,.5)));
   	return d;   
}




// FUNCION PRINCIPAL DE ESTIMACION DE DISTANCIA

vec2 de(vec3 p) {
    float id=0.; // Inicializa la variable indice del objeto mas cercano
    vec3 prot = p; // Crean una copia del punto p
    prot.xy*=rot2D(iTime*100.); // rota prot
    prot.zy*=rot2D(iTime*100.); // rota prot
	float esf = (esfera(prot, 1.)+superficie(p)*.5)*.7; // distancia de la esfera con desplazamiento de superfice
	float ani = anillos(prot); // distancia a los anillos
    float pis = piso(p); // distancia al piso
    float d = min(esf,ani); // combina esfera con anillos en d
    d = min(d,pis);
 	// seteo del indice del objeto segun el que tenga la menor distancia
    if (esf<ani && esf<pis) id=1.; 
    if (ani<esf && ani<pis) id=2.;
    if (pis<esf && pis<ani) id=3.;
    // calculo de la distancia a los anillos para el glow
    dist_ani=abs(ani-d);
    return vec2(d*.9,id);
}



// CALCULO DE NORMAL (VECTOR PERPENDICULAR A LA SUPERFICIE)


vec3 normal(vec3 p) {
	vec3 e = vec3(0.0,det*4.,0.0);
	
	return normalize(vec3( // normaliza el vector resultante de las diferencias de distancia estimada
        				  // entre dos puntos desplazados del punto central por distancia det, para cada eje
			de(p+e.yxx).x-de(p-e.yxx).x,
			de(p+e.xyx).x-de(p-e.xyx).x,
			de(p+e.xxy).x-de(p-e.xxy).x
			)
		);	
}                  




// SOFT SHADOW BASADA EN QUILEZ

float shadow(vec3 pos) {
  float sh = 1.0; 
  float totdist = 2.*det;
  float d = 10.;
  for (int i = 0; i < 30; i++) {
    if (d > det) {
      vec3 p = pos - totdist * luzdir;
      d = de(p).x;
      sh = min(sh, 10. * d / totdist);
      totdist += d;
    } else break;
  }
  return clamp(sh, 0.0, 1.0);
}



// CALCULO DE LUCES

vec3 light(vec3 p, vec3 dir, vec3 n, vec3 col) {
    float sh=shadow(p); // obtengo factor de sombra
    float luzdif=max(0.,dot(normalize(luzdir),-n))*sh; // luz difusa con direccion definida en luzdir
    float luzcam=max(0.,dot(dir,-n)); // luz difusa desde la cámara
	vec3 refl=reflect(dir,-n); // calculo del vector reflejo entre camara y normal
	float luzspec=pow(max(0.,dot(refl,normalize(-luzdir))),10.)*sh; // luz especular
	float luzamb=.1;
    return col*(luzcam*.4+luzdif*1.5+luzamb)+luzspec*.8; // multiplico color por valores de luces y sumo la especular
}



// SHADING DE LOS OBJETOS

vec3 shade(vec3 p, vec3 dir, vec3 nor, float id) {
	vec3 col=vec3(0.); // Incializo el color en 0
    col += color_esfera * (1.-step(.001,abs(id-1.))) * (1.-superficie(p)*7.); // sumo color de la esfera si id=1 
    																		// + fake AO de superficie
    col += color_anillo * (1.-step(.001,abs(id-2.))); // sumo color anillos si i=2
    col += color_piso * (1.-step(.001,abs(id-3.))); // sumo color de la jaula si i=4
    col = light(p, dir, nor, col); // aplico luces al color
    return col;
}


// FONDO - KALISET

vec3 background(vec3 p) {
    p/=maxdist;
    for (int i=0; i<9; i++) {
    	p=abs(p)/dot(p,p)-.5;
    }
    vec3 col=normalize(abs(normalize(p))+3.)*vec3(.8,.6,.7)*2.+pow(max(0.,1.-length(p)),3.)*.7;
	return col;
}



// RAYMARCHEO
                 
vec3 march(vec3 from, vec3 dir) {
    vec3 col=vec3(0.); // inicializa el color en 0
    vec3 colref=vec3(0.); // inicializa el color en 0
    float glow=0.; // inicializa el glow en 0
    float ref=0.; // inicializa la variable/flag de reflejo
    float totdist=0.; // inicializa la distancia recorrida total hasta la superficie
    
    // declaracion de variables de distancia (d) y posicion (p)
    vec2 d = vec2(0.); 
    vec3 p=from;
    vec3 dirorig = dir;
    for (int i=0; i<150; i++) { // bucle del raymarching
        p+=dir*d.x; // obtiene posicion del rayo partiendo desde el from y avanza en la direccion del rayo
        d=de(p); // obtiene la estimacion de distancia y el id del objeto
		totdist+=d.x*(1.-step(0.1,ref)); // incrementa la distancia recorrida si el rayo no choco todavia
        // si la distancia es menor al treshold y no es el piso, o la posicion esta fuera de los limites, sale del bucle
        if ((d.x<det && (abs(d.y-3.)>.001 || ref>0.)) || length(p)>maxdist) break; 
        									  
        if (d.x<det ) { // si la distancia es menor al treshold, choco con el piso, hay que reflejar el rayo
            ref = .8; // intensidad de reflejo (inversa) e indicador de que ya se hizo un reflejo
            p-=det*dir*2.; // retrocede el rayo fuera de la superficie
            vec3 n = normal(p); // calcula normal
            colref=shade(p,dir,n,d.y); // calcula el shading del piso
            dir = reflect(dir,-n); // refleja la direccion del rayo segun el normal
			d.x = det; // reinicializo la distancia actual a la de det para el proximo avance
        }
        glow+=max(0.,1.-dist_ani); // acumula el brillo (dist_tor es la distancia a los anillos, variable global)
    }
    float dotluz = max(0.,dot(dir,normalize(-luzdir))); // para el calculo de la fuente de luz
    if (d.x<det) { // choco el objeto
        p-=det*dir*2.; // retrocede fuera de la superficie
        vec3 n = normal(p); // calcula normal
        col=shade(p,dir,n,d.y); // el color lo obtiene de la funcion shade
  
    } else { // fondo
        if (ref<.1) totdist = maxdist; // si no reflejo, totdist es igual a la distancia maxima (correccion para fog)
	    p=maxdist*dir; // situa el rayo en el limite exterior máximo
    	col=background(p); // color de fondo
        col=max(col,dotluz*1.1)*vec3(1.,.95,1.); // suma fuente de luz
    }
	// calculo de niebla segun distancia del piso y distancia recorrida por el rayo
    float fog = smoothstep(0.,20.,20.-piso(from+dirorig*totdist))
        *(1.-exp(-.02*pow(totdist,2.)))*(1.+dotluz*.05);
    col = mix(col,colref,ref); // mezcla el color original con el color del reflejo
    vec3 glow_col = pow(glow*.025,2.)*(.3+color_anillo); // ajustes finales del glow
	col = mix(col,color_niebla*(1.+dotluz*.1),min(1.,fog)); // mezcla con la niebla
    col += min(vec3(1.),glow_col); // suma el glow 
    col = pow(col, vec3(1.7))*vec3(1.,.97,.93); // ajuste de contraste y color
    return col;
}

// CAMINO A SEGUIR POR LA CAMARA
vec3 path(float t) {
    vec3 p=vec3(0.);
    p.x=-1.5+sin(t*.5)*1.5;
    p.y=sin(t)*12.+1.;
    p.z=-cos(t)*7.;
	return p;
}

// DEVUELVE POSICION Y DIRECCION DE LA CAMARA
void camara(float t, out vec3 from, inout vec3 dir) {
	from = path(t); // Camino de la camara
    dir = lookat(-from+vec3(0.,sin(t)*4.,0.), vec3(-1.,0.5,0.)) * dir; // Alinea dir con from, mas desplazamiento en y
	dir = normalize(dir);
}



// MAIN

void main()
{
	vec2 uv=gl_FragCoord.xy;
//	uv.y=1.-uv.y;
	uv /= iResolution.xy; uv-=.5; // coordenadas centradas en 0 (-.5,.5)
	uv.y*=-1.;
	uv.x *= iResolution.x/iResolution.y; // aspect ratio
    vec2 mouse=iMouse.xy/iResolution.xy; // mouse 0..1
	vec3 dir=normalize(vec3(uv,1.2)); // direccion del rayo. 1.2 = Field of View (FOV)
    vec3 from=vec3(0.,-.3,-5.-(1.-mouse.y)*6.); // posicion de la camara
  	camara(iTime*.2, from, dir);
    fragColor = vec4(march(from,dir)*.7,1.);
    //o = mix(o,vec4(pow(length(o)*.2,1.5)),abs(sin(iTime*.2)));
}