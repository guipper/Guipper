#!/usr/bin/env python3
"""Fake usbmuxd + Record3D phone, for exercising the iPhone LiDAR box without
an iPhone.

Serves the usbmuxd plist protocol on a Unix socket with one USB device; a
Connect to port 1337 turns the socket into a Record3D stream of synthetic
LiDAR frames (a person-sized blob walking in front of a slanted wall).

    python3 tests/record3d_fake_device.py /tmp/fake-usbmuxd.sock &
    USBMUXD_SOCKET_ADDRESS=UNIX:/tmp/fake-usbmuxd.sock bin/Guipper

Needs liblzfse (liblzfse1) and Pillow. Options:
    --pause-after S   stop sending after S seconds, keeping the socket open
                      (Record3D with recording paused)
    --pause-file P    same, while the file P exists
    --drop-after S    close the stream after S seconds (cable pulled)
    --frames N        exit after N frames
"""

import argparse
import ctypes
import ctypes.util
import io
import math
import os
import plistlib
import socket
import struct
import sys
import threading
import time

from PIL import Image

DEPTH_W, DEPTH_H = 256, 192
RGB_W, RGB_H = 960, 720
PORT = 1337

lzfse = ctypes.CDLL(ctypes.util.find_library("lzfse") or "liblzfse.so.1")
lzfse.lzfse_encode_buffer.restype = ctypes.c_size_t
lzfse.lzfse_encode_buffer.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                      ctypes.c_void_p, ctypes.c_size_t,
                                      ctypes.c_void_p]


def compress(data):
    out = ctypes.create_string_buffer(len(data) + 4096)
    size = lzfse.lzfse_encode_buffer(out, len(out), data, len(data), None)
    if size == 0:
        raise RuntimeError("lzfse_encode_buffer failed")
    return out.raw[:size]


def frame(t):
    centre = DEPTH_W * (0.5 + 0.3 * math.sin(t))
    depth = bytearray()
    confidence = bytearray()
    for y in range(DEPTH_H):
        for x in range(DEPTH_W):
            person = abs(x - centre) < 30 and y > 40
            # A rounded body in front of a slanted wall: real gradients for
            # effects that follow the surface, not two flat planes.
            if person:
                metres = 2.0 + 0.2 * math.sin(t * 2) + 0.15 * ((x - centre) / 30) ** 2
            else:
                metres = 3.5 + x / DEPTH_W
            depth += struct.pack("<f", metres)
            # A low-confidence rim, like LiDAR edges: must be masked out.
            edge = person and abs(abs(x - centre) - 30) < 2
            confidence.append(0 if edge else 2)
    image = Image.new("RGB", (RGB_W, RGB_H), (40, 40, 60))
    image.paste((220, 120, 40), (int(centre * RGB_W / DEPTH_W) - 110, 150,
                                  int(centre * RGB_W / DEPTH_W) + 110, RGB_H))
    jpeg = io.BytesIO()
    image.save(jpeg, "JPEG", quality=70)
    rgb = jpeg.getvalue()
    depth_c = compress(bytes(depth))
    conf_c = compress(bytes(confidence))
    header = struct.pack("<11I", RGB_W, RGB_H, DEPTH_W, DEPTH_H, DEPTH_W,
                         DEPTH_H, len(rgb), len(depth_c), len(conf_c), 0, 1)
    intrinsics = struct.pack("<4f", 700.0, 700.0, 480.0, 360.0)
    pose = struct.pack("<7f", 0, 0, 0, 1, 0, 0, 0)
    body = header + intrinsics + pose + rgb + depth_c + conf_c
    return struct.pack(">4I", 1, 101, 0, len(body)) + body


def stream(conn, args):
    started = time.time()
    sent = 0
    while True:
        elapsed = time.time() - started
        if args.drop_after and elapsed > args.drop_after:
            print("fake: dropping stream", flush=True)
            return
        if ((args.pause_after and elapsed > args.pause_after) or
                (args.pause_file and os.path.exists(args.pause_file))):
            time.sleep(0.1)
            continue
        conn.sendall(frame(elapsed))
        sent += 1
        if args.frames and sent >= args.frames:
            print(f"fake: sent {sent} frames", flush=True)
            time.sleep(1)
            os._exit(0)
        time.sleep(1 / 30)


def recv_exact(conn, size):
    data = b""
    while len(data) < size:
        chunk = conn.recv(size - len(data))
        if not chunk:
            raise ConnectionError
        data += chunk
    return data


def reply(conn, tag, payload):
    body = plistlib.dumps(payload)
    conn.sendall(struct.pack("<4I", 16 + len(body), 1, 8, tag) + body)


def serve(conn, args):
    try:
        while True:
            length, _version, _kind, tag = struct.unpack(
                "<4I", recv_exact(conn, 16))
            request = plistlib.loads(recv_exact(conn, length - 16))
            message = request.get("MessageType")
            if message == "ListDevices":
                reply(conn, tag, {"DeviceList": [{
                    "DeviceID": 1, "MessageType": "Attached",
                    "Properties": {"ConnectionType": "USB", "DeviceID": 1,
                                   "LocationID": 0, "ProductID": 4776,
                                   "SerialNumber": "00008130-FAKE"}}]})
            elif message == "Connect":
                port = socket.ntohs(request["PortNumber"])
                if port != PORT:
                    reply(conn, tag, {"MessageType": "Result", "Number": 3})
                    continue
                reply(conn, tag, {"MessageType": "Result", "Number": 0})
                print("fake: Record3D stream connected", flush=True)
                stream(conn, args)
                return
            else:
                reply(conn, tag, {"MessageType": "Result", "Number": 2})
    except (ConnectionError, BrokenPipeError, ConnectionResetError):
        pass
    finally:
        conn.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("socket")
    parser.add_argument("--pause-after", type=float, default=0)
    parser.add_argument("--pause-file", default="")
    parser.add_argument("--drop-after", type=float, default=0)
    parser.add_argument("--frames", type=int, default=0)
    args = parser.parse_args()
    if os.path.exists(args.socket):
        os.unlink(args.socket)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(args.socket)
    server.listen()
    print(f"fake usbmuxd on {args.socket}", flush=True)
    while True:
        conn, _ = server.accept()
        threading.Thread(target=serve, args=(conn, args), daemon=True).start()


if __name__ == "__main__":
    sys.exit(main())
