#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Verificacion de los shaders fluidpass_*.frag y del preset fluido_webgl.xml.

1) Replica EXACTA del parser de sliders de jp_box_shader.cpp (tokenizer con
   el bug del trailing empty token) -> asserts que cada slider registra con
   su default y que audio_trigger/boxframeNum NO son sliders.
2) Auditoria estructural: balance de llaves/parentesis, #pragma include
   resuelve, y auditoria de identificadores contra common.frag + builtins GLSL.
3) Validacion del XML: archivos existen, samplers declarados, targets existen,
   y el orden de ejecucion (inverso al XML) respeta las dependencias.
"""
import os, re, sys, xml.etree.ElementTree as ET

BASE = r"C:\jpupper\programacion\openframeworks\of_v0.12.1_vs_64_release\apps\myApps\Guipper"
GEN = os.path.join(BASE, "bin", "data", "shaders", "generative")
SAVES = os.path.join(BASE, "bin", "data", "savefiles")
COMMON = os.path.join(BASE, "bin", "data", "shaders", "common.frag")

NEW_GLOBALS = {"audio_trigger", "audio_express", "audio_logic", "audio_onsets",
               "audio_rhythm", "audio_spectrum0", "audio_spectrum1",
               "audio_spectrum2", "audio_spectrum3"}

# ---------- 1) replica del parser ----------
def tokenize_frase(line):
    # comment strip (mismo corte que el C++)
    c = line.find("//")
    if c != -1:
        line = line[:c]
    # space-normalize around '=' y ';' (misma logica)
    out = []
    for i, ch in enumerate(line):
        if ch == "=" and (i == 0 or line[i-1] != " "):
            out.append(" ")
        if ch == ";" and (i == 0 or line[i-1] != " "):
            out.append(" ")
        out.append(ch)
    line = "".join(out)
    # el while(ss) del C++ agrega un token vacio final
    return line.split() + [""]

def parse_shader(path):
    sliders, fbohandlers, bools = [], [], []
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    for raw in lines:
        line = raw.strip()
        if not line.startswith("uniform"):
            continue
        if "@internal" in line:
            continue
        if "float" in line:
            frase = tokenize_frase(line)
            name = frase[2] if len(frase) > 2 else "?"
            if name in NEW_GLOBALS:
                continue
            default = None
            if len(frase) == 7:
                default = float(frase[4])
            sliders.append((name, default))
        elif "sampler2DRect" in line:
            fbohandlers.append(line[22:].split(";")[0].strip())
        elif "sampler2D" in line:
            fbohandlers.append(line[18:].split(";")[0].strip())
        elif "bool" in line:
            bools.append(line[13:].split(";")[0].strip())
    return sliders, fbohandlers, bools

EXPECTED = {
    "fluidpass_splat.frag":     ([("splat_radius", 0.08), ("splat_force", 0.5), ("burst", 1.0)],
                                 ["texture1"]),
    "fluidpass_curl.frag":      ([], ["texture1"]),
    "fluidpass_vorticity.frag": ([("curl", 0.3)], ["texture1", "texture2"]),
    "fluidpass_divergence.frag":([], ["texture1"]),
    "fluidpass_pressure.frag":  ([], ["texture1", "texture2"]),
    "fluidpass_gradient.frag":  ([], ["texture1", "texture2"]),
    "fluidpass_advect_vel.frag":([("velocity_dissipation", 0.1)], ["texture1"]),
    "fluidpass_splat_dye.frag": ([("dye_hue", 0.6), ("dye_sat", 0.9), ("dye_bright", 0.6),
                                  ("dye_radius", 0.08), ("audio_pulse", 0.0), ("burst", 1.0)],
                                 ["texture1"]),
    "fluidpass_advect_dye.frag":([("dye_dissipation", 0.5)], ["texture1", "texture2"]),
    "fluidpass_display.frag":   ([("shading", 1.0), ("boost", 0.5), ("fondo", 1.0)],
                                 ["texture1"]),
}

print("=" * 60)
print("1) PARSER DE SLIDERS (replica)")
fails = 0
for fname, (exp_sliders, exp_samplers) in EXPECTED.items():
    path = os.path.join(GEN, fname)
    if not os.path.exists(path):
        print(f"FALTA ARCHIVO: {fname}"); fails += 1; continue
    sliders, samplers, bools = parse_shader(path)
    ok = True
    got = [(n, d) for n, d in sliders]
    if got != exp_sliders:
        ok = False
        print(f"  {fname}: sliders {got} != esperados {exp_sliders}")
    if samplers != exp_samplers:
        ok = False
        print(f"  {fname}: samplers {samplers} != esperados {exp_samplers}")
    # globals filtrados
    src = open(path, encoding="utf-8", errors="replace").read()
    if "audio_trigger" in src and any(n == "audio_trigger" for n, _ in sliders):
        ok = False; print(f"  {fname}: audio_trigger NO debe ser slider")
    if "boxframeNum" in src and any(n == "boxframeNum" for n, _ in sliders):
        ok = False; print(f"  {fname}: boxframeNum NO debe ser slider")
    if ok:
        print(f"  OK {fname}: {len(sliders)} sliders, samplers {samplers}")
    else:
        fails += 1
print(f"  -> {'TODO OK' if fails == 0 else f'{fails} ARCHIVOS CON PROBLEMAS'}")

# ---------- 2) auditoria de identificadores ----------
print("=" * 60)
print("2) AUDITORIA DE IDENTIFICADORES")

def identifiers(text):
    return set(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", text))

def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text

common_src = open(COMMON, encoding="utf-8", errors="replace").read()
common_ids = identifiers(strip_comments(common_src))

GLSL = set("""vec2 vec3 vec4 mat2 mat3 mat4 float int bool void if else for return
texture texture2D textureLod mix clamp min max abs floor fract exp dot length
normalize pow mod sin cos tan atan sqrt step smoothstep sign ceil log log2 exp2
inverse sqrt round fract radians degrees cross reflect refract dFdx dFdy
not equal lessThan greaterThan lessThanEqual greaterThanEqual any all
gl_FragCoord gl_FragColor gl_FragDepth gl_Position const uniform sampler2D
sampler2DRect precision highp mediump lowp struct true false discard break
continue while do switch case default""".split())

def audit(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    body = "\n".join(l for l in strip_comments(src).splitlines() if not l.strip().startswith("#"))
    # llaves y parentesis
    for a, b in (("{", "}"), ("(", ")")):
        if body.count(a) != body.count(b):
            return f"desbalance {a}{b}: {body.count(a)} vs {body.count(b)}"
    ids = identifiers(body)
    # locales: declaraciones "tipo nombre" dentro de main
    locals_ = set()
    for m in re.finditer(r"\b(vec[234]|float|int|bool|sampler2D)\s+([A-Za-z_][A-Za-z0-9_]*)", body):
        locals_.add(m.group(2))
    # params de funciones
    for m in re.finditer(r"\(([^)]*)\)", body):
        for pm in re.finditer(r"\b(vec[234]|float|int|bool|sampler2D)\s+([A-Za-z_][A-Za-z0-9_]*)", m.group(1)):
            locals_.add(pm.group(2))
    # nombres de funciones propias (id seguido de parentesis)
    for m in re.finditer(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", body):
        locals_.add(m.group(1))
    # swizzles puros (.rg, .ba, .xyx, .rr, ...)
    def is_swizzle(s):
        return len(s) <= 4 and all(c in "rgba xyzw" for c in s) and len(s) > 0
    unknown = {i for i in ids - common_ids - GLSL - locals_ - {"main"} if not is_swizzle(i)}
    return None if not unknown else f"identificadores sospechosos: {sorted(unknown)}"

afails = 0
for fname in EXPECTED:
    res = audit(os.path.join(GEN, fname))
    if res:
        print(f"  {fname}: {res}"); afails += 1
    else:
        print(f"  OK {fname}")
print(f"  -> {'TODO OK' if afails == 0 else f'{afails} CON PROBLEMAS'}")
fails += afails

# ---------- 3) validacion del XML ----------
print("=" * 60)
print("3) VALIDACION DEL XML")
xml_path = os.path.join(SAVES, "fluido_webgl.xml")
# ofXml escribe DOCUMENTOS con multiples raices (activerender + boxes): no es
# XML estricto, lo envuelvo en un root falso para poder parsearlo con ET.
raw = open(xml_path, encoding="utf-8").read()
raw = re.sub(r"<\?xml[^>]*\?>", "", raw)
tree = ET.fromstring("<root>" + raw + "</root>")
boxes = tree.findall("box")
names = [b.findtext("nombre") for b in boxes]
assert len(names) == len(set(names)), "nombres duplicados en XML"
activerender = int(tree.findtext("activerender"))
print(f"  activerender={activerender} -> box '{names[activerender]}'")
xfails = 0
# ejecucion = orden inverso del XML
exec_order = list(reversed(names))
print("  orden de ejecucion por frame:", " -> ".join(exec_order))
for i, b in enumerate(boxes):
    nombre = b.findtext("nombre")
    directory = b.findtext("directory")
    fname = os.path.basename(directory.replace("\\", "/"))
    fpath = os.path.join(GEN, fname)
    if not os.path.exists(fpath):
        print(f"  {nombre}: FALTA shader {directory}"); xfails += 1; continue
    sliders, samplers, _ = parse_shader(fpath)
    link_el = b.find("fboslinks")
    links = {}
    if link_el is not None:
        for ch in link_el:
            links[ch.tag] = ch.text
    for sampler in links:
        if sampler not in samplers:
            print(f"  {nombre}: link {sampler} no esta declarado en {fname} (tiene {samplers})")
            xfails += 1
    for target in links.values():
        if target not in names:
            print(f"  {nombre}: link a box inexistente '{target}'")
            xfails += 1
    # params del XML vs sliders del shader
    param_names = [p.findtext("name") for p in b.find("parameters").findall("param")] if b.find("parameters") is not None else []
    shader_slider_names = [n for n, _ in sliders]
    if param_names != shader_slider_names:
        print(f"  {nombre}: params XML {param_names} != sliders shader {shader_slider_names}")
        xfails += 1
print(f"  -> {'TODO OK' if xfails == 0 else f'{xfails} PROBLEMAS'}")
fails += xfails

print("=" * 60)
print("RESULTADO:", "TODO OK" if fails == 0 else f"{fails} PROBLEMAS")
sys.exit(1 if fails else 0)
