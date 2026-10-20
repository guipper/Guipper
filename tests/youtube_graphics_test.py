"""Run against bin/Guipper in an isolated data/profile copy. Requires Linux GL and ffmpeg.
By default generates a moving pattern offline; --stream/--download opt into network tests.
Does not write the system profile.
Optionally use --video FILE --url URL to validate an already prepared real video.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--video", type=Path)
parser.add_argument("--stream", action="store_true", help="Test streaming then optional cache using --url (network)")
parser.add_argument("--download", action="store_true", help="Explicitly test a network download of --url")
parser.add_argument("--url", default="https://www.youtube.com/watch?v=BaW_jenozKc")
options = parser.parse_args()
root = Path(__file__).resolve().parents[1]
destination = Path(tempfile.mkdtemp(prefix="guipper-youtube-graphics-"))
print(destination, flush=True)
shutil.copy2(root / "bin/Guipper", destination / "Guipper")
shutil.copytree(root / "bin/data", destination / "data")
env = dict(os.environ, GUIPPER_USER_ROOT=str(destination / "profile"),
           LD_LIBRARY_PATH=str(root.parents[2] / "libs/fmod/lib/linux64"),
           __NV_PRIME_RENDER_OFFLOAD="1", __GLX_VENDOR_LIBRARY_NAME="nvidia")

def run(name, extra):
    with (destination / (name + ".log")).open("w") as log:
        result = subprocess.run([str(destination / "Guipper")], cwd=destination,
                                env=dict(env, **extra), stdout=log, stderr=subprocess.STDOUT, timeout=180)
    text = (destination / (name + ".log")).read_text()
    print(text[-1800:], flush=True)
    if result.returncode or ("youtube-test: PASS" if name == "persistence" else "youtube-ui: PASS") not in text:
        raise SystemExit(f"{name} failed ({result.returncode}); see {destination}")

run("persistence", {"GUIPPER_PERSISTENCE_TEST": "youtube"})
cache = destination / "profile/cache/youtube"
cache.mkdir(parents=True, exist_ok=True)
if options.download or options.stream:
    env["GUIPPER_YTDLP"] = str(Path(os.environ.get("XDG_CACHE_HOME", str(Path.home() / ".cache"))) / "guipper/tools/yt-dlp")
elif options.video:
    from urllib.parse import urlparse, parse_qs
    video_id = parse_qs(urlparse(options.url).query)["v"][0]
    shutil.copy2(options.video, cache / (video_id + "-1080p-v1.mp4"))
else:
    subprocess.run(["ffmpeg", "-loglevel", "error", "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30",
                    "-t", "4", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                    str(cache / "BaW_jenozKc-1080p-v1.mp4")], check=True)
extra = {"GUIPPER_UISHOT": "youtube", "GUIPPER_YOUTUBE_TEST_URL": options.url}
if options.stream: extra["GUIPPER_YOUTUBE_STREAM"] = "1"
run("ui", extra)
print("PASS: isolated persistence, " + ("streaming/cache switch and playhead, " if options.stream else "cached playback, ") + "shader rendering. Screenshots in", destination, flush=True)
