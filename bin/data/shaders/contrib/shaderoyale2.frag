#pragma include "../common.frag"
// Shader coded live on stream for Shader Royale #2 (about 1h20) - 4 december 2020
// Placed 3rd among 13 awesome shaders made simultaneously
//
// I added a bit of progression on time2 to this shadertoy version
// so effects activate one after the other in the first 60s
// I also added all the comments
//
// this shader may be a bit too fast if you don't have any music
// you can lower the SPEED bellow to adjust to your taste

uniform float SPEED  = 0.5 ;
#define COLOR_SHIFT 1
#define CENTER_COLOR 1
#define RAINBOW 1

float progression = 0.0;
float time2 =0.0;



// rotation matrix
mat2 rot(float a) {
  float ca=cos(a);
  float sa=sin(a);
  return mat2(ca,sa,-sa,ca);  
}

// simple box distance field
float box(vec3 p, vec3 s) {
  p=abs(p)-s;
  return max(p.x, max(p.y,p.z));
}

// random value
float rnd(float t) {
  return fract(sin(t*425.551)*974.512);  
}

// noise on uv 
float rnd(vec2 uv) {
  return fract(dot(sin(uv*352.742+uv.yx*254.741),vec2(642.541)));
  
}

// very useful function to get random beats and motion, d is the frequency of the pulses
float curve(float t, float d) {
  t/=d;
  return mix(rnd(floor(t)), rnd(floor(t)+1.0), pow(smoothstep(0.0,1.0,fract(t)), 10.0));
}


// tick function that shape the t value to had pulses every second
float tick(float t) {
  float g=fract(t);
  g=smoothstep(0.,1.,g);
  g=pow(g,10.);
  return floor(t)+g;
}

// KIFS like space folding
// used on the central piece
vec3 fractal(vec3 p, float t) {
  
  float s= 2.0 + (curve(time2, .3)-0.5)*.1;
  for(float i=0.; i<3.; ++i) {
    p.xz *= rot(t);
    p.yz *= rot(t*1.3);
    p.xz=abs(p.xz)-s*(1.+vec2(rnd(i),rnd(i+.1)));
    s*=0.7;
  }
  
  return p;
}

// smooth minimum between two distance fields
float smin(float a, float b, float h) {
  float k=clamp((a-b)/h*0.5+0.5,0.,1.);
  return mix(a,b,k) - k*(1.-k)*h;
}

// repeat a shape at each s intervals
vec3 repeat(vec3 p, vec3 s) {
  return (fract(p/s+0.5)-0.5)*s;
}

float center = 10000.; // keep distance value of the center piece for latter use in shading

// Main distance field function
float map(vec3 p) {  
  
  // add a ripple effect from the center
  p.y += curve(time2*3. - length(p.xz)*0.02, 1.)*8.;
  
  float t=tick(time2)*2.;
  
  // center piece made by intersecting two boxes on kifs
  float d=box(fractal(p, t*.3), vec3(0.3,1.3,0.4));
  float d2=box(fractal(p+vec3(1), t*.4), vec3(0.3,1.3,0.4)*2.);
  d = abs(max(d,d2))-0.2;
  
  // tried adding something interesting to the center piece, mostly useless
  float d5 = box(fractal(p-vec3(0,10,0), t*.1), vec3(-5,-5,1000.0));  
  d = min(d, d5);
  center = d;  
  
  // ground floor, blended with the center piece
  d = smin(d, -p.y+5., 1. + curve(time2, 0.3)*15.);
  
  
  // the little swirly pointy towers thingies, with 32 copies
  vec3 p2=p;  
  p2.xz = abs(p2.xz)-40.;
  p2.xz = abs(p2.xz)-20.;
  p2.xz = abs(p2.xz)-10.*curve(time2, .3);
  
  // make it swirly
  p2.xz *= rot(p2.y*curve(time2, 0.4)*0.5-time2);
  
  // make it pointy
  float sd = -abs(p.y)*0.3+3.;
  d = min(d, box(p2, vec3(sd,10,sd)));
  
  // the flying spheres
  vec3 p3=p;
  p3=repeat(p3, vec3(13));
  // randomise sphere sizes a bit
  float d3 = length(p3)-.1-sin(p.x*.3)*.3-sin(p.y*.2)*.3-sin(p.z*.1)*.3 - sin(length(p)*0.03-time2)*.1;
  // ne sphere near the ground
  d3 = max(d3, p.y+1.);
  d = min(d, d3);
  
  // makes it hollow, so that latter we can x-ray everything
  d=abs(d)-0.1;
  return d;
}

// camera rotations
void cam(inout vec3 p) {  
  float t=time2*0.3;
  p.yz *= rot(.4 + sin(t*.2)*.4);
  p.xz *= rot(t);
}

void main()
{
  // we loop every now and then so precision is not too much of an issue
  time2 = mod(iTime*SPEED, 300.); 
  progression = mod(iTime, 300./SPEED);
  
  // bonzomatic centered uv's
  vec2 uv = vec2(fragCoord.x / iResolution.x, fragCoord.y / iResolution.y);
  uv -= 0.5;
  uv /= vec2(iResolution.y / iResolution.x, 1);

  ///////
  // first step: uv effects
  ///////
    
  // centered ripples in the uvs
  uv *= 1.+max(0.,curve(time2 - length(uv),.2)-.7)*.6;
  // vertical pulses
  uv.y -= curve(time2, .2)*.1;
  
  // distance to the center circle
  float di2 = pow(curve(time2, .8),4.)*3.-length(uv)-.1;
  bool circ = di2<0.;
  bool truc = curve(time2, 6.)<.7;
  vec2 nu = vec2(0);
  vec2 nuv = uv;
  // every now and then, horizontal symmetry on mosaic
  if(!truc) nuv.x = abs(nuv.x)*fract(time2);
    
  if(progression<7.) {
      circ=false;
      truc=false;
  }
  
  // mosaic computation
  float di = 1000.; // distance to nearest mosaic piece
  // kaleidoscopic effect, 4 random successive symmetries and rotations
  for(float i=0.; i<4.; ++i) {
    // random rotation
    nuv *= rot(curve(time2+i*0.32,1.2)*2.);
    // random vertical offset evolving with time2
    nuv.y += curve(time2+.7+i*.1,.5)-.5;
    // at each step we add a slight uv distortion different for each piece
    nu += sign(nuv)*(1.+i*0.2);
    // symmetry
    nuv=abs(nuv);
    // retreive distance to nearest symmetry axe
    di = min(di, min(nuv.x,nuv.y));
    // random offset on the symmetry
    nuv-=0.4*rnd(nu);
  }
  
  // value used for camera cuts
  float pulse = floor(time2*0.7);  
  // apply uv distorting from mosaic pieces
  if(circ && truc) uv += nu*(0.03+0.03*curve(time2+.7,.3))*.5;
  
  ///////
  // second step: 3D raymarching
  ///////
    
  // camera orbit starting position, with random x offset and random distance
  vec3 s=vec3((curve(time2+2.3,.8)-.5)*20. ,-10.,-30. - curve(time2, 1.3)*40.);
  // camera perspective, random fov
  vec3 r=normalize(vec3(-uv, .5 + curve(pulse, .7)*2.));
  
  // apply camera rotation
  cam(s);
  cam(r);
  
  vec3 p=s;
  vec3 col = vec3(0);
    
  // noise value that will elimitate banding that comes from iteration glow
  float mu=mix(0.9,1.0,rnd(uv));
  
  // decide for each mosaic piece, if raymarching will be reflexive or xray
  bool band = rnd(nu)>0.7 && circ;
  if(rnd(pulse+.23)<.4) band=true;
    
  // color or the colored mosaic piece
  vec3 diff=vec3(1,0.4,0.5);
  float t2 = tick(time2*.5)*.7;
  #if COLOR_SHIFT
    // rotate the color with time2
  	diff.xz *= rot(t2);
    diff.yz *= rot(t2*.7);
    diff=abs(diff);
  #endif
  
  // change smoothness of the glow with time2
  float de = 0.01*(1.+sin(abs(uv.x*3.)-time2*4.)*0.8);
  
  // RAYMARCHING LOOP
  vec3 diff2 = vec3(1);
  bool maa=false;
 // int zero = min(0,iFrame); // disable possible loop unrolling
  int zero = 0; // disable possible loop unrolling
  for(int i=zero; i<100; ++i) {
    float d=map(p);
    if(d<0.001) {
      // We found a collision
      if(band) {
        // inside a xray mosaic piece
        // just advance the ray a bit and continue marching through everything  
      	d=0.1;
        #if CENTER_COLOR
        if(center<10. && progression>24.) {
          // this handle the color shift of the center 3D piece
          diff.xz *= rot(.1);
          diff=abs(diff);
          maa=abs(center-10.)<1.;
        }
        #endif
      } else {
        // inside a reflexive mosaic piece
        vec2 off=vec2(0.01,0);
        #if CENTER_COLOR
        if(center<10. && progression>24.) {
          // this handle the color shift of the center 3D piece
          diff2 = vec3(.3,8,10)*curve(time2, .75);
          maa=abs(center-10.)<1.;
        }
        #endif
        // compute surface normal
        vec3 n=normalize(d-vec3(map(p-off.xyy), map(p-off.yxy), map(p-off.yyx)));
        // reflect the ray
        r=reflect(r,n);        
        // avance a little bit so we are out of the surface  
      	d=0.1;
      }
      //break;
    }
    // if we are too far, break out the loop to save performance
    if(d>100.0) break;
    // raymarching step
    p+=r*d*mu;
    // accumulate diffrent color if we are reflexive or xray
    if(band) {
      col += diff*0.3*de/(de+.1+abs(d));
    } else {
      col += diff2*vec3(0.7,0.7,0.7)*0.1*de/(de+abs(d));
    }
  }
  
  ///////
  // third step: color postprocessing
  ///////
    
  // mosaic black edges (outside circle)
  if(circ) col *= smoothstep(0.0,0.01,di);
  // circle edge
  if(circ) col *= smoothstep(0.0,0.02,abs(di2));
  // 3D center piece dark edges
  if(maa) {
    col *= .2;
  }
  
  #if RAINBOW
  // rainbox band I added at the end, probably missing some black edges but I didn't have the time2 ...
  if(rnd(pulse)<0.6 && abs(uv.y)<.1 && progression>60.) {
    float t4 = time2 + uv.x;
    col.yz *= rot(t4);
    col.xz *= rot(t4*1.3);
    col=abs(col);
  }
  #endif
  
  // vignette
  col *= 1.2-length(uv);
  
  // "tonemapping"
  col=smoothstep(0.,1.,col);
  col=pow(col, vec3(0.4545));
  
  fragColor = vec4(col, 1);
}