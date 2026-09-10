// GPU side-viewport presentation of observed original camera-space polygons.
// No track reconstruction, persistent world cache, or emulated state writes.
static std::vector<SDL_Vertex> compat_wide_triangles;
struct WidePoint {float x,y,u,v;};
struct WideDrawCommand {SDL_Texture* texture;int first,count;};
static std::vector<WideDrawCommand> wide_commands;
static SDL_Texture* compat_recorded_material_textures[MATERIAL_COUNT]{};
static std::vector<uint32_t> compat_recorded_background_pixels;
#include "compat_wide_textures.h"
static float wide_cross(WidePoint a, WidePoint b, WidePoint c) {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}

// The complete first-track recording established that these palette slots are
// the recurring immutable road and turf faces. Keep this deliberately narrow:
// the recording also contains red, blue, yellow and neutral vehicle/scenery
// colors which a color-only classifier would incorrectly call mud, water, sand
// or gravel. The RGB check makes the rule self-disabling when another scene
// repurposes the same palette index.
static MaterialKind compat_recorded_surface(uint16_t index,uint32_t rgb) {
    rgb &= 0x00ffffffu;
    if((index==60 && rgb==0x526152u) || (index==61 && rgb==0x4a594au))
        return MATERIAL_ASPHALT;
    if((index==212 && rgb==0x83b629u) || (index==213 && rgb==0x73a518u) ||
       (index==214 && rgb==0x629508u) || (index==215 && rgb==0x528500u))
        return MATERIAL_GRASS;
    return MATERIAL_NONE;
}

static SDL_Texture* ensure_compat_recorded_material(MaterialKind kind) {
    if(!n64_materials || kind<0 || kind>=MATERIAL_COUNT || !renderer) return nullptr;
    if(compat_recorded_material_textures[kind]) return compat_recorded_material_textures[kind];
    const MaterialTexture& source=materials[kind];
    if(source.pixels.empty() || !source.width || !source.height) return nullptr;
    constexpr int texture_size=768;
    std::vector<uint32_t> pixels(static_cast<size_t>(texture_size)*texture_size);
    for(int y=0;y<texture_size;++y) for(int x=0;x<texture_size;++x) {
        const uint32_t sample=source.pixels[static_cast<size_t>(y%source.height)*source.width+x%source.width];
        const int r=static_cast<int>((sample>>16)&255u),g=static_cast<int>((sample>>8)&255u),b=static_cast<int>(sample&255u);
        const int luma=(r*54+g*183+b*19)>>8;
        // A neutral detail map lets the original face palette remain the base
        // color. Its restrained range avoids noisy/shimmering N64-style grain.
        const int detail=std::max(142,std::min(255,210+(luma-source.average_luma)*source.strength/70));
        pixels[static_cast<size_t>(y)*texture_size+x]=0xff000000u|
            (static_cast<uint32_t>(detail)<<16)|(static_cast<uint32_t>(detail)<<8)|detail;
    }
    SDL_Texture* material_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,texture_size,texture_size);
    if(!material_texture) return nullptr;
    if(SDL_UpdateTexture(material_texture,nullptr,pixels.data(),texture_size*4)!=0) {
        SDL_DestroyTexture(material_texture); return nullptr;
    }
    SDL_SetTextureBlendMode(material_texture,SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(material_texture,SDL_ScaleModeLinear);
    compat_recorded_material_textures[kind]=material_texture;
    ++active_profile.texture_recreates;
    ++active_profile.uploads;
    return material_texture;
}

static const uint8_t* prepare_compat_recorded_background(const uint8_t* bytes) {
    if(!n64_materials || !compat_hd_center || !race_scene_active) return bytes;
    constexpr unsigned width=768,height=128;
    const uint32_t* source=reinterpret_cast<const uint32_t*>(bytes);
    compat_recorded_background_pixels.assign(source,source+width*height);
    const MaterialTexture& grass=materials[MATERIAL_GRASS];
    if(grass.pixels.empty() || !grass.width || !grass.height) return bytes;
    // The recording shows that the broad first-course turf is part of BG2,
    // while the road itself travels through camera-face packets. Restrict this
    // pass to green pixels below the horizon so sky and distant blue layers
    // cannot be mistaken for water by the general material classifier.
    for(unsigned y=32;y<height;++y) for(unsigned x=0;x<width;++x) {
        uint32_t& color=compat_recorded_background_pixels[static_cast<size_t>(y)*width+x];
        if(classify_material(color)!=MATERIAL_GRASS) continue;
        const unsigned tx=static_cast<unsigned>(wrapped_coordinate(
            static_cast<int>(x)+(material_scroll_x&63),grass.width));
        const unsigned ty=static_cast<unsigned>(wrapped_coordinate(
            static_cast<int>(y)*2+(material_scroll_z&63),grass.height));
        color=material_modulate(color,grass.pixels[static_cast<size_t>(ty)*grass.width+tx],
            grass.average_luma,115);
    }
    return reinterpret_cast<const uint8_t*>(compat_recorded_background_pixels.data());
}

static void draw_compat_wide_sides(const SDL_Rect& center) {
    if (!compat_wide || render_path != RENDER_COMPATIBILITY || !race_scene_active ||
        compat_wide_age > 12 || compat_wide_display_bytes.empty() || !source_width || !source_height) return;
    int width = 0, height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width * 3 <= height * 4) return; // Keep 4:3 an unchanged baseline.
    const float sx = static_cast<float>(center.w) / source_width;
    const float sy = static_cast<float>(center.h) / source_height;
    const float origin_x = center.x + 24 * sx, origin_y = center.y + 32 * sy;
    const float limits[4] = {-origin_x/sx, (width-origin_x)/sx, 0, 128};
    compat_wide_triangles.clear();
    wide_commands.clear();
    const uint64_t palette=wide_palette_hash();
    if (compat_wide_triangles.capacity() < 2048 * 96)
        compat_wide_triangles.reserve(2048 * 96);
    for (size_t offset=0; offset<compat_wide_display_bytes.size(); offset+=sizeof(CompatWideCameraFace)) {
        CompatWideCameraFace face;
        memcpy(&face,compat_wide_display_bytes.data()+offset,sizeof(face));
        int count = face.count;
        if (count < 3 || count > 32) continue;
        const uint32_t rgb = compat_wide_colors[face.color&255];
        const MaterialKind recorded_material=face.textured ? MATERIAL_NONE :
            compat_recorded_surface(face.color&255,rgb);
        SDL_Texture* image=face.textured ? wide_original_texture(face,palette) :
            ensure_compat_recorded_material(recorded_material);
        if(face.textured && !image) continue;
        WidePoint a[64], b[64];
        int clipped = 0;
        for (int i = 0; i < count; ++i) {
            const float* p = &face.xyz[i*3];
            const float* q = &face.xyz[((i+1)%count)*3];
            const float projected_x=p[2]>=1 ? face.center_x+128*p[0]/p[2] : 0;
            const float projected_y=p[2]>=1 ? face.center_y+128*p[1]/p[2] : 0;
            const float u=image && !face.textured && p[2]>=1 ?
                (projected_x*2+256+(material_scroll_x&63))/768 : face.uv[i*2]/256;
            const float v=image && !face.textured && p[2]>=1 ?
                (projected_y*2+192+(material_scroll_z&63))/768 : face.uv[i*2+1]/256;
            const int next=(i+1)%count;
            if(p[2] >= 1) a[clipped++] = {projected_x,projected_y,u,v};
            if((p[2] >= 1) != (q[2] >= 1)) {
                const float t=(1-p[2])/(q[2]-p[2]);
                const float cross_x=face.center_x+128*(p[0]+t*(q[0]-p[0]));
                const float cross_y=face.center_y+128*(p[1]+t*(q[1]-p[1]));
                const float cross_u=image && !face.textured ?
                    (cross_x*2+256+(material_scroll_x&63))/768 :
                    u+t*(face.uv[next*2]/256-u);
                const float cross_v=image && !face.textured ?
                    (cross_y*2+192+(material_scroll_z&63))/768 :
                    v+t*(face.uv[next*2+1]/256-v);
                a[clipped++]={cross_x,cross_y,cross_u,cross_v};
            }
        }
        count = clipped;
        // Clip before GPU submission to avoid giant/offscreen raster bounds.
        for (int edge = 0; edge < 4 && count; ++edge) {
            int output = 0;
            bool overflow=false;
            for (int i = 0; i < count; ++i) {
                const WidePoint p = a[i], q = a[(i+1)%count];
                const float pv = edge < 2 ? p.x : p.y, qv = edge < 2 ? q.x : q.y;
                const float limit = limits[edge];
                const bool pin = edge%2 ? pv <= limit : pv >= limit;
                const bool qin = edge%2 ? qv <= limit : qv >= limit;
                if (pin) { if(output==64) {overflow=true;break;} b[output++] = p; }
                if (pin != qin) {
                    if(output==64) {overflow=true;break;}
                    const float t = (limit-pv)/(qv-pv);
                    b[output++] = {p.x+t*(q.x-p.x), p.y+t*(q.y-p.y),p.u+t*(q.u-p.u),p.v+t*(q.v-p.v)};
                }
            }
            count = overflow ? 0 : output;
            std::copy(b, b+count, a);
        }
        // Remove consecutive duplicate vertices introduced by original clipping.
        int unique = 0;
        for (int i = 0; i < count; ++i)
            if (!unique || fabsf(a[i].x-b[unique-1].x)+fabsf(a[i].y-b[unique-1].y) > 0.001f)
                b[unique++] = a[i];
        if (unique > 1 && fabsf(b[0].x-b[unique-1].x)+fabsf(b[0].y-b[unique-1].y) < 0.001f) --unique;
        count = unique;
        std::copy(b, b+count, a);
        float area = 0;
        for (int i = 0; i < count; ++i) area += a[i].x*a[(i+1)%count].y-a[(i+1)%count].x*a[i].y;
        if (count < 3 || fabsf(area) < 0.001f) continue;
        const float sign = area > 0 ? 1.0f : -1.0f;
        const bool material_image=image && !face.textured;
        const auto boosted=[](uint32_t channel)->Uint8 {return static_cast<Uint8>(std::min(255u,(channel*255u+105u)/210u));};
        const SDL_Color color=face.textured ? SDL_Color{255,255,255,255} :
            (material_image ? SDL_Color{boosted((rgb>>16)&255u),boosted((rgb>>8)&255u),boosted(rgb&255u),255} :
            SDL_Color{static_cast<Uint8>(rgb>>16),static_cast<Uint8>(rgb>>8),static_cast<Uint8>(rgb),255});
        int indices[64];
        for(int i=0;i<count;++i) indices[i]=i;
        const size_t first_triangle = compat_wide_triangles.size();
        while(count >= 3) {
            bool found = false;
            for(int i=0;i<count;++i) {
                const int ia=indices[(i+count-1)%count], ib=indices[i], ic=indices[(i+1)%count];
                if (sign*wide_cross(a[ia],a[ib],a[ic]) <= 0.0001f) continue;
                bool inside = false;
                for(int j=0;j<count;++j) {
                    const int k=indices[j];
                    if(k==ia || k==ib || k==ic) continue;
                    if(sign*wide_cross(a[ia],a[ib],a[k])>0.0001f &&
                       sign*wide_cross(a[ib],a[ic],a[k])>0.0001f &&
                       sign*wide_cross(a[ic],a[ia],a[k])>0.0001f) {inside=true;break;}
                }
                if(inside) continue;
                for(int k : {ia,ib,ic}) compat_wide_triangles.push_back({
                    {origin_x+a[k].x*sx,origin_y+a[k].y*sy},color,{a[k].u,a[k].v}});
                for(int j=i;j<count-1;++j) indices[j]=indices[j+1];
                --count; found=true; break;
            }
            if(!found) {compat_wide_triangles.resize(first_triangle);break;}
        }
        const int added=static_cast<int>(compat_wide_triangles.size()-first_triangle);
        if(added) {
            if(!wide_commands.empty() && wide_commands.back().texture==image)
                wide_commands.back().count+=added;
            else wide_commands.push_back({image,static_cast<int>(first_triangle),added});
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    // The original race framebuffer includes a one-texel vertical bezel at
    // each edge. Replace those border columns with the same world pass as
    // the extensions, rather than leaving two black dividers through the road.
    const int left = static_cast<int>(ceilf(origin_x+sx));
    const int right = static_cast<int>(floorf(origin_x+207*sx));
    const bool full_hd=compat_hd_center && compat_hd_center_ready && compat_hd_overlay_texture;
    const SDL_Rect regions[3] = {
        {0,static_cast<int>(origin_y),left,static_cast<int>(128*sy)},
        {full_hd?left:right,static_cast<int>(origin_y),full_hd?std::max(0,right-left):0,static_cast<int>(128*sy)},
        {right,static_cast<int>(origin_y),std::max(0,width-right),static_cast<int>(128*sy)}};
    for(const SDL_Rect& side : regions) {
        if(side.w <= 0 || side.h <= 0) continue;
        SDL_RenderSetClipRect(renderer, &side);
        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderFillRect(renderer, &side);
        if(compat_wide_background_texture) {
            const SDL_FRect bg_area{center.x-256*sx,origin_y,768*sx,128*sy};
            SDL_RenderCopyF(renderer,compat_wide_background_texture,nullptr,&bg_area);
        }
        for(const auto& command:wide_commands) SDL_RenderGeometry(renderer,command.texture,
            compat_wide_triangles.data()+command.first,command.count,nullptr,0);
        if(compat_wide_sprite_texture) {
            const SDL_FRect sprite_area{center.x-256*sx,origin_y,768*sx,128*sy};
            SDL_RenderCopyF(renderer,compat_wide_sprite_texture,nullptr,&sprite_area);
        }
        if(full_hd) {
            const SDL_FRect overlay_area{origin_x,origin_y,208*sx,128*sy};
            SDL_RenderCopyF(renderer,compat_hd_overlay_texture,nullptr,&overlay_area);
        }
    }
    SDL_RenderSetClipRect(renderer,nullptr);
}
