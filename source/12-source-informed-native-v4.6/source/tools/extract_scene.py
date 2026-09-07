#!/usr/bin/env python3
"""Extract the live SRF render list and camera transform from GSU work RAM."""

from __future__ import annotations

import argparse
import json
import os
import struct


def u16(data: bytes, address: int) -> int:
    return struct.unpack_from("<H", data, address)[0]


def s16(data: bytes, address: int) -> int:
    value = u16(data, address)
    return value - 0x10000 if value & 0x8000 else value


def object_record(data: bytes, address: int) -> dict:
    return {
        "address": f"70:{address:04X}",
        "previous": f"70:{u16(data, address + 0x04):04X}",
        "next": f"70:{u16(data, address + 0x06):04X}",
        "type": u16(data, address + 0x02),
        "model_address": f"{u16(data, address + 0x0A):04X}",
        "bounds": [
            s16(data, address + 0x0C),
            s16(data, address + 0x0E),
            s16(data, address + 0x10),
        ],
        "model_index": u16(data, address + 0x12) & 0x7F,
        "heading": s16(data, address + 0x14),
        "flags": u16(data, address + 0x1C),
        "camera_relative": [
            s16(data, address + 0x1E),
            s16(data, address + 0x20),
            s16(data, address + 0x22),
        ],
        "animation": u16(data, address + 0x24),
    }


def extract(data: bytes) -> dict:
    if len(data) < 0x247C:
        raise ValueError("capture is too small to contain the SRF scene control block")
    matrix_fixed = [s16(data, address) for address in range(0x00E4, 0x00F6, 2)]
    objects = []
    address = u16(data, 0x247A)
    seen = set()
    while address and address not in seen and address + 0x26 <= min(len(data), 0x3000):
        seen.add(address)
        record = object_record(data, address)
        objects.append(record)
        address = u16(data, address + 0x06)
    return {
        "format": "stunt-race-fx-live-scene-v1",
        "camera_object": f"70:{u16(data, 0x247A):04X}",
        "camera_matrix_fixed_1_15": matrix_fixed,
        "camera_matrix": [round(value / 32768.0, 7) for value in matrix_fixed],
        "object_count": len(objects),
        "objects": objects,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", help="snes9x_fXXXXXX_ram.bin")
    parser.add_argument("--output", help="write JSON here (stdout when omitted)")
    args = parser.parse_args()
    scene = extract(open(args.capture, "rb").read())
    encoded = json.dumps(scene, indent=2) + "\n"
    if args.output:
        os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
        with open(args.output, "w", encoding="utf-8") as file:
            file.write(encoded)
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
