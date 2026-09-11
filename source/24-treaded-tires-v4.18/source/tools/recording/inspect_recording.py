#!/usr/bin/env python3
"""Verify, summarize, and optionally extract an SRFX reconstruction session."""
import argparse
import ctypes
import json
import struct
from collections import Counter
from pathlib import Path

FILE_HEADER = struct.Struct("<8sIIQQQ")
FRAME_HEADER = struct.Struct("<4sIQQIHHIIQ")
CHUNK_HEADER = struct.Struct("<IIIIQ")
END_HEADER = struct.Struct("<4sIQQ")


def fnv1a64(data: bytes) -> int:
    value = 1469598103934665603
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def tag_name(value: int) -> str:
    return struct.pack("<I", value).decode("ascii", errors="replace")


class XpressHuffman:
    def __init__(self) -> None:
        self.api = ctypes.WinDLL("cabinet.dll")
        self.handle = ctypes.c_void_p()
        self.api.CreateDecompressor.argtypes = [ctypes.c_uint32, ctypes.c_void_p,
                                                ctypes.POINTER(ctypes.c_void_p)]
        self.api.CreateDecompressor.restype = ctypes.c_bool
        self.api.Decompress.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                        ctypes.c_void_p, ctypes.c_size_t,
                                        ctypes.POINTER(ctypes.c_size_t)]
        self.api.Decompress.restype = ctypes.c_bool
        self.api.CloseDecompressor.argtypes = [ctypes.c_void_p]
        self.api.CloseDecompressor.restype = ctypes.c_bool
        if not self.api.CreateDecompressor(4, None, ctypes.byref(self.handle)):
            raise OSError("Windows XPRESS Huffman decompressor is unavailable")

    def decompress(self, data: bytes, size: int) -> bytes:
        source = ctypes.create_string_buffer(data)
        target = ctypes.create_string_buffer(size)
        written = ctypes.c_size_t()
        if not self.api.Decompress(self.handle, source, len(data), target, size,
                                   ctypes.byref(written)) or written.value != size:
            raise OSError("XPRESS Huffman decompression failed")
        return target.raw

    def close(self) -> None:
        if self.handle:
            self.api.CloseDecompressor(self.handle)
            self.handle = None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("recording", type=Path, help="session.srfxrec path")
    parser.add_argument("--verify", action="store_true", help="hash every changed chunk")
    parser.add_argument("--frame", type=int, help="extract accumulated state at this frame")
    parser.add_argument("--output", type=Path, help="directory for --frame extraction")
    args = parser.parse_args()
    counts = Counter()
    stored = Counter()
    state: dict[str, bytes] = {}
    first_frame = last_frame = None
    frames = 0
    footer_frames = None
    extracted = False
    decompressor = None

    with args.recording.open("rb") as stream:
        raw = stream.read(FILE_HEADER.size)
        if len(raw) != FILE_HEADER.size:
            raise SystemExit("truncated recording header")
        magic, version, header_bytes, started, rom_bytes, rom_hash = FILE_HEADER.unpack(raw)
        if magic != b"SRFXREC\0" or version != 2 or header_bytes != FILE_HEADER.size:
            raise SystemExit("unsupported recording format")
        while True:
            marker = stream.read(4)
            if not marker:
                raise SystemExit("recording has no END! footer (possibly interrupted)")
            stream.seek(-4, 1)
            if marker == b"END!":
                raw = stream.read(END_HEADER.size)
                _, size, final_frame, footer_frames = END_HEADER.unpack(raw)
                if size != END_HEADER.size:
                    raise SystemExit("invalid footer")
                break
            raw = stream.read(FRAME_HEADER.size)
            if len(raw) != FRAME_HEADER.size:
                raise SystemExit("truncated frame header")
            (frame_magic, size, frame, ticks, input_mask, width, height,
             flags, chunk_count, payload_bytes) = FRAME_HEADER.unpack(raw)
            if frame_magic != b"FRAM" or size != FRAME_HEADER.size:
                raise SystemExit(f"invalid frame header at byte {stream.tell()-len(raw)}")
            payload_start = stream.tell()
            for _ in range(chunk_count):
                raw = stream.read(CHUNK_HEADER.size)
                if len(raw) != CHUNK_HEADER.size:
                    raise SystemExit(f"truncated chunk header in frame {frame}")
                tag, flags, raw_bytes, stored_bytes, expected_hash = CHUNK_HEADER.unpack(raw)
                stored_data = stream.read(stored_bytes)
                if len(stored_data) != stored_bytes:
                    raise SystemExit(f"truncated {tag_name(tag)} chunk in frame {frame}")
                name = tag_name(tag)
                if flags == 1:
                    decompressor = decompressor or XpressHuffman()
                    data = decompressor.decompress(stored_data, raw_bytes)
                elif flags == 0:
                    data = stored_data
                    if len(data) != raw_bytes:
                        raise SystemExit(f"raw-size mismatch for {name} in frame {frame}")
                else:
                    raise SystemExit(f"unknown compression flags for {name} in frame {frame}")
                if args.verify and fnv1a64(data) != expected_hash:
                    raise SystemExit(f"hash mismatch for {name} in frame {frame}")
                if name == "WDEL":
                    if len(data) < 8 or "WRAM" not in state:
                        raise SystemExit(f"invalid WDEL chunk in frame {frame}")
                    total, spans = struct.unpack_from("<II", data)
                    current = bytearray(state["WRAM"])
                    if total != len(current):
                        raise SystemExit(f"WDEL size mismatch in frame {frame}")
                    position = 8
                    for _ in range(spans):
                        if position + 8 > len(data):
                            raise SystemExit(f"truncated WDEL span in frame {frame}")
                        offset, length = struct.unpack_from("<II", data, position)
                        position += 8
                        if offset + length > total or position + length > len(data):
                            raise SystemExit(f"invalid WDEL span in frame {frame}")
                        current[offset:offset + length] = data[position:position + length]
                        position += length
                    if position != len(data):
                        raise SystemExit(f"WDEL trailing data in frame {frame}")
                    state["WRAM"] = bytes(current)
                else:
                    state[name] = data
                counts[name] += 1
                stored[name] += stored_bytes
            if stream.tell() - payload_start != payload_bytes:
                raise SystemExit(f"payload-size mismatch in frame {frame}")
            frames += 1
            first_frame = frame if first_frame is None else first_frame
            last_frame = frame
            if args.frame == frame:
                output = args.output or args.recording.parent / f"extracted-frame-{frame:06d}"
                output.mkdir(parents=True, exist_ok=True)
                for name, data in sorted(state.items()):
                    (output / f"{name}.bin").write_bytes(data)
                (output / "frame.json").write_text(json.dumps({
                    "frame": frame, "performance_ticks": ticks,
                    "input_mask": f"{input_mask:04X}", "width": width,
                    "height": height, "flags": flags,
                    "available_chunks": sorted(state),
                }, indent=2) + "\n", encoding="utf-8")
                extracted = True

    print(f"format_version={version}")
    print(f"started_unix_ms={started}")
    print(f"rom_bytes={rom_bytes}")
    print(f"rom_fnv1a64={rom_hash:016X}")
    print(f"frames={frames}")
    print(f"frame_range={first_frame}..{last_frame}")
    print(f"footer_frames={footer_frames}")
    for name in sorted(counts):
        latest = len(state[name]) if name in state else 0
        print(f"chunk_{name}=updates:{counts[name]},stored_bytes:{stored[name]},latest_bytes:{latest}")
    if footer_frames != frames:
        raise SystemExit("footer frame count does not match parsed frame count")
    if args.frame is not None and not extracted:
        raise SystemExit(f"frame {args.frame} was not found")
    if args.verify:
        print("verification=passed")
    if decompressor:
        decompressor.close()


if __name__ == "__main__":
    main()
