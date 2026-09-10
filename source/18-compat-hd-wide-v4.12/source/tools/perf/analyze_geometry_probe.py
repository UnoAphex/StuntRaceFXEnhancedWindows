"""Read version-1 little-endian, bounded geometry probe output (no ROM required)."""
import argparse
import struct
from pathlib import Path


def analyze(path):
    data = path.read_bytes()
    if len(data) != 16384 * 16:
        raise ValueError('Expected a 16384-entry geometry histogram')
    suffix = 'geometry_histogram.bin'
    if not str(path).endswith(suffix):
        raise ValueError('Expected a geometry_histogram.bin filename')
    prefix = str(path)[:-len(suffix)]
    status = Path(prefix + 'geometry_status.bin').read_bytes()
    values = struct.unpack('<' + 'I' * (len(status) // 4), status)
    version, lost, snapshots_lost, observed = values[:4]
    if version not in (1, 2):
        raise ValueError('Unsupported probe version')
    if version == 2:
        print(f'Polygons missed={values[4]} invalid={values[5]}')
    rows = [row for row in struct.iter_unpack('<4I', data) if row[1]]
    print(f'Observed={observed} histogram_missed={lost} snapshots_missed={snapshots_lost}')
    print('Bank:R15 is the pre-fetch pipe register, NOT a verified opcode address.')
    print('Top PLOT sites: bank:R15, instruction visits, PLOT, RPIX')
    for key, count, plots, reads in sorted(rows, key=lambda r: r[2], reverse=True)[:20]:
        if plots:
            print(f'{key >> 16:02X}:{key & 65535:04X} {count:9d} {plots:9d} {reads:9d}')
    registers = Path(prefix + 'geometry_registers.bin')
    if registers.exists():
        raw = registers.read_bytes()
        if len(raw) % 40:
            raise ValueError('Truncated register snapshot')
        print(f'Register snapshots: {len(raw) // 40}')
        for record in list(struct.iter_unpack('<20H', raw))[:8]:
            print('bank/op/status/cache:', ' '.join(f'{x:04X}' for x in record[:4]))
            print('R0..R15:', ' '.join(f'{x:04X}' for x in record[4:]))
    return rows


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('histogram', type=Path)
    analyze(parser.parse_args().histogram)
