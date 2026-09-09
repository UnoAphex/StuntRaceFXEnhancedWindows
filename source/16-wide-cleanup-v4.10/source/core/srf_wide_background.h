/* Presentation-only mode-3 BG2 extension at original texel density. */
static bool srf_wide_background_enabled;
static uint32_t srf_wide_background[128][768];
static unsigned srf_wide_display_slot = 2;
static uint32_t srf_wide_colors[256];
// Match the compatibility core's actual RGB565 output, including its green
// bit expansion and brightness table. Do not independently approximate CGRAM.
static uint32_t srf_wide_color(unsigned index) {
    const unsigned rgb = IPPU.ScreenColors[index & 255];
    return 0xff000000u | ((((rgb>>11)&31)*255/31)<<16) |
        ((((rgb>>5)&63)*255/63)<<8) | ((rgb&31)*255/31);
}
const uint8_t *S9xSRFWideColors(uint32_t *size) {
    *size=sizeof(srf_wide_colors); return (const uint8_t*)srf_wide_colors;
}
void S9xSRFWideBackgroundReset() {
    srf_wide_display_slot=2;
    memset(srf_wide_background,0,sizeof(srf_wide_background));
    memset(srf_wide_colors,0,sizeof(srf_wide_colors));
}
unsigned S9xSRFWideDisplaySlot() { return srf_wide_display_slot; }
void S9xSRFWideBackgroundEnable(bool enabled) { srf_wide_background_enabled = enabled; }
const uint8_t *S9xSRFWideBackground(uint32_t *size) {
    *size = sizeof(srf_wide_background);
    return (const uint8_t *)srf_wide_background;
}
static void srf_wide_background_line(unsigned line) {
    if(!srf_wide_background_enabled || line < 32 || line >= 160) return;
    uint32_t* out = srf_wide_background[line-32];
    if (line==96)
        for(unsigned i=0;i<256;++i) srf_wide_colors[i]=PPU.ForcedBlanking ? 0xff000000u : srf_wide_color(i);
    if (line==32) {
        srf_wide_display_slot=2;
        if (PPU.BGMode==3 && !PPU.BG[0].BGSize && (Memory.FillRAM[0x212c]&1)) {
            const auto& world=PPU.BG[0];
            unsigned x=(24+world.HOffset)&255, y=(line+world.VOffset+1)&255;
            unsigned map=((world.SCBase<<1)+((y>>3)*32+(x>>3))*2)&65535;
            unsigned tile=Memory.VRAM[map]|(Memory.VRAM[(map+1)&65535]<<8);
            unsigned address=((world.NameBase<<1)+(tile&1023)*64)&65535;
            // The first visible tile selects the two verified retail bitmaps.
            if(address==0) srf_wide_display_slot=0;
            else if(address==0x4400) srf_wide_display_slot=1;
        }
    }
    if(PPU.ForcedBlanking || PPU.BGMode != 3 || PPU.BG[1].BGSize || !(Memory.FillRAM[0x212c]&2)) {
        memset(out,0,768*sizeof(uint32_t)); return;
    }
    const auto& bg = PPU.BG[1];
    const unsigned y=(line+bg.VOffset+1)&((bg.SCSize&2)?511:255);
    for(int column=0; column<768; ++column) {
        const unsigned x=(column-256+bg.HOffset)&((bg.SCSize&1)?511:255);
        const unsigned tx=x>>3, ty=y>>3;
        const unsigned block=(tx>>5)+(ty>>5)*((bg.SCSize&1)?2:1);
        const unsigned map=((bg.SCBase<<1)+block*2048+((ty&31)*32+(tx&31))*2)&65535;
        const unsigned tile=Memory.VRAM[map]|(Memory.VRAM[(map+1)&65535]<<8);
        const unsigned u=(tile&0x4000)?7-(x&7):x&7;
        const unsigned v=(tile&0x8000)?7-(y&7):y&7;
        const unsigned base=(bg.NameBase<<1)+(tile&1023)*32+v*2;
        unsigned index=0;
        for(unsigned bit=0;bit<4;++bit)
            index|=((Memory.VRAM[(base+(bit/2)*16+bit%2)&65535]>>(7-u))&1)<<bit;
        out[column]=srf_wide_color(index?((tile>>10)&7)*16+index:0);
    }
}
