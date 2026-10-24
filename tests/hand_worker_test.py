"""Model/protocol smoke test, no camera and no recorded images."""
import json
import os
from pathlib import Path
import struct
import subprocess
import time
runtime = Path(os.environ.get("GUIPPER_HAND_RUNTIME", Path.home()/".local/share/Guipper/hand-tracking"))
# Exercise real worker serialization with four distinct synthetic poses, not only
# blank model frames (which cannot reveal a stale two-pose truncation).
subprocess.run([str(runtime/"venv/bin/python"), "-c", """
import runpy, sys
from types import SimpleNamespace
worker = runpy.run_path(sys.argv[1])
poses = [[SimpleNamespace(x=i/10, y=.5, visibility=1, presence=1) for _ in range(33)] for i in range(5)]
out = worker['body_points'](poses)
assert len(out) == 4 and all(len(p) == 33 for p in out)
assert [p[0][0] for p in out] == [0, .1, .2, .3]
print('PASS: four distinct poses serialized, bounded to four')
""", str(runtime/"worker.py")], check=True)
for detector, model, result_key in [("hands", "hand_landmarker.task", "hands"), ("body", "pose_landmarker_lite.task", "bodies")]:
    p = subprocess.Popen([str(runtime/"venv/bin/python"), str(runtime/"worker.py"), str(runtime/model), detector], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        ready = json.loads(p.stdout.readline())
        assert ready["protocol"] == 2 and ready["detector"] == detector
        if detector == "body":
            assert ready["max_bodies"] == 4
        sizes = [(960,540),(640,480),(512,424),(960,540)]
        for index in range(8):
            width,height=sizes[index%len(sizes)]
            frame=bytes(width*height*3)
            p.stdin.write(struct.pack("<IIIIQQ", len(frame),width,height,1,int(time.monotonic()*1000)+index,index))
            p.stdin.write(frame)
            p.stdin.flush()
            result = json.loads(p.stdout.readline())
            assert result["serial"] == index
            assert result[result_key] == [] and result["ms"] >= 0
        p.stdin.close()
        assert p.wait(timeout=5) == 0
        print(f"PASS: {detector} pinned model, RGB formats, empty results, clean EOF")
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
