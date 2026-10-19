"""Integration test: needs a running PulseAudio/PipeWire-Pulse server and pactl/paplay.
Uses a temporary null sink; never changes the system default or plays to speakers.
Run: python3 tests/audio_loopback_linux_test.py
"""
import math
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import time
import wave

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="guipper-audio-test-") as directory:
    binary = str(Path(directory) / "probe")
    flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "libpulse"], text=True))
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    str(root / "tests/audio_loopback_linux_probe.cpp"),
                    str(root / "src/JPutils/jp_audio_loopback.cpp"), *flags, "-o", binary], check=True)
    name = "guipper_audio_validation_" + str(os.getpid())
    module = subprocess.check_output(["pactl", "load-module", "module-null-sink",
                                      "sink_name=" + name], text=True).strip()
    probe = None
    try:
        audio = Path(directory) / "tone.wav"
        with wave.open(str(audio), "wb") as stream:
            stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
            stream.writeframes(b"".join(struct.pack("<hh", *([int(6000 * math.sin(
                2 * math.pi * 440 * i / 48000))] * 2)) for i in range(96000)))
        probe = subprocess.Popen([binary, name])
        time.sleep(.4)
        subprocess.run(["paplay", "--device=" + name, str(audio)], check=True, timeout=10)
        assert probe.wait(timeout=12) == 0, "Audio frames or level missing"
        probe = subprocess.Popen([binary, name, "expect-disconnect"])
        time.sleep(1)
        subprocess.run(["pactl", "unload-module", module], check=True)
        module = None
        assert probe.wait(timeout=12) == 0, "Disconnect not reported"
        assert subprocess.run([binary, name], timeout=12).returncode == 1, "Missing output opened"
        print("PASS: output frames, signal level, disconnect and missing device")
    finally:
        if probe and probe.poll() is None:
            probe.kill()
            probe.wait()
        if module:
            subprocess.run(["pactl", "unload-module", module], check=True)
