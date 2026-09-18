#!/usr/bin/env python3
"""Isolated CPU/CUDA reference benchmark; never opens a camera or imports Guipper."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--device', choices=['cpu', 'cuda'], default='cpu')
    parser.add_argument('--model', choices=['da2', 'vda'], required=True)
    parser.add_argument('--root', type=Path, default=Path('dist/depth-research'))
    parser.add_argument('--video', type=Path)
    parser.add_argument('--size', type=int, default=252)
    parser.add_argument('--frames', type=int, default=64)
    parser.add_argument('--warmup', type=int, default=5)
    parser.add_argument('--threads', type=int, default=6)
    parser.add_argument('--static', action='store_true', help='Repeat the first frame; diagnostic, not a real scene stability score')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if args.frames < args.warmup + 2 or args.warmup < 0 or args.size < 28 or args.threads < 1:
        parser.error('Require frames >= warmup + 2 and warmup >= 0, size >= 28, threads >= 1')

    import cv2
    import numpy as np
    import torch
    if args.device == 'cuda' and not torch.cuda.is_available():
        parser.error('CUDA requested but unavailable; refusing silent CPU fallback')
    torch.backends.cuda.matmul.allow_tf32 = False
    torch.backends.cudnn.allow_tf32 = False
    def synchronize():
        if args.device == 'cuda':
            torch.cuda.synchronize()
    torch.set_num_threads(args.threads)
    torch.set_num_interop_threads(1)
    cv2.setNumThreads(1)
    root = args.root.resolve()
    repo = root / ('Depth-Anything-V2' if args.model == 'da2' else 'Video-Depth-Anything')
    checkpoint = root / 'models' / ('depth_anything_v2_vits.pth' if args.model == 'da2' else 'video_depth_anything_vits.pth')
    video = args.video or root / 'Video-Depth-Anything/assets/example_videos/davis_rollercoaster.mp4'
    sys.path.insert(0, str(repo))
    start = time.perf_counter()
    if args.model == 'da2':
        from depth_anything_v2.dpt import DepthAnythingV2
        model = DepthAnythingV2(encoder='vits', features=64, out_channels=[48, 96, 192, 384])
    else:
        from video_depth_anything.video_depth_stream import VideoDepthAnything
        model = VideoDepthAnything(encoder='vits', features=64, out_channels=[48, 96, 192, 384])
    model.load_state_dict(torch.load(checkpoint, map_location='cpu', weights_only=True), strict=True)
    model.to(args.device).eval()
    synchronize()
    if args.device == 'cuda':
        torch.cuda.reset_peak_memory_stats()
    load_ms = (time.perf_counter() - start) * 1000
    cap = cv2.VideoCapture(str(video))
    if not cap.isOpened():
        raise RuntimeError(f'Cannot open {video}')
    fps = cap.get(cv2.CAP_PROP_FPS)
    frames, depths, timings, input_shapes = [], [], [], []
    # get_intermediate_layers bypasses Module.forward; observe patch embedding.
    # This records actual input shape without altering model preprocessing.
    hook = model.pretrained.patch_embed.register_forward_pre_hook(lambda module, values: input_shapes.append(list(values[0].shape)))
    try:
        with torch.inference_mode():
            first = None
            for i in range(args.frames):
                if args.static and first is not None:
                    frame = first.copy()
                else:
                    ok, frame = cap.read()
                    if not ok:
                        break
                    h, w = frame.shape[:2]
                    frame = cv2.resize(frame, (640, round(h * 640 / w)), interpolation=cv2.INTER_AREA)
                    first = frame.copy()
                synchronize()
                started = time.perf_counter()
                if args.model == 'da2':
                    depth = model.infer_image(frame, input_size=args.size)
                else:
                    depth = model.infer_video_depth_one(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB), input_size=args.size, device=args.device, fp32=True)
                synchronize()
                elapsed = (time.perf_counter() - started) * 1000
                depth = np.asarray(depth, dtype=np.float32)
                if depth.shape != frame.shape[:2] or not np.isfinite(depth).all():
                    raise RuntimeError(f'Invalid depth at frame {i}: {depth.shape}')
                frames.append(frame)
                depths.append(depth)
                timings.append(elapsed)
                if i % 8 == 0:
                    print(f'{args.model} frame={i} inference_ms={elapsed:.1f}', flush=True)
    finally:
        cap.release()
        hook.remove()
    if len(timings) < args.warmup + 2:
        raise RuntimeError('Clip too short to measure after warmup')
    args.out.mkdir(parents=True, exist_ok=False)
    depth = np.stack(depths)
    lo, hi = np.percentile(depth, [2, 98])
    normalized = np.clip((depth - lo) / max(float(hi - lo), 1e-6), 0, 1)
    values = np.asarray(timings[args.warmup:])
    report = {
        'model': args.model, 'backend': args.device.upper(), 'precision': 'float32', 'threads': args.threads,
        'gpu': torch.cuda.get_device_name() if args.device == 'cuda' else None,
        'gpu_peak_allocated_bytes': torch.cuda.max_memory_allocated() if args.device == 'cuda' else None,
        'gpu_peak_reserved_bytes': torch.cuda.max_memory_reserved() if args.device == 'cuda' else None,
        'python': platform.python_version(), 'torch': torch.__version__, 'opencv': cv2.__version__,
        'revision': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
        'weights_sha256': hashlib.sha256(checkpoint.read_bytes()).hexdigest(),
        'video': str(video), 'video_sha256': hashlib.sha256(video.read_bytes()).hexdigest(),
        'static_repeat': args.static, 'frames': len(timings), 'warmup': args.warmup,
        'source_fps': fps, 'frame_shape': list(frames[0].shape), 'requested_size': args.size,
        'network_shapes': sorted({tuple(s) for s in input_shapes}),
        'load_ms': load_ms, 'first_frame_ms': timings[0],
        'p50_ms': float(np.median(values)), 'p95_ms': float(np.percentile(values, 95)),
        'throughput_fps': float(1000 / values.mean()), 'per_frame_ms': timings,
        'display_range_p2_p98': [float(lo), float(hi)],
        'normalized_adjacent_mae': float(np.abs(np.diff(normalized[args.warmup:], axis=0)).mean()),
        'limitations': 'Inference wall time includes model preprocessing and output resize; excludes capture/decode/display. Adjacent difference on moving video is NOT flicker or accuracy. No concurrent Guipper load; CUDA wall time synchronized. GPU memory reports PyTorch allocator only.',
    }
    (args.out / 'report.json').write_text(json.dumps(report, indent=2))
    np.savez_compressed(args.out / 'depth.npz', depth=depth)
    h, w = frames[0].shape[:2]
    writer = cv2.VideoWriter(str(args.out / 'preview.avi'), cv2.VideoWriter_fourcc(*'MJPG'), fps if fps > 0 else 30, (w*2, h))
    if not writer.isOpened():
        raise RuntimeError('Cannot create comparison video')
    try:
        for i, frame in enumerate(frames):
            grey = cv2.cvtColor((normalized[i]*255).astype(np.uint8), cv2.COLOR_GRAY2BGR)
            pair = np.concatenate([frame, grey], axis=1)
            writer.write(pair)
            if i in {0, len(frames)//2, len(frames)-1}:
                cv2.imwrite(str(args.out / f'frame-{i:03d}.png'), pair)
    finally:
        writer.release()
    print(json.dumps({k: report[k] for k in ['model', 'p50_ms', 'p95_ms', 'throughput_fps', 'network_shapes']}, indent=2))


if __name__ == '__main__':
    main()
