import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('windows_runtime', Path(__file__).resolve().parents[1] / 'scripts/release/windows_runtime.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def pe(path, machine=0x8664):
    data=bytearray(70); data[:2]=b'MZ'; struct.pack_into('<I', data, 60, 64)
    data[64:68]=b'PE\0\0'; struct.pack_into('<H', data, 68, machine); path.write_bytes(data)


class WindowsRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name); self.exe=self.root/'Guipper.exe'; pe(self.exe)
        self.dll=self.root/'codec.dll'; pe(self.dll)
        self.notice=self.root/'LICENSE.txt'; self.notice.write_text('test notice')
        self.entry={'path':'codec.dll','sha256':hashlib.sha256(self.dll.read_bytes()).hexdigest(),
                    'source_url':'https://example.org/source','license':'test',
                    'notice':'LICENSE.txt','notice_sha256':hashlib.sha256(self.notice.read_bytes()).hexdigest()}
        self.manifest={'schema':1,'reviewed':True,'sdk_version':'fixture','review_note':'test only','files':[self.entry]}

    def validate(self, dependencies=None):
        return module.validate(self.exe,self.root,self.manifest,{'kernel32.dll'},
                               lambda path: (dependencies or {}).get(path.name,set()))

    def test_transitive_missing_dependency(self):
        with self.assertRaisesRegex(ValueError,'missing DLLs: other.dll'):
            self.validate({'Guipper.exe':{'codec.dll'},'codec.dll':{'other.dll'}})

    def test_complete_runtime(self):
        payloads, notices=self.validate({'Guipper.exe':{'codec.dll','kernel32.dll','api-ms-win-core-file-l1-1-0.dll'}})
        self.assertEqual(payloads,[self.dll]); self.assertEqual(notices['codec.dll.txt'],self.notice)

    def test_unreviewed(self):
        self.manifest['reviewed']=False
        with self.assertRaisesRegex(ValueError,'not been reviewed'): self.validate()

    def test_wrong_architecture(self):
        pe(self.exe,0x14c)
        with self.assertRaisesRegex(ValueError,'x64'): self.validate()

    def test_hash_mismatch(self):
        self.dll.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'hash mismatch'): self.validate()

    def test_missing_notice(self):
        self.notice.unlink()
        with self.assertRaisesRegex(ValueError,'Missing'): self.validate()

    def test_duplicate_case_insensitive(self):
        self.manifest['files'].append(dict(self.entry))
        with self.assertRaisesRegex(ValueError,'Duplicate'): self.validate()

    def test_path_escape(self):
        self.entry['path']='../outside.dll'
        with self.assertRaisesRegex(ValueError,'relative'): self.validate()

    def test_symlink_escape(self):
        with tempfile.TemporaryDirectory() as outside:
            foreign=Path(outside)/'foreign.dll';pe(foreign)
            self.dll.unlink();self.dll.symlink_to(foreign)
            with self.assertRaisesRegex(ValueError,'escaping'): self.validate()
