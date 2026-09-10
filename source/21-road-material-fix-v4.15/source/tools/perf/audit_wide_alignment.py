"""Offline alignment audit; never used to drive live gameplay or presentation."""
import argparse
import struct
from pathlib import Path
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('captures', type=Path)
parser.add_argument('reference', type=Path)
args = parser.parse_args()
# Original source image has a centered 4:3 presentation and a 208x128 game view.
reference = Image.open(args.reference).convert('RGB').resize((256,224), Image.NEAREST)
reference = list(reference.crop((24,32,232,160)).getdata())
results = []
for path in sorted(args.captures.glob('*wide_polygons.bin')):
    prefix = str(path)[:-len('wide_polygons.bin')]
    palette = struct.unpack('<256H', Path(prefix+'wide_palette.bin').read_bytes())
    image = Image.new('RGB', (208,128), (255,0,255))
    draw = ImageDraw.Draw(image)
    for record in struct.iter_unpack('<68H', path.read_bytes()):
        count = record[0]
        raw = palette[record[1]&255]
        color = tuple(((raw >> shift)&31)*255//31 for shift in (0,5,10))
        coords = [x if x < 32768 else x-65536 for x in record[4:4+count*2]]
        draw.polygon(list(zip(coords[::2], coords[1::2])), fill=color)
    pixels = list(image.getdata())
    match = sum(max(abs(a-b) for a,b in zip(x,y)) <= 10 for x,y in zip(pixels,reference))
    results.append((match/len(pixels), path.name))
for score, name in sorted(results, reverse=True)[:20]:
    print(f'{score:.4%} {name}')
