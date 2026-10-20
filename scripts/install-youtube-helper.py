#!/usr/bin/env python3
"""Install/update Guipper's private official yt-dlp executable, without replacing the system tool."""
import hashlib
import os
from pathlib import Path
import platform
import tempfile
import urllib.request

if platform.system() == "Windows":
    root = Path(os.environ["LOCALAPPDATA"]) / "Guipper" / "cache"
    asset, name = "yt-dlp.exe", "yt-dlp.exe"
else:
    root = Path(os.environ.get("XDG_CACHE_HOME", str(Path.home() / ".cache"))) / "guipper"
    # Official zipimport build includes EJS, uses the existing Python 3 runtime.
    asset, name = "yt-dlp", "yt-dlp"
if os.environ.get("GUIPPER_USER_ROOT"):
    root = Path(os.environ["GUIPPER_USER_ROOT"]) / "cache"
tools = root / "tools"
tools.mkdir(parents=True, exist_ok=True)
base = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/"
with urllib.request.urlopen(base + "SHA2-256SUMS", timeout=30) as response:
    hashes = response.read().decode("ascii")
expected = next(line.split()[0] for line in hashes.splitlines() if line.split()[-1].lstrip("*") == asset)
with urllib.request.urlopen(base + asset, timeout=60) as response:
    payload = response.read()
if hashlib.sha256(payload).hexdigest() != expected:
    raise SystemExit("Checksum mismatch; the existing helper was not changed. Retry the installation.")
with tempfile.NamedTemporaryFile(dir=tools, delete=False) as temporary:
    temporary.write(payload)
    temporary_path = Path(temporary.name)
try:
    temporary_path.chmod(0o755)
    temporary_path.replace(tools / name)
finally:
    temporary_path.unlink(missing_ok=True)
print("Installed Guipper YouTube helper:", tools / name)
print("Also required: ffmpeg and a supported JavaScript runtime (Node.js or Deno) on PATH.")
