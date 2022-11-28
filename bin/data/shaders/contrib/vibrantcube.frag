#pragma include "../common.frag"

#define rand(x) fract(sin(x)*43758.5453)
uniform float speed ;

float rand2(vec2 p) {
  return rand(dot(p, vec2(24.1214, 15.2321)));
}

float rand3(vec3 p) {
  return rand(dot(p, vec3(24.1214, 15.2321, 21.2362)));
}

mat2 rotate(float a) {
  return mat2(cos(a), -sin(a), sin(a), cos(a));
}

vec2 pmod(vec2 p, float n) {
  float a = pi*2. / n;
  float theta = atan(p.y, p.x) + .5*a;
  theta = floor(theta/a)*a;
  return p*rotate(-theta);
}

vec3 hsv2rgb(float h, float s, float v) {
  vec3 res = fract(h+vec3(0,2,1)/3.);
  res = abs(res*6.-3.)-1.;
  res = clamp(res, 0., 1.);
  res = (res-1.)*s+1.;
  res *= v;
  return res;
}

float interval_xy = 10.;
float interval_z = 5.;

float dist1(vec3 p) {
  vec3 z = p;
  vec2 id = floor(z.xy/interval_xy);
  
  z.z -= fract(iTime)*10.;
  z.xy = mod(z.xy, interval_xy) - .5*interval_xy;
  z.xy *= rotate(floor(z.z/interval_z)*.2 + iTime);
  z.z = mod(z.z, interval_z) - .5*interval_z;
  
  float r1 = rand2(id+floor(iTime*speed)*.1);
  z.xy = pmod(z.xy, floor(r1*8.+3.));
  z.x -= 1.5;

  return length(max(abs(z)-vec3(.5), 0.));
}

float dist2(vec3 p) {
  vec3 z = p;
  z = abs(z)-.25;
  z *= 2.;
  z = abs(z)-.25;
  z *= 2.;
  
  float size = .5+pow(sin(iTime*10.*speed)*.5+.5, 5.);
  size *= .5;
  return length(max(abs(z)-vec3(size), 0.))/4.;
}

vec3 calcNormal1(vec3 p, float eps) {
  vec2 e = vec2(0, eps);
  return normalize(vec3(dist1(p+e.yxx)-dist1(p-e.yxx),
  dist1(p+e.xyx)-dist1(p-e.xyx),
  dist1(p+e.xxy)-dist1(p-e.xxy)));
}

vec3 calcNormal2(vec3 p, float eps) {
  vec2 e = vec2(0, eps);
  return normalize(vec3(dist2(p+e.yxx)-dist2(p-e.yxx),
  dist2(p+e.xyx)-dist2(p-e.xyx),
  dist2(p+e.xxy)-dist2(p-e.xxy)));
}

float exp2Fog(float dist, float density) {
  float s = dist*density;
  return exp(-s*s);
}

void main() {
  vec2 p = (fragCoord*2.-iResolution.xy) / min(iResolution.x, iResolution.y);
  vec3 col = vec3(0);
  
  float c = iTime;
  float L = 1.-fract(c);
  float id = ceil(c);
  for(int i=0; i<10; i++) {
    float a = atan(.7, L)*3.;
    vec2 z = p/a;
    float r1 = rand(id);
    float r2 = rand(id+20.);
    z = pmod(z, floor(r1*10.+3.));
    z.x += -1.;
    z *= rotate(iTime*.5+pi*2/20.*id);
    z = pmod(z, floor(r1*8.+3.));
    z.x += -1.;
    z = pmod(z, floor(r1*8.+3.));
    col += .005/abs(z.x-.5)*hsv2rgb(r2, 1., 1.);
    L++;
    id++;
  }
  
  vec3 cPos = vec3(0, 0, 5.);
  vec3 cDir = vec3(0, 0, -1);
  vec3 cUp = vec3(0, 1, 0);
  cUp.xy *= rotate2d(iTime*.1);
  vec3 cSide = cross(cDir, cUp);
  vec3 ray = normalize(p.x*cSide + p.y*cUp + cDir*2.5);
  
  float d = 0.;
  vec3 rPos = cPos;
  for(int i=0; i<100; i++) {
    d = dist1(rPos);
    if(d<0.001) {
      break;
    }
    rPos += ray*d;
  }
  
  if(d<0.1) {
    vec3 normal = calcNormal1(rPos, 0.0001);
    vec3 normal2 = calcNormal1(rPos, 0.1);
    float edge = clamp(length(normal-normal2), 0., 1.);
    vec2 id = floor(rPos.xy/interval_xy);
    float r1 = rand2(id+floor(iTime*speed)*.1);
    float fog = exp2Fog(length(rPos-cPos), 0.01);
    col = mix(col, hsv2rgb(r1, 1., 1.)*edge, fog);
  }
  
  cPos.zx *= rotate(.5);
  cPos.yz *= rotate(iTime*speed);
  cDir = normalize(-cPos);
  vec3 up = vec3(0, 1, 0);
  cSide = normalize(cross(cDir, up));
  cUp = normalize(cross(cSide, cDir));
  ray = normalize(p.x*cSide + p.y*cUp + cDir*2.5);
  
  d = 0.;
  rPos = cPos;
  for(int i=0; i<30; i++) {
    d = dist2(rPos);
    if(d<0.0001 || length(rPos-cPos) > 10.) {
      break;
    }
    rPos += ray*d;
  }
  
  vec3 lightDir = normalize(vec3(1));
  if(d<0.1) {
    vec3 normal = calcNormal2(rPos, 0.0001);
    vec3 normal2 = calcNormal2(rPos, 0.01);
    float edge = clamp(length(normal-normal2)*1.5, 0., 1.);
    vec3 id = floor(rPos*4.);
    float diff = max(dot(lightDir, normal), .0);
    float r1 = rand3(id+floor(iTime*10.*speed)*.1);
    col = hsv2rgb(r1, diff*.5+.5, 1.)*(1.-edge);
  }
  
  fragColor = vec4(col, 1.);
}