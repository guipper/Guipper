"""Validate TEXTO against the compiled Linux app in an isolated profile/data copy."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
work = Path(tempfile.mkdtemp(prefix="guipper-text-graphics-"))
print(work, flush=True)
shutil.copy2(root / "bin/Guipper", work / "Guipper")
shutil.copytree(root / "bin/data", work / "data")
env = dict(os.environ, GUIPPER_USER_ROOT=str(work / "profile"),
           LD_LIBRARY_PATH=str(root.parents[2] / "libs/fmod/lib/linux64"),
           __NV_PRIME_RENDER_OFFLOAD="1", __GLX_VENDOR_LIBRARY_NAME="nvidia")
for name, mode, marker in [("persistence", {"GUIPPER_PERSISTENCE_TEST": "text"}, "text-test: PASS"),
                           ("ui", {"GUIPPER_UISHOT": "text"}, "text-ui: PASS")]:
    with (work / (name + ".log")).open("w") as log:
        result = subprocess.run([str(work / "Guipper")], cwd=work, env=dict(env, **mode),
                                stdout=log, stderr=subprocess.STDOUT, timeout=120)
    output = (work / (name + ".log")).read_text(errors="replace")
    print(output[-2000:], flush=True)
    if result.returncode or marker not in output:
        raise SystemExit(f"FAIL: {name}; see {work}")
print("PASS: text persistence, duplication, delete/undo/redo, transparent output and shader. Screenshots:", work)
