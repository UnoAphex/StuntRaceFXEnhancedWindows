// ROM-free regression test for the production wide OBJ edge decoder.
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#ifdef OBJ
#undef OBJ
#endif

struct SpriteObject { int HPos; unsigned Name, Palette, Priority; bool HFlip; };
struct { bool ForcedBlanking; unsigned BGMode, OBJNameBase, OBJNameSelect; SpriteObject OBJ[128]; } PPU;
struct { bool InterlaceOBJ; uint16_t ScreenColors[256]; } IPPU;
struct { uint8_t VRAM[65536], FillRAM[65536]; } Memory;
struct ObjLineEntry { int Sprite; unsigned Line; };
struct ObjLine { int Tiles; ObjLineEntry OBJ[128]; };
struct { ObjLine OBJLines[224]; int OBJWidths[128], OBJVisibleTiles[128]; } GFX;
struct { int MaxSpriteTilesPerLine; } Settings;

static bool srf_wide_background_enabled;
static uint32_t srf_wide_color(unsigned index) {
    const unsigned rgb=IPPU.ScreenColors[index&255];
    return 0xff000000u|((((rgb>>11)&31)*255/31)<<16)|
        ((((rgb>>5)&63)*255/63)<<8)|((rgb&31)*255/31);
}
#include "../../core/srf_wide_sprites.h"

int main() {
    memset(&GFX,0,sizeof(GFX));
    for(auto& line:GFX.OBJLines) line.OBJ[0].Sprite=-1;
    srf_wide_background_enabled=true;
    PPU.BGMode=3; Memory.FillRAM[0x212c]=16;
    Settings.MaxSpriteTilesPerLine=32;
    PPU.OBJ[0]={20,0,0,3,false};
    GFX.OBJWidths[0]=8; GFX.OBJVisibleTiles[0]=1;
    GFX.OBJLines[32].OBJ[0]={0,0}; GFX.OBJLines[32].OBJ[1].Sprite=-1;
    Memory.VRAM[0]=0x80; IPPU.ScreenColors[129]=0xf800;
    srf_wide_sprite_line(32);
    assert(srf_wide_sprites[0][276]==0xffff0000u);
    assert(srf_wide_sprites[0][281]==0); // central race viewport stays untouched
    PPU.OBJ[0].HPos=16; GFX.OBJWidths[0]=16;
    srf_wide_sprite_line(32);
    assert(srf_wide_sprites[0][272]==0); // fixed race-window bezel is not repeated
    PPU.OBJ[0].HPos=20; GFX.OBJWidths[0]=8;
    PPU.OBJ[0].Priority=2;
    srf_wide_sprite_line(32);
    assert(srf_wide_sprites[0][276]==0); // unsafe depth is left to the original compositor
    S9xSRFWideSpritesReset();
    assert(srf_wide_sprites[0][276]==0);
    puts("Wide sprite tests passed: edge decode, center exclusion, priority safety, reset.");
}
