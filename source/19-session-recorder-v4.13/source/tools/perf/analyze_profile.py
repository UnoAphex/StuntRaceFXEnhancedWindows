#!/usr/bin/env python3
"""Summarize a Stunt Race FX compatibility-profiler CSV."""

import argparse
import csv
import math
from pathlib import Path


BRIDGE_PHASES = (
    ("approach", 3060, 3179),
    ("bridge/descent", 3180, 3262),
    ("tunnel entry", 3263, 3297),
    ("under bridge", 3298, 3329),
    ("bridge exit", 3330, 3360),
)


def percentile(values, fraction):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, math.ceil(len(ordered) * fraction) - 1))
    return ordered[index]


def mean(values):
    return sum(values) / len(values) if values else 0.0


def summarize(rows, label):
    processing = [float(row["processing_ms"]) for row in rows]
    total = [float(row["total_ms"]) for row in rows]
    gsu = [float(row["gsu_ms"]) for row in rows]
    broad = [int(row["frame"]) for row in rows if int(row["broad_update"])]
    gaps = [right - left for left, right in zip(broad, broad[1:])]
    output_hz = 60.0988
    one_percent_ms = percentile(total, 0.99)
    one_percent_low = 1000.0 / one_percent_ms if one_percent_ms else 0.0
    return {
        "section": label,
        "frames": len(rows),
        "avg_ms": mean(total),
        "p99_ms": one_percent_ms,
        "one_percent_low": one_percent_low,
        "worst_ms": max(total, default=0.0),
        "processing_ms": mean(processing),
        "gsu_ms": mean(gsu),
        "broad_hz": output_hz * len(broad) / len(rows) if rows else 0.0,
        "max_broad_gap": max(gaps, default=0),
        "allocations": sum(int(row["allocations"]) for row in rows),
    }


def print_summary(item):
    print(
        f'{item["section"]:16} frames={item["frames"]:4d} '
        f'avg={item["avg_ms"]:7.3f} ms  p99={item["p99_ms"]:7.3f} ms  '
        f'1%low={item["one_percent_low"]:7.2f}  worst={item["worst_ms"]:7.3f} ms  '
        f'CPU={item["processing_ms"]:6.3f}  GSU={item["gsu_ms"]:6.3f}  '
        f'3D={item["broad_hz"]:5.1f} Hz  max-gap={item["max_broad_gap"]:2d}  '
        f'alloc={item["allocations"]}'
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile", type=Path)
    parser.add_argument("--spikes", type=float, default=20.0,
                        help="print frames at or above this processing time")
    args = parser.parse_args()
    with args.profile.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise SystemExit("profile contains no frames")

    print_summary(summarize(rows, "complete run"))
    for label, first, last in BRIDGE_PHASES:
        phase = [row for row in rows if first <= int(row["frame"]) <= last]
        if phase:
            print_summary(summarize(phase, label))

    spikes = [row for row in rows if float(row["processing_ms"]) >= args.spikes]
    if spikes:
        print(f"\nProcessing spikes >= {args.spikes:.1f} ms:")
        for row in spikes:
            print(
                f'  frame {int(row["frame"]):6d}: {float(row["processing_ms"]):7.3f} ms '
                f'(core {float(row["core_ms"]):.3f}, GSU {float(row["gsu_ms"]):.3f}, '
                f'video {float(row["video_ms"]):.3f}, present {float(row["present_ms"]):.3f})'
            )
    else:
        print(f"\nNo processing frames reached {args.spikes:.1f} ms.")


if __name__ == "__main__":
    main()
