#!/usr/bin/env python3
"""Generate public build configuration. Never reads or stores private keys."""
import argparse
import base64
import json
from pathlib import Path
from urllib.parse import urlparse
from linux_updates import update_information, fingerprint
parser=argparse.ArgumentParser()
parser.add_argument('--platform',choices=['windows','linux'],required=True)
parser.add_argument('--stable',required=True)
parser.add_argument('--beta',required=True)
parser.add_argument('--public-key',help='Windows: path to base64 Ed25519 PUBLIC key')
parser.add_argument('--signing-fingerprint',help='Linux: full OpenPGP signing fingerprint')
parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[2]/'src/JPutils/jp_update_config.h')
args=parser.parse_args()
for value in [args.stable,args.beta]:
    if args.platform=='windows':
        parsed=urlparse(value)
        if parsed.scheme!='https' or not parsed.hostname or parsed.username or parsed.password:
            parser.error('Appcast URLs must be HTTPS without credentials')
    else:
        try: update_information(value)
        except ValueError as error: parser.error(str(error))
lines=['#pragma once','// Generated public configuration; private keys never belong here.']
if args.platform=='windows':
    if not args.public_key: parser.error('--public-key is required')
    key=Path(args.public_key).read_text().strip()
    if len(base64.b64decode(key,validate=True))!=32: parser.error('Expected 32-byte Ed25519 public key')
    lines+=['#define GUIPPER_WINSPARKLE 1','#define GUIPPER_WINSPARKLE_PUBLIC_KEY '+json.dumps(key)]
    prefix='GUIPPER_WINSPARKLE'
else:
    if not args.signing_fingerprint: parser.error('--signing-fingerprint is required for Linux')
    try: key=fingerprint(args.signing_fingerprint)
    except ValueError as error: parser.error(str(error))
    lines+=['#define GUIPPER_APPIMAGE_UPDATES 1','#define GUIPPER_APPIMAGE_SIGNING_FINGERPRINT '+json.dumps(key)]
    prefix='GUIPPER_APPIMAGE'
lines += [f'#define {prefix}_STABLE '+json.dumps(args.stable),f'#define {prefix}_BETA '+json.dumps(args.beta)]
output=args.output
output.write_text('\n'.join(lines)+'\n')
# A newly created optional header is absent from the previous make dependency
# file. Invalidate the adapter so enabling updates cannot reuse a disabled build.
if output.resolve()==Path(__file__).resolve().parents[2]/'src/JPutils/jp_update_config.h':
    output.with_name('jp_update_platform.cpp').touch()
print('Public configuration generated. Sign the Windows installer before distributing this build.' if args.platform=='windows' else 'Public configuration generated. Package the native update worker and sign the AppImage before distributing this build.')
