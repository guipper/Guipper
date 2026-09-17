#!/usr/bin/env python3
"""Run the native alpha-chain regression without changing user sessions or profiles.

Linux: xvfb-run -a python3 tests/run_shader_alpha.py
Windows (with a desktop): python tests/run_shader_alpha.py
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'bin' / ('Guipper.exe' if os.name == 'nt' else 'Guipper'))
    parser.add_argument('--log', type=Path, default=ROOT / 'dist' / 'shader-alpha-chain.log')
    args = parser.parse_args()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='guipper-alpha-chain-') as folder:
        work = Path(folder)
        binary = work / args.binary.name
        shutil.copy2(args.binary, binary)
        shutil.copytree(ROOT / 'bin' / 'data', work / 'data')
        for dependency in args.binary.parent.iterdir():
            if dependency.is_file() and (dependency.suffix.lower() == '.dll' or '.so' in dependency.name):
                shutil.copy2(dependency, work / dependency.name)
        env = dict(os.environ)
        for key in list(env):
            if key.startswith('GUIPPER_') or key in ('APPIMAGE', 'APPDIR'):
                env.pop(key)
        env.update(GUIPPER_PERSISTENCE_TEST='shader_alpha_chain', GUIPPER_USER_ROOT=str(work / 'profile'))
        with args.log.open('w') as log:
            try:
                result = subprocess.run([str(binary)], cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            except subprocess.TimeoutExpired:
                print('Regression timed out. Log:', args.log)
                return 1
        text = args.log.read_text(errors='replace')
        for line in text.splitlines():
            if 'shader-alpha-chain' in line:
                print(line)
        passed = result.returncode == 0 and 'shader-alpha-chain: passed=1' in text
        print('PASS' if passed else 'FAIL', '— log:', args.log)
        return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
