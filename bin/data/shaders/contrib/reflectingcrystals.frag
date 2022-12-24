#pragma include "../common.frag"
// Watch the recording of the live coding on YouTube.
// https://youtu.be/bp37xTVNRrM?t=6125

// Added spaces, comments, etc. to the code.
uniform float speed = 0.1 ;

#define saturate(x) clamp(x, 0., 1.)

// Rotate by 45 degrees.
#define rotpi4(v) v = vec2(v.x + v.y, -v.x + v.y) / sqrt(2.)


// Rotation matrix in two dimensions.
mat2 rot(float a) {
  float s = sin(a), c = cos(a);
  return mat2(c, s, -s, c);
}

#define odd(x) step(1., mod(x, 2.))
// Square wave.
float sqWave(float x) {
  float i = floor(x);
  float s = 0.1;
  return mix(odd(i), odd(i + 1.), smoothstep(0.5 - s, 0.5 + s, fract(x)));
}

// Smooth minimum.
// reference:
// https://iquilezles.org/articles/smin
float smin(float a, float b, float k) {
  float h = max(k - abs(a - b), 0.);
  return min(a, b) - h * h * 0.25 / k;
}

vec3 pos; // Position of gyroid sphere.
// Distance function used for raymarching.
float map(vec3 p) {
  float d;
  //d = length(p) - 0.3;
  vec3 q = p;
  q.y = 0.7 - abs(q.y);
  
  d = q.y;
  q.zx = fract(q.zx) - 0.5; // Repetition in zx-plane.
  
  vec2 dq = mix(vec2(-0.2), vec2(0.27, 0.1), sqWave(time*speed*4. * 0.15)); // Movement of the branches.
  float a = 1.;
  for(int i = 0; i < 5; i++) { // IFS.
    vec3 v = q;
    v.zx = abs(v.zx);
    if(v.z > v.x) v.zx = v.xz;
    d = min(d, max(v.x - 0.1, (v.x * 2. + v.y) / sqrt(5.) - 0.3) / a);
    q.zx = abs(q.zx);
    rotpi4(q.xz);
    q.xy -= dq;
    rotpi4(q.yx);
    
    q *= 2.;
    a *= 2.;
  }
  
  // Add a gyroid sphere.
  q = p - pos;
  float t = length(q) - 0.3;
  q *= 15.;
  q.xy *= rot(time*speed*4. * 1.3);
  q.yz *= rot(time*speed*4. * 1.7);
  t = max(t, (abs(dot(sin(q), cos(q.yzx))) - 0.2) / 15.);
  d = smin(d, t, 0.3);
  
  return d;
}

// Calculate normal vector.
vec3 calcN(vec3 p) {
  vec2 e = vec2(0.001, 0);
  return normalize(vec3(map(p + e.xyy) - map(p - e.xyy),
  map(p + e.yxy) - map(p - e.yxy),
  map(p + e.yyx) - map(p - e.yyx)));
}

// Trasform HSV color to RGB.
vec3 hsv(float h, float s, float v) {
  vec3 res = fract(h + vec3(0, 2, 1) / 3.) * 6. - 3.;
  res = saturate(abs(res) - 1.);
  res = (res - 1.) * s + 1.;
  res *= v;
  return res;
}

// Raymarching.
vec3 march(inout vec3 rp, inout vec3 rd, inout vec3 ra, inout bool hit) {
  vec3 col = vec3(0);
  float d;
  float t=0.;
  hit = false;
  for(int i = 0; i < 100; i++) {
    d = map(rp + rd * t);
    if(abs(d) < 0.0001) {
      hit = true;
      break;
    }
    t += d;
  }
  rp += t * rd;
  
  vec3 ld = normalize(-rp); // Light direction.
  
  vec3 n = calcN(rp); // Normal vector.
  vec3 ref = reflect(rd, n); // Reflection vector.
 
  float diff = max(dot(ld, n), 0.); // Diffuse.
  float spec = pow(max(dot(reflect(ld, n), rd), 0.), 20.); // Specular.
  float fog = exp(-t * t * 0.2);
  
  d = length(rp - pos) - 0.3;
  float mat = smoothstep(0.01, 0.1, d); // Material parameter. 0.0: gyroid sphere  1.0:crystal
  float phase = length(rp) * 4. - time*speed*4. * 2.; // Phase of color changing.
  vec3 al = hsv(floor(phase / pi) * pi * 0.4, 0.8, 1.); // Albedo.
  al = mix(vec3(0.9), al, mat);
  float f0 = mix(0.01, 0.8, mat); // Fresnel factor when the reflection vector is parallel to the normal vector.
  float m = mix(0.01, 0.9, mat); // Metalness.
  float fs = f0 + (1. - f0) * pow(1. - dot(ref, n), 5.); // Schlick's approximation of Fresnel factor.
  float lp = 3. / abs(sin(phase)); // Light Power.
  
  col += al * diff * (1. - m) * lp; // Diffuse reflection of direct light.
  col += al * spec * m * lp; // Specular reflection of direct light.
  col = mix(vec3(0), col, fog); // Black Fog.
  
  col *= ra; // Reflection attenuation.
  
  // Update variables for next reflection.
  ra *= al * fs * fog;
  rp += 0.01 * n;
  rd = ref;
  
  return col;
}

void main()
{
  // Normalization of coordinates.
  vec2 uv = vec2(gl_FragCoord.x / iResolution.x, gl_FragCoord.y / iResolution.y);
  uv -= 0.5;
  uv /= vec2(iResolution.y / iResolution.x, 1) * 0.5;
  
  pos = sin(vec3(13, 0, 7) * time*speed*4. * 0.1); // Move the gyroid sphere.
  
  vec3 col = vec3(0); // Color.
  
  vec3 ro = vec3(0, -0.3, 1.9); // Ray origin.
  ro.zx *= rot(time*speed*4. * 0.1);
  
  vec3 rd = normalize(vec3(uv, -2)); // Ray direction.
  rd.zx *= rot(time*speed*4. * 0.1);
  
  vec3 ra = vec3(1); // Reflection attenuation.
  bool hit = false; // Whether the ray hit any object or not.
  
  col += march(ro, rd, ra, hit);
  if(hit) col += march(ro, rd, ra, hit); // 1st reflection.
  if(hit) col += march(ro, rd, ra, hit); // 2nd reflection.
  
  // Gamma correction.
  col = pow(col, vec3(1. / 2.2));
  
  fragColor = vec4(col,1.);
}