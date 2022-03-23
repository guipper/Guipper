#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// "Vines at Night" by dr2 - 2019
// License: Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
uniform float speed;
uniform float altura;
uniform float speedz;
#define AA  0

float PrCylDf (vec3 p, float r, float h);
float PrRoundBox2Df (vec2 p, vec2 b, float r);
vec2 PixToHex (vec2 p);
vec2 HexToPix (vec2 h);
vec3 HexGrid (vec2 p);
float Minv3 (vec3 p);
float Maxv3 (vec3 p);
float SmoothMin (float a, float b, float r);
float SmoothBump (float lo, float hi, float w, float x);
vec2 Rot2D (vec2 q, float a);
vec2 Hashv2v2 (vec2 p);
float Noisefv3 (vec3 p);
float Fbm2 (vec2 p);
vec3 VaryNf (vec3 p, vec3 n, float f);

#define N_GLOW 2

vec3 glPos[N_GLOW], ltPos, ltAx;
vec2 gId;
float tCur, dstFar, hgSize, bRad, bDel, vLev;
int idObj;
const float sqrt3 = 1.7320508;

#define DMIN(id) if (d < dMin) { dMin = d;  idObj = id; }

float ObjDf (vec3 p)
{
  vec3 q;
  float dMin, d, cRad, cLen, hp, br, tw, a, s, r;
  dMin = dstFar;
  p.xz -= HexToPix (gId * hgSize);
  cRad = 0.1;
  cLen = 1.;
  tw = 1.;
  d = dMin;
  vLev = 0.;
  for (int k = 0; k < 6; k ++) {
    tw = - tw;
    s = float (k + 1) / 6.;
    hp = tw * (16. - 10. * s) + bRad - 0.5;
    br = 0.015 - 0.01 * s + 0.005 * (bRad - 0.75);
    q = p;
    q.y -= (bDel + 1.3 * s) * hp;
    a = 2. * pi * q.y / hp;
    q.xz = Rot2D (q.xz, (5. - 3. * bDel) * pi * (sign (bRad - 0.5) * a +
       (0.01 * bDel + 0.3 * s) * sin (3. * a)));
    q.x -= cRad + br;
    r = length (q.xz) - br;
    if (r < d) vLev = s;
    d = SmoothMin (d, r, 0.005);
  }
  d = max (d, abs (p.y - cLen) - cLen);
  DMIN (1);
  q = p;
  q.y -= cLen;
  d = PrCylDf (q.xzy, cRad, cLen);
  DMIN (2);
  q = p;
  q.xz = Rot2D (q.xz, 2. * pi * (floor (6. * atan (q.z, - q.x) / (2. * pi) + 0.5) / 6.));
  q.y -= 2. * cLen + 0.065 - 0.1 * cos (pi * length (q.xz) / (0.5 * sqrt3 * hgSize));
  d = PrRoundBox2Df (q.yz, vec2 (0.02, 0.008), 0.01);
  DMIN (3);
  q = p;
  q.y = abs (q.y - cLen) - cLen + 0.03;
  d = PrCylDf (q.xzy, cRad + 0.05, 0.03);
  DMIN (3);
  return 0.7 * dMin;
}

void SetGrObjConf ()
{
  vec2 fRand;
  fRand = Hashv2v2 (gId * vec2 (37.3, 43.1) + 27.1);
  bRad = 0.5 + 0.5 * fRand.x;
  bDel = fRand.y;
}

float ObjRay (vec3 ro, vec3 rd)
{
  vec3 vri, vf, hv, p;
  vec2 edN[3], pM, gIdP;
  float dHit, d, s, eps;
  eps = 0.0005;
  edN[0] = vec2 (1., 0.);
  edN[1] = 0.5 * vec2 (1., sqrt3);
  edN[2] = 0.5 * vec2 (1., - sqrt3);
  for (int k = 0; k < 3; k ++) edN[k] *= sign (dot (edN[k], rd.xz));
  vri = hgSize / vec3 (dot (rd.xz, edN[0]), dot (rd.xz, edN[1]), dot (rd.xz, edN[2]));
  vf = 0.5 * sqrt3 - vec3 (dot (ro.xz, edN[0]), dot (ro.xz, edN[1]),
     dot (ro.xz, edN[2])) / hgSize;
  pM = HexToPix (PixToHex (ro.xz / hgSize));
  gIdP = vec2 (-99.);
  dHit = 0.;
  for (int j = 0; j < 160; j ++) {
    hv = (vf + vec3 (dot (pM, edN[0]), dot (pM, edN[1]), dot (pM, edN[2]))) * vri;
    s = Minv3 (hv);
    p = ro + dHit * rd;
    gId = PixToHex (p.xz / hgSize);
    if (gId.x != gIdP.x || gId.y != gIdP.y) {
      gIdP = gId;
      SetGrObjConf ();
    }
    d = ObjDf (p);
    if (dHit + d < s) {
      dHit += d;
    } else {
      dHit = s + eps;
      pM += sqrt3 * ((s == hv.x) ? edN[0] : ((s == hv.y) ? edN[1] : edN[2]));
    }
    if (d < eps || dHit > dstFar || p.y < 0. || p.y > 3.) break;
  }
  if (d >= eps) dHit = dstFar;
  return dHit;
}

vec3 ObjNf (vec3 p)
{
  vec4 v;
  vec2 e;
  e = vec2 (0.001, -0.001);
  v = vec4 (- ObjDf (p + e.xxx), ObjDf (p + e.xyy), ObjDf (p + e.yxy), ObjDf (p + e.yyx));
  return normalize (2. * v.yzw - dot (v, vec4 (1.)));
}

float ObjSShadow (vec3 ro, vec3 rd, float ltDist)
{
  vec3 p;
  vec2 gIdP;
  float sh, d, h;
  sh = 1.;
  d = 0.01;
  gIdP = vec2 (-99.);
  for (int j = 0; j < 40; j ++) {
    p = ro + d * rd;
    gId = PixToHex (p.xz / hgSize);
    if (gId.x != gIdP.x || gId.y != gIdP.y) {
      gIdP = gId;
      SetGrObjConf ();
    }
    h = ObjDf (p);
    sh = min (sh, smoothstep (0., 0.05 * d, h));
    d += clamp (h, 0.05, 0.3);
    if (sh < 0.05 || d > ltDist) break;
  }
  return 0.4 + 0.6 * sh;
}

vec3 StarPat (vec3 rd, float scl)
{
  vec3 tm, qn, u;
  vec2 q;
  float f;
  tm = -1. / max (abs (rd), 0.0001);
  qn = - sign (rd) * step (tm.zxy, tm) * step (tm.yzx, tm);
  u = Maxv3 (tm) * rd;
  q = atan (vec2 (dot (u.zxy, qn), dot (u.yzx, qn)), vec2 (1.)) / pi;
  f = 0.57 * (Fbm2 (11. * dot (0.5 * (qn + 1.), vec3 (1., 2., 4.)) + 131.13 * scl * q) +
      Fbm2 (13. * dot (0.5 * (qn + 1.), vec3 (1., 2., 4.)) + 171.13 * scl * q.yx));
  return 4. * vec3 (1., 1., 0.8) * pow (f, 16.);
}

vec3 SkyCol (vec3 rd)
{
  vec3 col, mDir, vn;
  float mRad, bs, ts;
  mDir = normalize (vec3 (0.6, 0.03, 1.));
  mRad = 0.025;
  col = vec3 (0.06, 0.06, 0.03) * pow (clamp (dot (rd, mDir), 0., 1.), 16.);
  bs = dot (rd, mDir);
  ts = bs * bs - dot (mDir, mDir) + mRad * mRad;
  if (ts > 0.) {
    ts = bs - sqrt (ts);
    if (ts > 0.) {
      vn = normalize ((ts * rd - mDir) / mRad);
      col += vec3 (1., 0.9, 0.5) * clamp (dot (vec3 (-0.77, 0.4, 0.5), vn) *
         (1. - 0.3 * Noisefv3 (8. * vn)), 0., 1.);
    }
  } else col += StarPat (rd, 6.);
  return col;
}

float GlowCol (vec3 ro, vec3 rd, float dstObj)
{
  vec3 dirGlow;
  float dstGlow, brGlow;
  brGlow = 0.;
  for (int k = 0; k < N_GLOW; k ++) {
    dirGlow = glPos[k] - ro;
    dstGlow = length (dirGlow);
    dirGlow /= dstGlow;
    if (dstGlow < dstObj) brGlow += 2. * pow (max (dot (rd, dirGlow), 0.), 1024.) / dstGlow;
  }
  return clamp (brGlow, 0., 1.);
}

vec3 ShowScene (vec3 ro, vec3 rd)
{
  vec4 col4;
  vec3 ltVec, ltDir, roo, col, vn, q, qh;
  vec2 vf;
  float dstObj, atten, a, s, r, sh;
  bool isBg;
  isBg = true;
  vf = vec2 (0.);
  roo = ro;
  dstObj = ObjRay (ro, rd);
  if (dstObj < dstFar) {
    ro += dstObj * rd;
    q = ro;
    q.xz -= HexToPix (gId * hgSize);
    vn = ObjNf (ro);
    if (idObj == 1) {
      col4 = vec4 (0.5, 0.7, 0.3, 0.3) * (0.4 + 0.6 * vLev);
      vf = vec2 (32., 2. - vLev);
    } else if (idObj == 2) {
      col4 = vec4 (0.6, 0.5, 0.4, 0.1);
      a = mod (32. * (atan (q.z, - q.x) / (2. * pi)), 1.);
      vn.xz = Rot2D (vn.xz, -0.3 * sin (pi * a * a));
      vf = vec2 (64., 0.5);
    } else if (idObj == 3) {
      col4 = vec4 (0.5, 0.4, 0.3, 0.1);
      if (vn.y > 0.99 && length (q.xz) < 0.14) col4 *= 0.6;
      vf = vec2 (32., 1.);
    }
    isBg = false;
  } else if (rd.y < 0.) {
    dstObj = - ro.y / rd.y;
    if (dstObj < dstFar) {
      ro += dstObj * rd;
      qh = HexGrid (4. * sqrt3 * ro.xz);
      r = length (qh.xy);
      s = max (r - 0.5, 0.);
      vn = vec3 (0., Rot2D (vec2 (1., 0.), 2. * s * s));
      vn.zx = vn.z * vec2 (qh.x, - qh.y) / r;
      s = smoothstep (0.03, 0.06, qh.z);
      col4 = vec4 (0.5, 0.45, 0.45, 0.1) * (0.8 + 0.2 * s);
      vf = vec2 (32., 1.5 * s);
      isBg = false;
    }
  }
  if (! isBg) {
    if (vf.x > 0.) vn = VaryNf (vf.x * ro, vn, vf.y);
    ltVec = roo - ro;
    s = length (ltVec);
    ltDir = ltVec / s;
    atten = 0.1 + 0.9 * smoothstep (0.8, 0.9, dot (ltAx, - ltDir)) / (1. + 0.05 * pow (s, 1.5));
    ltVec += ltPos;
    s = length (ltVec);
    ltDir = ltVec / s;
    sh = ObjSShadow (ro, ltDir, s);
    col = atten * col4.rgb * (0.1 + 1.1 * sh * max (dot (vn, ltDir), 0.)) +
       col4.a * step (0.95, sh) * pow (max (dot (normalize (ltDir - rd), vn), 0.), 32.);
  }
  dstObj = min (dstObj, dstFar);
  if (dstObj / dstFar > 0.6) col = mix (col, SkyCol (rd), smoothstep (0.6, 1., dstObj / dstFar));
  col = mix (col, vec3 (0.3, 0.8, 1.), GlowCol (roo, rd, dstObj));
  return clamp (col, 0., 1.);
}

vec2 TrackPath (float t)
{
  vec2 r;
  float tt;
  tt = mod (t, 4.);
  if (tt < 1.) r = mix (vec2 (sqrt3 * 0.5, -0.5), vec2 (sqrt3 * 0.5, 0.5), tt);
  else if (tt < 2.) r = mix (vec2 (sqrt3 * 0.5, 0.5), vec2 (0., 1.), tt - 1.);
  else if (tt < 3.) r = mix (vec2 (0., 1.), vec2 (0., 2.), tt - 2.);
  else r = mix (vec2 (0., 2.), vec2 (sqrt3 * 0.5, 2.5), tt - 3.);
  r += vec2 (0.001, 3. * floor (t / 4.));
  return r;
}

void main()
{
  mat3 vuMat;
  vec4 mPtr;
  vec3 ro, rd, col;
  vec2 canvas, uv, ori, ca, sa, p1, p2, vd;
  float el, az, zmFac, sr, vel, tCyc, s;
  canvas = iResolution.xy;
  uv = 2. * fragCoord.xy / canvas - 1.;
  uv.x *= canvas.x / canvas.y;
  tCur = iTime*mapr(speed,-1.0,1.0)*-1.;
  mPtr = iMouse;
  mPtr.xy = mPtr.xy / canvas ;

  mPtr.y = 1.-mPtr.y ;
  hgSize = 1.;
  vel = 0.4;
  p1 = 0.5 * (TrackPath (vel * tCur + 0.2) + TrackPath (vel * tCur + 0.4));
  p2 = 0.5 * (TrackPath (vel * tCur - 0.2) + TrackPath (vel * tCur - 0.4));
  ro.xz = 0.5 * (p1 + p2);
  ro.x += 0.2 * (2. * SmoothBump (0.25, 0.75, 0.15, mod (0.07 * vel * tCur, 1.)) - 1.);
  ro.y = altura*3.0;
  ro.z+=mapr(speedz,-2.0,2.0)*time;
  vd = p1 - p2;
  az = atan (vd.x, vd.y);
  el = 0.05 * pi * sin (0.07 * 2. * pi * tCur);
  if (mPtr.z > 0.) {
    az += 2. * pi * mPtr.x;
    el += pi * mPtr.y;
  }
  ori = vec2 (el, az);
  ca = cos (ori);
  sa = sin (ori);
  vuMat = mat3 (ca.y, 0., - sa.y, 0., 1., 0., sa.y, 0., ca.y) *
          mat3 (1., 0., 0., 0., ca.x, - sa.x, 0., sa.x, ca.x);
  zmFac = 2.5;
  dstFar = 25.;
  ltPos = 0.5 * vuMat * normalize (vec3 (cos (2. * pi * (0.022 * tCur)), 1., 0.));
  ltAx = vuMat * vec3 (0., 0., 1.);
  for (int k = 0; k < N_GLOW; k ++) {
    glPos[k] = vec3 (TrackPath (vel * tCur + 7. + 6. * float (k)),
       0.8 + 0.3 * sin (2. * pi * (0.13 * tCur + float (k) / float (N_GLOW)))).xzy;
    glPos[k].x *= 8.5;
  }
#if ! AA
  const float naa = 1.;
#else
  const float naa = 3.;
#endif
  col = vec3 (0.);
  sr = 2. * mod (dot (mod (floor (0.5 * (uv + 1.) * canvas), 2.), vec2 (1.)), 2.) - 1.;
  for (float a = 0.; a < naa; a ++) {
    rd = normalize (vec3 (uv + step (1.5, naa) * Rot2D (vec2 (0.5 / canvas.y, 0.),
       sr * (0.667 * a + 0.5) * pi), zmFac));
    rd = vuMat * rd;
    col += (1. / naa) * ShowScene (ro, rd);
  }
  fragColor = vec4 (pow (col, vec3 (0.9)), 1.);
}

float PrCylDf (vec3 p, float r, float h)
{
  return max (length (p.xy) - r, abs (p.z) - h);
}

float PrRoundBox2Df (vec2 p, vec2 b, float r)
{
  return length (max (abs (p) - b, 0.)) - r;
}

vec2 PixToHex (vec2 p)
{
  vec3 c, r, dr;
  c.xz = vec2 ((1./sqrt3) * p.x - (1./3.) * p.y, (2./3.) * p.y);
  c.y = - c.x - c.z;
  r = floor (c + 0.5);
  dr = abs (r - c);
  r -= step (dr.yzx, dr) * step (dr.zxy, dr) * dot (r, vec3 (1.));
  return r.xz;
}

vec2 HexToPix (vec2 h)
{
  return vec2 (sqrt3 * (h.x + 0.5 * h.y), (3./2.) * h.y);
}

vec3 HexGrid (vec2 p)
{
  vec2 q;
  p -= HexToPix (PixToHex (p));
  q = abs (p);
  return vec3 (p, 0.5 * sqrt3 - q.x + 0.5 * min (q.x - sqrt3 * q.y, 0.));
}

float Minv3 (vec3 p)
{
  return min (p.x, min (p.y, p.z));
}

float Maxv3 (vec3 p)
{
  return max (p.x, max (p.y, p.z));
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

vec2 Rot2D (vec2 q, float a)
{
  vec2 cs;
  cs = sin (a + vec2 (0.5 * pi, 0.));
  return vec2 (dot (q, vec2 (cs.x, - cs.y)), dot (q.yx, cs));
}

const float cHashM = 43758.54;

vec2 Hashv2v2 (vec2 p)
{
  vec2 cHashVA2 = vec2 (37., 39.);
  return fract (sin (vec2 (dot (p, cHashVA2), dot (p + vec2 (1., 0.), cHashVA2))) * cHashM);
}

vec4 Hashv4v3 (vec3 p)
{
  vec3 cHashVA3 = vec3 (37., 39., 41.);
  vec2 e = vec2 (1., 0.);
  return fract (sin (vec4 (dot (p + e.yyy, cHashVA3), dot (p + e.xyy, cHashVA3),
     dot (p + e.yxy, cHashVA3), dot (p + e.xxy, cHashVA3))) * cHashM);
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

float Noisefv3 (vec3 p)
{
  vec4 t;
  vec3 ip, fp;
  ip = floor (p);
  fp = fract (p);
  fp *= fp * (3. - 2. * fp);
  t = mix (Hashv4v3 (ip), Hashv4v3 (ip + vec3 (0., 0., 1.)), fp.z);
  return mix (mix (t.x, t.y, fp.x), mix (t.z, t.w, fp.x), fp.y);
}

float Fbm2 (vec2 p)
{
  float f, a;
  f = 0.;
  a = 1.;
  for (int j = 0; j < 5; j ++) {
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
  for (int j = 0; j < 5; j ++) {
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
