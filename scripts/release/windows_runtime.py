#!/usr/bin/env python3
"""Validate a reviewed app-local Windows runtime before creating staging."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess


def x64_pe(path):
    with path.open('rb') as source:
        header = source.read(64)
        if len(header) != 64 or header[:2] != b'MZ':
            raise ValueError(f'Not a PE image: {path.name}')
        source.seek(struct.unpack_from('<I', header, 60)[0])
        pe = source.read(6)
        if len(pe) != 6 or pe[:4] != b'PE\0\0' or struct.unpack_from('<H', pe, 4)[0] != 0x8664:
            raise ValueError(f'Expected Windows x64: {path.name}')


def inside(root, relative):
    path = Path(relative)
    if path.is_absolute() or '..' in path.parts or '\\' in relative or ':' in relative:
        raise ValueError('Manifest paths must be relative, with forward slashes')
    resolved = (root / path).resolve()
    if not resolved.is_relative_to(root.resolve()) or not resolved.is_file():
        raise ValueError(f'Missing or escaping runtime file: {relative}')
    return resolved


def imports(path, dumpbin):
    env = dict(os.environ, VSLANG='1033')
    result = subprocess.run([str(dumpbin), '/nologo', '/dependents', str(path)],
                            check=True, text=True, capture_output=True, env=env)
    # Includes normal and delay-load sections, but not arbitrary paths/headings.
    return {line.strip().lower() for line in result.stdout.splitlines()
            if re.fullmatch(r'[A-Za-z0-9_.+\-]+\.(?:dll|drv|exe)', line.strip(), re.I)}


def validate(binary, root, manifest, system_dlls, inspect):
    if manifest.get('schema') != 1 or manifest.get('reviewed') is not True:
        raise ValueError('Windows runtime manifest has not been reviewed')
    if not manifest.get('sdk_version') or not manifest.get('review_note'):
        raise ValueError('Record the SDK version and redistribution review')
    x64_pe(binary)
    entries = manifest.get('files', [])
    if not entries:
        raise ValueError('Reviewed runtime must explicitly list its DLLs')
    payloads, names, licenses = [], set(), {}
    for entry in entries:
        source = inside(root, entry['path'])
        name = source.name.lower()
        if source.suffix.lower() != '.dll' or name in names or name in system_dlls or name.startswith(('api-ms-', 'ext-ms-')):
            raise ValueError(f'Duplicate or disallowed runtime DLL: {name}')
        if not entry.get('source_url', '').startswith('https://') or not entry.get('license'):
            raise ValueError(f'Missing provenance or license: {name}')
        if hashlib.sha256(source.read_bytes()).hexdigest() != entry['sha256']:
            raise ValueError(f'Runtime hash mismatch: {name}')
        notice = inside(root, entry['notice'])
        if hashlib.sha256(notice.read_bytes()).hexdigest() != entry['notice_sha256']:
            raise ValueError(f'Notice hash mismatch: {name}')
        x64_pe(source)
        names.add(name)
        payloads.append(source)
        licenses[name + '.txt'] = notice
    for source in [binary, *payloads]:
        unresolved = sorted(name for name in inspect(source)
                            if name not in names and name not in system_dlls
                            and not name.startswith(('api-ms-win-', 'ext-ms-win-')))
        if unresolved:
            raise ValueError(f'{source.name}: missing DLLs: {", ".join(unresolved)}')
    return payloads, licenses


def package(binary, output, root, manifest_path, dumpbin):
    if output.exists():
        raise ValueError('Output exists; choose a fresh staging directory')
    manifest = json.loads(manifest_path.read_text())
    policy = json.loads((Path(__file__).resolve().parents[2] / 'release/windows-system-dlls.json').read_text())
    payloads, licenses = validate(binary, root, manifest, set(policy['dlls']),
                                  lambda path: imports(path, dumpbin))
    from stage import stage
    stage(output, binary)
    for source in payloads:
        shutil.copy2(source, output / source.name)
    notices = output / 'licenses/windows-runtime'
    notices.mkdir(parents=True)
    for name, source in licenses.items():
        shutil.copy2(source, notices / name)
    shutil.copy2(manifest_path, output / 'WINDOWS-RUNTIME.json')
    # Internal build receipt. Inno refuses the old executable-only staging.
    (output / 'WINDOWS-RUNTIME-VERIFIED').write_text('1\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    for name in ['binary', 'output', 'runtime-root', 'manifest', 'dumpbin']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    package(args.binary.resolve(), args.output.resolve(), args.runtime_root.resolve(),
            args.manifest.resolve(), args.dumpbin.resolve())
