#!/usr/bin/env python3
"""Build pinned AppImageUpdate against Ubuntu 24.04 system crypto/curl.
The output is a build SDK; no private signing material is read here.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tarfile

ROOT=Path(__file__).resolve().parents[2]

def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)

def build(work, output, jobs):
    if output.exists():
        raise ValueError('Choose a fresh SDK output directory')
    lock=json.loads((ROOT/'release/linux-update-sdk.json').read_text())
    sources={}
    for name, item in lock.items():
        source=work/name
        if not source.exists():
            source.mkdir(parents=True)
            run('git','init',source)
            run('git','-C',source,'remote','add','origin',item['url'])
        run('git','-C',source,'fetch','--depth=1','origin',item['commit'])
        run('git','-C',source,'checkout','--detach',item['commit'])
        if subprocess.check_output(['git','-C',str(source),'status','--porcelain']).strip():
            raise ValueError(f'Dirty SDK source: {source}')
        sources[name]=source
    # Upstream dereferences a missing GPG engine. Fail with a recoverable error.
    signing=sources['AppImageUpdate']/'src/signing/signaturevalidator.cpp'
    original=signing.read_text()
    needle='            // experience within AppImageKit'
    assert original.count(needle)==1
    signing.write_text(original.replace(needle, '''            if (!engine_info || !engine_info->version) {
                throw GpgError(GPG_ERR_NO_ERROR, "GnuPG 2.2 or newer is required to verify updates");
            }

'''+needle))
    # Expose the parsed destination after checkForChanges, without downloading.
    # This lets the UI name/skip a version and compare a renamed installed image.
    client=sources['zsync2']/'src/zsclient.cpp'
    client_original=client.read_text()
    guard='        if (d->state <= d->RUNNING)\n            return false;\n\n'
    assert client_original.count(guard)==1
    client.write_text(client_original.replace(guard,''))
    updater=sources['AppImageUpdate']/'src/updater/updater.cpp'
    updater_original=updater.read_text()
    constructor='new zsync2::ZSyncClient(zsyncUrl, appImage.path())'
    assert updater_original.count(constructor)==1
    updater.write_text(updater_original.replace(constructor,'new zsync2::ZSyncClient(zsyncUrl, appImage.path(), false)'))
    try:
        builddir=work/'build'
        options=[f'-DFETCHCONTENT_SOURCE_DIR_{name.upper()}={path}' for name,path in sources.items() if name!='AppImageUpdate']
        run('cmake','-S',sources['AppImageUpdate'],'-B',builddir,
            '-DCMAKE_BUILD_TYPE=Release','-DBUILD_TESTING=OFF',
            '-DBUILD_LIBAPPIMAGEUPDATE_ONLY=ON','-DBUILD_QT_UI=OFF',
            '-DLIBAPPIMAGE_SHARED_ONLY=ON','-DCPR_FORCE_USE_SYSTEM_CURL=ON',
            '-DCMAKE_CXX_FLAGS='+os.environ.get('CXXFLAGS','')+' -Wno-error=deprecated-declarations',*options)
        run('cmake','--build',builddir,'--target','libappimageupdate',f'-j{jobs}')
        (output/'lib').mkdir(parents=True)
        (output/'bin').mkdir()
        (output/'include/appimage').mkdir(parents=True)
        shutil.copy2(builddir/'src/updater/libappimageupdate.so',output/'lib')
        shutil.copy2(sources['AppImageUpdate']/'include/appimage/update.h',output/'include/appimage')
        flags=subprocess.check_output(['pkg-config','--cflags','gpg-error'],text=True).split()
        run(os.environ.get('CXX','c++'),'-std=c++17',ROOT/'scripts/release/native/verify-appimage.cpp',
            '-I'+str(sources['AppImageUpdate']/'src'),*shlex.split(os.environ.get('CXXFLAGS','')),*flags,'-L'+str(output/'lib'),
            '-lappimageupdate','-Wl,-rpath,$ORIGIN/../lib','-o',output/'bin/verify-appimage')
        run(os.environ.get('CXX','c++'),'-std=c++17',ROOT/'scripts/release/native/update-worker.cpp',
            '-I'+str(output/'include'),'-L'+str(output/'lib'),'-lappimageupdate','-pthread',
            '-Wl,-rpath,$ORIGIN/../lib','-o',output/'bin/guipper-update-worker')
        for name,item in lock.items():
            target=output/'licenses'/name
            target.mkdir(parents=True)
            shutil.copy2(sources[name]/item['license'],target)
        shutil.copy2(ROOT/'release/linux-update-sdk.json',output/'sources.json')
        # Corresponding source includes the exact patched SDK and worker used.
        with tarfile.open(output/'UPDATE-SOURCE.tar.gz','w:gz') as archive:
            for name,source in sources.items():
                files=subprocess.check_output(['git','-C',str(source),'ls-files','-z']).decode().split('\0')
                for filename in filter(None,files): archive.add(source/filename,arcname=name+'/'+filename,recursive=False)
            for filename in ['scripts/release/build-linux-update-sdk.py','scripts/release/native/update-worker.cpp',
                'scripts/release/native/verify-appimage.cpp','release/linux-update-sdk.json','release/LINUX_UPDATES.md']:
                archive.add(ROOT/filename,arcname='guipper/'+filename)
    finally:
        signing.write_text(original)
        client.write_text(client_original)
        updater.write_text(updater_original)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--work',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--jobs',type=int,default=2)
    a=p.parse_args()
    build(a.work.resolve(),a.output.resolve(),a.jobs)
