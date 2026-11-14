#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Genera bin/data/savefiles/fluido_webgl.xml - preset de Navier-Stokes multi-box.

14 boxes, cada uno con un shader fluidpass_*.frag, cableados por <fboslinks>
(sampler textureN <-> fbo de otro box). El orden del XML es el INVERSO al de
ejecucion: Guipper actualiza los boxes de atras hacia adelante (JPboxgroup.cpp
update(): for i = boxes.size()-1..0), asi que el primer paso del frame
(fluidpass_splat) va ULTIMO en el XML y el display va PRIMERO (y queda
encima en el z-order, ya que el draw tambien es en orden inverso).

Ejecucion por frame: splat -> curl -> vorticity -> divergence -> pressure_1..5
-> gradient -> advect_vel -> splat_dye -> advect_dye -> display.
"""
import os

OUT = os.path.join(os.path.dirname(__file__), "fluido_webgl.xml")
OUT = os.path.normpath(OUT)

# (nombre, archivo.frag, x, y, params[(name,value)...], fboslinks[(sampler,box)...], tooutput)
BOXES = [
    ("fluidpass_display",    "fluidpass_display.frag",    40, 20,
     [("shading", 1), ("boost", 0.5), ("fondo", 1)],
     [("texture1", "fluidpass_advect_dye")], 1),
    ("fluidpass_advect_dye", "fluidpass_advect_dye.frag", 40, 130,
     [("dye_dissipation", 0.5)],
     [("texture1", "fluidpass_splat_dye"), ("texture2", "fluidpass_advect_vel")], 0),
    ("fluidpass_splat_dye",  "fluidpass_splat_dye.frag",  40, 240,
     [("dye_hue", 0.6), ("dye_sat", 0.9), ("dye_bright", 0.6), ("dye_radius", 0.08),
      ("audio_pulse", 0), ("burst", 1)],
     [("texture1", "fluidpass_advect_dye")], 0),
    ("fluidpass_advect_vel", "fluidpass_advect_vel.frag", 40, 350,
     [("velocity_dissipation", 0.1)],
     [("texture1", "fluidpass_gradient")], 0),
    ("fluidpass_gradient",   "fluidpass_gradient.frag",   40, 460,
     [],
     [("texture1", "fluidpass_vorticity"), ("texture2", "fluidpass_pressure_5")], 0),
    ("fluidpass_pressure_5", "fluidpass_pressure.frag",   40, 570,
     [],
     [("texture1", "fluidpass_pressure_4"), ("texture2", "fluidpass_divergence")], 0),
    ("fluidpass_pressure_4", "fluidpass_pressure.frag",   40, 680,
     [],
     [("texture1", "fluidpass_pressure_3"), ("texture2", "fluidpass_divergence")], 0),
    ("fluidpass_pressure_3", "fluidpass_pressure.frag",  170, 20,
     [],
     [("texture1", "fluidpass_pressure_2"), ("texture2", "fluidpass_divergence")], 0),
    ("fluidpass_pressure_2", "fluidpass_pressure.frag",  170, 130,
     [],
     [("texture1", "fluidpass_pressure_1"), ("texture2", "fluidpass_divergence")], 0),
    ("fluidpass_pressure_1", "fluidpass_pressure.frag",  170, 240,
     [],
     [("texture1", "fluidpass_pressure_5"), ("texture2", "fluidpass_divergence")], 0),
    ("fluidpass_divergence", "fluidpass_divergence.frag", 170, 350,
     [],
     [("texture1", "fluidpass_vorticity")], 0),
    ("fluidpass_vorticity",  "fluidpass_vorticity.frag",  170, 460,
     [("curl", 0.3)],
     [("texture1", "fluidpass_splat"), ("texture2", "fluidpass_curl")], 0),
    ("fluidpass_curl",       "fluidpass_curl.frag",       170, 570,
     [],
     [("texture1", "fluidpass_splat")], 0),
    ("fluidpass_splat",      "fluidpass_splat.frag",      170, 680,
     [("splat_radius", 0.08), ("splat_force", 0.5), ("burst", 1)],
     [("texture1", "fluidpass_advect_vel")], 0),
]

def param_block(name, value):
    return (
        "\t\t<param>\n"
        f"\t\t\t<name>{name}</name>\n"
        "\t\t\t<min>0</min>\n"
        "\t\t\t<max>1</max>\n"
        f"\t\t\t<value>{value}</value>\n"
        "\t\t\t<movtype>0</movtype>\n"
        "\t\t\t<lastmovtype>0</lastmovtype>\n"
        "\t\t\t<speed>0.2</speed>\n"
        "\t\t\t<bpmrate>1</bpmrate>\n"
        "\t\t\t<audiosource>0</audiosource>\n"
        "\t\t\t<audiodiv>4</audiodiv>\n"
        "\t\t\t<audiobase>0</audiobase>\n"
        "\t\t\t<audioamount>1</audioamount>\n"
        "\t\t\t<audioinvert>0</audioinvert>\n"
        "\t\t\t<audiothreshold>0.5</audiothreshold>\n"
        "\t\t\t<audiocurve>1</audiocurve>\n"
        "\t\t\t<audioattackms>10</audioattackms>\n"
        "\t\t\t<audioreleasems>250</audioreleasems>\n"
        "\t\t</param>\n"
    )

def box_block(idx, nombre, frag, x, y, params, links, tooutput):
    lines = ["\t<box>\n"]
    lines.append(f"\t\t<nombre>{nombre}</nombre>\n")
    lines.append(f"\t\t<x>{x}</x>\n")
    lines.append(f"\t\t<y>{y}</y>\n")
    lines.append(f"\t\t<directory>data\\shaders\\generative\\{frag}</directory>\n")
    lines.append(f"\t\t<uid>fl-{idx:02d}</uid>\n")
    lines.append(f"\t\t<tooutput>{tooutput}</tooutput>\n")
    lines.append("\t\t<onoff>1</onoff>\n")
    lines.append("\t\t<bypass>0</bypass>\n")
    if params:
        lines.append("\t<parameters>\n")
        for name, value in params:
            lines.append(param_block(name, value))
        lines.append("\t</parameters>\n")
    else:
        lines.append("\t<parameters>\n\t</parameters>\n")
    if links:
        lines.append("\t<fboslinks>\n")
        for sampler, target in links:
            lines.append(f"\t\t<{sampler}>{target}</{sampler}>\n")
        lines.append("\t</fboslinks>\n")
    lines.append("\t</box>\n")
    return "".join(lines)

def main():
    # validaciones
    names = [b[0] for b in BOXES]
    assert len(names) == len(set(names)), "nombres duplicados"
    for _, _, _, _, _, links, _ in BOXES:
        for sampler, target in links:
            assert target in names, f"link a box inexistente: {target}"
            assert sampler.startswith("texture"), f"sampler raro: {sampler}"
    # activerender = indice del display (primer box del XML)
    out = ['<?xml version="1.0"?>\r\n', "<activerender>0</activerender>\r\n"]
    for idx, (nombre, frag, x, y, params, links, tooutput) in enumerate(BOXES):
        out.append(box_block(idx, nombre, frag, x, y, params, links, tooutput))
    with open(OUT, "w", encoding="utf-8", newline="") as f:
        f.write("".join(out))
    print("escrito:", os.path.abspath(OUT))
    print("boxes:", len(BOXES))

if __name__ == "__main__":
    main()
