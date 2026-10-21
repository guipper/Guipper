"""Private binary RGB protocol. stdout is reserved for newline-delimited results."""
import json
import struct
import sys
import time
import numpy as np
import mediapipe as mp
from mediapipe.tasks.python import vision


def read_exact(n):
    data = bytearray()
    while len(data) < n:
        chunk = sys.stdin.buffer.read(n - len(data))
        if not chunk:
            raise EOFError
        data.extend(chunk)
    return data


def emit(value):
    print(json.dumps(value, allow_nan=False, separators=(",", ":")), flush=True)


def main():
    options = vision.HandLandmarkerOptions(
        base_options=mp.tasks.BaseOptions(model_asset_path=sys.argv[1]),
        running_mode=vision.RunningMode.VIDEO, num_hands=2,
        min_hand_detection_confidence=0.6, min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.5)
    with vision.HandLandmarker.create_from_options(options) as detector:
        emit({"ready": 1, "protocol": 1})
        previous = 0
        while True:
            try:
                size, stamp = struct.unpack("<IQ", read_exact(12))
                if size != 960 * 540 * 3:
                    raise ValueError("invalid RGB frame size")
                rgb = np.frombuffer(read_exact(size), dtype=np.uint8).reshape(540, 960, 3)
            except EOFError:
                return
            stamp = max(stamp, previous + 1)
            previous = stamp
            start = time.monotonic()
            result = detector.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), stamp)
            hands = []
            for points, side in zip(result.hand_landmarks, result.handedness):
                category = side[0]
                hands.append({"right": category.category_name == "Right", "score": category.score,
                              "points": [[p.x, p.y] for p in points]})
            emit({"hands": hands, "ms": (time.monotonic() - start) * 1000})


if __name__ == "__main__":
    main()
