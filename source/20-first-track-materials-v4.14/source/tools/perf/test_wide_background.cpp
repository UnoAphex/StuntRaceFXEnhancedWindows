// ROM-free regression tests exercising the production background observer.
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
struct Background { unsigned SCBase,NameBase,SCSize,BGSize,HOffset,VOffset; };
struct { unsigned BGMode; bool ForcedBlanking; Background BG[4]; } PPU;
struct { uint16_t ScreenColors[256]; } IPPU;
struct { uint8_t VRAM[65536], FillRAM[65536]; } Memory;
#include "../../core/srf_wide_background.h"

int main() {
    PPU.BGMode=3;
    PPU.BG[0].SCBase=0x7000;
    PPU.BG[1].SCBase=0x6000;
    PPU.BG[1].NameBase=0x2000;
    Memory.FillRAM[0x212c]=3;
    IPPU.ScreenColors[1]=0x07e0; // full RGB565 green, not 5-bit green / 31
    for(unsigned row=0;row<8;++row) Memory.VRAM[0x4000+row*2]=255;
    S9xSRFWideBackgroundEnable(true);
    srf_wide_background_line(96);
    assert(srf_wide_colors[1]==0xff00ff00u);
    assert(srf_wide_background[64][0]==0xff00ff00u);
    assert(srf_wide_background[64][767]==0xff00ff00u);
    // Mode-3 BG1 is 8bpp. Its alpha distinguishes opaque GSU bitmap pixels
    // from transparent pixels whose visible RGB comes from BG2.
    const unsigned map=0xe000+(12*32+3)*2;
    Memory.VRAM[map]=1;
    Memory.VRAM[66]=0x80;
    srf_wide_background_line(96);
    assert(srf_wide_world_reference[64][0]==0xff00ff00u);
    assert(srf_wide_world_reference[64][1]==0x0000ff00u);
    Memory.VRAM[map]=0;
    IPPU.ScreenColors[1]=0xf800;
    srf_wide_background_line(96);
    assert(srf_wide_background[64][0]==0xffff0000u);
    IPPU.ScreenColors[1]=0x001f;
    srf_wide_background_line(96);
    assert(srf_wide_background[64][0]==0xff0000ffu);
    PPU.ForcedBlanking=true;
    srf_wide_background_line(96);
    assert(srf_wide_background[64][0]==0);
    assert(srf_wide_colors[1]==0xff000000u);
    PPU.ForcedBlanking=false;
    srf_wide_background_line(32);
    assert(S9xSRFWideDisplaySlot()==0);
    PPU.BG[0].NameBase=0x2200;
    srf_wide_background_line(32);
    assert(S9xSRFWideDisplaySlot()==1);
    PPU.BG[0].NameBase=0x1000;
    srf_wide_background_line(32);
    assert(S9xSRFWideDisplaySlot()==2);
    S9xSRFWideBackgroundReset();
    assert(srf_wide_colors[1]==0 && srf_wide_background[64][0]==0 && srf_wide_world_reference[64][0]==0);
    puts("Wide background tests passed: RGB565, world reference, scanline refresh, blanking, bitmap selection, reset.");
}
