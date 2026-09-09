// GPU side-viewport presentation of observed original camera-space polygons.
// No track reconstruction, persistent world cache, or emulated state writes.
static std::vector<SDL_Vertex> compat_wide_triangles;
struct WidePoint {float x,y,u,v;};
struct WideDrawCommand {SDL_Texture* texture;int first,count;};
static std::vector<WideDrawCommand> wide_commands;
#include "compat_wide_textures.h"
static float wide_cross(WidePoint a, WidePoint b, WidePoint c) {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
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
        WidePoint a[64], b[64];
        int clipped = 0;
        for (int i = 0; i < count; ++i) {
            const float* p = &face.xyz[i*3];
            const float* q = &face.xyz[((i+1)%count)*3];
            const float u=face.uv[i*2]/256, v=face.uv[i*2+1]/256;
            const int next=(i+1)%count;
            if(p[2] >= 1) a[clipped++] = {face.center_x+128*p[0]/p[2], face.center_y+128*p[1]/p[2],u,v};
            if((p[2] >= 1) != (q[2] >= 1)) {
                const float t=(1-p[2])/(q[2]-p[2]);
                a[clipped++]={face.center_x+128*(p[0]+t*(q[0]-p[0])),
                              face.center_y+128*(p[1]+t*(q[1]-p[1])),
                              u+t*(face.uv[next*2]/256-u),v+t*(face.uv[next*2+1]/256-v)};
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
        const uint32_t rgb = compat_wide_colors[face.color&255];
        SDL_Texture* image=face.textured ? wide_original_texture(face,palette) : nullptr;
        if(face.textured && !image) continue;
        const SDL_Color color=face.textured ? SDL_Color{255,255,255,255} : SDL_Color{static_cast<Uint8>(rgb>>16),
            static_cast<Uint8>(rgb>>8), static_cast<Uint8>(rgb),255};
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
    const SDL_Rect sides[2] = {{0,static_cast<int>(origin_y),left,static_cast<int>(128*sy)},
        {right,static_cast<int>(origin_y),std::max(0,width-right),static_cast<int>(128*sy)}};
    for(const SDL_Rect& side : sides) {
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
    }
    SDL_RenderSetClipRect(renderer,nullptr);
}
