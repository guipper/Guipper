#!/usr/bin/env python3
"""Verify the prepared OF SDK against the local dependency lock before builds."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--of-root', type=Path, required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
lock = json.loads((repo/'release/dependencies.lock.json').read_text())
version_header = (args.of_root/'libs/openFrameworks/utils/ofConstants.h').read_text()
for name, value in [('MAJOR',0), ('MINOR',12), ('PATCH',1)]:
    import re
    if not re.search(r'#define\s+OF_VERSION_'+name+r'\s+'+str(value)+r'\b', version_header):
        raise SystemExit('Expected openFrameworks 0.12.1')
for addon, expected in lock['addons'].items():
    base = args.of_root/'addons'/addon
    digest = hashlib.sha256()
    for file in sorted(base.rglob('*')):
        relative = file.relative_to(base)
        if file.is_file() and (relative.parts[0] in ['src','libs'] or relative.as_posix()=='addon_config.mk') and file.suffix not in ['.o','.obj','.log']:
            digest.update(relative.as_posix().encode()+b'\0'+file.read_bytes()+b'\0')
    if digest.hexdigest() != expected['sha256_tree']:
        raise SystemExit(f'{addon}: dependency differs from reviewed SDK; update the lock explicitly after validation')
print('Dependency lock verified')
