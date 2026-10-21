"""Isolated profile: real graph/codec/lifecycle tests and native UI captures."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys
root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix="guipper-hands-"))
print(work, flush=True)
shutil.copy2(root / "bin/Guipper", work / "Guipper")
shutil.copytree(root / "bin/data", work / "data")
env = dict(os.environ, GUIPPER_USER_ROOT=str(work / "profile"),
           LD_LIBRARY_PATH=str(root.parents[2] / "libs/fmod/lib/linux64"),
           __NV_PRIME_RENDER_OFFLOAD="1", __GLX_VENDOR_LIBRARY_NAME="nvidia")
cases = [("persistence", {"GUIPPER_PERSISTENCE_TEST": "hands"}, "hands-test: PASS"),
         ("ui", {"GUIPPER_UISHOT": "hands"}, "hands-ui: PASS")]
if "--persistence-only" in sys.argv:
    cases = cases[:1]
if "--real" in sys.argv:
    cases = [("real", {"GUIPPER_UISHOT": "hands-real"}, "hands-benchmark:")]
for name, mode, marker in cases:
    with (work / (name + ".log")).open("w") as log:
        result = subprocess.run([str(work / "Guipper")], cwd=work, env=dict(env, **mode),
                                stdout=log, stderr=subprocess.STDOUT, timeout=800 if name == "real" else 120)
    output = (work / (name + ".log")).read_text(errors="replace")
    print(output[-3000:], flush=True)
    if result.returncode or marker not in output:
        raise SystemExit(f"FAIL: {name}; see {work}")
print("PASS: harness completed; real performance/gesture acceptance requires reviewing CSV and user observations.", work)
