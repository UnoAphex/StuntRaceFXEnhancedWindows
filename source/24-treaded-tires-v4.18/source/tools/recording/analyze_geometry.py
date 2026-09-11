#!/usr/bin/env python3
"""Summarize material and shape evidence from extracted SRFX recording frames.

Run inspect_recording.py with --frame first, then pass one or more extraction
directories to this tool.  It consumes only the recorded camera-face and color
channels and never exports game assets.
"""
from __future__ import annotations
import argparse
import struct
from collections import Counter, defaultdict
from pathlib import Path

FACE = struct.Struct("<HHhh96f64f8H")


def classify(color: int) -> str:
    r, g, b = (color >> 16) & 255, (color >> 8) & 255, color & 255
    brightest, darkest = max(r, g, b), min(r, g, b)
    luma = (r * 54 + g * 183 + b * 19) >> 8
    chroma = brightest - darkest
    if g >= 55 and g >= r + 18 and g >= b + 14:
        return "grass"
    if b >= 70 and b >= r + 28 and b >= g + 10:
        return "water"
    if r >= g + 7 and g >= b - 8 and r >= b + 18:
        return "sand" if luma >= 150 else ("mud" if luma < 72 else "dirt")
    if chroma <= 38:
        if luma >= 198 and b >= r + 6 and b >= g + 2:
            return "snow"
        if luma >= 198:
            return "none"
        if luma >= 164:
            return "stone"
        if luma >= 120:
            return "gravel"
        if luma >= 42:
            return "asphalt"
    return "none"


def projected(face: tuple) -> tuple[list[tuple[float, float]], float]:
    count, _, cx, cy = face[:4]
    xyz = face[4:100]
    points = []
    depth = 0.0
    for index in range(count):
        x, y, z = xyz[index * 3:index * 3 + 3]
        depth += z
        if z >= 1.0:
            points.append((cx + 128.0 * x / z, cy + 128.0 * y / z))
    return points, depth / max(1, count)


def area(points: list[tuple[float, float]]) -> float:
    return abs(sum(points[i][0] * points[(i + 1) % len(points)][1] -
                   points[(i + 1) % len(points)][0] * points[i][1]
                   for i in range(len(points)))) * 0.5 if len(points) >= 3 else 0.0


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path, nargs="+")
    args = parser.parse_args()
    totals = Counter()
    colors = defaultdict(Counter)
    shape = defaultdict(lambda: [0, 0.0, 0.0, 0.0, 1.0e30, -1.0e30])
    for directory in args.frames:
        palette = struct.unpack("<256I", (directory / "COLR.bin").read_bytes())
        data = (directory / "DISP.bin").read_bytes()
        if len(data) % FACE.size:
            raise SystemExit(f"invalid DISP length in {directory}")
        local = Counter()
        for offset in range(0, len(data), FACE.size):
            face = FACE.unpack_from(data, offset)
            count, color_index = face[:2]
            textured = bool(face[-2])
            points, depth = projected(face)
            coverage = area(points)
            material = "original-texture" if textured else classify(palette[color_index & 255])
            local[material] += 1
            totals[material] += 1
            colors[material][(color_index & 255, palette[color_index & 255] & 0xFFFFFF)] += 1
            if not textured and points:
                key = (color_index & 255, palette[color_index & 255] & 0xFFFFFF)
                center_y = sum(point[1] for point in points) / len(points)
                item = shape[key]
                item[0] += 1
                item[1] += coverage
                item[2] += depth
                item[3] += center_y
                item[4] = min(item[4], center_y)
                item[5] = max(item[5], center_y)
        print(f"{directory.name}: faces={sum(local.values())} " +
              " ".join(f"{key}={value}" for key, value in local.most_common()))
    print("total: " + " ".join(f"{key}={value}" for key, value in totals.most_common()))
    for material, values in sorted(colors.items()):
        if material == "original-texture":
            continue
        print(material + ": " + " ".join(
            f"index={index}:rgb={rgb:06X}:faces={count}"
            for (index, rgb), count in values.most_common(12)))
    print("shape evidence for recurring flat colors:")
    for (index, rgb), item in sorted(shape.items(), key=lambda pair: -pair[1][0]):
        count = item[0]
        if count < 3:
            continue
        print(f"index={index}:rgb={rgb:06X}:faces={count}:area={item[1]/count:.1f}:"
              f"depth={item[2]/count:.1f}:cy={item[3]/count:.1f}:"
              f"cy_range={item[4]:.1f}..{item[5]:.1f}")


if __name__ == "__main__":
    main()
