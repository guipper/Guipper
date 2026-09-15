#!/usr/bin/env python3
"""Create a clean distribution tree using an explicit, hash-checked allowlist."""
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def stage(output, binary):
    if output.exists():
        raise ValueError('Output already exists; choose a fresh staging directory')
    version = (ROOT/'VERSION').read_text().strip()
    header = (ROOT/'src/JPutils/jp_version.h').read_text()
    compiled_version = re.search(r'^#define GUIPPER_VERSION "([^"]+)"$', header, re.M)
    if not compiled_version or compiled_version.group(1) != version:
        raise ValueError('VERSION and compiled version header differ')
    manifest = json.loads((ROOT/'release/assets.json').read_text())
    # Validate everything before copying any payload.
    for item in manifest['assets']:
        path = Path(item['path'])
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('Unsafe asset path')
        source = ROOT/'bin/data'/path
        if source.is_symlink() or hashlib.sha256(source.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError(f'Unreviewed resource changes: {path}')
    output.mkdir(parents=True)
    shutil.copy2(binary, output/binary.name)
    for item in manifest['assets']:
        target = output/'data'/item['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT/'bin/data'/item['path'], target)
    (output/'data/distribution.marker').write_text('1\n')
    shutil.copy2(output/'data/guipper.png',output/'data/preview2.png')
    for guide in ['START_HERE_ES.md','START_HERE_EN.md']:
        shutil.copy2(ROOT/'release'/guide,output/guide)
    shutil.copy2(ROOT/'LICENSE', output/'LICENSE.txt')
    shutil.copy2(ROOT/'VERSION', output/'VERSION')
    shutil.copy2(ROOT/'release/install-appimage.sh', output/'install-appimage.sh')
    shutil.copy2(ROOT/'release/assets.json', output/'ASSETS.json')
    shutil.copy2(ROOT/'release/THIRD_PARTY.md', output/'THIRD_PARTY.md')
    licenses = ROOT/'release/licenses'
    if licenses.exists(): shutil.copytree(licenses, output/'licenses')
    from demos import generate
    generate(output/'data')
    return output

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    args = parser.parse_args()
    stage(args.output.resolve(), args.binary.resolve())
