import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/release'))
import stage

class PackagingTests(unittest.TestCase):
    def test_allowlist_excludes_private_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            binary=root/'Guipper'; binary.write_bytes(b'test executable')
            output=stage.stage(root/'package',binary)
            self.assertFalse((output/'data/settings.xml').exists())
            self.assertFalse((output/'data/midi_keymap.xml').exists())
            self.assertFalse((output/'data/font/consola.ttf').exists())
            self.assertFalse((output/'data/savefiles/blobslocos.xml').exists())
            self.assertTrue((output/'data/distribution.marker').exists())
            self.assertTrue((output/'data/savefiles/examples/03-mix.xml').exists())
            self.assertTrue((output/'licenses/OFL-Montserrat.txt').exists())
            with self.assertRaises(ValueError): stage.stage(output,binary)

if __name__=='__main__': unittest.main()
