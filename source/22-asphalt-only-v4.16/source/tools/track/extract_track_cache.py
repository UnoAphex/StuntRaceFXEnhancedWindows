#!/usr/bin/env python3
"""Inspect a Stunt Race FX course centerline and optionally export a road mesh.

This developer tool deliberately reads the user's ROM at run time.  Generated
JSON/OBJ files are research artifacts and must not be included in releases.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


def number(value: str) -> int:
    return int(value, 0)


def lorom_offset(bank: int, address: int, copier_header: int) -> int:
    if address < 0x8000:
        raise ValueError("course pointers must address the upper half of a LoROM bank")
    return copier_header + ((bank & 0x7F) * 0x8000) + (address & 0x7FFF)


def signed16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def decode(data: bytes, bank: int, address: int) -> dict:
    copier_header = 512 if len(data) % 0x8000 == 512 else 0
    offset = lorom_offset(bank, address, copier_header)
    count = struct.unpack_from("<H", data, offset)[0]
    if not 3 <= count <= 2048:
        raise ValueError(f"implausible course node count {count} at ${bank:02X}:{address:04X}")
    end = offset + 2 + count * 8
    if end > len(data):
        raise ValueError("course table extends beyond the ROM")

    nodes = []
    for index in range(count):
        record = offset + 2 + index * 8
        x = signed16(data, record)
        y = signed16(data, record + 2)
        z = signed16(data, record + 4)
        half_width = struct.unpack_from("<H", data, record + 6)[0]
        if not 64 <= half_width <= 4096:
            raise ValueError(f"implausible half-width {half_width} at node {index}")
        nodes.append({"id": index, "center": [x, y, z], "half_width": half_width})

    first, last = nodes[0]["center"], nodes[-1]["center"]
    closure_distance = math.hypot(first[0] - last[0], first[2] - last[2])
    closed = closure_distance < max(nodes[0]["half_width"], nodes[-1]["half_width"]) * 6
    minimum = [min(node["center"][axis] for node in nodes) for axis in range(3)]
    maximum = [max(node["center"][axis] for node in nodes) for axis in range(3)]
    return {
        "format": "srf-native-track-centerline-v1",
        "source": {
            "bank": bank,
            "address": address,
            "lorom_offset": offset,
            "rom_sha256": hashlib.sha256(data).hexdigest(),
        },
        "node_count": count,
        "closed": closed,
        "bounds": {"min": minimum, "max": maximum},
        "nodes": nodes,
    }


def road_edges(track: dict) -> tuple[list[tuple[float, float, float]], list[tuple[float, float, float]]]:
    nodes = track["nodes"]
    left, right = [], []
    for index, node in enumerate(nodes):
        previous = nodes[(index - 1) % len(nodes)] if track["closed"] else nodes[max(0, index - 1)]
        following = nodes[(index + 1) % len(nodes)] if track["closed"] else nodes[min(len(nodes) - 1, index + 1)]
        dx = following["center"][0] - previous["center"][0]
        dz = following["center"][2] - previous["center"][2]
        length = max(1.0, math.hypot(dx, dz))
        px, pz = -dz / length, dx / length
        x, y, z = node["center"]
        width = node["half_width"]
        left.append((x + px * width, y, z + pz * width))
        right.append((x - px * width, y, z - pz * width))
    return left, right


def write_obj(path: Path, track: dict) -> None:
    left, right = road_edges(track)
    lines = ["# Stunt Race FX diagnostic road ribbon", "o course_road"]
    for point in left + right:
        lines.append(f"v {point[0]:.6f} {-point[1]:.6f} {point[2]:.6f}")
    segment_count = len(left) if track["closed"] else len(left) - 1
    for index in range(segment_count):
        following = (index + 1) % len(left)
        a, b = index + 1, following + 1
        c, d = len(left) + following + 1, len(left) + index + 1
        lines.append(f"g segment_{index:03d}")
        lines.append(f"f {a} {b} {c} {d}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom", type=Path, help="path to the user's US Rev 1 ROM")
    parser.add_argument("--bank", type=number, default=0x05)
    parser.add_argument("--address", type=number, default=0x88B7)
    parser.add_argument("--json", type=Path, help="write decoded metadata for inspection")
    parser.add_argument("--obj", type=Path, help="write a diagnostic world-space road ribbon")
    args = parser.parse_args()

    track = decode(args.rom.read_bytes(), args.bank, args.address)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(track, indent=2) + "\n", encoding="utf-8")
    if args.obj:
        args.obj.parent.mkdir(parents=True, exist_ok=True)
        write_obj(args.obj, track)
    print(f"decoded {track['node_count']} nodes from ${args.bank:02X}:{args.address:04X}; "
          f"closed={track['closed']} bounds={track['bounds']}")


if __name__ == "__main__":
    main()
