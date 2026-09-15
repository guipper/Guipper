#!/usr/bin/env python3
"""Read-only shader inventory. Outputs must be outside the source resource tree."""
import argparse,collections,hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
DATA=ROOT/'bin/data'
EXPERIMENTAL={'notworking','halfworking','roto','experimental','bckup','deprecated'}
ATTRIBUTION=re.compile(r'https?://|copyright|license|shadertoy|\bauthor\b|\bby\s+[A-Z]|\b(?:iq|kali)\b',re.I)
def dependencies(path,seen=None):
 seen=set() if seen is None else seen
 if path in seen:return [],['include cycle: '+str(path.relative_to(DATA))]
 seen=seen|{path};found=[];issues=[]
 for name in re.findall(r'^\s*#\s*(?:pragma\s+)?include\s*["<]([^">]+)',path.read_text(errors='replace'),re.M):
  dep=(path.parent/name).resolve()
  if not dep.is_relative_to(DATA.resolve()):issues.append('external include: '+name);continue
  rel=str(dep.relative_to(DATA.resolve()));found.append(rel)
  if not dep.is_file():issues.append('missing include: '+rel);continue
  nested,errors=dependencies(dep,seen);found.extend(nested);issues.extend(errors)
 return sorted(set(found)),issues
def inventory(parser):
 files=sorted((DATA/'shaders').rglob('*.frag'));hashes=collections.defaultdict(list)
 for p in files:hashes[hashlib.sha256(p.read_bytes()).hexdigest()].append(str(p.relative_to(DATA)))
 for group in hashes.values():group.sort(key=lambda name:('copy' in name.lower(),len(name),name))
 changed=set(subprocess.check_output(['git','diff','--name-only','HEAD','--','bin/data/shaders'],cwd=ROOT,text=True).splitlines())
 result=[]
 for p in files:
  rel=str(p.relative_to(DATA));source=p.read_text(errors='replace');digest=hashlib.sha256(p.read_bytes()).hexdigest()
  parsed=json.loads(subprocess.check_output([str(parser),str(p)],text=True));deps,issues=dependencies(p.resolve())
  history=subprocess.check_output(['git','log','--follow','--diff-filter=A','--format=%H%x09%an','--',str(p.relative_to(ROOT))],cwd=ROOT,text=True).splitlines()
  origin=history[-1].split('\t',1) if history else []
  attrib=[line.strip()[:250] for line in source.splitlines() if ATTRIBUTION.search(line)]
  category={'generative':'generative','imageprocessing':'effects','blending':'mixers'}.get(p.parent.name,'uncategorized')
  internal='private' in p.parts or not re.search(r'\bvoid\s+main\s*\(',source)
  reasons=[]
  if internal:reasons.append('internal/helper')
  if any(x in EXPERIMENTAL for x in p.parts):reasons.append('experimental folder')
  if 'contrib' in p.parts:reasons.append('contributed collection: permission review required')
  if attrib:reasons.append('source attribution requires review')
  if str(p.relative_to(ROOT)) in changed:reasons.append('local edits: do not redistribute without review')
  if not origin:reasons.append('no tracked origin')
  if parsed['has_errors']:reasons.append('uniform parser errors')
  reasons+=issues
  if hashes[digest][0]!=rel:reasons.append('exact duplicate of '+hashes[digest][0])
  result.append({'path':rel,'sha256':digest,'category':category,'classification':'internal' if internal else ('excluded' if reasons else 'candidate_unvalidated'),'reasons':reasons,'dependencies':deps,'duplicate_paths':hashes[digest] if len(hashes[digest])>1 else [],'origin_commit':origin[0] if origin else '', 'origin_author':origin[1] if origin else '', 'license_evidence':'LICENSE (repository MIT); individual origin/attribution still reviewed before approval','attribution_lines':attrib,**parsed})
 return {'format':1,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'shaders':result}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--parser',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 if a.output.resolve().is_relative_to(DATA.resolve()):p.error('Do not write inventory inside bin/data')
 report=inventory(a.parser.resolve());a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
 print(collections.Counter(x['classification'] for x in report['shaders']))
