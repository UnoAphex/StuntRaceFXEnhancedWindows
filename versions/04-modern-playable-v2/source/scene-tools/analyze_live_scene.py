#!/usr/bin/env python3
"""Summarize live Super FX RAM captured from the playable Snes9x backend."""

from __future__ import annotations

import argparse
import glob
import os
import re
import struct


FRAME_RE = re.compile(r"snes9x_f(\d+)_ram\.bin$")


def u16(data: bytes, address: int) -> int:
    return data[address] | (data[address + 1] << 8)


def s16(data: bytes, address: int) -> int:
    value = u16(data, address)
    return value - 0x10000 if value & 0x8000 else value


def changed_bytes(a: bytes, b: bytes, first: int, last: int) -> int:
    return sum(left != right for left, right in zip(a[first:last], b[first:last]))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory")
    parser.add_argument("--top", type=int, default=60)
    parser.add_argument("--work-end", type=lambda value: int(value, 0), default=0x3000)
    args = parser.parse_args()

    captures: list[tuple[int, str, bytes]] = []
    for path in glob.glob(os.path.join(args.directory, "snes9x_f*_ram.bin")):
        match = FRAME_RE.match(os.path.basename(path))
        if match:
            captures.append((int(match.group(1)), path, open(path, "rb").read()))
    captures.sort()
    if len(captures) < 2:
        raise SystemExit("need at least two live RAM captures")

    print(f"captures={len(captures)} first={captures[0][0]} last={captures[-1][0]}")
    print("\nFrame transitions (changed bytes):")
    print(" frame   work<3000   pixels3000-B000   upper>=B000   matrix")
    for (old_frame, _, old), (frame, _, new) in zip(captures, captures[1:]):
        matrix = [s16(new, address) for address in range(0x00E4, 0x00F6, 2)]
        print(
            f" {old_frame:5d}->{frame:5d}  "
            f"{changed_bytes(old, new, 0, min(args.work_end, len(new))):9d}   "
            f"{changed_bytes(old, new, 0x3000, min(0xB000, len(new))):14d}   "
            f"{changed_bytes(old, new, 0xB000, len(new)):11d}   "
            + " ".join(f"{value:6d}" for value in matrix)
        )

    word_changes: list[tuple[int, int, list[int]]] = []
    limit = min(args.work_end, min(len(data) for _, _, data in captures)) & ~1
    for address in range(0, limit, 2):
        values = [u16(data, address) for _, _, data in captures]
        count = sum(a != b for a, b in zip(values, values[1:]))
        if count:
            word_changes.append((count, address, values))
    word_changes.sort(key=lambda item: (-item[0], item[1]))

    print("\nMost active work-RAM words:")
    for count, address, values in word_changes[: args.top]:
        signed = [value - 0x10000 if value & 0x8000 else value for value in values]
        preview = ", ".join(f"{value:6d}" for value in signed[:10])
        print(f"  70:{address:04X} transitions={count:3d} signed=[{preview}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
