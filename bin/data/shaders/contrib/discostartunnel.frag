#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// License: CC0
//  Star tunnel created while randomly coding while listening to 90s music
//  Code is kind of crappy but I liked the visual result so I decided to share it.



//#define PI              3.141592654
#define TAU             (2.0*PI)
#define TIME            iTime

float mod1(inout float p, float size) {
  float halfsize = size*0.5;
  float c = floor((p + halfsize)/size);
  p = mod(p + halfsize, size) - halfsize;
  return c;
}

vec2 offset(float time, float z) {
  z *= 0.1*sqrt(0.5);
  vec2 o = vec2(0.0);
  vec2 r = vec2(2.0);
  o += r*vec2(cos(z), sin((sqrt(0.5))*z + pow(0.5 + 0.5*sin(sqrt(0.25)*z),5.0)));
  return o;
}

void rot2(inout vec2 p, float a) {
  float c = cos(a);
  float s = sin(a);
  p = vec2(c*p.x + s*p.y, -s*p.x + c*p.y);
}

vec2 toPolar(vec2 p) {
  return vec2(length(p), atan(p.y, p.x));
}

vec2 toRect(vec2 p) {
  return vec2(p.x*cos(p.y), p.x*sin(p.y));
}

const float smallRadii = pow(0.5, 6.0);
const float largeRadii = 1.0 + 2.0*smallRadii;
const float reps = float(int(largeRadii*TAU/(2.0*smallRadii)));
const float degree = TAU/reps;

float circle(vec2 p, float r) {
  return length(p) - r;
}

float star5(vec2 p, float r, float rf) {
  const vec2 k1 = vec2(0.809016994375, -0.587785252292);
  const vec2 k2 = vec2(-k1.x,k1.y);
  p.x = abs(p.x);
  p -= 2.0*max(dot(k1,p),0.0)*k1;
  p -= 2.0*max(dot(k2,p),0.0)*k2;
  p.x = abs(p.x);
  p.y -= r;
  vec2 ba = rf*vec2(-k1.y,k1.x) - vec2(0,1);
  float h = clamp( dot(p,ba)/dot(ba,ba), 0.0, r );
  return length(p-ba*h) * sign(p.y*ba.x-p.x*ba.y);
}

vec3 df(vec2 p, int gi) {
  float divend = 2.0*smallRadii + 0.5*pow(1.0 - cos(0.0), 2.0);
  float dx = largeRadii - 0.5*divend;
  vec2 op = p;
  vec2 pp = toPolar(p);
  float ny = pp.y;
  pp.y +=-0.33*smallRadii*float(gi);
  pp.x -= dx;
  float nx = pp.x/divend;
  pp.x = mod(pp.x, divend);
  pp.x += dx;
  float nny = mod1(pp.y, degree);
  pp.y += PI/2.0;
  p = toRect(pp);

  p -= vec2(0.0, largeRadii);

  float ymul = mod(nny, 2.0) > 0.0 ? 1.0 : -1.0;
  float xmul = mod(float(int(nx)), 2.0) > 0.0 ? 1.0 : -1.0;

  rot2(p, ymul*xmul*TIME*TAU/5.0);

  float d = star5(p, smallRadii, 0.25);
  float id = circle(op, largeRadii - smallRadii);
  d = max(d, -id);

  return vec3(d, nx, ny);
}

vec3 tunnelEffect(vec2 p) {
  vec3 col = vec3(0.0);

  float smoothPixel = 5.0/iResolution.x;

  const vec3 baseCol = vec3(1.0);
  const float zbase  = 10.0;
  const float zdtime = 0.25;
  const float zspeed = 10.0;
  float gtime   = TIME*0.25;
  float gz      = zspeed*gtime;
  vec2 outerOff = offset(gtime, gz);
  float fgtime  = mod(gtime, zdtime);
  for (int i = 22; i >= -2; --i) {
    int   gi      = i + int(gtime/zdtime);
    float lz      = zspeed*(zdtime*float(i) - fgtime);
    float zscale  = zbase/(zbase + lz);

    float iz      = gz + lz;
    vec2 innerOff = offset(gtime, iz);

    vec2 ip       = p + 0.5*zscale*(-innerOff + outerOff);
    float ld      = length(ip)/zscale;

    vec3 ddd      = df(ip/zscale, gi)*zscale;
    float d       = ddd.x;
    vec3 scol = baseCol*vec3(0.6 + 0.4*sin(TAU*ddd.y*0.005 - 0.2*iz), pow(0.6 + 0.4*cos(-2.0*abs(ddd.z)-0.4*iz-0.5*gtime), 1.0), 0.8);

    float diff = exp(-0.0125*lz)*(1.0 - 1.0*tanh(pow(0.4*max(ld - largeRadii, 0.0), 2.0) + 3.0*smallRadii*max(ddd.y, 0.0)));

    vec4 icol = diff*vec4(scol, smoothstep(0.0, -smoothPixel, d));

    icol.w += diff*diff*diff*0.75*clamp(1.0 - 30.0*d, 0.0, 1.0);
    icol.w += tanh(0.125*0.125*lz)*0.5*ld*clamp(1.5 - ld, 0.0, 1.0);

    col = mix(col, icol.xyz, clamp(icol.w, 0.0, 1.0));
  }


  return col;
}

vec3 postProcess(vec3 col, vec2 q) {
  col=pow(clamp(col,0.0,1.0),vec3(0.75));
  col=col*0.6+0.4*col*col*(3.0-2.0*col);  // contrast
  col=mix(col, vec3(dot(col, vec3(0.33))), -0.4);  // satuation
  col*=0.5+0.5*pow(19.0*q.x*q.y*(1.0-q.x)*(1.0-q.y),0.7);  // vigneting
  return col;
}


void main() {
  vec2 q = fragCoord/iResolution.xy;
  vec2 p = -1. + 2. * q;
  p.x *= iResolution.x/iResolution.y;

  vec3 col = tunnelEffect(p);

  col = postProcess(col, q);

  fragColor = vec4(col.xyz, 1.0);
}
