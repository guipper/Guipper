#!/usr/bin/env python3
"""Opt-in native A/B test. All keys, AppImages, downloads and profiles are temporary.
Uses real SDK verification, with loopback HTTP only in this test harness.
"""
import argparse
import functools
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import signal
import select
import time
import subprocess
import tempfile
import threading

ROOT=Path(__file__).resolve().parents[2]

def main(a):
    root=Path(tempfile.mkdtemp(prefix='guipper-native-updates-'))
    print('Evidence: '+str(root),flush=True)
    env=dict(os.environ,GNUPGHOME=str(root/'keys'),ARCH='x86_64',APPIMAGE_EXTRACT_AND_RUN='1')
    env['LD_LIBRARY_PATH']=str(a.sdk/'lib')+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
    (root/'keys').mkdir(mode=0o700)
    results={}
    def run(args, **kw):
        result=subprocess.run([str(v) for v in args],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90,**kw)
        with (root/'test.log').open('a') as log: log.write(result.stdout+'\n')
        if result.returncode: raise RuntimeError(f'Command failed ({result.returncode}): {args[0]}; see {root}/test.log')
        return result.stdout
    server=None
    try:
        keys=[]
        for identity in ['Guipper disposable test A','Guipper disposable test B']:
            run(['gpg','--batch','--pinentry-mode','loopback','--passphrase','','--quick-generate-key',identity,'rsa2048','sign','1d'])
            listing=run(['gpg','--batch','--with-colons','--list-keys',identity])
            keys.append(next(line.split(':')[9] for line in listing.splitlines() if line.startswith('fpr:')))
        serve=root/'serve';serve.mkdir()
        appdir=root/'fixture.AppDir';appdir.mkdir()
        shutil.copy2(ROOT/'release/Guipper.desktop',appdir/'Guipper.desktop')
        run(['python3',ROOT/'scripts/release/icon.py',ROOT/'bin/data/guipper.png',appdir/'guipper.svg'])
        (appdir/'payload').write_bytes(os.urandom(1024*1024))
        for name,key in [('A',keys[0]),('B',keys[0]),('wrong-key',keys[1]),('unsigned',None),('crash',keys[0])]:
            (appdir/'payload').write_bytes(os.urandom(3*1024*1024))
            launcher=appdir/'AppRun'
            launcher.write_text('#!/bin/sh\n'+('exit 1\n' if name=='crash' else f'echo {name}\nif test -n "${{GUIPPER_UPDATE_HEALTH_FILE:-}}"; then touch "$GUIPPER_UPDATE_HEALTH_FILE"; fi\n'))
            launcher.chmod(0o755)
            image=serve/f'{name}.AppImage'
            cmd=[a.tool,'--runtime-file',a.runtime,'-u','zsync|https://example.invalid/Guipper.AppImage.zsync']
            if key:
                run(['python3',ROOT/'scripts/release/sign-linux.py','--appdir',appdir,'--output',image,
                    '--sdk',a.sdk,'--tool',a.tool,'--runtime',a.runtime,
                    '--info','zsync|https://example.invalid/Guipper.AppImage.zsync',
                    '--key',key,'--url','https://example.invalid/'+image.name])
            else: run([*cmd,appdir,image],cwd=serve)
        tampered=serve/'tampered.AppImage';shutil.copy2(serve/'B.AppImage',tampered)
        with tampered.open('ab') as stream: stream.write(b'tampered')
        class Handler(http.server.SimpleHTTPRequestHandler):
            def log_message(self,*args): pass
            def copyfile(self,source,output):
                try:
                    while data:=source.read(32768):
                        output.write(data);output.flush();time.sleep(.01)
                except (BrokenPipeError,ConnectionResetError): pass
            # zsync2 uses byte ranges. SimpleHTTPRequestHandler ignores them.
            def send_head(self):
                path=Path(self.translate_path(self.path))
                if 'Range' not in self.headers: return super().send_head()
                if not path.is_file(): self.send_error(404); return None
                value=self.headers['Range']
                if not value.startswith('bytes=') or ',' in value:
                    self.send_error(416); return None
                start,end=value[6:].split('-');start=int(start);end=int(end) if end else path.stat().st_size-1
                size=path.stat().st_size;end=min(end,size-1)
                if start>=size: self.send_error(416); return None
                self.send_response(206)
                self.send_header('Content-Length',str(end-start+1))
                self.send_header('Content-Range',f'bytes {start}-{end}/{size}')
                self.end_headers()
                import io
                with path.open('rb') as source: source.seek(start); return io.BytesIO(source.read(end-start+1))
        server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(serve)))
        threading.Thread(target=server.serve_forever,daemon=True).start()
        base=f'http://127.0.0.1:{server.server_port}'
        tools=root/'tools';tools.mkdir()
        run([a.tool,'--appimage-extract'],cwd=tools)
        zsyncmake=tools/'squashfs-root/usr/bin/zsyncmake'
        import importlib.util
        spec=importlib.util.spec_from_file_location('sign_linux',ROOT/'scripts/release/sign-linux.py')
        signing=importlib.util.module_from_spec(spec);spec.loader.exec_module(signing)
        for image in serve.glob('*.AppImage'):
            url=base+'/'+image.name
            run([zsyncmake,'-u',url,'-o',str(image)+'.zsync',image],cwd=serve)
            signing.validate_zsync(image,Path(str(image)+'.zsync'),url)
        profile=root/'profile';profile.mkdir()
        (profile/'personal.frag').write_text('personal shader')
        (profile/'project.xml').write_text('<test>keep</test>')
        saved={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in profile.iterdir()}
        def worker_case(old,feed,folder,expected):
            reader,writer=os.pipe()
            process=subprocess.Popen(['/bin/sh','-c','exec 3>&"$1"; shift; exec "$@"','sh',str(writer),
                str(a.sdk/'bin/guipper-update-worker'),str(old),feed,str(folder)],
                env=env,stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,
                pass_fds=(writer,),start_new_session=True,text=True)
            os.close(writer)
            buffered=b''
            def event():
                nonlocal buffered
                deadline=time.monotonic()+60
                while b'\n' not in buffered:
                    assert time.monotonic()<deadline,'Worker response timed out'
                    if select.select([reader],[],[],1)[0]:
                        chunk=os.read(reader,4096)
                        assert chunk,'Worker exited without a result'
                        buffered+=chunk
                line,buffered=buffered.split(b'\n',1)
                message=line.decode()
                with (root/'test.log').open('a') as log: log.write(message+'\n')
                return message
            try:
                first=event()
                if expected=='idle':
                    assert first=='GUIPPER_UPDATE IDLE';return None
                if expected=='offline':
                    assert first.startswith('GUIPPER_UPDATE ERROR ');return None
                assert first.startswith('GUIPPER_UPDATE AVAILABLE '),first
                process.stdin.write('DOWNLOAD\n');process.stdin.flush()
                if expected=='cancel':
                    while True:
                        progress=event()
                        assert 'READY' not in progress and 'ERROR' not in progress,progress
                        if progress.startswith('GUIPPER_UPDATE PROGRESS ') and 0<float(progress.split()[-1])<1: break
                    started=time.monotonic();os.killpg(process.pid,signal.SIGTERM);process.wait(timeout=3)
                    assert time.monotonic()-started<3;return None
                while True:
                    message=event()
                    if message.startswith('GUIPPER_UPDATE ERROR '):
                        assert expected=='invalid',message;return None
                    if message=='GUIPPER_UPDATE READY':
                        assert expected=='valid',message
                        process.stdin.write('INSTALL\n');process.stdin.flush()
                        result=event();assert result.startswith('GUIPPER_UPDATE INSTALL '),result
                        return Path(result[len('GUIPPER_UPDATE INSTALL '):])
            finally:
                if process.poll() is None: os.killpg(process.pid,signal.SIGTERM)
                process.wait(timeout=5);process.stdin.close();os.close(reader)
        for name,expect in [('B','valid'),('wrong-key','invalid'),('unsigned','invalid'),('tampered','invalid'),('crash','valid')]:
            case=root/name;case.mkdir();old=case/'Guipper.AppImage';shutil.copy2(serve/'A.AppImage',old)
            candidate=worker_case(old,f'zsync|{base}/{name}.AppImage.zsync',case,expect)
            if expect=='valid':
                health=case/'healthy'
                original=old.read_bytes()
                env['GUIPPER_USER_ROOT']=str(profile)
                run(['/bin/sh',ROOT/'release/install-appimage.sh',old,candidate,'999999999',health],cwd=case)
                if name=='B':
                    assert health.exists() and old.read_bytes()==(serve/'B.AppImage').read_bytes()
                    assert Path(str(old)+'.previous').read_bytes()==original
                else:
                    assert old.read_bytes()==original and Path(str(old)+'.failed').exists()
            else:
                assert old.read_bytes()==(serve/'A.AppImage').read_bytes()
            results[name]='passed'
        case=root/'cancel';case.mkdir();old=case/'Guipper.AppImage';shutil.copy2(serve/'A.AppImage',old)
        worker_case(old,f'zsync|{base}/B.AppImage.zsync',case,'cancel')
        assert old.read_bytes()==(serve/'A.AppImage').read_bytes();results['cancel']='passed'
        offline=root/'offline';offline.mkdir()
        worker_case(old,'zsync|http://127.0.0.1:1/missing.zsync',offline,'offline');results['offline']='passed'
        idle=root/'idle';idle.mkdir()
        worker_case(root/'B/Guipper.AppImage',f'zsync|{base}/B.AppImage.zsync',idle,'idle');results['renamed-current-version']='passed'
        assert saved=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in profile.iterdir()}
        results['personal-data']='passed'
        (root/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        print(json.dumps(results,indent=2))
    finally:
        if server: server.shutdown();server.server_close()
        subprocess.run(['gpgconf','--homedir',str(root/'keys'),'--kill','all'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        shutil.rmtree(root/'keys')
        print('Disposable private keys removed; evidence retained in '+str(root),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    for name in ['sdk','tool','runtime']: p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args()
    for name in ['sdk','tool','runtime']: setattr(args,name,getattr(args,name).resolve())
    main(args)
