#pragma include "../common.frag"
// Watch the recording of the live coding on YouTube.
// https://youtu.be/bp37xTVNRrM?t=16035

// Added spaces, comments, etc. to the code.
uniform float speed ; 
#define hash(x) fract(sin(x) * 1763.2632)
#define saturate(x) clamp(x, 0., 1.)
//const float pi = acos(-1.);
const float pi2 = acos(-1.) * 2.;
const float N = 50.; // Number of butterflies.

// Rotation matrix in two dimensions.
mat2 rot(float a) {
  float s = sin(a), c = cos(a);
  return mat2(c, s, -s, c);
}

float rt = 1e5; // Length of ray to butterfly's wings (result).
vec3 rn; // Normal vector (result).
float rid; // Butterfly ID (result).
vec2 ruv; // UV coordinates of wings (result).
// Perform raycasting (intersection detection) to butterfly's wings (planes).
void intersect(vec3 ro, vec3 rd, vec3 ce, mat2 M, float id, float s) {
  // Right wing rotates in the opposite direction of the left wing, so we change the matrix.
  // Note that sin(-x) equals -sin(x), and cos(-x) equals cos(x).
  M[0][1] *= s;
  M[1][0] *= s;
  
  vec3 n = vec3(vec2(0, 1) * M, 0); // Normal vector.
  float t = dot(ce - ro, n) / dot(rd, n); // Length of ray to plane (wing).
  if(t < 0. || t > rt) return; // The wing is behind the camera, or farther than wings so far.
  vec3 q = ro + t * rd - ce; // Local coordinates of ray tip.
  q.yx *= M; // Rotate the coordinates backwards to get UV coordinates.
  if(q.x * s < 0.) return; // Remove unwanted wings.
  
  // Erase all parts of the plane except the butterfly's wings.
  vec2 p = q.xz;
  p.x = abs(p.x) * 0.8;
  if(p.x > sin(p.y * 50.) * 0.025 + 0.4 + p.y * 0.4) return;
  if(p.y < 0.) p *= 1.5;
  p.y = abs(p.y);
  if(length(p) > sin(atan(p.y, p.x) * 2.) + smoothstep(0.1, 0., p.y) * 0.3) return;
  
  // For the back side of the wing, reverse the normal vector.
  if(dot(rd, n) > 0.) n *= -1.;
  
  // Update results because the wing is closer to camera than wings so far.
  // Note that there are some "return;" processes above here.
  rt = t;
  rn = n;
  rid = id;
  ruv = q.xz;
}

// HSV to RGB function.
vec3 hsv(float h, float s, float v) {
  vec3 res = fract(h + vec3(0, 2, 1) / 3.) * 6. - 3.;
  res = saturate(abs(res) - 1.);
  res = (res - 1.) * s + 1.;
  res *= v;
  return res;
}

// 3D value noise function.
// reference:
// https://www.shadertoy.com/view/4ttGDH
float n3d(vec3 p) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  vec3 b = vec3(13, 193, 9);
  vec4 h = vec4(0, b.yz, b.y + b.z) + dot(i, b);
  f = f * f * (3. - 2. * f);
  h = mix(hash(h), hash(h + b.x), f.x);
  h.xy = mix(h.xz, h.yw, f.y);
  return mix(h.x, h.y, f.z);
}

// fBm noise function.
float fbm(vec3 p) {
  float ac = 0., a = 1.;
  for(int i = 0; i < 5; i++) {
    ac += n3d(p * a) / a;
    a *= 2.;
  }
  return ac - 0.5;
}

// Cloud density.
// range [0.0, 1.0]
float density(vec3 p) {
  return saturate(fbm(p * 0.5) - p.y * 0.03 - 0.7);
}

// Square wave for camera movement.
#define odd(x) step(1., mod(x, 2.))

void main()
{
  // Normalization of coordinates.
  vec2 uv = vec2(gl_FragCoord.x / iResolution.x, gl_FragCoord.y / iResolution.y);
  uv.y = 1.-uv.y;
  uv -= 0.5;
  uv /= vec2(iResolution.y / iResolution.x, 1) * 0.5;
  
  vec3 col = vec3(0); // Color.
  
  float cam = odd(time*speed*2. * 0.2); // Variable for camera switching.
  float L = 4. + odd(time*speed*2. * 0.4 - 1.) * 4.; // Camera distance from z-axis.
  vec3 ro = vec3(0, 0, time*speed*2.); // Ray origin (Camera position).
  ro.xy = mix(vec2(0, L), vec2(L * 0.5, 0), cam); // Switch ray origin.
  vec3 rd = normalize(vec3(uv, -2)); // Ray direction.
  rd = mix(vec3(-rd.x, rd.z, rd.y), vec3(rd.z, rd.y, -rd.x), cam); // Switch ray direction.
  //rd = vec3(-rd.x, rd.z, rd.y);
  
  // Prepare to draw butterflies (Raycasting to 2N wings (planes)).
  // (Find the butterfly ID, etc. of the wing closest to the camera.)
  for(float i = 0.; i < N; i++) {
    float T = i / N + time*speed*2. * 0.1;
    float id = i / N + floor(T); // Butterfly ID.
    vec3 ce = vec3(0, 0, ro.z + fract(T) * 14. - 7.); // Coordinates of butterfly center.
    mat2 M = rot(sin(T * 50. + hash(id) * pi2)); // Rotation matrix.
    
    // Randomly move butterflies.
    ce.xy += hash(vec2(1.1, 1.2) * id) * 6. - 3.;
    ce.xy += sin(vec2(5, 7) * time*speed*2. * 0.2 + hash(id * 1.3) * pi2);
    
    intersect(ro, rd, ce, M, id, 1.); // Left wing.
    intersect(ro, rd, ce, M, id, -1.); // Right wing.
  }
  
  /* vec2 p = uv;
  p.x = abs(p.x) * 0.8;
  if(p.x > sin(p.y * 50.) * 0.025 + 0.4 + p.y * 0.4) return;
  if(p.y < 0.) p *= 1.5;
  p.y = abs(p.y);
  if(length(p) > sin(atan(p.y, p.x) * 2.) + smoothstep(0.1, 0., p.y) * 0.3) return; */
  
  vec3 ld = normalize(vec3(-5, 2, -2)); // Light direction.
  
  if(rt < 100.) { // Ray hit a butterfly.
    float h = hash(rid);
    ruv.x = abs(ruv.x); // Make the pattern symmetrical.
    float w = fbm(vec3(ruv, hash(rid * 1.2) * 500.)); // Variable for domain warping.
    h += fbm(vec3(ruv + w * 5., hash(rid * 1.1) * 500.)) * 0.3; // Add some domain warping.
    col += hsv(h, 0.8, fract(h * 5. + hash(rid * 1.3))); // Pattern of wings.
    col *= smoothstep(-1., -0.93, sin(atan(ruv.y, ruv.x) * 40.)); // Black lines.
    rn.x += fbm(vec3(ruv * 10., hash(rid * 1.3) * 500.)); // Add some noise to normal vector.
    rn = normalize(rn);
    float diff = max(dot(ld, rn), 0.); // Diffuse.
    float spec = pow(max(dot(reflect(ld, rn), rd), 0.), 20.); // Specular.
    float m = 0.6; // Metalness.
    float lp = 5.; // Light power.
    
    col *= diff * (1. - m) * lp + spec * m * lp + 0.3;
  } else { // Ray didn't hit a butterfly.
    col += vec3(0.5, 0.6, 0.9) * 0.1; // Sky.
    col = mix(col, vec3(1), pow(max(dot(ld, rd), 0.), 100.) * 2.); // Sun.
  }
  
  // Draw Cloud (Volume raymarching).
  // Note that we need to draw only the clouds that exist in front of the butterflies.
  vec3 rp = ro; // Ray position.
  float tra = 1.; // Transparency.
  float rs = 1.; // Ray step.
  float t = 0.; // Ray length.
  float den; // Cloud density.
  float ac = 0.; // Accumulation of cloud color.
  for(int i = 0; i < 20; i++) {
    if(t > rt) break; // Stop when the ray reaches a butterfly's wing.
    den = density(rp + t * rd);
    ac += tra * den;
    tra *= 1. - den;
    if(tra < 0.001) break;
    t += rs;
  }
  col += ac;
  
  // Gamma correction.
  col = pow(col, vec3(1. / 2.2));
  
  fragColor = vec4(col, 1);
}