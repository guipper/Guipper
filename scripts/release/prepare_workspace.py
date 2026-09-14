#!/usr/bin/env python3
"""Copy a prepared SDK into an isolated runner directory; never build in it."""
import argparse
import shutil
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('--sdk',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
source=Path(__file__).resolve().parents[2]
if args.output.exists(): raise SystemExit('Use a fresh output directory for each release job')
if not (args.sdk/'libs/openFrameworks').is_dir(): raise SystemExit('Prepared OF SDK is required')
args.output.mkdir(parents=True)
for name in ['libs','addons','scripts','export','other']:
    if (args.sdk/name).exists(): shutil.copytree(args.sdk/name,args.output/name)
project=args.output/'apps/myApps/Guipper'
shutil.copytree(source,project,ignore=shutil.ignore_patterns('.git','dist','obj','__pycache__'))
print(project)
