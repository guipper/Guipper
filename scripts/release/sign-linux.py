#!/usr/bin/env python3
"""Create a signed AppImage and .zsync; never publishes or exports private keys."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from linux_updates import fingerprint, update_information, https_url

ROOT=Path(__file__).resolve().parents[2]

def validate_zsync(image, metadata, url):
    header=metadata.read_bytes().split(b'\n\n',1)[0].decode('utf-8')
    fields=dict(line.split(': ',1) for line in header.splitlines())
    if (fields.get('Length')!=str(image.stat().st_size) or
            fields.get('SHA-1')!=hashlib.sha1(image.read_bytes()).hexdigest() or
            fields.get('Filename')!=image.name or fields.get('URL')!=url):
        raise ValueError('zsync metadata does not match the complete signed AppImage')

def package(appdir, output, sdk, tool, runtime, info, key, url):
    update_information(info); key=fingerprint(key); https_url(url)
    if Path(url.split('/')[-1]).name!=output.name:
        raise ValueError('Download URL must use the final AppImage filename')
    if output.exists() or Path(str(output)+'.zsync').exists():
        raise ValueError('Signed releases are immutable; choose a fresh output path')
    output.parent.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,ARCH='x86_64')
    env['LD_LIBRARY_PATH']=str(sdk/'lib')+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    with tempfile.TemporaryDirectory(prefix='.signed-',dir=output.parent) as folder:
        candidate=Path(folder)/output.name
        subprocess.run([str(tool),'--runtime-file',str(runtime),'--updateinformation',info,
            '--sign','--sign-key',key,'--file-url',url,str(appdir),str(candidate)],cwd=folder,env=env,check=True)
        # appimagetool can warn and still return success when signing fails.
        subprocess.run([str(sdk/'bin/verify-appimage'),str(candidate),key],env=env,check=True)
        zsync=Path(str(candidate)+'.zsync')
        if not zsync.is_file(): raise ValueError('appimagetool did not produce zsync metadata')
        validate_zsync(candidate,zsync,url)
        receipt={'sha256':hashlib.sha256(candidate.read_bytes()).hexdigest(),
            'signing_fingerprint':key,'update_information':info,'url':url,
            'sdk_sources':json.loads((sdk/'sources.json').read_text())}
        candidate.replace(output)
        zsync.replace(Path(str(output)+'.zsync'))
        Path(str(output)+'.json').write_text(json.dumps(receipt,indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for arg in ['appdir','output','sdk','tool','runtime']: p.add_argument('--'+arg,type=Path,required=True)
    for arg in ['info','key','url']: p.add_argument('--'+arg,required=True)
    a=p.parse_args()
    package(a.appdir.resolve(),a.output.resolve(),a.sdk.resolve(),a.tool.resolve(),a.runtime.resolve(),a.info,a.key,a.url)
