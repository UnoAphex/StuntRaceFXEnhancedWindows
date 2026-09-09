"""Render captured original polygon outlines as a diagnostic SVG (not a game asset)."""
import argparse
import struct
from pathlib import Path


def read_polygons(path):
    raw = path.read_bytes()
    if len(raw) % 136:
        raise ValueError('Truncated polygon record')
    polygons = []
    for record in struct.iter_unpack('<68H', raw):
        count, color, address, bank = record[:4]
        if not 3 <= count <= 32:
            raise ValueError('Invalid polygon size')
        points = [(record[4+i*2], record[5+i*2]) for i in range(count)]
        # The original scan converter reads the low byte of each word.
        polygons.append((color, address, bank, [(x & 255, y & 255) for x, y in points]))
    return polygons


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('output', type=Path, help='Use a temporary directory outside the repository')
    args = parser.parse_args()
    polygons = read_polygons(args.capture)
    lines = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 224" width="1024" height="896">',
             '<rect width="256" height="224" fill="#111827"/>',
             '<g fill="none" stroke="#60eeff" stroke-width="0.25">']
    for color, address, bank, points in polygons:
        coords = ' '.join(f'{x},{y}' for x,y in points)
        lines.append(f'<polygon points="{coords}"><title>RAM {bank:02X}:{address:04X}, color {color:04X}</title></polygon>')
    lines.extend(['</g>', '</svg>'])
    args.output.write_text('\n'.join(lines), encoding='utf-8')
    print(f'{len(polygons)} captured polygons; wrote {args.output}')


if __name__ == '__main__':
    main()
