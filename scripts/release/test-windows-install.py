#!/usr/bin/env python3
"""Install the verified candidate in an isolated directory and preserve an A profile.

A is the previous executable supplied explicitly. The installer is executed
without elevation and with no icons.
"""
import argparse,ctypes,ctypes.wintypes,hashlib,json,os,pathlib,shutil,subprocess,time

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def launch(binary,profile,log):
    health=profile/'cache/startup-health';health.parent.mkdir(parents=True,exist_ok=True)
    if health.exists():health.unlink()
    env=dict(os.environ,GUIPPER_USER_ROOT=str(profile),GUIPPER_UPDATE_HEALTH_FILE=str(health))
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    with log.open('w') as output:
        proc=subprocess.Popen([str(binary)],cwd=binary.parent,env=env,stdout=output,stderr=subprocess.STDOUT,startupinfo=startup)
        try:
            deadline=time.monotonic()+40
            while not health.exists() and proc.poll() is None and time.monotonic()<deadline:time.sleep(.2)
            assert health.exists() and proc.poll() is None,'Application did not complete startup'
            time.sleep(3)
            assert proc.poll() is None,'Application exited after startup'
            callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.wintypes.HWND,ctypes.wintypes.LPARAM)
            @callback
            def close(hwnd,param):
                pid=ctypes.wintypes.DWORD();ctypes.windll.user32.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
                if pid.value==proc.pid:ctypes.windll.user32.PostMessageW(hwnd,0x0010,0,0)
                return True
            ctypes.windll.user32.EnumWindows(close,0)
            proc.wait(timeout=20)
            assert proc.returncode==0,f'Unclean exit: {proc.returncode}'
        finally:
            if proc.poll() is None:proc.terminate();proc.wait()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--previous-stage',type=pathlib.Path)
    for name in ['installer','previous','stage','output']:parser.add_argument('--'+name,type=pathlib.Path,required=True)
    args=parser.parse_args();root=args.output.resolve();root.mkdir(exist_ok=False)
    previous=root/'A';shutil.copytree(args.previous_stage or args.stage,previous);shutil.copy2(args.previous,previous/'Guipper.exe')
    profile=root/'profile';launch(previous/'Guipper.exe',profile,root/'A.log')
    # Personal shader, composition and preference must survive installer+startup.
    files={profile/'data/shaders/personal-upgrade-test.frag':b'// personal shader\nvoid main() {}\n',
           profile/'config/personal-upgrade-test.json':b'{"keep":true}\n',
           profile/'data/savefiles/personal-upgrade-test.xml':b'<guipper_format>1</guipper_format>\n<activerender>0</activerender>\n'}
    for file,data in files.items():file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data)
    before={str(p.relative_to(profile)):digest(p) for p in files}
    target=root/'B'
    run=subprocess.run([str(args.installer.resolve()),'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS',f'/DIR={target}',f'/LOG={root / "install.log"}'],timeout=90)
    assert run.returncode==0,run.returncode
    assert (target/'Guipper.exe').is_file()
    assert digest(target/'Guipper.exe')==digest(args.stage/'guipper.exe'),'Installed executable differs'
    assert (target/'VERSION').read_text().strip()==(args.stage/'VERSION').read_text().strip()
    launch(target/'Guipper.exe',profile,root/'B.log')
    after={str(p.relative_to(profile)):digest(p) for p in files}
    assert before==after,'Personal data changed'
    # The prior executable remains usable against the same profile.
    launch(previous/'Guipper.exe',profile,root/'A-after-B.log')
    assert before=={str(p.relative_to(profile)):digest(p) for p in files}
    report=dict(installed=True,healthy_start=True,personal_data_preserved=True,previous_version_still_runs=True,installer_sha256=digest(args.installer),files=before)
    (root/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0
if __name__=='__main__':raise SystemExit(main())
