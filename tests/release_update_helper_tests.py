import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
HELPER=ROOT/'release/install-appimage.sh'

@unittest.skipUnless(os.name=='posix','POSIX AppImage helper')
class InstallerHelperTests(unittest.TestCase):
    def script(self,path,text):
        path.write_text('#!/bin/sh\n'+text+'\n');path.chmod(0o700)
    def test_success_keeps_previous(self):
        with tempfile.TemporaryDirectory(prefix='guipper update ') as folder:
            root=Path(folder); old=root/'Guipper.AppImage';new=root/'new.AppImage';health=root/'healthy'
            self.script(old,'exit 0')
            self.script(new,': > "$GUIPPER_UPDATE_HEALTH_FILE"\nexit 0')
            subprocess.run(['/bin/sh',str(HELPER),str(old),str(new),'999999999',str(health)],check=True,timeout=8)
            self.assertTrue(health.is_file())
            self.assertTrue(Path(str(old)+'.previous').is_file())
            self.assertFalse(new.exists())
    def test_failed_start_restores_previous(self):
        with tempfile.TemporaryDirectory(prefix='guipper rollback ') as folder:
            root=Path(folder);old=root/'Guipper.AppImage';new=root/'new.AppImage'
            self.script(old,'exit 0');original=old.read_bytes()
            self.script(new,'exit 1')
            subprocess.run(['/bin/sh',str(HELPER),str(old),str(new),'999999999',str(root/'healthy')],check=True,timeout=8)
            self.assertEqual(old.read_bytes(),original)
            self.assertTrue(Path(str(old)+'.failed').is_file())

if __name__=='__main__': unittest.main()
