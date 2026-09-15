"""Approved library is opt-in. Candidates never enter an AppImage implicitly."""
import hashlib,json,re
from pathlib import Path
CATEGORIES={'generative':12,'effects':8,'mixers':4}
def safe_path(value):
 return value.startswith('shaders/') and value.endswith('.frag') and not value.startswith('shaders/private/') and not any(x in value for x in ['\\',':','\x00']) and all(x not in ['','.','..'] for x in value.split('/'))
def validate(root,assets):
 file=root/'release/shader-library.json'
 if not file.exists():return None
 catalog=json.loads(file.read_text())
 if type(catalog.get('format')) is not int or catalog['format']!=1:raise ValueError('Unsupported shader catalog format')
 if type(catalog.get('approved')) is not bool or not isinstance(catalog.get('entries'),list):raise ValueError('Invalid shader catalog')
 if not catalog['approved']:
  if catalog['entries']:raise ValueError('Unapproved shaders must stay in shader-candidates.json')
  return catalog
 manifest={a['path']:a for a in assets};seen=set();counts=dict.fromkeys(CATEGORIES,0)
 for entry in catalog['entries']:
  path=entry['path']
  if not safe_path(path) or path in seen:raise ValueError('Unsafe or duplicate catalog path')
  seen.add(path)
  if entry['category'] not in counts:raise ValueError('Unknown shader category')
  counts[entry['category']]+=1
  for key in ['name','description']:
   if any(not isinstance(entry[key].get(lang),str) or not entry[key][lang].strip() for lang in ['en','es']):raise ValueError('Missing translation')
  for key in ['tags','inputs']:
   if not isinstance(entry[key],list) or any(not isinstance(v,str) for v in entry[key]):raise ValueError('Invalid catalog list')
  if not entry.get('author') or not entry['license'].get('reviewed') or not entry['license'].get('spdx') or not entry['license'].get('source'):raise ValueError('Unreviewed shader provenance')
  evidence=Path(entry['license']['source'])
  if evidence.is_absolute() or '..' in evidence.parts or not (root/evidence).is_file():raise ValueError('License evidence must be a documented repository file')
  if path not in manifest or manifest[path]['sha256']!=entry['sha256'] or manifest[path]['license']!=entry['license']['spdx']:raise ValueError('Catalog shader absent from reviewed assets')
  source=root/'bin/data'/path
  if source.is_symlink() or hashlib.sha256(source.read_bytes()).hexdigest()!=entry['sha256']:raise ValueError('Catalog shader changed since review')
  def check_dependencies(source,visited):
   if source in visited:raise ValueError('Shader include cycle')
   for inc in re.findall(r'^\s*#\s*(?:pragma\s+)?include\s*["<]([^">]+)',source.read_text(errors='replace'),re.M):
    dep=(source.parent/inc).resolve();base=(root/'bin/data').resolve()
    if not dep.is_relative_to(base) or str(dep.relative_to(base)) not in manifest:raise ValueError('Shader dependency absent from reviewed assets')
    check_dependencies(dep,visited|{source})
  check_dependencies(source.resolve(),set())
 if counts!=CATEGORIES:raise ValueError('Approved library must contain 12 generative, 8 effects and 4 mixers')
 return catalog
