#!/usr/bin/env python3
"""Run each shader in an isolated native process with bounded runtime."""
import argparse,json,os,shutil,subprocess,tempfile,sys,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def run(binary,inventory,output,paths):
 if output.exists():raise ValueError('Choose a new output directory')
 output.mkdir(parents=True)
 rows={x['path']:x for x in json.loads(inventory.read_text())['shaders']}
 with tempfile.TemporaryDirectory(prefix='guipper-shader-audit-') as folder:
  work=Path(folder);shutil.copy2(binary,work/'Guipper');data=work/'data';data.mkdir()
  for name in ['shaders','font','img']:
   shutil.copytree(ROOT/'bin/data'/name,data/name)
  shutil.copy2(ROOT/'bin/data/guipper.png',data/'guipper.png')
  (data/'distribution.marker').write_text('1\n')
  sys.path.insert(0,str(ROOT/'scripts/release'));from demos import generate
  generate(data)
  results=[]
  for index,path in enumerate(paths):
   item=rows[path];destination=output/f'{index+1:02d}';destination.mkdir()
   if hashlib.sha256((data/path).read_bytes()).hexdigest()!=item['sha256']:
    raise ValueError('Shader changed since inventory: '+path)
   if item['classification']!='candidate_unvalidated':
    result={'path':path,'passed':False,'excluded':item['reasons']}
   else:
    env=dict(os.environ,GUIPPER_USER_ROOT=str(work/'profile'),GUIPPER_SHADER_AUDIT=path,GUIPPER_GPU='default')
    for k in ['APPIMAGE','GUIPPER_LEGACY_DATA','GUIPPER_UISHOT','GUIPPER_PERSISTENCE_TEST']:env.pop(k,None)
    # Every process starts from the same clean profile, including managed resources.
    if (work/'profile').exists():shutil.rmtree(work/'profile')
    try:
     with (destination/'run.log').open('w') as log:
      process=subprocess.run([str(work/'Guipper')],cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=60)
     report=work/'profile/cache/shader-audit'
     if report.is_dir():
      for f in report.iterdir():shutil.copy2(f,destination/f.name)
     result=json.loads((destination/'result.json').read_text()) if (destination/'result.json').exists() else {'path':path,'passed':False,'error':'No native report'}
     result['exit_code']=process.returncode
     if process.returncode:result['passed']=False
    except subprocess.TimeoutExpired:result={'path':path,'passed':False,'error':'60 second timeout'}
   result['folder']=destination.name;results.append(result)
   (output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
   print(index+1,path,'PASS' if result['passed'] else 'REVIEW',flush=True)
 return results
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--inventory',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--candidates',type=Path,required=True);a=p.parse_args()
 results=run(a.binary.resolve(),a.inventory,a.output.resolve(),[x['path'] for x in json.loads(a.candidates.read_text())['entries']])
 sys.exit(0 if results and all(r['passed'] for r in results) else 1)
