"""Summarize the isolated GUIPPER_TRACK_CROWD hardware benchmark (no images)."""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path


def summarize(path):
    with Path(path).open() as stream:
        rows = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(stream)]
    active = [r for r in rows if r['tracking'] == 1 and r['simulated'] == 0]
    baseline = [r for r in rows if r['tracking'] == 0]
    if not active:
        raise ValueError('No hay resultados reales de captura')

    def stats(values):
        values = sorted(v for v in values if math.isfinite(v))
        return {'median': statistics.median(values), 'p95': values[int((len(values)-1)*.95)], 'max': values[-1]} if values else None

    gaps = [b['elapsed']-a['elapsed'] for a, b in zip(active, active[1:])]
    return {
        'results': len(active), 'active_seconds_including_startup': active[-1]['elapsed'],
        'results_per_second_including_startup': len(active)/active[-1]['elapsed'],
        'valid_motion_results': sum(r['crowd_motion'] >= 0 for r in active),
        'valid_occupancy_results': sum(r['crowd_occupancy'] >= 0 for r in active),
        'gaps_over_250ms': sum(g > .25 for g in gaps), 'result_gap_ms': stats(g*1000 for g in gaps),
        'analysis_ms': stats(r['inference_ms'] for r in active),
        'age_ms': stats(r['age_ms'] for r in active),
        'coverage': stats(r['crowd_coverage'] for r in active),
        'render_fps_active': stats(r['render_fps'] for r in active),
        'render_fps_baseline': stats(r['render_fps'] for r in baseline),
        'consumer_skipped_frames': max(r['dropped'] for r in active),
        'notes': ['Age begins at decoded frame receipt, not sensor exposure.',
                  'Consumer skips do not measure USB packet loss.',
                  'Room endurance does not validate response to a real audience.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', type=Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.csv), indent=2, ensure_ascii=False))
