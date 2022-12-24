#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// "Voronoi Vegetation" by dr2 - 2017
// License: Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License

float PrSphDf (vec3 p, float s);
float PrCylDf (vec3 p, float r, float h);
vec2 PixToHex (vec2 p);
vec2 HexToPix (vec2 h);
vec2 Rot2D (vec2 q, float a);
float SmoothMin (float a, float b, float r);
float SmoothBump (float lo, float hi, float w, float x);
float Hashfv2 (vec2 p);
vec2 Hashv2v2 (vec2 p);
float Fbm2 (vec2 p);
vec3 VaryNf (vec3 p, vec3 n, float f);
vec3 HsvToRgb (vec3 c);

vec3 sunDir, qHit;
vec2 gVec[7], hVec[7], uVec[7];
float tCur, dstFar;
int idObj;

void HexVorInit ()
{
  vec3 e = vec3 (1., 0., -1.);
  gVec[0] = e.yy;
  gVec[1] = e.xy;
  gVec[2] = e.yx;
  gVec[3] = e.xz;
  gVec[4] = e.zy;
  gVec[5] = e.yz;
  gVec[6] = e.zx;
  for (int k = 0; k < 7; k ++) hVec[k] = HexToPix (gVec[k]);
}

void HexVorVec (vec3 p)
{
  vec2 ip;
  ip = PixToHex (p.xz);
  for (int k = 0; k < 7; k ++) uVec[k] = Hashv2v2 (ip + gVec[k]);
}

vec4 TwHexVor (vec3 p, float s)
{
  vec4 sd, udm;
  vec2 ip, fp, d, u;
  float amp, a, twf, twa, da;
  amp = 0.7;
  da = 0.5 + 0.5 * smoothstep (1., 3., p.y);
  ip = PixToHex (p.xz);
  fp = p.xz - HexToPix (ip);
  sd = vec4 (4.);
  udm = vec4 (4.);
  for (int k = 0; k < 7; k ++) {
    u = uVec[k];
    a = 2. * pi * (u.y - 0.5);
    twf = 4. * mod (32. * u.x, 1.) - 2.;
    twa = (3. - 0.25 * abs (twf)) * da;
    a += (1. - s) * pi +  twa * sin (a + s * twf * p.y);
    d = hVec[k] + amp * (0.4 + 0.6 * u.x) * vec2 (cos (a), sin (a)) - fp;
    sd.w = dot (d, d);
    if (sd.w < sd.x) {
      sd = sd.wxyw;
      udm = vec4 (d, u);
    } else sd = (sd.w < sd.y) ? sd.xwyw : ((sd.w < sd.z) ? sd.xyww : sd);
  }
  sd.xyz = sqrt (sd.xyz);
  return vec4 (SmoothMin (sd.y, sd.z, 0.3) - sd.x, udm.xy, Hashfv2 (udm.zw));
}

float ObjDf (vec3 p)
{
  vec4 vc;
  vec3 q;
  float dMin, d, h, r;
  dMin = dstFar;
  HexVorVec (p);
  vc = TwHexVor (p, 1.);
  h = 2. + vc.w;
  q.xz = vc.yz;
  q.y = p.y - h;
  r = 0.005 + 0.05 * (2. - p.y / h);
  r = mix (r, 0.25 * vc.x, 1. - smoothstep (0.1 * h, 0.4 * h, p.y));
  r *= 1. + 0.1 * sin (4. * pi * p.y);
  r += 0.02 * (1. - smoothstep (0., 0.05, p.y));
  q.xz = abs (q.xz) - 0.3 * r;
  d = 0.35 * PrCylDf (q.xzy, r, h);
  if (d < dMin) { dMin = d;  idObj = 1;  qHit = q; }
  vc = TwHexVor (p, 0.);
  q.xz = vc.yz;
  q.y = p.y;
  r = 0.1 + 0.05 * vc.w;
  q.xz = abs (Rot2D (q.xz, 0.5 * pi * (1. - vc.w))) - (1. + 0.5 * vc.w) * r * vec2 (1.2, 0.8);
  q.y -= 0.75 * r;
  d = PrCylDf (q.xzy, 0.2 * r, 0.75 * r);
  if (d < dMin) { dMin = d;  idObj = 2;  qHit = q; }
  q.y -= 0.75 * r;
  d = PrSphDf (q, r);
  if (d < dMin) { dMin = d;  idObj = 3;  qHit = q; }
  return dMin;
}

float ObjRay (vec3 ro, vec3 rd)
{
  float dHit, d;
  dHit = 0.;
  for (int j = 0; j < 250; j ++) {
    d = ObjDf (ro + dHit * rd);
    dHit += d;
    if (d < 0.0005 || dHit > dstFar) break;
  }
   return dHit;
}

vec3 ObjNf (vec3 p)
{
  vec4 v;
  const vec3 e = vec3 (0.001, -0.001, 0.);
  v = vec4 (ObjDf (p + e.xxx), ObjDf (p + e.xyy), ObjDf (p + e.yxy), ObjDf (p + e.yyx));
  return normalize (vec3 (v.x - v.y - v.z - v.w) + 2. * v.yzw);
}

float ObjSShadow (vec3 ro, vec3 rd)
{
  float sh, d, h;
  sh = 1.;
  d = 0.05;
  for (int j = 0; j < 32; j ++) {
    h = ObjDf (ro + rd * d);
    sh = min (sh, smoothstep (0., 0.05 * d, h));
    d += clamp (h, 0.05, 0.5);
    if (sh < 0.05) break;
  }
  return sh;
}

vec3 SkyCol (vec3 ro, vec3 rd)
{
  vec3 col;
  float f;
  ro.x += 0.5 * tCur;
  f = Fbm2 (0.1 * (rd.xz * (50. - ro.y) / rd.y + ro.xz));
  col = vec3 (0.2, 0.3, 0.55) + 0.1 * pow (1. - max (rd.y, 0.), 4.) +
     0.35 * pow (max (dot (rd, sunDir), 0.), 8.);
  return mix (col, vec3 (0.85), clamp (f * rd.y + 0.1, 0., 1.));
}

vec3 ShowScene (vec3 ro, vec3 rd)
{
  vec4 vc;
  vec3 col, vn;
  float dstObj, sh, a, spec;
  bool isBg;
  HexVorInit ();
  dstObj = ObjRay (ro, rd);
  isBg = true;
  if (dstObj < dstFar) {
    isBg = false;
    ro += dstObj * rd;
    vn = ObjNf (ro);
    a = atan (qHit.z, - qHit.x) / pi;
    if (idObj == 1) {
      vn = VaryNf (32. * ro, vn, 5. - 4.5 * smoothstep (0.5, 1., ro.y));
      HexVorVec (ro);
      vc = TwHexVor (ro, 1.);
      col = HsvToRgb (vec3 (0.15 + 0.3 * mod (37. * vc.w, 1.), 1.,
         0.8 + 0.2 * SmoothBump (0.1, 0.9, 0.05, mod (6. * a, 1.))));
      col = mix (col, vec3 (0.4, 0.2, 0.1), 1. - 0.9 * smoothstep (0., 1., ro.y)) *
        (1. - 0.3 * Fbm2 (64. * vec2 (a, 4. * qHit.y))) * (0.5 + 0.5 * smoothstep (0., 0.5, ro.y));
      spec = 0.3 * smoothstep (2., 4., ro.y);
    } else {
      vn = VaryNf (32. * ro, vn, 3.);
      HexVorVec (ro);
      vc = TwHexVor (ro, 0.);
      if (idObj == 2) col = vec3 (0., 0.4, 0.);
      else if (idObj == 3) col = HsvToRgb (vec3 (0.1 * mod (37. * vc.w, 1.), 1.,
         0.8 + 0.2 * SmoothBump (0.2, 0.8, 0.1, mod (9. * a, 1.))));
      spec = 0.2;
    }
  } else if (rd.y < 0.) {
    isBg = false;
    dstObj = (- ro.y / rd.y);
    ro += dstObj * rd;
    vn = VaryNf (16. * ro, vec3 (0., 1., 0.), 10.);
    col = mix (vec3 (0.2, 0.05, 0.), vec3 (0.05, 0.2, 0.), Fbm2 (16. * ro.xz));
    spec = 0.;
  }
  if (! isBg) {
    sh = 0.3 + 0.7 * ObjSShadow (ro, sunDir);
    col = col * (0.2 + 0.8 * sh * max (0., max (dot (vn, sunDir), 0.))) +
       0.1 * max (dot (- normalize (sunDir.xz), normalize (vn.xz)), 0.) +
       spec * sh * pow (max (dot (normalize (sunDir - rd), vn), 0.), 64.);
    col = mix (col, SkyCol (ro, reflect (rd, vec3 (0., 1., 0.))), smoothstep (0.7, 1., dstObj / dstFar));
  } else col = SkyCol (ro, rd);
  return pow (clamp (col, 0., 1.), vec3 (0.8));
}

void main()
{
  mat3 vuMat;
  vec4 mPtr;
  vec3 ro, rd;
  vec2 canvas, uv, ori, ca, sa;
  float az, el;
  canvas = iResolution.xy;
  uv = 2. * gl_FragCoord.xy.xy / canvas - 1.;
  uv.x *= canvas.x / canvas.y;
  uv.y = -uv.y;
  tCur = iTime;
  mPtr = vec4(1.0,0.0,0.0,0.0);
  mPtr.xy = mPtr.xy / canvas - 0.5;
  az = 0.;
  el = -0.05 * pi;
  if (mPtr.z > 0.) {
    az += 1.5 * pi * mPtr.x;
    el += 0.5 * pi * mPtr.y;
  } else {
    az += 0.3 * pi * sin (0.05 * pi * tCur);
  }
  el = clamp (el, -0.4 * pi, 0.1 * pi);
  ori = vec2 (el, az);
  ca = cos (ori);
  sa = sin (ori);
  vuMat = mat3 (ca.y, 0., - sa.y, 0., 1., 0., sa.y, 0., ca.y) *
          mat3 (1., 0., 0., 0., ca.x, - sa.x, 0., sa.x, ca.x);
  rd = normalize (vec3 (uv, 2.5));
  rd = vuMat * rd;
  ro = vec3 (0., 1.5, 1.5 * tCur);
  sunDir = normalize (vec3 (0.5, 1., -1.));
  dstFar = 120.;
  fragColor = vec4 (ShowScene (ro, rd), 1.);
}

float PrSphDf (vec3 p, float s)
{
  return length (p) - s;
}

float PrCylDf (vec3 p, float r, float h)
{
  return max (length (p.xy) - r, abs (p.z) - h);
}

#define SQRT3 1.7320508

vec2 PixToHex (vec2 p)
{
  vec3 c, r, dr;
  c.xz = vec2 ((1./SQRT3) * p.x - (1./3.) * p.y, (2./3.) * p.y);
  c.y = - c.x - c.z;
  r = floor (c + 0.5);
  dr = abs (r - c);
  r -= step (dr.yzx, dr) * step (dr.zxy, dr) * dot (r, vec3 (1.));
  return r.xz;
}

vec2 HexToPix (vec2 h)
{
  return vec2 (SQRT3 * (h.x + 0.5 * h.y), (3./2.) * h.y);
}

vec2 Rot2D (vec2 q, float a)
{
  return q * cos (a) + q.yx * sin (a) * vec2 (-1., 1.);
}

float SmoothMin (float a, float b, float r)
{
  float h;
  h = clamp (0.5 + 0.5 * (b - a) / r, 0., 1.);
  return mix (b, a, h) - r * h * (1. - h);
}

float SmoothBump (float lo, float hi, float w, float x)
{
  return (1. - smoothstep (hi - w, hi + w, x)) * smoothstep (lo - w, lo + w, x);
}

const float cHashM = 43758.54;

float Hashfv2 (vec2 p)
{
  return fract (sin (dot (p, vec2 (37., 39.))) * cHashM);
}

vec2 Hashv2v2 (vec2 p)
{
  const vec2 cHashVA2 = vec2 (37.1, 61.7);
  const vec2 e = vec2 (1., 0.);
  return fract (sin (vec2 (dot (p + e.yy, cHashVA2), dot (p + e.xy, cHashVA2))) * cHashM);
}

float Noisefv2 (vec2 p)
{
  vec2 t, ip, fp;
  ip = floor (p);  
  fp = fract (p);
  fp = fp * fp * (3. - 2. * fp);
  t = mix (Hashv2v2 (ip), Hashv2v2 (ip + vec2 (0., 1.)), fp.y);
  return mix (t.x, t.y, fp.x);
}

float Fbm2 (vec2 p)
{
  float f, a;
  f = 0.;
  a = 1.;
  for (int i = 0; i < 5; i ++) {
    f += a * Noisefv2 (p);
    a *= 0.5;
    p *= 2.;
  }
  return f * (1. / 1.9375);
}

float Fbmn (vec3 p, vec3 n)
{
  vec3 s;
  float a;
  s = vec3 (0.);  
  a = 1.;
  for (int i = 0; i < 5; i ++) {
    s += a * vec3 (Noisefv2 (p.yz), Noisefv2 (p.zx), Noisefv2 (p.xy));
    a *= 0.5;  
    p *= 2.;
  }
  return dot (s, abs (n));
}

vec3 VaryNf (vec3 p, vec3 n, float f)
{
  vec3 g;
  vec2 e = vec2 (0.1, 0.);
  g = vec3 (Fbmn (p + e.xyy, n), Fbmn (p + e.yxy, n), Fbmn (p + e.yyx, n)) - Fbmn (p, n);
  return normalize (n + f * (g - n * dot (n, g)));
}

vec3 HsvToRgb (vec3 c)
{
  vec3 p;
  p = abs (fract (c.xxx + vec3 (1., 2./3., 1./3.)) * 6. - 3.);
  return c.z * mix (vec3 (1.), clamp (p - 1., 0., 1.), c.y);
}
