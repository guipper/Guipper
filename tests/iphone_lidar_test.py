"""iPhone LiDAR box end to end against a fake phone, in a disposable profile.

Starts tests/record3d_fake_device.py and points the build at it through
USBMUXD_SOCKET_ADDRESS, so it needs no iPhone, no usbmuxd daemon and no root.
The binary must be built with IPHONE_LIDAR (scripts/install_iphone_lidar_linux.sh).

    python3 tests/iphone_lidar_test.py [path/to/Guipper]
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "bin/Guipper"
work = Path(tempfile.mkdtemp(prefix="guipper-iphone-"))
print(work, flush=True)
shutil.copy2(binary, work / "Guipper")
shutil.copytree(root / "bin/data", work / "data")
socket_path = work / "usbmuxd.sock"
pause_file = work / "pause"
fake = subprocess.Popen([sys.executable, str(root / "tests/record3d_fake_device.py"),
                         str(socket_path), "--pause-file", str(pause_file)],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
try:
    print(fake.stdout.readline().strip(), flush=True)
    env = dict(os.environ, GUIPPER_USER_ROOT=str(work / "profile"),
               GUIPPER_PERSISTENCE_TEST="iphone",
               GUIPPER_IPHONE_PAUSE_FILE=str(pause_file),
               USBMUXD_SOCKET_ADDRESS="UNIX:" + str(socket_path),
               LD_LIBRARY_PATH=os.pathsep.join(filter(None, [
                   str(root.parents[2] / "libs/fmod/lib/linux64"),
                   os.environ.get("LD_LIBRARY_PATH")])))
    with (work / "iphone.log").open("w") as log:
        result = subprocess.run([str(work / "Guipper")], cwd=work, env=env,
                                stdout=log, stderr=subprocess.STDOUT, timeout=240)
finally:
    fake.kill()
output = (work / "iphone.log").read_text(errors="replace")
print("\n".join(line for line in output.splitlines() if "iphone" in line.lower()), flush=True)
if result.returncode or "iphone-test: PASS" not in output:
    raise SystemExit(f"FAIL; see {work}")
print("PASS: depth/color/confidence, 16 bit, PointerCloud, depth shaders, Publico, phone pose, persistence, delete/undo, pause.")
print("Screenshots:", work / "data")
