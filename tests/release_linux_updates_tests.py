import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/release'))
from linux_updates import update_information, fingerprint
spec=importlib.util.spec_from_file_location('sign_linux',ROOT/'scripts/release/sign-linux.py')
signing=importlib.util.module_from_spec(spec);spec.loader.exec_module(signing)

class LinuxUpdateTests(unittest.TestCase):
    def test_explicit_channels_and_https(self):
        for value in ['gh-releases-zsync|guipper|Guipper|stable|Guipper-*-linux-x64.AppImage.zsync','zsync|https://example.com/stable.zsync']:
            self.assertEqual(update_information(value),value)
    def test_reject_ambiguous_or_insecure_channels(self):
        for value in ['gh-releases-zsync|a|b|latest|*.AppImage.zsync','gh-releases-zsync|a|b|beta',
                      'zsync|http://example.com/a','zsync|https://user:pass@example.com/a',
                      'zsync|https://example.com/a\n','gh-releases-zsync|a|b|../stable|*.AppImage.zsync']:
            with self.subTest(value=value),self.assertRaises(ValueError): update_information(value)
    def test_full_fingerprint_required(self):
        self.assertEqual(fingerprint('ab'*20),'AB'*20)
        for value in ['abc','a'*39,'g'*40,'a'*40+'\n']:
            with self.subTest(value=value),self.assertRaises(ValueError): fingerprint(value)
    def test_zsync_covers_partial_final_block(self):
        with tempfile.TemporaryDirectory() as folder:
            image=Path(folder)/'Guipper.AppImage';image.write_bytes(b'x'*2051)
            metadata=Path(folder)/'update.zsync';url='https://example.com/Guipper.AppImage'
            def write(size,digest):
                metadata.write_text(f'Filename: {image.name}\nLength: {size}\nSHA-1: {digest}\nURL: {url}\n\n')
            write(2051,hashlib.sha1(image.read_bytes()).hexdigest());signing.validate_zsync(image,metadata,url)
            write(2048,hashlib.sha1(image.read_bytes()[:2048]).hexdigest())
            with self.assertRaises(ValueError): signing.validate_zsync(image,metadata,url)
    def test_zsync_rejects_wrong_url_or_name(self):
        with tempfile.TemporaryDirectory() as folder:
            image=Path(folder)/'Guipper.AppImage';image.write_bytes(b'test')
            metadata=Path(folder)/'update.zsync'
            metadata.write_text(f'Filename: other.AppImage\nLength: 4\nSHA-1: {hashlib.sha1(b"test").hexdigest()}\nURL: https://example.com/a\n\n')
            with self.assertRaises(ValueError): signing.validate_zsync(image,metadata,'https://example.com/a')
if __name__=='__main__': unittest.main()
