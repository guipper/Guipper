"""Run particle render/persistence checks in a disposable profile. --live also uses Kinect."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix="guipper-particles-"))
print(work, flush=True)
shutil.copy2(root / "bin/Guipper", work / "Guipper")
shutil.copytree(root / "bin/data", work / "data")
env = dict(os.environ, GUIPPER_USER_ROOT=str(work / "profile"),
           LD_LIBRARY_PATH=str(root.parents[2] / "libs/fmod/lib/linux64"),
           __NV_PRIME_RENDER_OFFLOAD="1", __GLX_VENDOR_LIBRARY_NAME="nvidia")
modes = ["particles"] + (["particles-live"] if "--live" in sys.argv else [])
for mode in modes:
    with (work / (mode + ".log")).open("w") as log:
        result = subprocess.run([str(work / "Guipper")], cwd=work,
                                env=dict(env, GUIPPER_PERSISTENCE_TEST=mode),
                                stdout=log, stderr=subprocess.STDOUT, timeout=120)
    output = (work / (mode + ".log")).read_text(errors="replace")
    print(output[-2500:], flush=True)
    if result.returncode or "particles-test: PASS" not in output:
        raise SystemExit(f"FAIL: {mode}; see {work}")
print("PASS: particles render, downstream shader, resize, persistence, duplicate, delete/undo/redo.")
print("Screenshots:", work / "data")
