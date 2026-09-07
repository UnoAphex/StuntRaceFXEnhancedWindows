#!/usr/bin/env python3
"""Small linear Super FX disassembler for ROM research.

The mnemonic table is read from the Snes9x fxdbg.cpp already present in the
workspace so this helper stays synchronized with the emulator implementation.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def parse_tables(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    start = text.index("fx_apvMnemonicTable[]")
    end = text.index("};", start)
    entries = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', text[start:end])
    if len(entries) != 1024:
        raise ValueError(f"expected 1024 mnemonic entries, found {len(entries)}")
    return entries


def lorom_offset(bank: int, address: int) -> int:
    return (bank & 0x7F) * 0x8000 + (address & 0x7FFF)


def signed8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("rom")
    parser.add_argument("entry", help="GSU bank:address, for example 01:BBC7")
    parser.add_argument("--count", type=int, default=256)
    parser.add_argument("--fxdbg", required=True)
    parser.add_argument("--raw", action="store_true",
                        help="treat the input as a flat GSU/cache byte image")
    args = parser.parse_args()

    bank_text, address_text = args.entry.split(":", 1)
    bank = int(bank_text, 16)
    address = int(address_text, 16)
    rom = Path(args.rom).read_bytes()
    mnemonics = parse_tables(Path(args.fxdbg))
    alt = 0
    for _ in range(args.count):
        offset = address if args.raw else lorom_offset(bank, address)
        if offset >= len(rom):
            break
        opcode = rom[offset]
        size = 1
        operand = None
        if 0x05 <= opcode <= 0x0F:
            size = 2
            operand = rom[offset + 1]
        elif 0xA0 <= opcode <= 0xAF:
            size = 2
            operand = rom[offset + 1]
        elif opcode >= 0xF0:
            size = 3
            operand = rom[offset + 1] | (rom[offset + 2] << 8)

        raw = " ".join(f"{value:02X}" for value in rom[offset:offset + size])
        mnemonic = mnemonics[alt * 256 + opcode]
        if 0x05 <= opcode <= 0x0F and operand is not None:
            target = (address + 2 + signed8(operand)) & 0xFFFF
            mnemonic = re.sub(r"\$%04x", f"${target:04X}", mnemonic)
        elif 0xA0 <= opcode <= 0xAF and operand is not None:
            value = operand * 2 if alt in (1, 2) else operand
            mnemonic = re.sub(r"\$%0[24]x", f"${value:04X}", mnemonic)
            mnemonic = mnemonic.replace("%02x", f"{value & 0xFF:02X}")
        elif opcode >= 0xF0 and operand is not None:
            mnemonic = mnemonic.replace("%04x", f"{operand:04X}")

        print(f"{bank:02X}:{address:04X}  {raw:<8} {mnemonic}")
        address = (address + size) & 0xFFFF
        if opcode == 0x3D:
            alt = 1
        elif opcode == 0x3E:
            alt = 2
        elif opcode == 0x3F:
            alt = 3
        else:
            alt = 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
