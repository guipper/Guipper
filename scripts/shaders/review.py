#!/usr/bin/env python3
"""Generate local review sheets from native audit captures (requires Pillow)."""
import argparse,json,csv,statistics
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

def generate(candidates,inventory,audit,output):
 output.mkdir(parents=True,exist_ok=True)
 entries=json.loads(candidates.read_text())['entries']
 rows=json.loads(inventory.read_text())['shaders']
 reports={r['path']:r for r in json.loads((audit/'results.json').read_text())}
 font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',15)
 headings={'generative':'Generativos','effects':'Efectos','mixers':'Mezcladores'}
 # All links remain valid when the complete review directory is shared.
 import os
 relative=lambda p:os.path.relpath(p,output)
 lines=['# Preselección visual — pendiente de aprobación','',
 'Objetivo: elegir 12 generativos, 8 efectos y 4 mezcladores. Ningún candidato está aprobado para distribución.', '',
 'Las capturas usan 320×180, semilla 20260915, 24 instantes de 0 a 3,833 s e imágenes de referencia generadas. Los efectos muestran un gradiente; los mezcladores también reciben un damero. Las animaciones no sustituyen una prueba en vivo.', '',
 'La licencia MIT del repositorio y el autor del commit de incorporación son indicios, no una confirmación de autoría individual. Todos requieren confirmar procedencia y permiso antes de empaquetar.', '']
 with (output/'inventory.csv').open('w') as f:
  writer=csv.writer(f);writer.writerow(['path','category','classification','reasons','dependencies','duplicates','origin_commit','origin_author','sha256'])
  for r in rows:writer.writerow([r.get(k,'') for k in ['path','category','classification','reasons','dependencies','duplicate_paths','origin_commit','origin_author','sha256']])
 for category,title in headings.items():
  group=[(i+1,e) for i,e in enumerate(entries) if e['category']==category]
  sheet=Image.new('RGB',(3*340,((len(group)+2)//3)*220),(15,19,25));draw=ImageDraw.Draw(sheet)
  for j,(number,e) in enumerate(group):
   r=reports[e['path']];folder=audit/r['folder'];frames=[s for s in r.get('samples',[]) if s['label'].startswith('initial/') and 'image' in s]
   x=(j%3)*340+10;y=(j//3)*220+10
   if frames:
    im=Image.open(folder/frames[0]['image']).convert('RGB');sheet.paste(im,(x,y))
    sequence=[Image.open(folder/s['image']).convert('RGB') for s in frames]
    sequence[0].save(output/f'{number:02d}.gif',save_all=True,append_images=sequence[1:],duration=167,loop=0)
   draw.text((x,y+185),f'{number:02d}. {e["name"]["es"]}',font=font,fill='white')
  sheet.save(output/f'{category}.jpg',quality=92)
  lines += [f'## {title}','',f'![{title}]({category}.jpg)','']
  for number,e in group:
   r=reports[e['path']];folder=audit/r['folder'];samples=r.get('samples',[]);gpu=[s['gpu_ms'] for s in samples if s.get('gpu_ms') is not None]
   means=[s['mean_rgb'] for s in samples if s['label'].startswith('initial/')]
   flat=' REVISIÓN REQUERIDA: salida inicial casi negra/blanca; no seleccionar sin resolverla.' if means and (max(means)<0.01 or min(means)>0.99) else ''
   lines += [f'### {number:02d}. {e["name"]["es"]} / {e["name"]["en"]}', '',
    f'![Captura]({relative(folder/"frame-0.png")})', '',f'[Ver movimiento]({number:02d}.gif) · [Vista previa]({relative(folder/"preview.png")}) · [Resultados completos]({relative(folder/"result.json")})','',
    e['description']['es'],'',e.get('review_notes',''),'',f'- Ruta: `{e["path"]}`.',f'- Entradas: {", ".join(e["inputs"]) or "ninguna"}.',
    f'- Prueba técnica: {"aprobada" if r["passed"] else "FALLIDA"}; {len(samples)} muestras.{flat}',
    f'- GPU: {r.get("gpu","no disponible")}; mediana {statistics.median(gpu):.3f} ms a 320×180.' if gpu else '- Tiempo GPU no disponible.',
    '- Límites: barrido de extremos individuales, booleanos y cuatro semillas RDM; no certifica todas las combinaciones, automatización, audio, historial de feedback ni rendimiento en tu GPU.',
    f'- Procedencia pendiente: incorporación por {e["license"]["origin_committer"]}, commit `{e["license"]["origin_commit"]}`. Autoría individual por confirmar.',
    '- Decisión: pendiente de revisión visual y permiso documentado.','']
 (output/'REVIEW.md').write_text('\n'.join(lines)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--candidates',type=Path,required=True);p.add_argument('--inventory',type=Path,required=True);p.add_argument('--audit',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(a.candidates,a.inventory,a.audit,a.output)
