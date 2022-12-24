#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float startRandom ;

uniform float r1 ; 
uniform float g1 ; 
uniform float b1 ; 
 
uniform float r2 ; 
uniform float g2 ; 
uniform float b2 ;

uniform float lmode ;
uniform float escenaactiva ;

uniform float lerpm ;
uniform float seed ;
uniform float frc ;
uniform float shapes ;
uniform float posz ;
uniform float posx ;
uniform float posy ;
uniform float lookx ;
uniform float looky ;
uniform float lookz ;

#define iTime time
#define iResolution resolution

#define OCTAVES 8


#define fx resolution.x/resolution.y
#define h1 (rnd(startRandom))
#define h2 (rnd(startRandom+2.))



float rdm(float p){
    p*=1234.56;
    p = fract(p * .1031);
    p *= p + 33.33;
    return fract(2.*p*p);
}
float ridge2(float h, float offset) {
    h = abs(h);     // create creases
    h = offset - h; // invert so creases are at top
    h = h * h;      // sharpen creases
    return h;
}

/************************************************/
// VARIABLES GLOBALES
float det = .0004; // umbral para detectar choque 
vec3 lightpos1, lightpos2,lightpos3; // posicion de las luces
float light1, light2,light3; // distancia a las luces
vec3 light1color = vec3(r1,g1,b1); // color luz 1
vec3 light2color = vec3(2.,2.,2.); // color luz 2
vec3 light3color = vec3(0.,0.2,.0); // color luz 2
vec3 camoffset = vec3(0.0,0.0,0.0);
vec3 from = vec3(0., 0., -25.);
vec3 look = vec3(0.);
vec3 v1 = vec3(0.0,1.0,.0);
//vec3 look =  vec3(lookx,looky,lookz);
     


// matriz de rotación
mat2 rot(float a) {
    float s=sin(a), c=cos(a);
    return mat2(c,s,-s,c);
}
float opSmoothUnion( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h); }
float opSmoothSubtraction( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2+d1)/k, 0.0, 1.0 );
    return mix( d2, -d1, h ) + k*h*(1.0-h); }

float opSmoothIntersection( float d1, float d2, float k ) {
    float h = clamp( 0.5 - 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) + k*h*(1.0-h); 
}
// devuelve un mat3 para alinear un vector con el vector dir, especificando la dirección que se tomaría como "arriba"
mat3 lookat(vec3 dir, vec3 up) {
    dir = normalize(dir);
    vec3 rt = normalize(cross(dir, up));
    return mat3(rt, cross(rt, dir), dir);
}
vec3 tile(vec3 p, float t) {
    return abs(t - mod(p, t*2.));
}

// distancia a un octaedro
float sdOctahedron( vec3 p, float s){
  p = abs(p);
  return (p.x+p.y+p.z-s)*0.57735027;
}
float sdTorus( vec3 p, vec2 t ){
  vec2 q = vec2(length(p.xz)-t.x,p.y);
  return length(q)-t.y;
}
float sdSphere( vec3 p, float s ){
  return length(p)-s;
}
float sdDeathStar( in vec3 p2, in float ra, float rb, in float d ){
  // sampling independent composytations (only depend on shape)
  float a = (ra*ra - rb*rb + d*d)/(2.0*d);
  float b = sqrt(max(ra*ra-a*a,0.0));
	
  // sampling dependant composytations
  vec2 p = vec2( p2.x, length(p2.yz) );
  if( p.x*b-p.y*a > d*max(b-p.y,0.0) )
    return length(p-vec2(a,b));
  else
    return max( (length(p          )-ra),
               -(length(p-vec2(d,0))-rb));
}
float sdCapsule( vec3 p, vec3 a, vec3 b, float r ){
  vec3 pa = p - a, ba = b - a;
  float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  return length( pa - ba*h ) - r;
}
float sdBoxFrame( vec3 p, vec3 b, float e ){
       p = abs(p  )-b;
  vec3 q = abs(p+e)-e;
  return min(min(
      length(max(vec3(p.x,q.y,q.z),0.0))+min(max(p.x,max(q.y,q.z)),0.0),
      length(max(vec3(q.x,p.y,q.z),0.0))+min(max(q.x,max(p.y,q.z)),0.0)),
      length(max(vec3(q.x,q.y,p.z),0.0))+min(max(q.x,max(q.y,p.z)),0.0));
}

float sdRoundBox( vec3 p, vec3 b, float r ){
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0) - r;
}

float sdComon(vec3 p,float _fr ,float _shape,float _smof )
{   
    float ms = floor(mapr(_fr,4.0,8.))+1.;
    ms = _fr;
    float id2 = abs(floor(p.x/ms)*2.-1.);

    p.x = mod(p.x, ms) - ms/2.;
    p.z = mod(p.z, ms) - ms/2.;
    p.y = mod(p.y, ms) - ms/2.;
   float idx2 = sin(id2*10.);
   float oct = 0.;
   if(_shape < .33){
        oct = sdSphere(p,mapr(_smof,0.1,1.5)+sin(id2*100.+time)*0.1);
   }else if(_shape < .66){
        oct = sdOctahedron(p,mapr(_smof,0.5,2.0));
   }else{
        oct = sdBoxFrame(p,vec3(1.0),mapr(_smof,0.1,0.4));
   }
   return oct;
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}
float sdQuad(vec3 p,float _fr ,float _r ){   
    
    float ms = floor(10.)+1.;
    ms = _fr;
    float id2 = abs(floor(p.x/ms)*2.-1.);
    float idxz = abs(floor(p.z/ms)*2.-1.);
    p.x = mod(p.x, ms) - ms/2.;
    p.z = mod(p.z, ms) - ms/2.;
    p.y = mod(p.y, ms) - ms/2.;
 

  if(_r < .333){
    p.yz*=rotate2d(time);
  }else if(_r < .66){
    p.yx*=rotate2d(time);
  }else {
    p.xz*=rotate2d(time);
  }
   //p.z+=sin(time+_r)*2.;
 //p.x+=cos(time+_r)*2.;

   float idx2 = sin(id2*5.);
   // p.z+=sin(time*3.+p.z*5.+idxz)*.1;
   float oct = 0.;
          oct = sdOctahedron(p,sin(id2*5.+time)*.01+_r*2.);

   return oct;
}

float sdSpheres(vec3 p,float _fr ,float _smof ){   
    float ms = floor(mapr(_fr,4.0,8.))+1.;
    ms = _fr;
    float idxx = abs(floor(p.x/ms)*2.-1.);
    float idxz = abs(floor(p.z/ms)*4.-2.);
    float idxy = abs(floor(p.x/ms)*2.-1.);

    float indexz = floor(p.x/ms);
    p.xy *= rot(sin(idxz+time*.01)*PI);
    //p.xz *= rot(time*.1);

    p.x = mod(p.x, ms) - ms/2.;
    p.z = mod(p.z, ms) - ms/2.;
    p.y = mod(p.y, ms) - ms/2.;
   float idx2 = sin(idxz*10.);
   float oct = 0.;

   oct = sdSphere(p,mapr(_smof,0.1,1.5)+sin(idxx*100.+time)*0.1);

   return oct;
}
float sdEscena1(vec3 p){
    
    const int cnt = 4;
    
    float spg = 0.5;
    vec3 mov1 = vec3(mapr(rdm(seed+1001.),-spg,spg),
                      mapr(rdm(seed+2351.),-spg,spg),
                      mapr(rdm(seed+4321.),-spg,spg));
    float oct = 1.;
    for(int i=0; i<cnt;i++){
        vec3 p3 = p;
    
        vec3 mov = vec3(mapr(rdm(seed+1001.+float(i)*120.),-spg,spg),
                        mapr(rdm(seed+3213.+float(i)*120.),-spg,spg),
                        mapr(rdm(seed+53413.+float(i)*120.),-spg,spg));
       // vec3 mov = vec3(0.5,0.5,0.5);
        float idx = float(i)/float(cnt);     
        float fr = floor(mapr(idx,8.0,12.0));
            fr = 6.+float(i)*2.;

           // p3+=mov*time;
         oct = min(oct,sdSpheres(p3,fr,rdm(seed+3214.+float(i)*2132.)));
    }
    return oct ;
}
float sdEscena2(vec3 p){
    
    const int cnt = 4;
  

    float ms = floor(4.)+1.;

    float spg = 3.;
    float oct = 1.;
    float idxx = abs(floor(p.x/ms)*2.-1.);
    float idxz = abs(floor(p.z/ms)*4.-2.);
    float idxy = abs(floor(p.x/ms)*2.-1.);

    

    for(int i=0; i<cnt;i++){
        vec3 p3 = p;   
        float ms = 10.;
		float idxi = float(i)/float(cnt);
        float idxx = abs(floor(p.x/ms)*2.-1.);
        float idxz = abs(floor(p.z/ms)*2.-2.);
        float idxy = abs(floor(p.x/ms)*2.-1.);
        
        float indexz = floor(p.x/ms);

        float idx = float(i)/float(cnt);     
        float fr = floor(mapr(idx,1.0,12.0));
              fr = 4.+float(i);

        // p3.z+= sin(idxx+time+idxi*10.)*5.;
        // p3.z+= sin(float(i)*1.+time+idxx)*0.03;
		 p.x = abs((mod(p.x,ms) - ms/2.)*2.-.5);
		 p.y = abs((mod(p.y,ms) - ms/2.)*2.-.5);
		 p.z =  mod(p.z,ms) - ms/2.;
		
		//p.z+=sin(idx*4.+time);
		//p.x+=sin(idx*4.+time);
		//p.y+=cos(idx*8.+time);
		
		p.xz+=vec2(0.5);
		p.xy*=rotate2d(time*.25);
		p.xz-=vec2(0.5);
		
		p.z+=sin(idx*2.+time*.5)*1.;
		//p.x+=cos(idx*8.+time*0.1)*2.;
		
		float sr = rdm(seed+255.+float(i)*535.01);
		vec3 s = vec3(sin(time+time)*2.,1.0,1.0);
		float r2 =rdm(seed+2302.)*4.5;
	//	sr = sin(time*10.)*.5+.5;
		float r3 = rdm(seed+10.)*4.8;
		
		float z = mapr(rdm(seed+5321.),0.2,0.6);
		z = 0.1;
		if(sr < 0.3){
			s = vec3(r3,r3,r2*z);
		}else if(sr < 0.6){
			s = vec3(r3,r2,r3*z);
		}else{
			s = vec3(r2,r3,r2*z);
		}
		/*s = vec3(r2,1.,1.);
        	s = vec3(r3,r2,r3);*/
        oct = min(oct,sdBox(p3,s));
    }
    return oct *.8;

}
float sdEscena2_2(vec3 p){
    
    const int cnt = 4;
  

    float ms = floor(4.)+1.;

    float spg = 3.;
    float oct = 1.;
    float idxx = abs(floor(p.x/ms)*2.-1.);
    float idxz = abs(floor(p.z/ms)*4.-2.);
    float idxy = abs(floor(p.x/ms)*2.-1.);

    

    for(int i=0; i<cnt;i++){
        vec3 p3 = p;   
        float ms = 10.;
		float idxi = float(i)/float(cnt);
        float idxx = abs(floor(p.x/ms)*2.-1.);
        float idxz = abs(floor(p.z/ms)*2.-2.);
        float idxy = abs(floor(p.x/ms)*2.-1.);
        
        float indexz = floor(p.x/ms);

        float idx = float(i)/float(cnt);     
        

        // p3.z+= sin(idxx+time+idxi*10.)*5.;
        // p3.z+= sin(float(i)*1.+time+idxx)*0.03;
		//p.z+=sin(idx*4.+time);
		
		//p.x+=sin(idx*4.+time*.2)*4.;
		//p.y+=sin(idx*4.+time*.2)*4.;
		 p.x = abs((mod(p.x,ms) - ms/2.)*2.-.5);
		 p.y = abs((mod(p.y,ms) - ms/2.)*2.-.5);
		 p.z =  mod(p.z,ms) - ms/2.;
		
		//p.z+=sin(idx*4.+time);
		//p.x+=sin(idx*4.+time);
		//p.y+=cos(idx*8.+time);
		
		p.xz+=vec2(0.5);
		p.xy*=rotate2d(time*.25);
		p.xz-=vec2(0.5);
		
		
		//p.xz+=vec2(0.5);
		//p.xz*=rotate2d(time*.25);
		//p.xz-=vec2(0.5);
		
		//p.z+=sin(idx*2.+time*.5)*1.;
		//p.x+=cos(idx*8.+time*0.1)*2.;
		
		float sr = rdm(seed+255.+float(i)*535.01);
		vec3 s = vec3(sin(time+time)*2.,1.0,1.0);
		float r2 =rdm(seed+2302.)*4.5;
	    //sr = sin(time*10.)*.5+.5;
		float r3 = rdm(seed+10.)*4.8;
		
		float z = mapr(rdm(seed+5321.),0.2,0.6);
		z = 0.1;
		if(sr < 0.3){
			s = vec3(r3,r3,r2*z);
		}else if(sr < 0.6){
			s = vec3(r3,r2,r3*z);
		}else{
			s = vec3(r2,r3,r2*z);
		}
		/*s = vec3(r2,1.,1.);
        	s = vec3(r3,r2,r3);*/
        oct = min(oct,sdBox(p3,s));
    }
    return oct *.8;

}
float sdEscena2_3(vec3 p){
    
    const int cnt = 4;
  

    float ms = floor(4.)+1.;

    float spg = 3.;
    float oct = 1.;
    float idxx = abs(floor(p.x/ms)*2.-1.);
    float idxz = abs(floor(p.z/ms)*4.-2.);
    float idxy = abs(floor(p.x/ms)*2.-1.);

    
    for(int i=0; i<cnt;i++){
        vec3 p3 = p;   
        float ms = 5.;
		float idxi = float(i)/float(cnt);
        float idxx = abs(floor(p.x/ms)*2.-1.);
        float idxz = abs(floor(p.z/ms)*2.-2.);
        float idxy = abs(floor(p.x/ms)*2.-1.);
        
        float indexz = floor(p.x/ms);

        float idx = float(i)/float(cnt);     
        
        //p.xz*=rotate2d(time+idxz);
       
        p.x = abs(ms-mod(p.x,ms*2.));
        p.y = abs(ms-mod(p.y,ms*2.));
        p.z = abs(ms-mod(p.z,ms*2.));

		p.xz+=vec2(0.5);
		p.xy*=rotate2d(time*.25);
		p.xz-=vec2(0.5);
		
		
		float sr = rdm(seed+255.+float(i)*535.01);
		vec3 s = vec3(sin(time+time)*2.,1.0,1.0);



		float r2 =mapr(rdm(seed+2302.),0.5,2.5);
		float r3 = mapr(rdm(seed+10.),0.5,2.8);
		float z = mapr(rdm(seed+5321.),0.2,0.6);
		z = 0.1;
		if(sr < 0.3){
			s = vec3(r3,r3,r2*z);
		}else if(sr < 0.6){
			s = vec3(r3,r2,r3*z);
		}else{
			s = vec3(r2,r3,r2*z);
		}
		/*s = vec3(r2,1.,1.);
        	s = vec3(r3,r2,r3);*/
        oct = min(oct,sdBox(p3,s));
    }
    return oct *.8;

}
float sdEscena3(vec3 p){
    
    const int cnt = 1;


    float ms = floor(4.)+1.;

    float spg = 3.;
    float oct = 1.;
    
    float idxx = abs(floor(p.x/ms)*2.-1.);
    float idxz = abs(floor(p.z/ms)*4.-2.);
    float idxy = abs(floor(p.x/ms)*2.-1.);

    for(int i=0; i<cnt;i++){
        vec3 p3 = p;   

        float ms = floor(2.+float(i)*2.);
        

         //abs(t - mod(p, t*2.));
        float idxx = abs(floor(p.x/ms)*2.-1.);
        float idxz = abs(floor(p.z/ms)*2.-1.);
        float idxy = abs(floor(p.x/ms)*2.-1.);

        float indexz = floor(p.x/ms);
		float idx = float(i)/float(cnt);     
		//p.z+=time;
	
		//float idxrd = rdm(idx+seed*410.);
		//p.xy *=rotate2d(time*.01+mapr(rdm(idx+seed*75.),-0.1,0.1));
		//p.xz *=rotate2d(time*.01+mapr(rdm(idx+seed*24.),-0.1,0.1));
		
		//p.x = mod(p.x,ms) - ms/2.;

        // abs(t - mod(p, t*2.));
		//p.x = abs((mod(p.x,ms) - ms/2.)*2.-1.);
		// p.y+=sin(idxx*10.+time*.1)*4.;
         // p.xz*=rotate2d(time*0.02+p.y*0.02);
        p.x = abs(ms - mod(p.x,ms*2.));
        //p.y = mod(p.y,ms) - ms/2.;
		
        
      //  p.y = mod(p.y,ms*2.)-ms/2.;
        p.y =  abs(ms - mod(p.y,ms*2.));
       // p.z = mod(p.z,ms) - ms/2.;
        p.z = abs(ms-mod(p.z,ms*2.));
        //p.z = mod(p.z,ms) - ms/2.;
		
		
        
		vec3 mov = vec3(mapr(rdm(seed+1001.+float(i)*120.),-spg,spg),
                        mapr(rdm(seed+3213.+float(i)*120.),-spg,spg),
                        mapr(rdm(seed+53413.+float(i)*120.),-spg,spg));
	//	p+=sin(mov*10.);
        
        float fr = floor(mapr(idx,8.0,12.0));
              fr = 4.+float(i)*2.;
	
		p.x+=sin(p.y*4.+time*2.+idx+idx*PI+sin(p.x*5.+time*.5+idx*10.)*0.1+idxx+idxz)*.1;
		p.y+=cos(p.x*4.+time*2.+idx+idx*PI+sin(p.y*5.+time*.5+idx*5.)*0.1+idxx+idxz)*.1;
		
		
      
		float idr = rdm(seed+23091.+idxz)*1.;
		float idr2 = rdm(seed+321.+idx)*1.;
		float idr3 = rdm(seed+4123.+idx)*1.;
        float idr4 = rdm(seed+41234.+idx)*1.;
		

        p*=vec3(1.5);
    


        vec3 a = vec3(mapr(idr4,0.1,1.0),
                    mapr(idr3,0.1,1.0),
	                mapr(idr2,0.1,1.0));

          vec3 b = vec3(mapr(idr,0.1,10.0+idr2),
                        mapr(idr,0.1,10.0+idr3),
                        mapr(idr,0.1,10.0+idr4));

		oct = min(oct,sdCapsule(p,
		a,
		b,
		mapr(rdm(seed+564.4),0.1,0.18)
        ));
       
    }
    return oct ;

}
float sdEscena4(vec3 p){
    
    const int cnt = 4;
    
    float spg = 0.5;
    vec3 mov1 = vec3(mapr(rdm(seed+1001.),-spg,spg),
                      mapr(rdm(seed+2351.),-spg,spg),
                      mapr(rdm(seed+4321.),-spg,spg));
    float oct = 1.;
  
    for(int i=0; i<cnt;i++){
        vec3 p3 = p;
        vec3 mov = vec3(mapr(rdm(seed+4582.+float(i)*120.),-spg,spg),
                        mapr(rdm(seed+1535.+float(i)*240.),-spg,spg),
                        mapr(rdm(seed+5683.+float(i)*510.),-spg,spg));
       // vec3 mov = vec3(0.5,0.5,0.5);
        float idx = float(i)/float(cnt);     
        float fr = floor(mapr(idx,8.0,12.0));
            fr = 6.+float(i)*2.;
         oct = min(oct,sdQuad(p3,fr*0.9 ,rdm(idx*24.+4653.)));
    }
    return oct ;
}
float sdEscena5(vec3 p){
    
    const int cnt = 4;
    
    float spg = 0.5;
    vec3 p2 = p;
  //  p2.y+=time*2.0;
    p2.xz*=rotate2d(time*0.02+p.y*0.02);
    
    float fsize = mapr(rdm(seed+46852.),2.0,3.5);
	float ftile = mapr(rdm(seed+62653.),2.0,5.0);


    p2 = tile(p2,ftile);
   float oct = 1.0;
   oct = min(oct,sdTorus(p2,vec2(fsize,mapr(rdm(234109.+seed),0.1,1.0))));
     p2.x+=time*0.02;


    return oct ;
}


float de(vec3 p) {

    light1 = length(p - lightpos1) - .1; // distancia a las luces, que están definidas como esferas de radio .1
    light2 = length(p - lightpos2) - .1; // 
    
    vec3 p2 = p;
	
    float oct = 0.;
         
	float sceneindex = floor(escenaactiva*4.);
       // sceneindex = escenaactiva;
		//sceneindex =1.0;
//	p.x+=time;
	if( sceneindex == 0.0){
		oct = sdEscena1(p);
	}else if(sceneindex == 1.0){
		oct = sdEscena2_2(p);
	}else if(sceneindex == 2.0){
		oct = sdEscena3(p);
	}else if(sceneindex == 3.0){
        oct = sdEscena4(p);
    }else if(sceneindex == 4.0){
        oct = sdEscena5(p);
    }
	//oct = sdEscena5(p);
   // oct = sdEscena3(p);
    //oct = min(oct,sdEscena4(p));
    float d = min(oct, min(light1, light2)); // obtención de distancia mínima (combinar objetos)

    float spd = 8.8;
    float d2 = max(d,1.-sdSphere(p2-from,spd)); //Por que mierda no lo corta ? ni idea. 
       
    return d2*0.5;
}


// función normal (vector perpendicular a la superficie)
vec3 normal(vec3 p) {
    vec2 d = vec2(0., det);
    return normalize(vec3(de(p+d.yxx), de(p+d.xyx), de(p+d.xxy)) - de(p));
}

// función shade (p = posynto en el que golpeó el rayo, dir = dirección del rayo)
vec3 shade(vec3 p, vec3 dir)
{  
   float l1 = length(p);
    
   light1color = vec3(1.0,fract(abs(sin(l1*5.+time*.01)*2.)-1.),1.0);
   light2color = vec3(0.0,0.0,0.0);
    
   vec3 c1 = vec3(r1,g1,b1);
   vec3 c2 = vec3(r2,g2,b2);
    
   float rsc1 = 1.2;
   float rsc2 = 0.8;
   float e = ridgedMF(vec2(p.x*0.05,p.y*0.05)
			  *ridgedMF(vec2(p.x*0.0025,
			                 p.y*0.0025))-p.xz);
      //  e = smoothstep(0.2,0.8,e);
        
        float mlmode = floor(lmode*3.);
                mlmode = lmode;
        if(mlmode == 0.){
            e = sin(l1*2.+time*0.1)*.5+.5;
            light1color = mix(c1,c2,e);
        }else if( mlmode == 1.){
            e = smoothstep(0.2,0.4,abs(fract(l1*2.+time)*2.-1.));
            light1color = mix(c1,c2,e);
        }else if( mlmode == 2.){
          light1color = hsb2rgb(vec3(sin(l1*2.+time)*.5+.5,cos(l1*2.+time)*.2+.6,cos(l1*4.+time)*.5+.5));
        }

    if (light1 < det) return light1color; // si golpeó a una luz, devolver el color de la misma
    if (light2 < det) return light2color; // sin aplicar obviamente el cálculo de su propia iluminación
    
    vec3 lightdir1 = normalize(lightpos1 - p); // obtención de la dirección hacia donde están las luces
    vec3 lightdir2 = normalize(lightpos2 - p); // desde el posynto p
    vec3 lightdir3 = normalize(lightpos3 - p); // desde el posynto p
     
    
    float fade1 = exp(-.2 * distance(p, lightpos1))*3.; // atenuación de la luz basada en la distancia entre p
    float fade2 = exp(-.2 * distance(p, lightpos2)); // y la posición de las mismas
    
    vec3 n = normal(p); // obtención de la normal
    
    
    float amb = .01; // luz ambiental
         amb = 0.01; // luz ambiental
    vec3 dif1 = max(0., dot(lightdir1, n)) * light1color * fade1 * .7; // luces difusas, se aplica el color de la luz
    vec3 dif2 = max(0., dot(lightdir2, n)) * light2color * fade2 * .7; // y la atenuación según la distancia
    vec3 ref1 = reflect(lightdir1, n); // vector reflejo entre la dirección de la luz y
    vec3 ref2 = reflect(lightdir2, n); // el normal de la superficie
    
    vec3 spe1 = pow(max(0., dot(ref1, dir)),10.) * light1color * fade1; // calculo de luz especular, también 
    vec3 spe2 = pow(max(0., dot(ref2, dir)),10.) * light2color * fade2; // teniendo en cuenta la atenuación por distancia
    
    return amb + dif1 + spe1  ; // color final combinando las luces
}

// función de raymarching
vec3 march(vec3 from, vec3 dir) 
{
    float maxdist = 50.;
    float totdist = 0.;
    const float steps = 100.;
    float d;
    vec3 p;
    vec3 col = vec3(0.);
    float glow1 = 0., glow2 = 0.,glow3 = 0.; // variables para la obtención del brillo "glow" alrededor de las luces
    float glowgeneral = 0.; // variable para la obtención de glow general con "step count"
    for (float i=0.; i<steps; i++)
    {
        p = from + totdist * dir;
        d = de(p);
        if (d < det || totdist > maxdist) break;
        totdist += d;
        glow1 = max(glow1, 1. - light1); // capturamos cuando el rayo pasa cerca de las luces, obteniendo un valor
        glow2 = max(glow2, 1. - light2); // entre 0 y 1 según la distancia a la que pasó
      
        glowgeneral++; // step counting para obtener brillo glow general
    }
    
    if (d < det){
        col = shade(p, dir);
    }/*else{
		col = sin(p*2.)*.1;
	}*/
    col += pow(glow1, 5.) * light1color; // sumamos el brillo glow de las luces, elevando a un exponente
	
    return col;
}


void main(void){
    vec2 uv = (gl_FragCoord.xy - resolution / 2.) / resolution.y;
		//uv = vTexCoord*1.-.5;
		uv.x*=resolution.x/resolution.y;


    camoffset = vec3(posx,posy,posz);
  
    look = vec3(lookx,looky,lookz);
    from = vec3(0.+posx, 0.+posy, -25.+posz);
	//from = vec3(0., 0., -25.);
	//from.z = posz;
	
    v1 = vec3(0.0,1.0,.0);
    vec3 dir = normalize(vec3(uv, 1.0));
        
    //lightpos1 = vec3(sin(time) * 8., sin(time * 0.), cos(time) * 4.); // definimos la posición de las luces


    vec3 mousevec =  vec3(mapr(mouse.x,-12.,12.),mapr(1.-mouse.y,8.,-8.),  cos(time) * 1.);
    vec3 animvec =  vec3( cos(time) * 4.,sin(time)*2.,  cos(time) * 2.);

    mousevec = lookat(look,v1)*mousevec;
    animvec = lookat(look,v1)*animvec;
	vec3 p1 = from + animvec; // Animacion standart
	vec3 p2 = from + mousevec; //Control mouse

    
    dir= lookat(look,v1) *dir;
    vec3 p3 = mix(p1,p2,lerpm) + look*10.;
	lightpos1 = p3;

    vec3 col = march(from, dir);
    fragColor = vec4(col, 1.);
}