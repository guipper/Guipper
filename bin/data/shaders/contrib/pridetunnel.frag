#pragma include "../common.frag"

// Copyright © 2021-2022 rimina.
// All rights to the likeness of the visuals reserved.

// Any individual parts of the code that produces the visuals is
// available in the public domain or licensed under the MIT license,
// whichever suits you best under your local legislation.

// This is to say: you can NOT use the code as a whole or the visual
// output it produces for any purposes without an explicit permission,
// nor can you remix or adapt the work itself without a permission.*
// You absolutely CANNOT mint any NFTs based on the Work or part of it.
// You CAN however use any individual algorithms or parts of the Code
// for any purpose, commercial or otherwise, without attribution.

// *(In practice, for most reasonable requests, I will gladly grant
//   any wishes to remix or adapt this work :)).
uniform float speed ;


const float E = 0.001;
const float FAR = 200.0;
const int STEPS = 64;

float ID = 0.0;
bool FLIP = false;

vec3 glow = vec3(0.0);

struct Material{
  vec3 l;
  float li;
  vec3 s;
  float si;
};

Material red(){
  
  Material m;
  m.l = vec3(1.0, 0.0, 0.0);
  m.li = 0.5;
  m.s = vec3(1.2, 0.2, 0.2);
  m.si = 0.5;
  
  return m;
}

Material orange(){
  
  Material m;
  m.l = vec3(1.0, 0.5, 0.0);
  m.li = 0.5;
  m.s = vec3(1.2, 0.7, 0.2);
  m.si = 0.5;
  
  return m;
}

Material yellow(){
  
  Material m;
  m.l = vec3(1.0, 0.8, 0.0);
  m.li = 0.5;
  m.s = vec3(1.2, 1.0, 0.2);
  m.si = 0.5;
  
  return m;
}

Material green(){
  
  Material m;
  m.l = vec3(0.0, 1.0, 0.0);
  m.li = 0.5;
  m.s = vec3(0.2, 1.2, 0.2);
  m.si = 0.5;
  
  return m;
}

Material blue(){
  
  Material m;
  m.l = vec3(0.0, 0.0, 1.0);
  m.li = 0.5;
  m.s = vec3(0.2, 0.2, 1.2);
  m.si = 0.5;
  
  return m;
}

Material purple(){
  
  Material m;
  m.l = vec3(0.8, 0.0, 0.6);
  m.li = 0.5;
  m.s = vec3(1.0, 0.2, 0.8);
  m.si = 0.5;
  
  return m;
}

float sphere(vec3 p, float r){
  return length(p) - r;
}

float cylinder(vec3 p, vec3 c){
  return length(p.xz-c.xy)-c.z;
}

// Cylinder standing upright on the xz plane
float fCylinder(vec3 p, float r, float height) {
	float d = length(p.xz) - r;
	d = max(d, abs(p.y) - height);
	return d;
}


void rotate(inout vec2 p, float angle){
  p = cos(angle) * p + sin(angle) * vec2(p.y, -p.x);
}

//USING HG SDF LIBRARY!
// Repeat around the origin by a fixed angle.
// For easier use, num of repetitions is use to specify the angle.
float pModPolar(inout vec2 p, float repetitions) {
	float angle = 2.0*PI/repetitions;
	float a = atan(p.y, p.x) + angle/2.0;
	float r = length(p);
	float c = floor(a/angle);
	a = mod(a,angle) - angle/2.0;
	p = vec2(cos(a), sin(a))*r;
	// For an odd number of repetitions, fix cell index of the cell in -x direction
	// (cell index would be e.g. -5 and 5 in the two halves of the cell):
	if (abs(c) >= (repetitions/2.0)) c = abs(c);
	return c;
}

// 3D noise function (IQ)
float noise(vec3 p){
    vec3 ip = floor(p);
    p -= ip;
    vec3 s = vec3(7.0,157.0,113.0);
    vec4 h = vec4(0.0, s.yz, s.y+s.z)+dot(ip, s);
    p = p*p*(3.0-2.0*p);
    h = mix(fract(sin(h)*43758.5), fract(sin(h+s.x)*43758.5), p.x);
    h.xy = mix(h.xz, h.yw, p.y);
    return mix(h.x, h.y, p.z);
}

float scene(vec3 p){
  
  vec3 pp = p;
  
  float offset = 12.0;
  
  float id = floor((pp.z + offset*0.5) / offset);
  pp.z = mod(pp.z+offset*0.5, offset)-offset*0.5;
  
  rotate(pp.yz, radians(90.0));
  
  float tunnel = -fCylinder(pp, 12.0, 12.0);
  
  rotate(pp.yz, -radians(90.0));
  
  if(mod(id, 2.0) == 0.0){
    rotate(pp.xy, time*speed*4.);
  }
  else{
    rotate(pp.xy, -time*speed*4.);
  }
  
  ID = pModPolar(pp.xy, offset);
  pp.x -= offset*0.5;
  pp -= noise(p)*0.9;
  
  float blob = sphere(pp, 1.0);
  
  glow += vec3(0.8, 0.2, 0.6) * 0.01 / (abs(tunnel) + 0.06);
  
  if(tunnel < blob){
    FLIP = true;
  }
  else{
    FLIP = false;
  }
  
  return min(tunnel, blob);
}

float march(vec3 ro, vec3 rd){
  float t = E;
  vec3 p = ro;
  for(int i = 0; i < STEPS; ++i){
    float d = scene(p);
    t += d;
    p = ro + rd*t;
    
    if(d < E || t > FAR){
      break;
    }
  }
  
  return t;
}


vec3 normals(vec3 p){
  vec3 e = vec3(E, 0.0, 0.0);
  return normalize(vec3(
    scene(p + e.xyy) - scene(p - e.xyy),
    scene(p + e.yxy) - scene(p - e.yxy),
    scene(p + e.yyx) - scene(p - e.yyx)
  ));
}

vec3 shade(vec3 rd, vec3 p, vec3 n, vec3 ld){
  if(FLIP){
    n = -n;
    ld = -ld;
  }
  float lambertian = max(dot(n, ld), 0.0);
  float angle = max(dot(reflect(ld, n), rd), 0.0);
  float specular = pow(angle, 10.0);
  
  Material m = red();
  
  if(abs(ID) > 0.0 && abs(ID) <= 1.0){
    m = orange();
  }
  else if(abs(ID) > 1.0 && abs(ID) <= 2.0){
    m = yellow();
  }
  else if(abs(ID) > 2.0 && abs(ID) <= 3.0){
    m = green();
  }
  else if(abs(ID) > 3.0 && abs(ID) <= 4.0){
    m = blue();
  }
  else if(abs(ID) > 4.0 && abs(ID) <= 5.0){
    m = purple();
  }
  
  return lambertian * m.li * m.l + specular * m.si * m.s;
}

void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;
    uv -= 0.5;
	uv /= vec2(iResolution.y / iResolution.x, 1);

    vec3 ro = vec3(0.0, 2.0, time*speed*4.*10.0);
    vec3 rt = vec3(0.0, 1.0, ro.z + 20.0);

    vec3 z = normalize(rt-ro);
    vec3 x = normalize(cross(z, vec3(0.0, 1.0, 0.0)));
    vec3 y = normalize(cross(x, z));

    vec3 rd = normalize(mat3(x,y,z) * vec3(uv, radians(40.0)));

    float t = march(ro, rd);
    vec3 p = ro + t * rd;
    vec3 n = normals(p);

    vec3 ld = -z;

    vec3 col = vec3(0.7, 0.3, 0.5);
    if(t < FAR){
    col = shade(rd, p, n, ld);
    }
    col += glow * 0.2;
    float d = distance(p, ro);
    float fog = 1.0 - exp(-d*0.005);
    col = mix(col, vec3(0.8, 0.3, 0.6), fog);

    col = smoothstep(-0.2, 1.1, col);
  

    // Output to screen
    fragColor = vec4(col,1.0);
}