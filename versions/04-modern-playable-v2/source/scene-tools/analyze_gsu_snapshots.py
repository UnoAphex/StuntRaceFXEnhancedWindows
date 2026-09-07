#!/usr/bin/env python3
"""Find candidate live scene structures in selected GSU RAM snapshots.

The tool compares same-sized binary snapshots as little-endian words, groups
changing addresses into ranges, and reports values that behave like signed
coordinates or fixed-point transforms. It never embeds or redistributes ROM
data; reports contain addresses and small numeric samples only.
"""

from __future__ import annotations

import argparse
import glob
import os
import re
import struct
from collections import defaultdict


SNAPSHOT_RE = re.compile(
    r"gsu_f(?P<frame>\d+)_l(?P<launch>\d+)_(?P<bank>[0-9A-Fa-f]{2})_"
    r"(?P<pc>[0-9A-Fa-f]{4})_(?P<phase>before|after)\.bin$"
)


def words(data: bytes) -> tuple[int, ...]:
    usable = len(data) & ~1
    return struct.unpack(f"<{usable // 2}H", data[:usable])


def signed(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def group_addresses(addresses: list[int], gap: int) -> list[list[int]]:
    groups: list[list[int]] = []
    for address in addresses:
        if not groups or address - groups[-1][-1] > gap:
            groups.append([address])
        else:
            groups[-1].append(address)
    return groups


def snapshot_info(path: str) -> dict[str, int | str] | None:
    match = SNAPSHOT_RE.match(os.path.basename(path))
    if not match:
        return None
    return {
        "frame": int(match.group("frame")),
        "launch": int(match.group("launch")),
        "bank": int(match.group("bank"), 16),
        "pc": int(match.group("pc"), 16),
        "phase": match.group("phase"),
    }


def print_changes(captures: list[tuple[int, ...]], labels: list[str], args) -> None:
    size = min(map(len, captures))
    changes: dict[int, int] = {}
    samples: dict[int, list[int]] = defaultdict(list)
    for index in range(size):
        values = [capture[index] for capture in captures]
        count = sum(a != b for a, b in zip(values, values[1:]))
        if args.min_changes <= count <= args.max_changes:
            address = index * 2
            changes[address] = count
            samples[address] = values

    ranked = sorted(changes, key=lambda address: (-changes[address], address))[: args.top]
    print(f"snapshots={len(captures)} bytes={size * 2} changing_words={len(changes)}")
    print("  " + " -> ".join(labels))
    print("\nTop changing words:")
    for address in ranked:
        values = samples[address]
        preview = ", ".join(f"{signed(value):6d}" for value in values[:8])
        print(f"  70:{address:04X} changes={changes[address]:3d} signed=[{preview}]")

    print("\nChanging ranges:")
    for group in group_addresses(sorted(changes), args.range_gap):
        total = sum(changes[address] for address in group)
        print(f"  70:{group[0]:04X}-70:{group[-1]:04X} words={len(group):4d} transitions={total:5d}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", help="folder containing gsu_*_after.bin files")
    parser.add_argument("--phase", default="after", choices=("before", "after"))
    parser.add_argument("--frame", type=int)
    parser.add_argument("--launch", type=int)
    parser.add_argument("--program", help="GSU program as bank:PC, for example 04:8CC6")
    parser.add_argument(
        "--delta",
        action="store_true",
        help="compare before/after for each selected launch instead of snapshots over time",
    )
    parser.add_argument("--min-changes", type=int, default=2)
    parser.add_argument("--max-changes", type=int, default=1_000_000)
    parser.add_argument("--range-gap", type=lambda value: int(value, 0), default=8)
    parser.add_argument("--top", type=int, default=80)
    args = parser.parse_args()

    paths = sorted(glob.glob(os.path.join(args.directory, f"gsu_*_{args.phase}.bin")))
    if args.program:
        bank_text, pc_text = args.program.split(":", 1)
        wanted_program = (int(bank_text, 16), int(pc_text, 16))
    else:
        wanted_program = None
    selected: list[str] = []
    for path in paths:
        info = snapshot_info(path)
        if not info:
            continue
        if args.frame is not None and info["frame"] != args.frame:
            continue
        if args.launch is not None and info["launch"] != args.launch:
            continue
        if wanted_program and (info["bank"], info["pc"]) != wanted_program:
            continue
        selected.append(path)
    paths = selected

    if args.delta:
        if not paths:
            raise SystemExit("no matching snapshots")
        for path in paths:
            before = path.removesuffix("_after.bin") + "_before.bin"
            if not os.path.exists(before):
                raise SystemExit(f"missing paired snapshot: {before}")
            print(f"\n=== {os.path.basename(path).removesuffix('_after.bin')} ===")
            print_changes(
                [words(open(before, "rb").read()), words(open(path, "rb").read())],
                ["before", "after"],
                args,
            )
        return 0

    if len(paths) < 2:
        raise SystemExit("need at least two matching snapshots")

    captures = [words(open(path, "rb").read()) for path in paths]
    labels = [os.path.basename(path).removesuffix(f"_{args.phase}.bin") for path in paths]
    print_changes(captures, labels, args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
