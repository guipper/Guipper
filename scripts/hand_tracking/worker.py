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
    body = len(sys.argv) > 2 and sys.argv[2] == "body"
    options = vision.PoseLandmarkerOptions(
        base_options=mp.tasks.BaseOptions(model_asset_path=sys.argv[1]),
        running_mode=vision.RunningMode.VIDEO, num_poses=1,
        min_pose_detection_confidence=0.6, min_pose_presence_confidence=0.6,
        min_tracking_confidence=0.5, output_segmentation_masks=False) if body else vision.HandLandmarkerOptions(
        base_options=mp.tasks.BaseOptions(model_asset_path=sys.argv[1]),
        running_mode=vision.RunningMode.VIDEO, num_hands=2,
        min_hand_detection_confidence=0.6, min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.5)
    implementation = vision.PoseLandmarker if body else vision.HandLandmarker
    with implementation.create_from_options(options) as detector:
        emit({"ready": 1, "protocol": 2, "detector": "body" if body else "hands"})
        previous = 0
        while True:
            try:
                size, width, height, pixel_format, stamp, serial = struct.unpack("<IIIIQQ", read_exact(32))
                if not (0 < width <= 4096 and 0 < height <= 4096) or pixel_format != 1 or size != width * height * 3:
                    raise ValueError("invalid RGB frame size")
                rgb = np.frombuffer(read_exact(size), dtype=np.uint8).reshape(height, width, 3)
            except EOFError:
                return
            stamp = max(stamp, previous + 1)
            previous = stamp
            start = time.monotonic()
            result = detector.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb), stamp)
            if body:
                points = result.pose_landmarks[0] if result.pose_landmarks else []
                emit({"serial": serial, "body": [[p.x, p.y, p.visibility or 0, p.presence or 0] for p in points],
                      "ms": (time.monotonic() - start) * 1000})
                continue
            hands = []
            for points, side in zip(result.hand_landmarks, result.handedness):
                category = side[0]
                # MediaPipe's handedness assumes mirrored/selfie input. Kinect
                # RGB is unmirrored: swap only the label to report the person's
                # anatomical hand. Keep image/landmark coordinates unchanged.
                hands.append({"right": category.category_name == "Left", "score": category.score,
                              "points": [[p.x, p.y] for p in points]})
            emit({"serial": serial, "hands": hands, "ms": (time.monotonic() - start) * 1000})


if __name__ == "__main__":
    main()
