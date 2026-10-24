"""Compile/render five audience effects in an isolated Guipper profile."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix="guipper-audience-shaders-"))
print(work, flush=True)
shutil.copy2(root / "bin/Guipper", work / "Guipper")
shutil.copytree(root / "bin/data", work / "data")
env = dict(os.environ, GUIPPER_USER_ROOT=str(work / "profile"),
           LD_LIBRARY_PATH=str(root.parents[2] / "libs/fmod/lib/linux64"),
           __NV_PRIME_RENDER_OFFLOAD="1", __GLX_VENDOR_LIBRARY_NAME="nvidia")
modes = ["audience-shaders"]
for mode in modes:
    with (work / (mode + ".log")).open("w") as log:
        result = subprocess.run([str(work / "Guipper")], cwd=work,
                                env=dict(env, GUIPPER_PERSISTENCE_TEST=mode),
                                stdout=log, stderr=subprocess.STDOUT, timeout=120)
    output = (work / (mode + ".log")).read_text(errors="replace")
    print(output[-2500:], flush=True)
    if result.returncode or "audience-shaders: PASS" not in output:
        raise SystemExit(f"FAIL: {mode}; see {work}")
print("PASS: five shaders, landscape/portrait, neutral RGBA, extreme controls and visible effects.")
print("Screenshots:", work / "data")
