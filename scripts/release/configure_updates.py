#!/usr/bin/env python3
"""Generate public build configuration. Never reads or stores private keys."""
import argparse
import base64
import json
from pathlib import Path
from urllib.parse import urlparse
parser=argparse.ArgumentParser()
parser.add_argument('--platform',choices=['windows','linux'],required=True)
parser.add_argument('--stable',required=True)
parser.add_argument('--beta',required=True)
parser.add_argument('--public-key',help='Windows: path to base64 Ed25519 PUBLIC key')
args=parser.parse_args()
for value in [args.stable,args.beta]:
    if args.platform=='windows':
        parsed=urlparse(value)
        if parsed.scheme!='https' or not parsed.hostname or parsed.username or parsed.password:
            parser.error('Appcast URLs must be HTTPS without credentials')
    elif not value.startswith(('gh-releases-zsync|','zsync|https://')):
        parser.error('Use GitHub release or HTTPS zsync update information')
lines=['#pragma once','// Generated public configuration; private keys never belong here.']
if args.platform=='windows':
    if not args.public_key: parser.error('--public-key is required')
    key=Path(args.public_key).read_text().strip()
    if len(base64.b64decode(key,validate=True))!=32: parser.error('Expected 32-byte Ed25519 public key')
    lines+=['#define GUIPPER_WINSPARKLE 1','#define GUIPPER_WINSPARKLE_PUBLIC_KEY '+json.dumps(key)]
    prefix='GUIPPER_WINSPARKLE'
else:
    lines+=['#define GUIPPER_APPIMAGE_UPDATES 1']
    prefix='GUIPPER_APPIMAGE'
lines += [f'#define {prefix}_STABLE '+json.dumps(args.stable),f'#define {prefix}_BETA '+json.dumps(args.beta)]
output=Path(__file__).resolve().parents[2]/'src/JPutils/jp_update_config.h'
output.write_text('\n'.join(lines)+'\n')
print('Public configuration generated. The native SDK must be linked, and packages signed before distributing this build.')
