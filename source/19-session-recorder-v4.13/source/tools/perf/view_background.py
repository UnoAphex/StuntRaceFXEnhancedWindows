"""Offline inspection of the mode-3 BG2 tilemap, using captured state."""
import argparse
import struct
from pathlib import Path
from PIL import Image

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('prefix', help='Path ending in snes9x_fNNNNNN_')
p.add_argument('output', type=Path)
a=p.parse_args()
state=struct.unpack('<32H',Path(a.prefix+'wide_ppu.bin').read_bytes())
vram=Path(a.prefix+'wide_vram.bin').read_bytes()
palette=struct.unpack('<256H',Path(a.prefix+'wide_palette.bin').read_bytes())
base,name,size,big,h,v=state[8:14]
assert state[0]==3 and not big
out=Image.new('RGB',(768,224))
pixels=[]
for y in range(224):
    for x in range(-256,512):
        px=(x+h)&255
        py=(y+v+1)&(511 if size&2 else 255)
        tx,ty=px//8,py//8
        address=((base<<1)+(ty//32)*2048+(ty%32*32+tx)*2)&65535
        tile=vram[address]|vram[(address+1)&65535]<<8
        u,w=px&7,py&7
        if tile&0x4000:u=7-u
        if tile&0x8000:w=7-w
        char=((name<<1)+(tile&1023)*32)&65535
        index=sum(((vram[(char+(bit//2)*16+w*2+(bit%2))&65535]>>(7-u))&1)<<bit for bit in range(4))
        raw=palette[((tile>>10)&7)*16+index] if index else palette[0]
        pixels.append(tuple(((raw>>shift)&31)*255//31 for shift in (0,5,10)))
out.putdata(pixels)
out.save(a.output)
