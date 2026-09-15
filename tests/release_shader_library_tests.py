import importlib.util,json,tempfile,unittest,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('shader_library',ROOT/'scripts/release/shader_library.py');library=importlib.util.module_from_spec(spec);spec.loader.exec_module(library)
class LibraryTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name);(self.root/'release').mkdir();(self.root/'bin/data/shaders').mkdir(parents=True)
 def write(self,value):
  (self.root/'release/shader-library.json').write_text(json.dumps(value))
 def test_no_approval_never_stages_candidates(self):
  self.write({'format':1,'approved':False,'entries':[]});self.assertEqual(library.validate(self.root,[])['entries'],[])
  self.write({'format':1,'approved':False,'entries':[{'path':'shaders/test.frag'}]})
  with self.assertRaises(ValueError):library.validate(self.root,[])
 def test_reject_future_format(self):
  self.write({'format':2,'approved':True,'entries':[]})
  with self.assertRaises(ValueError):library.validate(self.root,[])
 def test_approved_library_requires_exact_counts_hashes_and_dependencies(self):
  (self.root/'LICENSE').write_text('Synthetic test license evidence')
  entries=[];assets=[]
  for category,count in library.CATEGORIES.items():
   for i in range(count):
    path=f'shaders/{category}{i}.frag';content=b'void main() {}';(self.root/'bin/data'/path).write_bytes(content);digest=hashlib.sha256(content).hexdigest()
    entries.append({'path':path,'sha256':digest,'category':category,'name':{'en':'Test','es':'Prueba'},'description':{'en':'Test source','es':'Fuente de prueba'},'tags':[],'inputs':[],'author':'Test','license':{'spdx':'MIT','source':'LICENSE','reviewed':True}})
    assets.append({'path':path,'sha256':digest,'license':'MIT'})
  catalog={'format':1,'approved':True,'entries':entries};self.write(catalog);self.assertEqual(len(library.validate(self.root,assets)['entries']),24)
  first=self.root/'bin/data'/entries[0]['path']
  first.write_text('#pragma include \"missing.frag\"\nvoid main() {}')
  entries[0]['sha256']=assets[0]['sha256']=hashlib.sha256(first.read_bytes()).hexdigest();self.write(catalog)
  with self.assertRaisesRegex(ValueError,'dependency'):library.validate(self.root,assets)
  first.write_text('changed')
  with self.assertRaises(ValueError):library.validate(self.root,assets)
  entries.pop();self.write(catalog)
  with self.assertRaises(ValueError):library.validate(self.root,assets)
 def test_path_safety(self):
  for path in ['shaders/../a.frag','shaders//a.frag','shaders/private/mix.frag','shaders/a\\b.frag','/shaders/a.frag']:
   self.assertFalse(library.safe_path(path))
if __name__=='__main__':unittest.main()
