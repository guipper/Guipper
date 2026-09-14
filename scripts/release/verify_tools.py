#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('directory',type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
for name,item in json.loads((root/'release/tools.lock.json').read_text()).items():
    if hashlib.sha256((args.directory/name).read_bytes()).hexdigest()!=item['sha256']:
        raise SystemExit('Packaging tool mismatch: '+name)
print('Packaging tools verified')
