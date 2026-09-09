/* Extend eligible original OBJ pixels crossing a race-view edge. The game's
 * scanline OBJ selection remains authoritative; hidden/wrapped OAM entries
 * are not guessed into world-space objects. */
static uint32_t srf_wide_sprites[128][768];
void S9xSRFWideSpritesReset() { memset(srf_wide_sprites,0,sizeof(srf_wide_sprites)); }
const uint8_t *S9xSRFWideSprites(uint32_t *size) {
    *size=sizeof(srf_wide_sprites); return (const uint8_t*)srf_wide_sprites;
}
static void srf_wide_sprite_line(unsigned line) {
    if(!srf_wide_background_enabled || line<32 || line>=160) return;
    uint32_t* out=srf_wide_sprites[line-32];
    memset(out,0,768*sizeof(*out));
    if(PPU.ForcedBlanking || PPU.BGMode!=3 || !(Memory.FillRAM[0x212c]&16) || IPPU.InterlaceOBJ) return;
    uint8_t occupied[768]={0};
    const int limit=Settings.MaxSpriteTilesPerLine==128 ? 128 : 32;
    int tiles=GFX.OBJLines[line].Tiles;
    for(int i=0;i<limit;++i) {
        const int id=GFX.OBJLines[line].OBJ[i].Sprite;
        if(id<0) break;
        const auto& obj=PPU.OBJ[id];
        tiles+=GFX.OBJVisibleTiles[id];
        if(tiles<=0) continue;
        const int width=GFX.OBJWidths[id], x0=obj.HPos;
        // The race-window bezel is assembled from 16-pixel OBJ strips at
        // these two fixed positions. Repeating those strips into the wider
        // world produces vertical seams and is never world content.
        if(width==16 && (x0==16 || x0==224)) continue;
        if(x0==-256 || !((x0<25 && x0+width>24) || (x0<232 && x0+width>231))) continue;
        const unsigned row=GFX.OBJLines[line].OBJ[i].Line;
        const unsigned base=((row*2+(obj.Name&0xf0))&0xf0)|(obj.Name&0x100);
        for(int x=0;x<width;++x) {
            const int screen=x0+x, column=screen+256;
            if(column<0 || column>=768 || (screen>=25 && screen<231) || occupied[column]) continue;
            const unsigned local=obj.HFlip ? width-1-x : x;
            const unsigned tile=base|(((obj.Name&15)+(local>>3))&15);
            const unsigned address=PPU.OBJNameBase+tile*32+((tile&256)?PPU.OBJNameSelect:0)+(row&7)*2;
            unsigned index=0;
            for(unsigned bit=0;bit<4;++bit)
                index|=((Memory.VRAM[(address+(bit/2)*16+bit%2)&65535]>>(7-(local&7)))&1)<<bit;
            if(!index) continue;
            occupied[column]=1;
            // Only priority 3 is unconditionally in front of the race BG1.
            // Lower priorities and color-math effects need a depth-aware pass.
            if(obj.Priority!=3 || ((Memory.FillRAM[0x2131]&16) && (obj.Palette&4))) continue;
            out[column]=srf_wide_color(128+obj.Palette*16+index);
        }
    }
}
