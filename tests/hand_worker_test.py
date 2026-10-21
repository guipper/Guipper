"""Model/protocol smoke test, no camera and no recorded images."""
import json
import os
from pathlib import Path
import struct
import subprocess
import time
runtime = Path(os.environ.get("GUIPPER_HAND_RUNTIME", Path.home()/".local/share/Guipper/hand-tracking"))
p = subprocess.Popen([str(runtime/"venv/bin/python"), str(runtime/"worker.py"), str(runtime/"hand_landmarker.task")], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
try:
    assert json.loads(p.stdout.readline())["protocol"] == 1
    frame = bytes(960*540*3)
    for index in range(8):
        p.stdin.write(struct.pack("<IQ", len(frame), int(time.monotonic()*1000)+index))
        p.stdin.write(frame)
        p.stdin.flush()
        result = json.loads(p.stdout.readline())
        assert result["hands"] == [] and result["ms"] >= 0
    p.stdin.close()
    assert p.wait(timeout=5) == 0
    print("PASS: pinned model, repeated RGB frames, empty results, clean EOF")
finally:
    if p.poll() is None:
        p.kill()
        p.wait()
