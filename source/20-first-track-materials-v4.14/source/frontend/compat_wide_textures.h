// Runtime-only original texture sampling. No extracted assets are persisted.
struct WideTextureKey {
    uint16_t bank,base,mask,mode,color,u,v;
    bool operator==(const WideTextureKey& b) const {
        return bank==b.bank && base==b.base && mask==b.mask && mode==b.mode &&
            color==b.color && u==b.u && v==b.v;
    }
};
struct WideTextureEntry {
    WideTextureKey key{};
    SDL_Texture* texture=nullptr;
    uint64_t palette=0, used=UINT64_MAX;
};
static std::vector<WideTextureEntry> wide_texture_cache;
static uint64_t wide_palette_hash() {
    uint64_t hash=1469598103934665603ull;
    for(uint32_t color:compat_wide_colors) {hash^=color;hash*=1099511628211ull;}
    return hash;
}
static bool wide_rom_byte(unsigned bank,unsigned address,uint8_t& value) {
    bank&=127;
    if(game_rom.size()<65536 || bank>=0x70) return false;
    const size_t offset=bank<0x40 ?
        (bank%(game_rom.size()/32768))*32768+(address&32767) :
        (bank%(game_rom.size()/65536))*65536+(address&65535);
    if(offset>=game_rom.size()) return false;
    value=game_rom[offset]; return true;
}
static SDL_Texture* wide_original_texture(const CompatWideCameraFace& face,uint64_t palette) {
    const WideTextureKey key{face.tex_bank,face.tex_base,face.tex_mask,face.color_mode,
        face.color,face.scroll_u,face.scroll_v};
    if(key.bank>=0x70 || (key.mode&~15u)) return nullptr;
    WideTextureEntry* entry=nullptr;
    for(auto& candidate:wide_texture_cache) if(candidate.key==key) {entry=&candidate;break;}
    if(!entry) {
        if(wide_texture_cache.capacity()<128) wide_texture_cache.reserve(128);
        if(wide_texture_cache.size()<128) {wide_texture_cache.emplace_back();entry=&wide_texture_cache.back();}
        else {
            for(auto& candidate:wide_texture_cache)
                if(candidate.used!=emulated_frame && (!entry || candidate.used<entry->used)) entry=&candidate;
            if(!entry) return nullptr; // Never evict a texture referenced by this frame's commands.
        }
        entry->key=key; entry->palette=0;
    }
    entry->used=emulated_frame;
    if(!entry->texture) {
        entry->texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,256,256);
        if(!entry->texture) return nullptr;
        SDL_SetTextureBlendMode(entry->texture,SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(entry->texture,SDL_ScaleModeNearest);
        ++active_profile.texture_recreates;
    }
    if(entry->palette!=palette) {
        static std::array<uint32_t,256*256> pixels;
        const Uint64 start=profile_now();
        for(unsigned y=0;y<256;++y) for(unsigned x=0;x<256;++x) {
            // GSU MERGE uses the upper bytes of the two 8.8 UV coordinates.
            unsigned u=(x+(key.u>>8))&255, v=(y+(key.v>>8))&255;
            uint8_t color;
            if(!wide_rom_byte(key.bank,(key.base+((u|(v<<8))&key.mask))&65535,color)) return nullptr;
            if(key.mode&4) color=(color&240)|(color>>4);
            if(key.mode&8) color=(key.color&240)|(color&15);
            const bool opaque=(key.mode&1) || ((key.mode&8)?(color&15)!=0:color!=0);
            pixels[y*256+x]=opaque ? compat_wide_colors[color] : 0;
        }
        if(SDL_UpdateTexture(entry->texture,nullptr,pixels.data(),256*4)!=0) return nullptr;
        ++active_profile.uploads;
        active_profile.upload_ms+=profile_elapsed_ms(start,profile_now());
        entry->palette=palette;
    }
    return entry->texture;
}
