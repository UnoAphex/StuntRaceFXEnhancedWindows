/*
 * Stunt Race FX compatibility frontend
 * Copyright (C) 2026
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, subject to inclusion of this notice. The Software is
 * provided "as is", without warranty of any kind.
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <compressapi.h>
#include <mmsystem.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "libretro.h"

static HMODULE core_module;
#define CORE_FN(name) static decltype(&name) core_##name
CORE_FN(retro_init); CORE_FN(retro_deinit); CORE_FN(retro_api_version);
CORE_FN(retro_get_system_info); CORE_FN(retro_get_system_av_info);
CORE_FN(retro_set_environment); CORE_FN(retro_set_video_refresh);
CORE_FN(retro_set_audio_sample); CORE_FN(retro_set_audio_sample_batch);
CORE_FN(retro_set_input_poll); CORE_FN(retro_set_input_state);
CORE_FN(retro_set_controller_port_device); CORE_FN(retro_load_game);
CORE_FN(retro_unload_game); CORE_FN(retro_run); CORE_FN(retro_reset);
CORE_FN(retro_serialize_size); CORE_FN(retro_serialize); CORE_FN(retro_unserialize);
CORE_FN(retro_get_memory_data); CORE_FN(retro_get_memory_size);
#undef CORE_FN

using srf_get_blob_t = const uint8_t* (*)(size_t*);
using srf_set_draw_distance_t = void (*)(int);
using srf_reset_trace_t = void (*)();
using srf_get_u64_t = uint64_t (*)();
static srf_get_blob_t core_srf_get_gsu_ram;
static srf_get_blob_t core_srf_get_gsu_registers;
static srf_get_blob_t core_srf_get_gsu_trace;
static srf_get_blob_t core_srf_get_ram_write_trace;
static srf_set_draw_distance_t core_srf_set_draw_distance;
static srf_reset_trace_t core_srf_reset_gsu_trace;
static srf_reset_trace_t core_srf_reset_gsu_profile;
static srf_get_u64_t core_srf_get_gsu_time_ns;
static srf_get_u64_t core_srf_get_gsu_instruction_count;
static srf_get_u64_t core_srf_get_gsu_exec_calls;
static void (*core_srf_start_geometry_probe)(uint32_t);
static const uint8_t* (*core_srf_get_geometry_probe)(size_t*, int);
static srf_reset_trace_t core_srf_start_wide_capture;
static srf_get_blob_t core_srf_get_wide_capture;
static srf_get_blob_t core_srf_get_cgram;
static srf_get_blob_t core_srf_get_wide_camera;
static srf_get_blob_t core_srf_get_wide_ppu, core_srf_get_vram;
static srf_get_blob_t core_srf_get_wide_background;
static srf_get_blob_t core_srf_get_wide_world_reference;
static srf_get_blob_t core_srf_get_wide_dma;
static srf_get_blob_t core_srf_get_wide_display_camera;
static srf_get_blob_t core_srf_get_wide_colors;
static srf_get_blob_t core_srf_get_wide_sprites;
static SDL_Texture* compat_wide_sprite_texture;
static std::vector<uint8_t> compat_wide_sprite_bytes;
static srf_reset_trace_t core_srf_reset_wide_capture;
static std::array<uint32_t,256> compat_wide_colors{};
static std::vector<uint8_t> compat_wide_display_bytes;
static SDL_Texture* compat_wide_background_texture;
static SDL_Texture* compat_hd_overlay_texture;
static std::vector<uint8_t> compat_wide_world_reference;
static std::vector<uint32_t> compat_hd_overlay_pixels;
static std::array<uint32_t,208*128> compat_hd_predicted{};
static uint64_t compat_hd_predicted_key;
static bool compat_hd_center;
static bool compat_hd_center_ready;
struct CompatWideCameraFace {
    uint16_t count, color;
    int16_t center_x, center_y;
    float xyz[96];
    float uv[64];
    uint16_t tex_bank,tex_base,tex_mask,color_mode;
    uint16_t scroll_u,scroll_v,textured,reserved;
};
static_assert(sizeof(CompatWideCameraFace) == 664, "Wide camera ABI");
static std::vector<CompatWideCameraFace> compat_wide_camera, compat_wide_camera_previous;
static bool compat_wide;
static bool compat_wide_observing;
static std::vector<uint16_t> compat_wide_polygons;
static std::vector<uint16_t> compat_wide_previous;
static std::array<uint16_t, 256> compat_wide_palette{};
static unsigned compat_wide_age = 120;

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_Texture* native_material_textures[9]{};
static SDL_GameController* controller;
static SDL_AudioDeviceID audio_device;
static retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
static int texture_width;
static int texture_height;
static Uint32 texture_format;
enum AspectMode { ASPECT_4_3, ASPECT_AMBIENT, ASPECT_STRETCH };
static AspectMode aspect_mode = ASPECT_4_3;
enum FilterMode { FILTER_NEAREST, FILTER_LINEAR, FILTER_EDGE };
static FilterMode filter_mode = FILTER_EDGE;
static bool show_fps;
static double measured_fps;
static double measured_scene_fps;
static double native_fps = 60.0988;
static int speed_mode;
static bool motion_smoothing;
static bool runahead_enabled;
static bool n64_materials = true;
static int draw_distance_mode = 1;
static int gsu_clock_percent = 300;
static bool paused;
static bool frame_advance;
static int core_av_enable = 3;
static int visual_preset = 4;
static bool replay_recording;
static bool replay_playing;
static size_t replay_position;
static uint16_t reported_input_mask;
static std::vector<uint16_t> replay_masks;
// Compatibility smoothing uses eighths so a new scene frame can begin mostly
// visible instead of forcing the old 50/50 blend that made steering feel late.
static int motion_step = 8;
static Uint64 toast_until;
static std::string toast_text;
static std::vector<uint32_t> source_pixels;
static std::vector<uint32_t> decode_pixels;
static std::vector<uint32_t> filtered_pixels;
static std::vector<uint32_t> motion_previous;
static std::vector<uint32_t> motion_target;
static std::vector<uint32_t> motion_pixels;
static std::vector<uint8_t> motion_local_pixels;
static std::vector<size_t> motion_changed_indices;
static std::vector<uint32_t> previous_scene_pixels;
static bool visual_frame_changed;
static std::string app_directory;
static uint64_t emulated_frame;
static unsigned source_width;
static unsigned source_height;
static std::vector<uint8_t> game_rom;
static bool reconstruction_recording;
static std::string reconstruction_session_directory;

enum RenderPath { RENDER_COMPATIBILITY, RENDER_NATIVE_TRACK, RENDER_HYBRID_COMPARE };
static RenderPath render_path = RENDER_COMPATIBILITY;
static int compat_draw_stream_debug;
static std::vector<uint16_t> compat_draw_stream;
static std::vector<uint16_t> compat_draw_stream_previous;
static unsigned compat_draw_stream_age;
static float native_vertical_fov = 58.0f;
static float native_draw_distance = 60000.0f;
// The course table's final word is the collision/search width used by the GSU.
// The visible road builder expands it before submitting the road surface.  Keep
// that presentation conversion PC-side and adjustable while it is calibrated
// against hybrid captures from additional courses.
// XLR8's authored road pieces use track_scale=5 (one source-space unit is 32
// world units).  The retail centerline table stores the smaller collision/search
// radius, not the visible polygon edge.  A 4x presentation conversion maps the
// first-course 330..605 radii to the 1,320..2,420 half-width range used by the
// authored 30..80-unit road pieces much more closely than the old 2x guess.
static float native_road_width_scale = 4.0f;
static bool native_fog = true;
static bool native_debug_bounds;
static bool native_debug_ids;
static bool native_cache_enabled = true;
static int native_segments_drawn;
static int native_segments_culled;

struct NativeVec3 {
    float x, y, z;
};

struct NativeTrackNode {
    NativeVec3 center;
    float half_width;
};

struct NativeTrackCache {
    uint8_t source_bank;
    uint16_t source_address;
    uint32_t generation;
    std::vector<NativeTrackNode> nodes;
    std::vector<NativeVec3> left;
    std::vector<NativeVec3> right;
    std::vector<NativeVec3> outer_left;
    std::vector<NativeVec3> outer_right;
    NativeVec3 bounds_min;
    NativeVec3 bounds_max;
    bool closed;
};

struct NativeCamera {
    NativeVec3 position;
    float matrix[9];
    bool valid;
};

struct NativeVehicle {
    NativeVec3 position;
    float axis_x;
    float axis_z;
    float pitch;
    float roll;
    float half_width;
    float half_length;
    bool player;
    bool ordered_wheel_pose;
};

static NativeTrackCache native_track;
static NativeCamera native_camera_previous{};
static NativeCamera native_camera_current{};
static float native_camera_alpha = 1.0f;
static std::vector<NativeVehicle> native_vehicles_previous;
static std::vector<NativeVehicle> native_vehicles_current;
static float native_vehicle_alpha = 1.0f;
enum MaterialKind {
    MATERIAL_GRASS,
    MATERIAL_ASPHALT,
    MATERIAL_GRAVEL,
    MATERIAL_DIRT,
    MATERIAL_SAND,
    MATERIAL_STONE,
    MATERIAL_SNOW,
    MATERIAL_WATER,
    MATERIAL_MUD,
    MATERIAL_COUNT,
    MATERIAL_NONE = -1
};
struct MaterialTexture {
    const char* filename;
    std::vector<uint32_t> pixels;
    unsigned width;
    unsigned height;
    int average_luma;
    int strength;
};
static MaterialTexture materials[MATERIAL_COUNT] = {
    {"grass-n64-v1.bmp", {}, 0, 0, 128, 160},
    {"asphalt-n64-v1.bmp", {}, 0, 0, 128, 260},
    {"gravel-n64-v1.bmp", {}, 0, 0, 128, 190},
    {"dirt-n64-v1.bmp", {}, 0, 0, 128, 190},
    {"sand-n64-v1.bmp", {}, 0, 0, 128, 170},
    {"stone-n64-v1.bmp", {}, 0, 0, 128, 190},
    {"snow-n64-v1.bmp", {}, 0, 0, 128, 150},
    {"water-n64-v1.bmp", {}, 0, 0, 128, 160},
    {"mud-n64-v1.bmp", {}, 0, 0, 128, 190},
};
static bool race_scene_active;
static int live_track_chunks;
static int live_display_mode = -1;
static int live_runtime_mode = -1;
static int live_nmi_mode = -1;
static bool native_race_motion_ready;
static unsigned native_race_warmup_frames;
static uint64_t native_last_race_nmi_frame = UINT64_MAX;
static int material_scroll_x;
static int material_scroll_z;

struct TrackVisibilityPatch {
    uint16_t address;
    uint16_t base_flags;
    uint16_t patched_flags;
};
static std::vector<TrackVisibilityPatch> track_visibility_patches;

struct ScriptedInput {
    uint64_t first_frame;
    uint64_t last_frame;
    uint16_t mask;
};
static std::vector<ScriptedInput> input_script;
static uint16_t scripted_input_mask;

struct FrameProfile {
    uint64_t frame;
    double total_ms;
    double processing_ms;
    double scene_state_ms;
    double core_ms;
    double gsu_ms;
    double video_ms;
    double decode_ms;
    double materials_ms;
    double interpolation_ms;
    double sprite_ms;
    double edge_ms;
    double upload_ms;
    double native_state_ms;
    double render_ms;
    double overlay_ms;
    double capture_ms;
    double present_ms;
    double audio_ms;
    double limiter_ms;
    uint64_t gsu_instructions;
    uint64_t gsu_calls;
    size_t changed_pixels;
    unsigned allocations;
    unsigned uploads;
    unsigned texture_recreates;
    unsigned render_copies;
    uint64_t audio_bytes;
    unsigned audio_callbacks;
    unsigned scene_interval_frames;
    unsigned scene_age_frames;
    bool broad_scene_update;
    bool scene_changed;
    bool race;
};

static FrameProfile active_profile{};
static FrameProfile last_profile{};
static FrameProfile profile_totals{};
static Uint64 profile_frequency;
static Uint64 profile_frame_start;
static FILE* profile_log;
static std::string profile_log_path;
static std::string renderer_backend = "UNKNOWN";
static Uint32 renderer_backend_flags;
static int profile_output_width;
static int profile_output_height;
static std::array<double, 600> frame_time_history{};
static size_t frame_time_history_count;
static size_t frame_time_history_cursor;
static uint64_t profile_frame_count;
static uint64_t last_scene_change_frame;
static double rolling_average_ms;
static double rolling_one_percent_low;
static double rolling_worst_ms;
static double profile_worst_ms;

static Uint64 profile_now() {
    return SDL_GetPerformanceCounter();
}

static double profile_elapsed_ms(Uint64 start, Uint64 end) {
    return profile_frequency
        ? static_cast<double>(end - start) * 1000.0 / static_cast<double>(profile_frequency)
        : 0.0;
}

static void profile_begin_frame() {
    active_profile = {};
    active_profile.frame = emulated_frame + 1;
    profile_frame_start = profile_now();
}

static void update_rolling_profile() {
    if (!frame_time_history_count) return;
    double sum = 0.0;
    rolling_worst_ms = 0.0;
    std::array<double, 600> sorted{};
    for (size_t index = 0; index < frame_time_history_count; ++index) {
        sorted[index] = frame_time_history[index];
        sum += sorted[index];
        rolling_worst_ms = std::max(rolling_worst_ms, sorted[index]);
    }
    std::sort(sorted.begin(), sorted.begin() + frame_time_history_count);
    const size_t p99_index = std::min(frame_time_history_count - 1,
        (frame_time_history_count * 99 + 99) / 100 - 1);
    const double p99_ms = sorted[p99_index];
    rolling_average_ms = sum / static_cast<double>(frame_time_history_count);
    rolling_one_percent_low = p99_ms > 0.0 ? 1000.0 / p99_ms : 0.0;
}

static void profile_accumulate(const FrameProfile& frame) {
    profile_totals.total_ms += frame.total_ms;
    profile_totals.processing_ms += frame.processing_ms;
    profile_totals.scene_state_ms += frame.scene_state_ms;
    profile_totals.core_ms += frame.core_ms;
    profile_totals.gsu_ms += frame.gsu_ms;
    profile_totals.video_ms += frame.video_ms;
    profile_totals.decode_ms += frame.decode_ms;
    profile_totals.materials_ms += frame.materials_ms;
    profile_totals.interpolation_ms += frame.interpolation_ms;
    profile_totals.sprite_ms += frame.sprite_ms;
    profile_totals.edge_ms += frame.edge_ms;
    profile_totals.upload_ms += frame.upload_ms;
    profile_totals.native_state_ms += frame.native_state_ms;
    profile_totals.render_ms += frame.render_ms;
    profile_totals.overlay_ms += frame.overlay_ms;
    profile_totals.capture_ms += frame.capture_ms;
    profile_totals.present_ms += frame.present_ms;
    profile_totals.audio_ms += frame.audio_ms;
    profile_totals.limiter_ms += frame.limiter_ms;
    profile_totals.gsu_instructions += frame.gsu_instructions;
    profile_totals.gsu_calls += frame.gsu_calls;
    profile_totals.allocations += frame.allocations;
    profile_totals.uploads += frame.uploads;
    profile_totals.texture_recreates += frame.texture_recreates;
    profile_totals.render_copies += frame.render_copies;
    profile_totals.audio_bytes += frame.audio_bytes;
    profile_totals.audio_callbacks += frame.audio_callbacks;
}

static void profile_commit_frame() {
    const Uint64 end = profile_now();
    active_profile.total_ms = profile_elapsed_ms(profile_frame_start, end);
    active_profile.processing_ms = std::max(0.0, active_profile.total_ms - active_profile.limiter_ms);
    active_profile.scene_changed = visual_frame_changed;
    active_profile.race = race_scene_active;
    if (visual_frame_changed) {
        active_profile.scene_interval_frames = last_scene_change_frame
            ? static_cast<unsigned>(active_profile.frame - last_scene_change_frame) : 0;
        last_scene_change_frame = active_profile.frame;
    }
    active_profile.scene_age_frames = last_scene_change_frame
        ? static_cast<unsigned>(active_profile.frame - last_scene_change_frame) : 0;

    frame_time_history[frame_time_history_cursor] = active_profile.total_ms;
    frame_time_history_cursor = (frame_time_history_cursor + 1) % frame_time_history.size();
    frame_time_history_count = std::min(frame_time_history_count + 1, frame_time_history.size());
    ++profile_frame_count;
    profile_worst_ms = std::max(profile_worst_ms, active_profile.total_ms);
    profile_accumulate(active_profile);
    if ((profile_frame_count % 30) == 0) update_rolling_profile();

    if (profile_log) {
        const double core_other = std::max(0.0,
            active_profile.core_ms - active_profile.video_ms - active_profile.gsu_ms);
        const double spike_basis = std::max(active_profile.total_ms, active_profile.processing_ms);
        const int spike = spike_basis > 33.3 ? 3 : (spike_basis > 25.0 ? 2 : (spike_basis > 20.0 ? 1 : 0));
        fprintf(profile_log,
            "%llu,%d,%d,%u,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%u,%u,%u,%llu,%u,%llu,%llu,%zu,%d,%d,%d,%d,%d,%d,%04X,%d\n",
            static_cast<unsigned long long>(active_profile.frame), active_profile.race ? 1 : 0,
            active_profile.scene_changed ? 1 : 0, active_profile.scene_interval_frames,
            active_profile.scene_age_frames, active_profile.total_ms, active_profile.processing_ms,
            active_profile.limiter_ms, active_profile.core_ms, active_profile.gsu_ms, core_other,
            active_profile.video_ms, active_profile.decode_ms, active_profile.materials_ms,
            active_profile.interpolation_ms, active_profile.sprite_ms, active_profile.edge_ms,
            active_profile.upload_ms, active_profile.native_state_ms, active_profile.render_ms,
            active_profile.overlay_ms, active_profile.present_ms, active_profile.audio_ms,
            active_profile.allocations,
            active_profile.uploads, active_profile.texture_recreates, active_profile.render_copies,
            static_cast<unsigned long long>(active_profile.audio_bytes),
            active_profile.audio_callbacks,
            static_cast<unsigned long long>(active_profile.gsu_instructions),
            static_cast<unsigned long long>(active_profile.gsu_calls), active_profile.changed_pixels,
            active_profile.broad_scene_update ? 1 : 0, live_track_chunks,
            static_cast<int>(filter_mode), static_cast<int>(aspect_mode),
            profile_output_width, profile_output_height, reported_input_mask, spike);
    }
    last_profile = active_profile;
}

static void show_error(const char* message) {
    MessageBoxA(nullptr, message, "Stunt Race FX", MB_OK | MB_ICONERROR);
}

template <class T>
static bool bind_core(T& output, const char* name) {
    output = reinterpret_cast<T>(GetProcAddress(core_module, name));
    if (output) return true;
    char message[512];
    snprintf(message, sizeof(message), "The emulator core is missing a required function:\n%s", name);
    show_error(message);
    return false;
}

static bool load_core_functions() {
#define BIND(name) if (!bind_core(core_##name, #name)) return false
    BIND(retro_init); BIND(retro_deinit); BIND(retro_api_version);
    BIND(retro_get_system_info); BIND(retro_get_system_av_info);
    BIND(retro_set_environment); BIND(retro_set_video_refresh);
    BIND(retro_set_audio_sample); BIND(retro_set_audio_sample_batch);
    BIND(retro_set_input_poll); BIND(retro_set_input_state);
    BIND(retro_set_controller_port_device); BIND(retro_load_game);
    BIND(retro_unload_game); BIND(retro_run); BIND(retro_reset);
    BIND(retro_serialize_size); BIND(retro_serialize); BIND(retro_unserialize);
    BIND(retro_get_memory_data); BIND(retro_get_memory_size);
#undef BIND
    core_srf_get_gsu_ram = reinterpret_cast<srf_get_blob_t>(
        GetProcAddress(core_module, "srf_get_gsu_ram"));
    core_srf_start_wide_capture = reinterpret_cast<srf_reset_trace_t>(GetProcAddress(core_module, "srf_start_wide_capture"));
    core_srf_get_wide_capture = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module, "srf_get_wide_capture"));
    core_srf_get_cgram = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module, "srf_get_cgram"));
    core_srf_get_wide_camera = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module, "srf_get_wide_camera"));
    core_srf_get_wide_ppu = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module, "srf_get_wide_ppu"));
    core_srf_get_vram = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module, "srf_get_vram"));
    core_srf_get_wide_background = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module,"srf_get_wide_background"));
    core_srf_get_wide_world_reference = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module,"srf_get_wide_world_reference"));
    core_srf_get_wide_dma = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module,"srf_get_wide_dma"));
    core_srf_get_wide_display_camera = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module,"srf_get_wide_display_camera"));
    core_srf_get_wide_colors = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module,"srf_get_wide_colors"));
    core_srf_get_wide_sprites = reinterpret_cast<srf_get_blob_t>(GetProcAddress(core_module,"srf_get_wide_sprites"));
    core_srf_reset_wide_capture = reinterpret_cast<srf_reset_trace_t>(GetProcAddress(core_module,"srf_reset_wide_capture"));
    core_srf_start_geometry_probe = reinterpret_cast<decltype(core_srf_start_geometry_probe)>(
        GetProcAddress(core_module, "srf_start_geometry_probe"));
    core_srf_get_geometry_probe = reinterpret_cast<decltype(core_srf_get_geometry_probe)>(
        GetProcAddress(core_module, "srf_get_geometry_probe"));
    core_srf_get_gsu_registers = reinterpret_cast<srf_get_blob_t>(
        GetProcAddress(core_module, "srf_get_gsu_registers"));
    core_srf_get_gsu_trace = reinterpret_cast<srf_get_blob_t>(
        GetProcAddress(core_module, "srf_get_gsu_trace"));
    core_srf_get_ram_write_trace = reinterpret_cast<srf_get_blob_t>(
        GetProcAddress(core_module, "srf_get_ram_write_trace"));
    core_srf_set_draw_distance = reinterpret_cast<srf_set_draw_distance_t>(
        GetProcAddress(core_module, "srf_set_draw_distance"));
    core_srf_reset_gsu_trace = reinterpret_cast<srf_reset_trace_t>(
        GetProcAddress(core_module, "srf_reset_gsu_trace"));
    core_srf_reset_gsu_profile = reinterpret_cast<srf_reset_trace_t>(
        GetProcAddress(core_module, "srf_reset_gsu_profile"));
    core_srf_get_gsu_time_ns = reinterpret_cast<srf_get_u64_t>(
        GetProcAddress(core_module, "srf_get_gsu_time_ns"));
    core_srf_get_gsu_instruction_count = reinterpret_cast<srf_get_u64_t>(
        GetProcAddress(core_module, "srf_get_gsu_instruction_count"));
    core_srf_get_gsu_exec_calls = reinterpret_cast<srf_get_u64_t>(
        GetProcAddress(core_module, "srf_get_gsu_exec_calls"));
    return true;
}

static void load_input_script() {
    const char* path = getenv("SRF_INPUT_SCRIPT");
    if (!path || !*path) return;
    FILE* file = fopen(path, "r");
    if (!file) return;
    char line[192];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        unsigned first = 0, last = 0, mask = 0;
        if (sscanf(line, "%u %u %x", &first, &last, &mask) == 3 && last >= first)
            input_script.push_back({first, last, static_cast<uint16_t>(mask)});
    }
    fclose(file);
    fprintf(stderr, "Input script: loaded %zu events from %s\n", input_script.size(), path);
}

static void update_scripted_input() {
    scripted_input_mask = 0;
    for (const ScriptedInput& event : input_script)
        if (emulated_frame >= event.first_frame && emulated_frame <= event.last_frame)
            scripted_input_mask |= event.mask;
}

static std::string replay_path() {
    return app_directory + "replay-last.srf";
}

static void save_replay() {
    FILE* file = fopen(replay_path().c_str(), "wb");
    if (!file) return;
    fprintf(file, "SRFREPLAY1\n");
    for (uint16_t mask : replay_masks) fprintf(file, "%04X\n", mask);
    fclose(file);
}

static bool load_replay() {
    FILE* file = fopen(replay_path().c_str(), "rb");
    if (!file) return false;
    char line[64];
    if (!fgets(line, sizeof(line), file) || strcmp(line, "SRFREPLAY1\n") != 0) {
        fclose(file);
        return false;
    }
    replay_masks.clear();
    unsigned mask = 0;
    while (fgets(line, sizeof(line), file))
        if (sscanf(line, "%x", &mask) == 1) replay_masks.push_back(static_cast<uint16_t>(mask));
    fclose(file);
    replay_position = 0;
    return !replay_masks.empty();
}

static uint16_t joypad_bit(unsigned id) {
    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_B:      return 0x8000;
        case RETRO_DEVICE_ID_JOYPAD_Y:      return 0x4000;
        case RETRO_DEVICE_ID_JOYPAD_SELECT: return 0x2000;
        case RETRO_DEVICE_ID_JOYPAD_START:  return 0x1000;
        case RETRO_DEVICE_ID_JOYPAD_UP:     return 0x0800;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:   return 0x0400;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:   return 0x0200;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:  return 0x0100;
        case RETRO_DEVICE_ID_JOYPAD_A:      return 0x0080;
        case RETRO_DEVICE_ID_JOYPAD_X:      return 0x0040;
        case RETRO_DEVICE_ID_JOYPAD_L:      return 0x0020;
        case RETRO_DEVICE_ID_JOYPAD_R:      return 0x0010;
        default: return 0;
    }
}

static bool capture_frame_selected(uint64_t frame) {
    const char* directory = getenv("SRF_GSU_CAPTURE_DIR");
    if (!directory || !*directory) return false;
    const char* from_text = getenv("SRF_GSU_CAPTURE_FROM");
    const char* to_text = getenv("SRF_GSU_CAPTURE_TO");
    const char* step_text = getenv("SRF_GSU_CAPTURE_STEP");
    const uint64_t from = from_text && *from_text ? _strtoui64(from_text, nullptr, 0) : 0;
    const uint64_t to = to_text && *to_text ? _strtoui64(to_text, nullptr, 0) : UINT64_MAX;
    uint64_t step = step_text && *step_text ? _strtoui64(step_text, nullptr, 0) : 1;
    if (!step) step = 1;
    return frame >= from && frame <= to && ((frame - from) % step) == 0;
}

static void write_blob(const char* directory, const char* kind,
                       const uint8_t* data, size_t size) {
    if (!data || !size) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s\\snes9x_f%06llu_%s.bin", directory,
             static_cast<unsigned long long>(emulated_frame), kind);
    FILE* file = fopen(path, "wb");
    if (!file) return;
    fwrite(data, 1, size, file);
    fclose(file);
}

static void capture_gsu_state() {
    if (!capture_frame_selected(emulated_frame)) return;
    const char* directory = getenv("SRF_GSU_CAPTURE_DIR");
    size_t size = 0;
    if (compat_wide) {
        if(core_srf_get_wide_sprites) { const uint8_t* p=core_srf_get_wide_sprites(&size);write_blob(directory,"wide_sprites",p,size); }
        if(core_srf_get_wide_world_reference) { const uint8_t* p=core_srf_get_wide_world_reference(&size);write_blob(directory,"wide_world_reference",p,size); }
        write_blob(directory,"wide_hd_overlay",reinterpret_cast<const uint8_t*>(compat_hd_overlay_pixels.data()),compat_hd_overlay_pixels.size()*4);
        write_blob(directory,"wide_display_camera",compat_wide_display_bytes.data(),compat_wide_display_bytes.size());
        if (core_srf_get_wide_dma) { const uint8_t* p=core_srf_get_wide_dma(&size); write_blob(directory,"wide_dma",p,size); }
        if (core_srf_get_wide_ppu) { const uint8_t* p = core_srf_get_wide_ppu(&size); write_blob(directory,"wide_ppu",p,size); }
        if (core_srf_get_vram) { const uint8_t* p = core_srf_get_vram(&size); write_blob(directory,"wide_vram",p,size); }
        write_blob(directory, "wide_polygons", reinterpret_cast<const uint8_t*>(compat_wide_polygons.data()), compat_wide_polygons.size() * 2);
        write_blob(directory, "wide_palette", reinterpret_cast<const uint8_t*>(compat_wide_palette.data()), 512);
        write_blob(directory, "wide_camera", reinterpret_cast<const uint8_t*>(compat_wide_camera.data()), compat_wide_camera.size()*sizeof(CompatWideCameraFace));
    }
    if (core_srf_get_geometry_probe && getenv("SRF_GEOMETRY_PROBE")) {
        const char* names[] = {"geometry_histogram", "geometry_registers", "geometry_status", "geometry_polygons"};
        for (int kind = 0; kind < 4; ++kind) {
            const uint8_t* data = core_srf_get_geometry_probe(&size, kind);
            write_blob(directory, names[kind], data, size);
        }
    }
    const uint8_t* system_ram = static_cast<const uint8_t*>(
        core_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    const size_t system_ram_size = core_retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    write_blob(directory, "wram", system_ram, system_ram_size);
    if (core_srf_get_gsu_ram) {
        const uint8_t* ram = core_srf_get_gsu_ram(&size);
        write_blob(directory, "ram", ram, size);
    } else {
        const uint8_t* ram = static_cast<const uint8_t*>(
            core_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        size = core_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
        write_blob(directory, "ram", ram, size);
    }
    if (core_srf_get_gsu_registers) {
        const uint8_t* registers = core_srf_get_gsu_registers(&size);
        write_blob(directory, "regs", registers, size);
    }
    if (core_srf_get_gsu_trace) {
        const uint8_t* trace = core_srf_get_gsu_trace(&size);
        write_blob(directory, "trace", trace, size);
    }
    if (core_srf_get_ram_write_trace) {
        const uint8_t* trace = core_srf_get_ram_write_trace(&size);
        write_blob(directory, "writes", trace, size);
    }

    const char* video_directory = getenv("SRF_VIDEO_CAPTURE_DIR");
    if (video_directory && *video_directory && source_width && source_height &&
        source_pixels.size() == static_cast<size_t>(source_width) * source_height) {
        char path[1024];
        snprintf(path, sizeof(path), "%s\\snes9x_f%06llu.ppm", video_directory,
                 static_cast<unsigned long long>(emulated_frame));
        FILE* file = fopen(path, "wb");
        if (file) {
            fprintf(file, "P6\n%u %u\n255\n", source_width, source_height);
            for (uint32_t pixel : source_pixels) {
                const uint8_t rgb[3] = {
                    static_cast<uint8_t>(pixel >> 16),
                    static_cast<uint8_t>(pixel >> 8),
                    static_cast<uint8_t>(pixel),
                };
                fwrite(rgb, 1, sizeof(rgb), file);
            }
            fclose(file);
        }
    }
}

static uint16_t ram_read16(const uint8_t* ram, size_t address) {
    return static_cast<uint16_t>(ram[address] | (static_cast<uint16_t>(ram[address + 1]) << 8));
}

static int16_t ram_read_s16(const uint8_t* ram, size_t address) {
    return static_cast<int16_t>(ram_read16(ram, address));
}

static void ram_write16(uint8_t* ram, size_t address, int16_t value) {
    const uint16_t encoded = static_cast<uint16_t>(value);
    ram[address] = static_cast<uint8_t>(encoded);
    ram[address + 1] = static_cast<uint8_t>(encoded >> 8);
}

static NativeVec3 native_add(NativeVec3 a, NativeVec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static NativeVec3 native_sub(NativeVec3 a, NativeVec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static NativeVec3 native_scale(NativeVec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

static float native_length_xz(NativeVec3 value) {
    return sqrtf(value.x * value.x + value.z * value.z);
}

static size_t lorom_offset(uint8_t bank, uint16_t address) {
    const size_t header = game_rom.size() % 0x8000u == 512u ? 512u : 0u;
    return header + (static_cast<size_t>(bank & 0x7f) * 0x8000u) + (address & 0x7fffu);
}

static int16_t rom_read_s16(size_t offset) {
    const uint16_t value = static_cast<uint16_t>(game_rom[offset] |
        (static_cast<uint16_t>(game_rom[offset + 1]) << 8));
    return static_cast<int16_t>(value);
}

static uint16_t rom_read16(size_t offset) {
    return static_cast<uint16_t>(game_rom[offset] |
        (static_cast<uint16_t>(game_rom[offset + 1]) << 8));
}

static void rebuild_native_track(uint8_t bank, uint16_t address) {
    const size_t offset = lorom_offset(bank, address);
    if (offset + 2 > game_rom.size()) return;
    const uint16_t count = rom_read16(offset);
    if (count < 4 || count > 512 || offset + 2u + static_cast<size_t>(count) * 8u > game_rom.size())
        return;

    std::vector<NativeTrackNode> nodes;
    nodes.reserve(count);
    for (uint16_t index = 0; index < count; ++index) {
        const size_t node_offset = offset + 2u + static_cast<size_t>(index) * 8u;
        NativeTrackNode node{};
        node.center = {
            static_cast<float>(rom_read_s16(node_offset + 0)),
            static_cast<float>(rom_read_s16(node_offset + 2)),
            static_cast<float>(rom_read_s16(node_offset + 4)),
        };
        const float encoded_width = static_cast<float>(rom_read16(node_offset + 6));
        if (encoded_width < 64.0f || encoded_width > 4096.0f) return;
        node.half_width = encoded_width * native_road_width_scale;
        nodes.push_back(node);
    }

    const NativeVec3 closing = native_sub(nodes.front().center, nodes.back().center);
    const bool closed = native_length_xz(closing) < 2400.0f;
    std::vector<NativeVec3> left(count), right(count), outer_left(count), outer_right(count);
    NativeVec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max()};
    NativeVec3 maximum{-minimum.x, -minimum.y, -minimum.z};
    for (size_t index = 0; index < nodes.size(); ++index) {
        const size_t previous = index ? index - 1 : (closed ? nodes.size() - 1 : index);
        const size_t next = index + 1 < nodes.size() ? index + 1 : (closed ? 0 : index);
        NativeVec3 tangent = native_sub(nodes[next].center, nodes[previous].center);
        float length = native_length_xz(tangent);
        if (length < 1.0f) {
            tangent = native_sub(nodes[next].center, nodes[index].center);
            length = native_length_xz(tangent);
        }
        const NativeVec3 perpendicular = length > 0.0f
            ? NativeVec3{-tangent.z / length, 0.0f, tangent.x / length}
            : NativeVec3{1.0f, 0.0f, 0.0f};
        left[index] = native_add(nodes[index].center,
                                 native_scale(perpendicular, nodes[index].half_width));
        right[index] = native_sub(nodes[index].center,
                                  native_scale(perpendicular, nodes[index].half_width));
        const float terrain_width = nodes[index].half_width + 5200.0f;
        outer_left[index] = native_add(nodes[index].center,
                                       native_scale(perpendicular, terrain_width));
        outer_right[index] = native_sub(nodes[index].center,
                                        native_scale(perpendicular, terrain_width));
        for (NativeVec3 point : {outer_left[index], outer_right[index]}) {
            minimum.x = std::min(minimum.x, point.x);
            minimum.y = std::min(minimum.y, point.y);
            minimum.z = std::min(minimum.z, point.z);
            maximum.x = std::max(maximum.x, point.x);
            maximum.y = std::max(maximum.y, point.y);
            maximum.z = std::max(maximum.z, point.z);
        }
    }

    native_track.source_bank = bank;
    native_track.source_address = address;
    ++native_track.generation;
    native_track.nodes.swap(nodes);
    native_track.left.swap(left);
    native_track.right.swap(right);
    native_track.outer_left.swap(outer_left);
    native_track.outer_right.swap(outer_right);
    native_track.bounds_min = minimum;
    native_track.bounds_max = maximum;
    native_track.closed = closed;
    fprintf(stderr, "Native track cache: %u nodes from %02X:%04X (%s)\n",
            count, bank, address, closed ? "closed" : "open");
}

static bool native_camera_changed(const NativeCamera& left, const NativeCamera& right) {
    if (!left.valid || !right.valid) return true;
    if (fabsf(left.position.x - right.position.x) > 0.5f ||
        fabsf(left.position.y - right.position.y) > 0.5f ||
        fabsf(left.position.z - right.position.z) > 0.5f) return true;
    for (int index = 0; index < 9; ++index)
        if (fabsf(left.matrix[index] - right.matrix[index]) > 0.0002f) return true;
    return false;
}

static std::vector<NativeVehicle> extract_native_vehicles(const uint8_t* ram, size_t size,
                                                          uint16_t scene_root) {
    std::vector<NativeVehicle> vehicles;
    std::vector<uint16_t> seen;
    uint16_t address = scene_root;
    while (address && static_cast<size_t>(address) + 0x26 <= size && seen.size() < 128) {
        if (std::find(seen.begin(), seen.end(), address) != seen.end()) break;
        seen.push_back(address);
        const uint16_t next = ram_read16(ram, address + 0x06);
        const int16_t record_kind = ram_read_s16(ram, address + 0x02);
        if ((record_kind == 0x0098 || record_kind == -0x0098) &&
            ram_read16(ram, address + 0x0a) == 0xb1a1) {
            NativeVehicle vehicle{};
            vehicle.position = {
                static_cast<float>(ram_read_s16(ram, address + 0x1e)),
                static_cast<float>(ram_read_s16(ram, address + 0x20)),
                static_cast<float>(ram_read_s16(ram, address + 0x22)),
            };
            vehicle.player = (ram_read16(ram, address + 0x1c) & 0x0008) != 0;
            std::vector<NativeVec3> corners;
            uint16_t child = next;
            for (int index = 0; index < 4 && child && static_cast<size_t>(child) + 0x26 <= size; ++index) {
                const int16_t child_kind = ram_read_s16(ram, child + 0x02);
                if (child_kind != 0x0052 && child_kind != -0x0052) break;
                corners.push_back({
                    static_cast<float>(ram_read_s16(ram, child + 0x1e)),
                    static_cast<float>(ram_read_s16(ram, child + 0x20)),
                    static_cast<float>(ram_read_s16(ram, child + 0x22)),
                });
                child = ram_read16(ram, child + 0x06);
            }
            vehicle.axis_x = 0.0f;
            vehicle.axis_z = 1.0f;
            vehicle.pitch = 0.0f;
            vehicle.roll = 0.0f;
            vehicle.half_width = 115.0f;
            vehicle.half_length = 155.0f;
            vehicle.ordered_wheel_pose = false;
            if (corners.size() == 4) {
                // The XLR8 car definitions create wheels in axle order: the
                // first pair belongs to one axle and the second pair to the
                // other.  Unlike PCA this gives us a directed longitudinal
                // axis, so steering cannot flip 180 degrees or drift toward
                // the course tangent.  Retail list order can be reversed, so
                // select the axle direction closest to the prior pose/course
                // direction below rather than guessing from screen motion.
                const NativeVec3 axle_a = native_scale(native_add(corners[0], corners[1]), 0.5f);
                const NativeVec3 axle_b = native_scale(native_add(corners[2], corners[3]), 0.5f);
                NativeVec3 longitudinal = native_sub(axle_a, axle_b);
                const float wheelbase = native_length_xz(longitudinal);
                const NativeVec3 side_a = native_scale(native_add(corners[0], corners[3]), 0.5f);
                const NativeVec3 side_b = native_scale(native_add(corners[1], corners[2]), 0.5f);
                const NativeVec3 lateral = native_sub(side_a, side_b);
                const float wheeltrack = native_length_xz(lateral);
                if (wheelbase > 24.0f && wheeltrack > 24.0f) {
                    vehicle.axis_x = longitudinal.x / wheelbase;
                    vehicle.axis_z = longitudinal.z / wheelbase;
                    vehicle.pitch = atan2f(longitudinal.y, wheelbase);
                    vehicle.roll = atan2f(lateral.y, wheeltrack);
                    // Bodywork extends beyond the wheel contact rectangle.
                    vehicle.half_length = std::max(120.0f, std::min(260.0f, wheelbase * 0.5f + 48.0f));
                    vehicle.half_width = std::max(85.0f, std::min(175.0f, wheeltrack * 0.5f + 28.0f));
                    vehicle.ordered_wheel_pose = true;
                }
            }
            if (!vehicle.ordered_wheel_pose && corners.size() >= 3) {
                float mean_x = 0.0f, mean_y = 0.0f, mean_z = 0.0f;
                for (const NativeVec3& corner : corners) {
                    mean_x += corner.x; mean_y += corner.y; mean_z += corner.z;
                }
                mean_x /= static_cast<float>(corners.size());
                mean_y /= static_cast<float>(corners.size());
                mean_z /= static_cast<float>(corners.size());
                float xx = 0.0f, xz = 0.0f, zz = 0.0f, xy = 0.0f, zy = 0.0f;
                for (const NativeVec3& corner : corners) {
                    const float x = corner.x - mean_x;
                    const float y = corner.y - mean_y;
                    const float z = corner.z - mean_z;
                    xx += x * x; xz += x * z; zz += z * z;
                    xy += x * y; zy += z * y;
                }
                const float angle = 0.5f * atan2f(2.0f * xz, xx - zz);
                float major_x = cosf(angle), major_z = sinf(angle);
                float major_extent = 0.0f, minor_extent = 0.0f;
                for (const NativeVec3& corner : corners) {
                    const float x = corner.x - mean_x;
                    const float z = corner.z - mean_z;
                    major_extent = std::max(major_extent, fabsf(x * major_x + z * major_z));
                    minor_extent = std::max(minor_extent, fabsf(-x * major_z + z * major_x));
                }
                if (major_extent < minor_extent) {
                    std::swap(major_extent, minor_extent);
                    const float old_x = major_x;
                    major_x = -major_z;
                    major_z = old_x;
                }
                vehicle.axis_x = major_x;
                vehicle.axis_z = major_z;
                const float determinant = xx * zz - xz * xz;
                if (fabsf(determinant) > 1.0f) {
                    const float plane_x = (xy * zz - zy * xz) / determinant;
                    const float plane_z = (zy * xx - xy * xz) / determinant;
                    const float side_x = -major_z;
                    const float side_z = major_x;
                    vehicle.pitch = atanf(plane_x * major_x + plane_z * major_z);
                    vehicle.roll = atanf(plane_x * side_x + plane_z * side_z);
                }
                // Child records give a useful heading, but their transient working
                // extents are not guaranteed to be a model-space bounding box.
                // Keep the debug proxy car-sized instead of allowing a reused slot
                // to turn into a screen-filling polygon.
                vehicle.half_length = std::max(120.0f, std::min(210.0f, major_extent));
                vehicle.half_width = std::max(85.0f, std::min(140.0f, minor_extent));
            }
            vehicles.push_back(vehicle);
        }
        address = next;
    }
    return vehicles;
}

static void update_native_live_state() {
    size_t size = 0;
    const uint8_t* ram = core_srf_get_gsu_ram ? core_srf_get_gsu_ram(&size) : nullptr;
    if (!ram || size < 0x3000) return;

    const uint8_t bank = static_cast<uint8_t>(ram_read16(ram, 0x1094));
    const uint16_t address = ram_read16(ram, 0x1092);
    if (native_cache_enabled && address >= 0x8000 &&
        (native_track.nodes.empty() || native_track.source_bank != bank ||
         native_track.source_address != address))
        rebuild_native_track(bank, address);

    const uint16_t scene_root = ram_read16(ram, 0x247a);
    if (!scene_root || static_cast<size_t>(scene_root) + 0x26 > size) return;
    std::vector<NativeVehicle> vehicles = extract_native_vehicles(ram, size, scene_root);
    NativeCamera sample{};
    sample.position = {
        static_cast<float>(ram_read_s16(ram, scene_root + 0x1e)),
        static_cast<float>(ram_read_s16(ram, scene_root + 0x20)),
        static_cast<float>(ram_read_s16(ram, scene_root + 0x22)),
    };
    for (int index = 0; index < 9; ++index)
        sample.matrix[index] = static_cast<float>(ram_read_s16(ram, 0x00e4 + index * 2)) / 32768.0f;
    const float raw_forward_length = sqrtf(sample.matrix[6] * sample.matrix[6] +
                                           sample.matrix[8] * sample.matrix[8]);
    if (raw_forward_length < 0.45f || raw_forward_length > 1.35f ||
        native_track.nodes.size() < 3) return;

    for (size_t index = 0; index < vehicles.size(); ++index) {
        // Wheel ordering gives a real axle direction, but the linked list may
        // begin at either axle. Resolve that single ambiguity using the last
        // authoritative pose, falling back to the current GSU view direction.
        const float reference_x = index < native_vehicles_current.size()
            ? native_vehicles_current[index].axis_x : sample.matrix[6] / raw_forward_length;
        const float reference_z = index < native_vehicles_current.size()
            ? native_vehicles_current[index].axis_z : sample.matrix[8] / raw_forward_length;
        if (vehicles[index].axis_x * reference_x + vehicles[index].axis_z * reference_z < 0.0f) {
            vehicles[index].axis_x = -vehicles[index].axis_x;
            vehicles[index].axis_z = -vehicles[index].axis_z;
            vehicles[index].pitch = -vehicles[index].pitch;
            vehicles[index].roll = -vehicles[index].roll;
        }
    }

    const NativeVehicle* followed_vehicle = nullptr;
    for (const NativeVehicle& vehicle : vehicles)
        if (vehicle.player) { followed_vehicle = &vehicle; break; }
    if (!followed_vehicle && !vehicles.empty()) followed_vehicle = &vehicles.front();

    // $00E4 is shared GSU work space and can contain a palette or secondary
    // render matrix between scene frames. Use it only to resolve the initial
    // direction, then build a stable camera from the immutable course tangent.
    size_t nearest_node = 0;
    float nearest_distance = std::numeric_limits<float>::max();
    for (size_t index = 0; index < native_track.nodes.size(); ++index) {
        const float distance = native_length_xz(native_sub(native_track.nodes[index].center,
                                                            sample.position));
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_node = index;
        }
    }
    const size_t previous_node = nearest_node ? nearest_node - 1 : native_track.nodes.size() - 1;
    const size_t next_node = (nearest_node + 1) % native_track.nodes.size();
    const NativeVec3 tangent = native_sub(native_track.nodes[next_node].center,
                                          native_track.nodes[previous_node].center);
    const float tangent_length = native_length_xz(tangent);
    if (tangent_length < 1.0f) return;
    float forward_x = tangent.x / tangent_length;
    float forward_z = tangent.z / tangent_length;
    const float reference_x = followed_vehicle
        ? followed_vehicle->axis_x
        : (native_camera_current.valid ? native_camera_current.matrix[6]
                                       : sample.matrix[6] / raw_forward_length);
    const float reference_z = followed_vehicle
        ? followed_vehicle->axis_z
        : (native_camera_current.valid ? native_camera_current.matrix[8]
                                       : sample.matrix[8] / raw_forward_length);
    if (followed_vehicle && followed_vehicle->ordered_wheel_pose) {
        // Follow the actual steered chassis rather than aiming the camera down
        // the centerline. This keeps native vehicle motion locked to gameplay.
        forward_x = followed_vehicle->axis_x;
        forward_z = followed_vehicle->axis_z;
        sample.position = followed_vehicle->position;
    } else if (forward_x * reference_x + forward_z * reference_z < 0.0f) {
        forward_x = -forward_x;
        forward_z = -forward_z;
    }
    const float stable_matrix[9] = {
        forward_z, 0.0f, -forward_x,
        0.0f, 1.0f, 0.0f,
        forward_x, 0.0f, forward_z,
    };
    memcpy(sample.matrix, stable_matrix, sizeof(stable_matrix));
    // $247A identifies the followed vehicle/object rather than a fully offset
    // eye point.  Reconstruct the chase camera so the authoritative player
    // object sits in front of the native camera instead of at its near plane.
    sample.position.x -= forward_x * 1550.0f;
    sample.position.y -= 430.0f;
    sample.position.z -= forward_z * 1550.0f;
    sample.valid = true;
    if (!native_camera_current.valid) {
        native_camera_previous = native_camera_current = sample;
        native_camera_alpha = 1.0f;
    } else if (native_camera_changed(native_camera_current, sample)) {
        native_camera_previous = native_camera_current;
        native_camera_current = sample;
        native_camera_alpha = 0.0f;
    } else {
        native_camera_alpha = std::min(1.0f, native_camera_alpha + 0.34f);
    }

    bool vehicles_changed = vehicles.size() != native_vehicles_current.size();
    if (!vehicles_changed)
        for (size_t index = 0; index < vehicles.size(); ++index)
            if (native_length_xz(native_sub(vehicles[index].position,
                                            native_vehicles_current[index].position)) > 0.5f) {
                vehicles_changed = true;
                break;
            }
    if (native_vehicles_current.empty()) {
        native_vehicles_previous = native_vehicles_current = vehicles;
        native_vehicle_alpha = 1.0f;
    } else if (vehicles_changed) {
        native_vehicles_previous = native_vehicles_current;
        native_vehicles_current = vehicles;
        native_vehicle_alpha = 0.0f;
    } else {
        native_vehicle_alpha = std::min(1.0f, native_vehicle_alpha + 0.34f);
    }
}

static TrackVisibilityPatch* find_track_visibility_patch(uint16_t address) {
    for (TrackVisibilityPatch& patch : track_visibility_patches)
        if (patch.address == address) return &patch;
    return nullptr;
}

static void restore_track_visibility() {
    if (core_srf_set_draw_distance) core_srf_set_draw_distance(0);
    size_t size = 0;
    uint8_t* ram = nullptr;
    if (core_srf_get_gsu_ram) {
        ram = const_cast<uint8_t*>(core_srf_get_gsu_ram(&size));
    } else {
        ram = static_cast<uint8_t*>(core_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        size = core_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    }
    if (!ram || size < 0x2400) {
        track_visibility_patches.clear();
        return;
    }
    for (const TrackVisibilityPatch& patch : track_visibility_patches) {
        if (static_cast<size_t>(patch.address) + 0x1E > size) continue;
        if (ram_read16(ram, patch.address + 0x02) != 42) continue;
        if (ram_read16(ram, patch.address + 0x1C) == patch.patched_flags)
            ram_write16(ram, patch.address + 0x1C, static_cast<int16_t>(patch.base_flags));
    }
    track_visibility_patches.clear();
}

static void update_scene_and_draw_distance() {
    if (core_srf_set_draw_distance) core_srf_set_draw_distance(draw_distance_mode);
    size_t size = 0;
    uint8_t* ram = nullptr;
    if (core_srf_get_gsu_ram) {
        ram = const_cast<uint8_t*>(core_srf_get_gsu_ram(&size));
    } else {
        ram = static_cast<uint8_t*>(core_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        size = core_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    }
    live_track_chunks = 0;
    const bool was_race_scene_active = race_scene_active;
    race_scene_active = false;
    if (!ram || size < 0x3000) return;

    const uint16_t scene_root = ram_read16(ram, 0x247A);
    const int camera_x = scene_root && static_cast<size_t>(scene_root) + 0x24 <= size
        ? ram_read_s16(ram, scene_root + 0x1E) : 0;
    const int camera_z = scene_root && static_cast<size_t>(scene_root) + 0x24 <= size
        ? ram_read_s16(ram, scene_root + 0x22) : 0;
    const int radius = draw_distance_mode == 2 ? 30000 : 20000;
    const int64_t radius_squared = static_cast<int64_t>(radius) * radius;
    int track_chunks_total = 0;

    for (uint16_t address = 0x2000; address < 0x2400; address = static_cast<uint16_t>(address + 2)) {
        if (static_cast<size_t>(address) + 0x24 > size) break;
        if (ram_read16(ram, address + 0x02) != 42 || ram_read16(ram, address + 0x08) != 0x1D70)
            continue;
        ++track_chunks_total;
        const uint16_t current_flags = ram_read16(ram, address + 0x1C);
        if (core_srf_set_draw_distance) {
            if (current_flags & 0x0004) ++live_track_chunks;
            continue;
        }
        TrackVisibilityPatch* patch = find_track_visibility_patch(address);
        if (!patch) {
            track_visibility_patches.push_back({address, current_flags, current_flags});
            patch = &track_visibility_patches.back();
        } else if (current_flags != patch->base_flags && current_flags != patch->patched_flags) {
            patch->base_flags = current_flags;
        }

        uint16_t desired_flags = patch->base_flags;
        if (draw_distance_mode && scene_root) {
            const int dx = static_cast<int>(ram_read_s16(ram, address + 0x1E)) - camera_x;
            const int dz = static_cast<int>(ram_read_s16(ram, address + 0x22)) - camera_z;
            const int64_t distance_squared = static_cast<int64_t>(dx) * dx +
                                             static_cast<int64_t>(dz) * dz;
            if (distance_squared <= radius_squared) desired_flags |= 0x0004;
        }
        patch->patched_flags = desired_flags;
        ram_write16(ram, address + 0x1C, static_cast<int16_t>(desired_flags));
        if (desired_flags & 0x0004) ++live_track_chunks;
    }
    const uint8_t* system_ram = static_cast<const uint8_t*>(
        core_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    const size_t system_ram_size = core_retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    live_display_mode = system_ram && system_ram_size > 0x0D2B ? system_ram[0x0D2B] : -1;
    live_runtime_mode = system_ram && system_ram_size > 0x0D62 ? system_ram[0x0D62] : -1;
    live_nmi_mode = system_ram && system_ram_size > 0x0D3F ? system_ram[0x0D3F] : -1;
    const bool race_nmi_pulse = live_nmi_mode == 0x04 || live_nmi_mode == 0x06 ||
                                live_nmi_mode == 0x08 || live_nmi_mode == 0x0A ||
                                live_nmi_mode == 0x0C || live_nmi_mode == 0x0E;
    if (race_nmi_pulse) native_last_race_nmi_frame = emulated_frame;
    // Race NMI work alternates with common display work. Debounce the verified
    // race-only states without treating menu state $10 as a race.
    const bool race_display_mode = native_last_race_nmi_frame != UINT64_MAX &&
        emulated_frame - native_last_race_nmi_frame <= 60;
    // A course pointer and scene root remain resident across menus/transitions.
    // Only replace the original framebuffer while the game is actively
    // submitting visible race chunks in its verified race display mode.
    race_scene_active = race_display_mode && !native_track.nodes.empty();
    if (!race_scene_active || !was_race_scene_active) {
        native_race_motion_ready = false;
        native_race_warmup_frames = 0;
        native_camera_previous.valid = false;
        native_camera_current.valid = false;
    } else {
        ++native_race_warmup_frames;
        // Keep the complete original presentation through the grid overview,
        // loading masks, and countdown. The native camera becomes eligible only
        // after the race viewport has remained continuously active.
        if (native_race_warmup_frames >= 650)
            native_race_motion_ready = true;
    }
    if (scene_root && static_cast<size_t>(scene_root) + 0x24 <= size) {
        material_scroll_x = ram_read_s16(ram, scene_root + 0x1E) >> 3;
        material_scroll_z = ram_read_s16(ram, scene_root + 0x22) >> 3;
    }
    if (!race_scene_active && !track_visibility_patches.empty()) restore_track_visibility();
}

static bool load_material_bitmap(const char* filename, std::vector<uint32_t>& pixels,
                                 unsigned& width, unsigned& height, int& average_luma) {
    const std::string path = app_directory + "assets\\" + filename;
    SDL_Surface* loaded = SDL_LoadBMP(path.c_str());
    if (!loaded) return false;
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(loaded);
    if (!converted) return false;
    width = static_cast<unsigned>(converted->w);
    height = static_cast<unsigned>(converted->h);
    pixels.resize(static_cast<size_t>(width) * height);
    uint64_t total_luma = 0;
    for (unsigned y = 0; y < height; ++y) {
        const uint32_t* row = reinterpret_cast<const uint32_t*>(
            static_cast<const uint8_t*>(converted->pixels) + y * converted->pitch);
        for (unsigned x = 0; x < width; ++x) {
            const uint32_t color = row[x] | 0xff000000u;
            pixels[static_cast<size_t>(y) * width + x] = color;
            total_luma += (((color >> 16) & 255u) * 54u + ((color >> 8) & 255u) * 183u +
                           (color & 255u) * 19u) >> 8;
        }
    }
    SDL_FreeSurface(converted);
    average_luma = pixels.empty() ? 128 : static_cast<int>(total_luma / pixels.size());
    return !pixels.empty();
}

static bool load_materials() {
    bool loaded_all = true;
    for (MaterialTexture& material : materials) {
        if (!load_material_bitmap(material.filename, material.pixels, material.width,
                                  material.height, material.average_luma))
            loaded_all = false;
    }
    return loaded_all;
}

static bool environment_callback(unsigned command, void* data) {
    switch (command) {
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            pixel_format = *static_cast<const retro_pixel_format*>(data);
            return pixel_format == RETRO_PIXEL_FORMAT_0RGB1555 ||
                   pixel_format == RETRO_PIXEL_FORMAT_RGB565 ||
                   pixel_format == RETRO_PIXEL_FORMAT_XRGB8888;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *static_cast<const char**>(data) = app_directory.c_str();
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *static_cast<bool*>(data) = false;
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            retro_variable* variable = static_cast<retro_variable*>(data);
            if (!strcmp(variable->key, "snes9x_overclock_superfx")) {
                const char* requested = getenv("SRF_GSU_CLOCK");
                variable->value = requested && *requested ? requested : "300%";
                gsu_clock_percent = atoi(variable->value);
                return true;
            }
            if (!strcmp(variable->key, "snes9x_superfx_timing")) {
                variable->value = "compat";
                return true;
            }
            if (!strcmp(variable->key, "snes9x_overclock_cycles")) {
                variable->value = "disabled";
                return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
            *static_cast<int*>(data) = core_av_enable;
            return true;
        case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
            return true;
        default:
            return false;
    }
}

static Uint32 sdl_pixel_format() {
    if (pixel_format == RETRO_PIXEL_FORMAT_XRGB8888) return SDL_PIXELFORMAT_ARGB8888;
    if (pixel_format == RETRO_PIXEL_FORMAT_RGB565) return SDL_PIXELFORMAT_RGB565;
    return SDL_PIXELFORMAT_ARGB1555;
}

static bool ensure_texture(unsigned width, unsigned height, Uint32 format) {
    if (texture && texture_width == static_cast<int>(width) &&
        texture_height == static_cast<int>(height) && texture_format == format) return true;
    if (texture) SDL_DestroyTexture(texture);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                filter_mode == FILTER_NEAREST ? "0" : "2");
    texture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING,
                                width, height);
    ++active_profile.texture_recreates;
    texture_width = static_cast<int>(width);
    texture_height = static_cast<int>(height);
    texture_format = format;
    return texture != nullptr;
}

static uint32_t read_source_pixel(const uint8_t* row, unsigned x) {
    if (pixel_format == RETRO_PIXEL_FORMAT_XRGB8888) {
        uint32_t value;
        memcpy(&value, row + x * 4, sizeof(value));
        return value | 0xff000000u;
    }
    uint16_t value;
    memcpy(&value, row + x * 2, sizeof(value));
    if (pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
        const uint32_t r = ((value >> 11) & 31u) * 255u / 31u;
        const uint32_t g = ((value >> 5) & 63u) * 255u / 63u;
        const uint32_t b = (value & 31u) * 255u / 31u;
        return 0xff000000u | (r << 16) | (g << 8) | b;
    }
    const uint32_t r = ((value >> 10) & 31u) * 255u / 31u;
    const uint32_t g = ((value >> 5) & 31u) * 255u / 31u;
    const uint32_t b = (value & 31u) * 255u / 31u;
    return 0xff000000u | (r << 16) | (g << 8) | b;
}

static int color_distance(uint32_t a, uint32_t b) {
    return abs(static_cast<int>((a >> 16) & 255u) - static_cast<int>((b >> 16) & 255u)) +
           abs(static_cast<int>((a >> 8) & 255u) - static_cast<int>((b >> 8) & 255u)) +
           abs(static_cast<int>(a & 255u) - static_cast<int>(b & 255u));
}

static uint32_t blend_edge(uint32_t center, uint32_t a, uint32_t b) {
    const uint32_t ar = ((a >> 16) & 255u), ag = ((a >> 8) & 255u), ab = (a & 255u);
    const uint32_t br = ((b >> 16) & 255u), bg = ((b >> 8) & 255u), bb = (b & 255u);
    const uint32_t cr = ((center >> 16) & 255u), cg = ((center >> 8) & 255u), cb = (center & 255u);
    const uint32_t r = (cr * 2u + ar + br) / 4u;
    const uint32_t g = (cg * 2u + ag + bg) / 4u;
    const uint32_t blue = (cb * 2u + ab + bb) / 4u;
    return 0xff000000u | (r << 16) | (g << 8) | blue;
}

static uint32_t blend_motion(uint32_t previous, uint32_t target, int step) {
    const unsigned inverse = static_cast<unsigned>(8 - step);
    const unsigned amount = static_cast<unsigned>(step);
    const unsigned r = (((previous >> 16) & 255u) * inverse + ((target >> 16) & 255u) * amount) / 8u;
    const unsigned g = (((previous >> 8) & 255u) * inverse + ((target >> 8) & 255u) * amount) / 8u;
    const unsigned b = ((previous & 255u) * inverse + (target & 255u) * amount) / 8u;
    return 0xff000000u | (r << 16) | (g << 8) | b;
}

static const std::vector<uint32_t>& apply_motion_smoothing() {
    if (!motion_smoothing) {
        motion_previous.clear();
        motion_target.clear();
        motion_pixels.clear();
        motion_local_pixels.clear();
        motion_step = 8;
        return source_pixels;
    }

    // Menus and other non-race scenes are 2D presentations. Keep every original
    // animation frame instead of trying to classify their motion as a 3D update.
    if (!race_scene_active) {
        motion_previous = source_pixels;
        motion_target = source_pixels;
        motion_pixels = source_pixels;
        motion_local_pixels.assign(source_pixels.size(), 0);
        motion_step = 8;
        return motion_pixels;
    }

    if (motion_target.size() != source_pixels.size()) {
        motion_previous = source_pixels;
        motion_target = source_pixels;
        motion_pixels = source_pixels;
        motion_local_pixels.assign(source_pixels.size(), 0);
        motion_step = 8;
        return motion_pixels;
    }

    // Compare every source pixel. The old 1-in-4 sampling plus a 2% threshold
    // could entirely miss a bobbing car or a small pause-menu animation.
    motion_changed_indices.clear();
    if (motion_changed_indices.capacity() < source_pixels.size() / 32) {
        motion_changed_indices.reserve(source_pixels.size() / 32);
        ++active_profile.allocations;
    }
    for (size_t index = 0; index < source_pixels.size(); ++index) {
        if (color_distance(source_pixels[index], motion_target[index]) > 24)
            motion_changed_indices.push_back(index);
    }

    const bool broad_scene_update =
        motion_changed_indices.size() * 100 > source_pixels.size() * 2;
    active_profile.changed_pixels = motion_changed_indices.size();
    active_profile.broad_scene_update = broad_scene_update;
    motion_local_pixels.assign(source_pixels.size(), 0);

    if (broad_scene_update) {
        motion_previous = motion_target;
        motion_target = source_pixels;
        // Begin at 75% newest frame normally and 87.5% while steering. The old
        // implementation began at 50%, creating visible input-response delay.
        const bool steering = (reported_input_mask & 0x0300u) != 0;
        motion_step = steering ? 7 : 6;
    } else {
        const Uint64 sprite_start = profile_now();
        if (!motion_changed_indices.empty()) {
            motion_target = source_pixels;

            // Sparse updates are usually SNES tile/sprite animation rather than
            // a newly rendered 3D scene. Pass each affected 8x8 tile through at
            // native cadence while the remaining scene completes its soft blend.
            for (size_t changed_index : motion_changed_indices) {
                const unsigned changed_x =
                    static_cast<unsigned>(changed_index % source_width);
                const unsigned changed_y =
                    static_cast<unsigned>(changed_index / source_width);
                const unsigned first_x = (changed_x / 8) * 8;
                const unsigned first_y = (changed_y / 8) * 8;
                const unsigned last_x = std::min(first_x + 8, source_width);
                const unsigned last_y = std::min(first_y + 8, source_height);
                for (unsigned y = first_y; y < last_y; ++y)
                    for (unsigned x = first_x; x < last_x; ++x)
                        motion_local_pixels[static_cast<size_t>(y) * source_width + x] = 1;
            }
        }
        active_profile.sprite_ms += profile_elapsed_ms(sprite_start, profile_now());
        if (motion_step < 8) ++motion_step;
    }

    motion_pixels.resize(source_pixels.size());
    for (size_t index = 0; index < source_pixels.size(); ++index) {
        const unsigned x = source_width ? static_cast<unsigned>(index % source_width) : 0;
        const unsigned y = source_width ? static_cast<unsigned>(index / source_width) : 0;
        // Never delay the HUD, the player-car/control-feedback region, or a
        // locally animated 2D tile. High-contrast changing edges also use the
        // newest frame immediately to prevent doubled silhouettes.
        const bool hud = source_height && y >= source_height * 3 / 4;
        const bool player_feedback = source_width && source_height &&
            y >= source_height * 23 / 100 && y < source_height * 3 / 4 &&
            x >= source_width / 4 && x <= source_width * 3 / 4;
        const bool local_animation =
            index < motion_local_pixels.size() && motion_local_pixels[index] != 0;
        const bool changing_edge =
            color_distance(motion_previous[index], motion_target[index]) > 180;
        motion_pixels[index] = (hud || player_feedback || local_animation || changing_edge)
            ? motion_target[index]
            : blend_motion(motion_previous[index], motion_target[index], motion_step);
    }
    return motion_pixels;
}

static MaterialKind classify_material(uint32_t color) {
    const int r = static_cast<int>((color >> 16) & 255u);
    const int g = static_cast<int>((color >> 8) & 255u);
    const int b = static_cast<int>(color & 255u);
    const int brightest = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const int darkest = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const int luma = (r * 54 + g * 183 + b * 19) >> 8;
    const int chroma = brightest - darkest;

    if (g >= 55 && g >= r + 18 && g >= b + 14) return MATERIAL_GRASS;
    if (b >= 70 && b >= r + 28 && b >= g + 10) return MATERIAL_WATER;
    if (r >= g + 7 && g >= b - 8 && r >= b + 18) {
        if (luma >= 150) return MATERIAL_SAND;
        if (luma < 72) return MATERIAL_MUD;
        return MATERIAL_DIRT;
    }
    if (chroma <= 38) {
        // Neutral white is usually painted track furniture. Reserve snow for
        // the distinctly blue-white palette used by frozen terrain.
        if (luma >= 198 && b >= r + 6 && b >= g + 2) return MATERIAL_SNOW;
        if (luma >= 198) return MATERIAL_NONE;
        if (luma >= 164) return MATERIAL_STONE;
        if (luma >= 120) return MATERIAL_GRAVEL;
        if (luma >= 42) return MATERIAL_ASPHALT;
    }
    return MATERIAL_NONE;
}

static int wrapped_coordinate(int value, unsigned size) {
    if (!size) return 0;
    int result = value % static_cast<int>(size);
    return result < 0 ? result + static_cast<int>(size) : result;
}

static uint32_t material_modulate(uint32_t base, uint32_t material, int average_luma,
                                  int strength) {
    const int mr = static_cast<int>((material >> 16) & 255u);
    const int mg = static_cast<int>((material >> 8) & 255u);
    const int mb = static_cast<int>(material & 255u);
    const int luma = (mr * 54 + mg * 183 + mb * 19) >> 8;
    int factor = 256 + (luma - average_luma) * strength / 100;
    if (factor < 160) factor = 160;
    if (factor > 352) factor = 352;
    int r = static_cast<int>((base >> 16) & 255u) * factor / 256;
    int g = static_cast<int>((base >> 8) & 255u) * factor / 256;
    int b = static_cast<int>(base & 255u) * factor / 256;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return 0xff000000u | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

static void apply_n64_materials() {
    if (!n64_materials || !race_scene_active || source_pixels.empty()) return;
    const unsigned first_y = draw_distance_mode == 2 ? source_height / 4 :
        (draw_distance_mode == 1 ? source_height * 3 / 10 : source_height / 3);
    const unsigned last_y = source_height * 3 / 5;
    for (unsigned y = first_y; y < last_y; ++y) {
        int perspective_scale = 1 + static_cast<int>(y - first_y) /
                                      static_cast<int>(source_height / 6 + 1);
        if (perspective_scale > 4) perspective_scale = 4;
        for (unsigned x = 0; x < source_width; ++x) {
            // The compatibility material pass is a presentation effect for the
            // race image, not the surrounding SNES bezel or menu frame.
            if (render_path==RENDER_COMPATIBILITY && source_width==256 && source_height==224 &&
                (x<=24 || x>=231 || y<32 || y>=160)) continue;
            uint32_t& color = source_pixels[static_cast<size_t>(y) * source_width + x];
            // Protect the cockpit/car silhouette. Surface polygons on either side
            // still receive detail while vehicle paint remains untouched.
            const bool upper_car = y >= source_height * 27 / 100 &&
                x >= source_width * 39 / 100 && x <= source_width * 61 / 100;
            const bool lower_car = y >= source_height * 2 / 5 &&
                x >= source_width * 7 / 20 && x <= source_width * 13 / 20;
            if (upper_car || lower_car) continue;
            // Texturing isolated sprite pixels makes them shimmer. Require broad
            // flat-color support, characteristic of the game's ground polygons.
            const unsigned sample_left = x >= 5 ? x - 5 : x;
            const unsigned sample_right = x + 5 < source_width ? x + 5 : x;
            // Use the unmodified current frame for classification support. The
            // left neighbor in source_pixels may already have a texture applied.
            const auto& support = render_path == RENDER_COMPATIBILITY &&
                previous_scene_pixels.size()==source_pixels.size() ? previous_scene_pixels : source_pixels;
            const uint32_t left = support[static_cast<size_t>(y) * source_width + sample_left];
            const uint32_t right = support[static_cast<size_t>(y) * source_width + sample_right];
            if (color_distance(color, left) > 28 && color_distance(color, right) > 28) continue;

            const MaterialKind kind = classify_material(color);
            if (kind == MATERIAL_NONE) continue;
            const MaterialTexture& material = materials[kind];
            if (material.pixels.empty() || !material.width || !material.height) continue;
            const int u = static_cast<int>(x) * perspective_scale + material_scroll_x;
            const int v = static_cast<int>(y) * perspective_scale + material_scroll_z;
            const unsigned tx = static_cast<unsigned>(wrapped_coordinate(u, material.width));
            const unsigned ty = static_cast<unsigned>(wrapped_coordinate(v, material.height));
            color = material_modulate(color,
                material.pixels[static_cast<size_t>(ty) * material.width + tx],
                material.average_luma, material.strength);
        }
    }
}

static void decode_source_frame(const void* pixels, unsigned width, unsigned height, size_t pitch) {
    source_width = width;
    source_height = height;
    const size_t pixel_count = static_cast<size_t>(width) * height;
    if (decode_pixels.size() != pixel_count) {
        decode_pixels.resize(pixel_count);
        ++active_profile.allocations;
    }
    for (unsigned y = 0; y < height; ++y) {
        const uint8_t* row = static_cast<const uint8_t*>(pixels) + y * pitch;
        for (unsigned x = 0; x < width; ++x)
            decode_pixels[static_cast<size_t>(y) * width + x] = read_source_pixel(row, x);
    }

    visual_frame_changed = false;
    if (previous_scene_pixels.size() == decode_pixels.size()) {
        size_t tested = 0, changed = 0;
        const unsigned first_y = height / 10;
        const unsigned last_y = height * 4 / 5;
        for (unsigned y = first_y; y < last_y; y += 2)
            for (unsigned x = width / 12; x < width * 11 / 12; x += 2) {
                const size_t index = static_cast<size_t>(y) * width + x;
                ++tested;
                if (color_distance(decode_pixels[index], previous_scene_pixels[index]) > 30)
                    ++changed;
            }
        visual_frame_changed = tested && changed * 200 > tested;
    }
    previous_scene_pixels = decode_pixels;
    source_pixels.swap(decode_pixels);
}

static void edge_upscale(unsigned width, unsigned height) {
    const Uint64 interpolation_start = profile_now();
    const std::vector<uint32_t>& input_pixels = apply_motion_smoothing();
    active_profile.interpolation_ms += profile_elapsed_ms(interpolation_start, profile_now());
    const Uint64 edge_start = profile_now();
    const unsigned output_width = width * 2;
    if (filtered_pixels.capacity() < static_cast<size_t>(output_width) * height * 2)
        ++active_profile.allocations;
    filtered_pixels.resize(static_cast<size_t>(output_width) * height * 2);
    const int match_threshold = 36;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const uint32_t c = input_pixels[static_cast<size_t>(y) * width + x];
            const uint32_t n = input_pixels[static_cast<size_t>(y ? y - 1 : y) * width + x];
            const uint32_t s = input_pixels[static_cast<size_t>(y + 1 < height ? y + 1 : y) * width + x];
            const uint32_t w = input_pixels[static_cast<size_t>(y) * width + (x ? x - 1 : x)];
            const uint32_t e = input_pixels[static_cast<size_t>(y) * width + (x + 1 < width ? x + 1 : x)];
            uint32_t tl = c, tr = c, bl = c, br = c;
            if (color_distance(w, n) < match_threshold &&
                color_distance(w, s) > match_threshold && color_distance(n, e) > match_threshold)
                tl = blend_edge(c, w, n);
            if (color_distance(n, e) < match_threshold &&
                color_distance(n, w) > match_threshold && color_distance(e, s) > match_threshold)
                tr = blend_edge(c, n, e);
            if (color_distance(w, s) < match_threshold &&
                color_distance(w, n) > match_threshold && color_distance(s, e) > match_threshold)
                bl = blend_edge(c, w, s);
            if (color_distance(s, e) < match_threshold &&
                color_distance(s, w) > match_threshold && color_distance(e, n) > match_threshold)
                br = blend_edge(c, s, e);
            const size_t top = static_cast<size_t>(y * 2) * output_width + x * 2;
            const size_t bottom = top + output_width;
            filtered_pixels[top] = tl;
            filtered_pixels[top + 1] = tr;
            filtered_pixels[bottom] = bl;
            filtered_pixels[bottom + 1] = br;
        }
    }
    active_profile.edge_ms += profile_elapsed_ms(edge_start, profile_now());
}

struct NativeCameraVertex {
    float x, y, z;
    float u, v;
};

struct NativeDrawTriangle {
    SDL_Vertex vertices[3];
    SDL_Texture* texture;
    float depth;
    int segment_id;
    int layer;
};

static std::vector<NativeDrawTriangle> native_draw_triangles;
static void draw_text(const std::string& text, int x, int y, int scale);

static NativeCamera interpolated_native_camera() {
    if (!native_camera_previous.valid) return native_camera_current;
    NativeCamera result{};
    const float alpha = native_camera_alpha;
    result.position = native_add(native_scale(native_camera_previous.position, 1.0f - alpha),
                                 native_scale(native_camera_current.position, alpha));
    for (int index = 0; index < 9; ++index)
        result.matrix[index] = native_camera_previous.matrix[index] * (1.0f - alpha) +
                               native_camera_current.matrix[index] * alpha;
    result.valid = native_camera_current.valid;
    return result;
}

static NativeVec3 native_to_camera(const NativeCamera& camera, NativeVec3 world) {
    const NativeVec3 value = native_sub(world, camera.position);
    return {
        camera.matrix[0] * value.x + camera.matrix[1] * value.y + camera.matrix[2] * value.z,
        camera.matrix[3] * value.x + camera.matrix[4] * value.y + camera.matrix[5] * value.z,
        camera.matrix[6] * value.x + camera.matrix[7] * value.y + camera.matrix[8] * value.z,
    };
}

static SDL_Color native_fog_color(SDL_Color color, float depth, Uint8 alpha) {
    if (!native_fog) { color.a = alpha; return color; }
    const float start = native_draw_distance * 0.52f;
    const float amount = std::max(0.0f, std::min(1.0f,
        (depth - start) / std::max(1.0f, native_draw_distance - start)));
    const float fog_r = 112.0f, fog_g = 151.0f, fog_b = 177.0f;
    color.r = static_cast<Uint8>(color.r * (1.0f - amount) + fog_r * amount);
    color.g = static_cast<Uint8>(color.g * (1.0f - amount) + fog_g * amount);
    color.b = static_cast<Uint8>(color.b * (1.0f - amount) + fog_b * amount);
    color.a = alpha;
    return color;
}

static NativeCameraVertex interpolate_camera_vertex(const NativeCameraVertex& first,
                                                     const NativeCameraVertex& second,
                                                     float alpha) {
    return {
        first.x + (second.x - first.x) * alpha,
        first.y + (second.y - first.y) * alpha,
        first.z + (second.z - first.z) * alpha,
        first.u + (second.u - first.u) * alpha,
        first.v + (second.v - first.v) * alpha,
    };
}

static void emit_native_triangle(const NativeCamera& camera,
                                 NativeVec3 world_a, NativeVec3 world_b, NativeVec3 world_c,
                                 SDL_FPoint uv_a, SDL_FPoint uv_b, SDL_FPoint uv_c,
                                 SDL_Color color, SDL_Texture* material_texture,
                                 int segment_id, int layer, int output_width, int output_height,
                                 Uint8 alpha) {
    (void)camera;
    NativeCameraVertex input[3] = {
        {world_a.x, world_a.y, world_a.z, uv_a.x, uv_a.y},
        {world_b.x, world_b.y, world_b.z, uv_b.x, uv_b.y},
        {world_c.x, world_c.y, world_c.z, uv_c.x, uv_c.y},
    };
    constexpr float near_plane = 48.0f;
    NativeCameraVertex clipped[5]{};
    int clipped_count = 0;
    NativeCameraVertex previous = input[2];
    bool previous_inside = previous.z >= near_plane;
    for (const NativeCameraVertex& current : input) {
        const bool current_inside = current.z >= near_plane;
        if (current_inside != previous_inside) {
            const float mix = (near_plane - previous.z) / (current.z - previous.z);
            clipped[clipped_count++] = interpolate_camera_vertex(previous, current, mix);
        }
        if (current_inside) clipped[clipped_count++] = current;
        previous = current;
        previous_inside = current_inside;
    }
    if (clipped_count < 3) return;

    const float radians = native_vertical_fov * 3.1415926535f / 180.0f;
    const float focal = output_height * 0.5f / tanf(radians * 0.5f);
    const float center_x = output_width * 0.5f;
    const float center_y = output_height * 0.465f;
    for (int triangle = 1; triangle + 1 < clipped_count; ++triangle) {
        const NativeCameraVertex fan[3] = {clipped[0], clipped[triangle], clipped[triangle + 1]};
        NativeDrawTriangle output{};
        output.texture = material_texture;
        output.segment_id = segment_id;
        output.layer = layer;
        output.depth = (fan[0].z + fan[1].z + fan[2].z) / 3.0f;
        for (int index = 0; index < 3; ++index) {
            output.vertices[index].position.x = center_x + fan[index].x * focal / fan[index].z;
            output.vertices[index].position.y = center_y + fan[index].y * focal / fan[index].z;
            output.vertices[index].tex_coord = {fan[index].u, fan[index].v};
            output.vertices[index].color = native_fog_color(color, fan[index].z, alpha);
        }
        native_draw_triangles.push_back(output);
    }
}

static SDL_Texture* ensure_native_material_texture(MaterialKind material) {
    if (!n64_materials) return nullptr;
    if (material < 0 || material >= MATERIAL_COUNT) return nullptr;
    if (native_material_textures[material]) return native_material_textures[material];
    const MaterialTexture& source = materials[material];
    if (source.pixels.empty() || !source.width || !source.height) return nullptr;
    SDL_Texture* result = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STATIC,
                                            static_cast<int>(source.width),
                                            static_cast<int>(source.height));
    if (!result) return nullptr;
    SDL_UpdateTexture(result, nullptr, source.pixels.data(),
                      static_cast<int>(source.width * sizeof(uint32_t)));
    SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(result, SDL_ScaleModeLinear);
#endif
    native_material_textures[material] = result;
    return result;
}

static void add_native_quad(const NativeCamera& camera,
                            NativeVec3 a, NativeVec3 b, NativeVec3 c, NativeVec3 d,
                            SDL_Color color, MaterialKind material, int segment_id, int layer,
                            int output_width, int output_height, Uint8 alpha) {
    a = native_to_camera(camera, a);
    b = native_to_camera(camera, b);
    c = native_to_camera(camera, c);
    d = native_to_camera(camera, d);
    SDL_Texture* material_texture = ensure_native_material_texture(material);
    emit_native_triangle(camera, a, b, c, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
                         color, material_texture, segment_id, layer, output_width, output_height, alpha);
    emit_native_triangle(camera, a, c, d, {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
                         color, material_texture, segment_id, layer, output_width, output_height, alpha);
}

static bool native_segment_visible(const NativeCamera& camera, size_t index, size_t next,
                                   int output_width, int output_height) {
    NativeVec3 center = native_scale(native_add(native_track.nodes[index].center,
                                                native_track.nodes[next].center), 0.5f);
    const NativeVec3 camera_center = native_to_camera(camera, center);
    const float segment_length = native_length_xz(native_sub(native_track.nodes[next].center,
                                                              native_track.nodes[index].center));
    const float radius = segment_length * 0.55f + 5600.0f;
    if (camera_center.z + radius < 48.0f || camera_center.z - radius > native_draw_distance)
        return false;
    const float vertical = tanf(native_vertical_fov * 3.1415926535f / 360.0f);
    const float horizontal = vertical * static_cast<float>(output_width) /
                             static_cast<float>(std::max(1, output_height));
    const float visible_depth = std::max(48.0f, camera_center.z);
    return fabsf(camera_center.x) <= visible_depth * horizontal + radius &&
           fabsf(camera_center.y) <= visible_depth * vertical + radius;
}

static void add_native_vehicle(const NativeCamera& camera, const NativeVehicle& vehicle,
                               SDL_Color color, int object_id, int output_width,
                               int output_height, Uint8 alpha) {
    const float side_x = -vehicle.axis_z * vehicle.half_width;
    const float side_z = vehicle.axis_x * vehicle.half_width;
    const float forward_x = vehicle.axis_x * vehicle.half_length;
    const float forward_z = vehicle.axis_z * vehicle.half_length;
    const float pitch_height = tanf(std::max(-0.55f, std::min(0.55f, vehicle.pitch))) *
                               vehicle.half_length;
    const float roll_height = tanf(std::max(-0.45f, std::min(0.45f, vehicle.roll))) *
                              vehicle.half_width;
    NativeVec3 bottom[4] = {
        {vehicle.position.x + forward_x + side_x, vehicle.position.y + pitch_height + roll_height,
         vehicle.position.z + forward_z + side_z},
        {vehicle.position.x + forward_x - side_x, vehicle.position.y + pitch_height - roll_height,
         vehicle.position.z + forward_z - side_z},
        {vehicle.position.x - forward_x - side_x, vehicle.position.y - pitch_height - roll_height,
         vehicle.position.z - forward_z - side_z},
        {vehicle.position.x - forward_x + side_x, vehicle.position.y - pitch_height + roll_height,
         vehicle.position.z - forward_z + side_z},
    };
    NativeVec3 top[4];
    const NativeVec3 center_top{vehicle.position.x, vehicle.position.y - 150.0f, vehicle.position.z};
    for (int index = 0; index < 4; ++index)
        top[index] = native_add(center_top, native_scale(native_sub(bottom[index], vehicle.position), 0.68f));
    for (int index = 0; index < 4; ++index) {
        const int next = (index + 1) & 3;
        add_native_quad(camera, bottom[index], bottom[next], top[next], top[index],
                        color, MATERIAL_NONE, 1000 + object_id, 4, output_width, output_height, alpha);
    }
    add_native_quad(camera, top[0], top[1], top[2], top[3],
                    color, MATERIAL_NONE, 1000 + object_id, 4, output_width, output_height, alpha);
}

static std::vector<NativeVehicle> interpolated_native_vehicles() {
    if (native_vehicles_previous.size() != native_vehicles_current.size())
        return native_vehicles_current;
    std::vector<NativeVehicle> result = native_vehicles_current;
    for (size_t index = 0; index < result.size(); ++index) {
        const float alpha = native_vehicle_alpha;
        result[index].position = native_add(native_scale(native_vehicles_previous[index].position, 1.0f - alpha),
                                            native_scale(native_vehicles_current[index].position, alpha));
        result[index].axis_x = native_vehicles_previous[index].axis_x * (1.0f - alpha) +
                               native_vehicles_current[index].axis_x * alpha;
        result[index].axis_z = native_vehicles_previous[index].axis_z * (1.0f - alpha) +
                               native_vehicles_current[index].axis_z * alpha;
        result[index].pitch = native_vehicles_previous[index].pitch * (1.0f - alpha) +
                              native_vehicles_current[index].pitch * alpha;
        result[index].roll = native_vehicles_previous[index].roll * (1.0f - alpha) +
                             native_vehicles_current[index].roll * alpha;
        const float length = sqrtf(result[index].axis_x * result[index].axis_x +
                                   result[index].axis_z * result[index].axis_z);
        if (length > 0.001f) { result[index].axis_x /= length; result[index].axis_z /= length; }
    }
    return result;
}

static void draw_native_hud_strip() {
    if (!texture || !source_width || !source_height) return;
    int output_width = 0, output_height = 0;
    SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
    const int safe_width = std::min(output_width, static_cast<int>(output_height * 4.0 / 3.0 + 0.5));
    const int safe_x = (output_width - safe_width) / 2;
    const int hud_source_y = texture_height * 3 / 4;
    SDL_Rect source{0, hud_source_y, texture_width, texture_height - hud_source_y};
    SDL_Rect destination{safe_x, output_height * 3 / 4, safe_width, output_height / 4};
    SDL_SetTextureAlphaMod(texture, 255);
    SDL_RenderCopy(renderer, texture, &source, &destination);
}

static bool draw_native_track(bool hybrid) {
    if (!race_scene_active || !native_race_motion_ready || native_track.nodes.size() < 4 ||
        !native_camera_current.valid) return false;
    int output_width = 0, output_height = 0;
    SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
    if (!hybrid) {
        SDL_SetRenderDrawColor(renderer, 96, 142, 188, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 130, 174, 202, 255);
        SDL_Rect horizon{0, output_height * 43 / 100, output_width, output_height * 8 / 100};
        SDL_RenderFillRect(renderer, &horizon);
        SDL_SetRenderDrawColor(renderer, 91, 145, 82, 255);
        SDL_Rect distant_ground{0, output_height * 51 / 100,
                                output_width, output_height - output_height * 51 / 100};
        SDL_RenderFillRect(renderer, &distant_ground);
    }

    const NativeCamera camera = interpolated_native_camera();
    native_draw_triangles.clear();
    native_segments_drawn = 0;
    native_segments_culled = 0;
    const size_t segment_count = native_track.closed ? native_track.nodes.size()
                                                     : native_track.nodes.size() - 1;
    std::vector<uint8_t> topology_visible(segment_count, 0);
    size_t nearest_node = 0;
    float nearest_distance = std::numeric_limits<float>::max();
    for (size_t index = 0; index < native_track.nodes.size(); ++index) {
        const float distance = native_length_xz(native_sub(native_track.nodes[index].center,
                                                            camera.position));
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest_node = index;
        }
    }
    const size_t next_node = (nearest_node + 1) % native_track.nodes.size();
    const size_t previous_node = nearest_node ? nearest_node - 1 : native_track.nodes.size() - 1;
    const float next_depth = native_to_camera(camera, native_track.nodes[next_node].center).z;
    const float previous_depth = native_to_camera(camera, native_track.nodes[previous_node].center).z;
    const int forward_step = next_depth >= previous_depth ? 1 : -1;

    auto walk_course = [&](int direction, float distance_limit) {
        size_t node = nearest_node;
        float distance = 0.0f;
        for (size_t visited = 0; visited < segment_count && distance <= distance_limit; ++visited) {
            const size_t adjacent = direction > 0
                ? (node + 1) % native_track.nodes.size()
                : (node ? node - 1 : native_track.nodes.size() - 1);
            const size_t segment = direction > 0 ? node : adjacent;
            if (segment >= segment_count) break;
            topology_visible[segment] = 1;
            distance += native_length_xz(native_sub(native_track.nodes[adjacent].center,
                                                     native_track.nodes[node].center));
            node = adjacent;
            if (!native_track.closed && (node == 0 || node + 1 == native_track.nodes.size())) break;
        }
    };
    // Render the course ahead to the selected native far distance, plus a small
    // stable tail behind the camera. This avoids submitting unrelated portions
    // of a closed circuit whose painter order can alternate every scene tick.
    walk_course(forward_step, native_draw_distance);
    walk_course(-forward_step, std::min(9000.0f, native_draw_distance * 0.22f));

    const Uint8 alpha = hybrid ? 112 : 255;
    for (size_t index = 0; index < segment_count; ++index) {
        const size_t next = (index + 1) % native_track.nodes.size();
        if (!topology_visible[index] ||
            !native_segment_visible(camera, index, next, output_width, output_height)) {
            ++native_segments_culled;
            continue;
        }
        ++native_segments_drawn;
        NativeVec3 left_a = native_track.left[index], left_b = native_track.left[next];
        NativeVec3 right_a = native_track.right[index], right_b = native_track.right[next];
        NativeVec3 outer_left_a = native_track.outer_left[index], outer_left_b = native_track.outer_left[next];
        NativeVec3 outer_right_a = native_track.outer_right[index], outer_right_b = native_track.outer_right[next];
        outer_left_a.y += 36.0f; outer_left_b.y += 36.0f;
        outer_right_a.y += 36.0f; outer_right_b.y += 36.0f;
        const NativeVec3 left_direction = native_scale(native_sub(left_a, native_track.nodes[index].center),
                                                        1.0f / native_track.nodes[index].half_width);
        const NativeVec3 left_direction_next = native_scale(native_sub(left_b, native_track.nodes[next].center),
                                                             1.0f / native_track.nodes[next].half_width);
        const NativeVec3 right_direction = native_scale(native_sub(right_a, native_track.nodes[index].center),
                                                         1.0f / native_track.nodes[index].half_width);
        const NativeVec3 right_direction_next = native_scale(native_sub(right_b, native_track.nodes[next].center),
                                                              1.0f / native_track.nodes[next].half_width);
        const NativeVec3 shoulder_left_a = native_add(left_a, native_scale(left_direction, 100.0f));
        const NativeVec3 shoulder_left_b = native_add(left_b, native_scale(left_direction_next, 100.0f));
        const NativeVec3 shoulder_right_a = native_add(right_a, native_scale(right_direction, 100.0f));
        const NativeVec3 shoulder_right_b = native_add(right_b, native_scale(right_direction_next, 100.0f));
        add_native_quad(camera, outer_left_a, outer_left_b, shoulder_left_b, shoulder_left_a,
                        {205, 238, 205, 255}, MATERIAL_GRASS, static_cast<int>(index), 0,
                        output_width, output_height, alpha);
        add_native_quad(camera, shoulder_right_a, shoulder_right_b, outer_right_b, outer_right_a,
                        {205, 238, 205, 255}, MATERIAL_GRASS, static_cast<int>(index), 0,
                        output_width, output_height, alpha);
        add_native_quad(camera, shoulder_left_a, shoulder_left_b, left_b, left_a,
                        {220, 220, 214, 255}, MATERIAL_GRAVEL, static_cast<int>(index), 1,
                        output_width, output_height, alpha);
        add_native_quad(camera, right_a, right_b, shoulder_right_b, shoulder_right_a,
                        {220, 220, 214, 255}, MATERIAL_GRAVEL, static_cast<int>(index), 1,
                        output_width, output_height, alpha);
        add_native_quad(camera, left_a, left_b, right_b, right_a,
                        {224, 224, 224, 255}, MATERIAL_ASPHALT, static_cast<int>(index), 2,
                        output_width, output_height, alpha);

        if ((index & 3u) < 2u) {
            const NativeVec3 center_a = native_track.nodes[index].center;
            const NativeVec3 center_b = native_track.nodes[next].center;
            const NativeVec3 stripe_left_a = native_add(center_a, native_scale(left_direction, 16.0f));
            const NativeVec3 stripe_left_b = native_add(center_b, native_scale(left_direction_next, 16.0f));
            const NativeVec3 stripe_right_a = native_add(center_a, native_scale(right_direction, 16.0f));
            const NativeVec3 stripe_right_b = native_add(center_b, native_scale(right_direction_next, 16.0f));
            add_native_quad(camera, stripe_left_a, stripe_left_b, stripe_right_b, stripe_right_a,
                            {238, 234, 194, 255}, MATERIAL_NONE, static_cast<int>(index), 3,
                            output_width, output_height, alpha);
        }
    }

    const std::vector<NativeVehicle> vehicles = interpolated_native_vehicles();
    for (size_t index = 0; index < vehicles.size(); ++index) {
        const NativeVec3 vehicle_camera = native_to_camera(camera, vehicles[index].position);
        // These are source-informed low-poly bodies pending retail model-stream
        // decoding. Their position, directed axle pose, pitch and roll all come
        // from the game's live wheel state.
        const float near_limit = vehicles[index].player ? 220.0f : 640.0f;
        if (vehicle_camera.z < near_limit || vehicle_camera.z > native_draw_distance) continue;
        const SDL_Color color = vehicles[index].player
            ? SDL_Color{60, 128, 244, 255}
            : (index & 1u ? SDL_Color{236, 78, 52, 255} : SDL_Color{244, 206, 58, 255});
        add_native_vehicle(camera, vehicles[index], color, static_cast<int>(index),
                           output_width, output_height, alpha);
    }

    std::stable_sort(native_draw_triangles.begin(), native_draw_triangles.end(),
        [](const NativeDrawTriangle& left, const NativeDrawTriangle& right) {
            // SDL_RenderGeometry has no depth buffer. Sorting every triangle by
            // its own camera depth is substantially more stable than the old
            // whole-segment midpoint order, which swapped long overlapping road
            // pieces as the camera crossed their midpoint.
            if (left.segment_id == right.segment_id && left.layer != right.layer)
                return left.layer < right.layer;
            if (fabsf(left.depth - right.depth) > 0.5f) return left.depth > right.depth;
            if (left.segment_id != right.segment_id) return left.segment_id < right.segment_id;
            return left.layer < right.layer;
        });
    for (const NativeDrawTriangle& triangle : native_draw_triangles)
        SDL_RenderGeometry(renderer, triangle.texture, triangle.vertices, 3, nullptr, 0);

    if (native_debug_bounds) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 92, 48, hybrid ? 190 : 255);
        for (const NativeDrawTriangle& triangle : native_draw_triangles) {
            SDL_RenderDrawLineF(renderer, triangle.vertices[0].position.x, triangle.vertices[0].position.y,
                                triangle.vertices[1].position.x, triangle.vertices[1].position.y);
            SDL_RenderDrawLineF(renderer, triangle.vertices[1].position.x, triangle.vertices[1].position.y,
                                triangle.vertices[2].position.x, triangle.vertices[2].position.y);
            SDL_RenderDrawLineF(renderer, triangle.vertices[2].position.x, triangle.vertices[2].position.y,
                                triangle.vertices[0].position.x, triangle.vertices[0].position.y);
        }
    }
    if (native_debug_ids) {
        const float radians = native_vertical_fov * 3.1415926535f / 180.0f;
        const float focal = output_height * 0.5f / tanf(radians * 0.5f);
        SDL_SetRenderDrawColor(renderer, 255, 236, 80, hybrid ? 210 : 255);
        for (size_t index = 0; index < native_track.nodes.size(); ++index) {
            const NativeVec3 point = native_to_camera(camera, native_track.nodes[index].center);
            if (point.z < 48.0f || point.z > native_draw_distance) continue;
            const int x = static_cast<int>(output_width * 0.5f + point.x * focal / point.z);
            const int y = static_cast<int>(output_height * 0.465f + point.y * focal / point.z);
            if (x < 0 || x >= output_width || y < 0 || y >= output_height) continue;
            draw_text(std::to_string(index), x, y, output_height >= 1200 ? 2 : 1);
        }
    }
    if (!hybrid) draw_native_hud_strip();
    return true;
}

static const char* glyph_rows(char c) {
    switch (c) {
        case '0': return "01110100011001110101110011000101110";
        case '1': return "00100011000010000100001000010001110";
        case '2': return "01110100010000100110010001000011111";
        case '3': return "11110000010000101110000010000111110";
        case '4': return "00010001100101010010111110001000010";
        case '5': return "11111100001111000001000011000101110";
        case '6': return "00110010001000011110100011000101110";
        case '7': return "11111000010001000100010000100001000";
        case '8': return "01110100011000101110100011000101110";
        case '9': return "01110100011000101111000010001001100";
        case 'A': return "01110100011000111111100011000110001";
        case 'B': return "11110100011000111110100011000111110";
        case 'C': return "01111100001000010000100001000001111";
        case 'D': return "11110100011000110001100011000111110";
        case 'E': return "11111100001000011110100001000011111";
        case 'F': return "11111100001000011110100001000010000";
        case 'G': return "01111100001000010111100011000101111";
        case 'H': return "10001100011000111111100011000110001";
        case 'I': return "01110001000010000100001000010001110";
        case 'J': return "00111000100001000010000101001001100";
        case 'K': return "10001100101010011000101001001010001";
        case 'L': return "10000100001000010000100001000011111";
        case 'M': return "10001110111010110101100011000110001";
        case 'N': return "10001110011010110011100011000110001";
        case 'O': return "01110100011000110001100011000101110";
        case 'P': return "11110100011000111110100001000010000";
        case 'Q': return "01110100011000110001101011001001101";
        case 'R': return "11110100011000111110101001001010001";
        case 'S': return "01111100001000001110000010000111110";
        case 'T': return "11111001000010000100001000010000100";
        case 'U': return "10001100011000110001100011000101110";
        case 'V': return "10001100011000110001100010101000100";
        case 'W': return "10001100011000110101101011010101010";
        case 'X': return "10001100010101000100010101000110001";
        case 'Y': return "10001100010101000100001000010000100";
        case 'Z': return "11111000010001000100010001000011111";
        case '.': return "00000000000000000000000000110001100";
        case ':': return "00000011000110000000011000110000000";
        case '%': return "11001110100010000100010101100100000";
        case '-': return "00000000000000011111000000000000000";
        default:  return "00000000000000000000000000000000000";
    }
}

static void draw_text(const std::string& text, int x, int y, int scale) {
    for (char c : text) {
        const char* rows = glyph_rows(c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c);
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < 5; ++column)
                if (rows[row * 5 + column] == '1') {
                    SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
        x += 6 * scale;
    }
}

static const char* filter_name() {
    if (filter_mode == FILTER_NEAREST) return "NEAREST";
    if (filter_mode == FILTER_LINEAR) return "LINEAR";
    return "EDGE SMOOTH";
}

static const char* aspect_name() {
    if (compat_hd_center && compat_wide && render_path == RENDER_COMPATIBILITY) return "HD WORLD VIEW - PREVIEW";
    if (compat_wide && render_path == RENDER_COMPATIBILITY) return "EXPANDED SIDES - PREVIEW";
    if (aspect_mode == ASPECT_AMBIENT) return "AMBIENT 21:9";
    if (aspect_mode == ASPECT_STRETCH) return "STRETCH";
    return "ASPECT 4:3";
}

static const char* draw_distance_name() {
    if (draw_distance_mode == 2) return "SURFACE RANGE FAR";
    if (draw_distance_mode == 1) return "SURFACE RANGE EXTENDED";
    return "SURFACE RANGE ORIGINAL";
}

static const char* render_path_name() {
    if (render_path == RENDER_NATIVE_TRACK) return "RENDER NATIVE TRACK";
    if (render_path == RENDER_HYBRID_COMPARE) return "RENDER HYBRID DEBUG";
    return "RENDER COMPATIBILITY";
}

static void set_toast(const std::string& text);

static void recreate_texture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

static void apply_visual_preset(int preset) {
    visual_preset = (preset % 5 + 5) % 5;
    motion_smoothing = false;
    n64_materials = false;
    draw_distance_mode = 0;
    if (visual_preset == 0) {
        filter_mode = FILTER_NEAREST;
        aspect_mode = ASPECT_4_3;
        set_toast("PRESET ORIGINAL");
    } else if (visual_preset == 1) {
        filter_mode = FILTER_EDGE;
        aspect_mode = ASPECT_4_3;
        set_toast("PRESET CRISP");
    } else if (visual_preset == 2) {
        filter_mode = FILTER_EDGE;
        aspect_mode = ASPECT_AMBIENT;
        set_toast("PRESET ULTRAWIDE");
    } else if (visual_preset == 3) {
        filter_mode = FILTER_EDGE;
        aspect_mode = ASPECT_AMBIENT;
        motion_smoothing = true;
        set_toast("PRESET SPRITE-SAFE SMOOTH");
    } else {
        filter_mode = FILTER_EDGE;
        aspect_mode = ASPECT_AMBIENT;
        motion_smoothing = true;
        n64_materials = true;
        draw_distance_mode = 1;
        set_toast("PRESET N64 STYLE");
    }
    motion_previous.clear();
    motion_target.clear();
    motion_pixels.clear();
    recreate_texture();
}

static double cap_fps() {
    static const double multipliers[] = {1.0, 1.5, 2.0, 0.0};
    return multipliers[speed_mode] == 0.0 ? 0.0 : native_fps * multipliers[speed_mode];
}

static void set_toast(const std::string& text) {
    toast_text = text;
    toast_until = SDL_GetTicks64() + 1800;
}

static void draw_status_overlay() {
    const bool show_toast = !toast_text.empty() && SDL_GetTicks64() < toast_until;
    if (!show_fps && !show_toast && !paused) return;
    int output_width = 0, output_height = 0;
    SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
    const int scale = output_height >= 1200 ? 4 : (output_height >= 700 ? 3 : 2);
    std::vector<std::string> lines;
    if (show_fps) {
        char fps_line[64], scene_line[64], cap_line[64], gsu_line[64];
        char frame_line[96], low_line[96], core_line[96], video_line[96];
        char pipeline_line[96], blend_line[96], gpu_line[96], work_line[96], audio_line[96];
        snprintf(fps_line, sizeof(fps_line), "OUTPUT %.1F FPS", measured_fps);
        snprintf(scene_line, sizeof(scene_line), "SCENE %.1F FPS", measured_scene_fps);
        snprintf(frame_line, sizeof(frame_line), "FRAME %.2F MS AVG %.2F",
                 last_profile.total_ms, rolling_average_ms);
        snprintf(low_line, sizeof(low_line), "1 PCT LOW %.1F WORST %.2F MS",
                 rolling_one_percent_low, rolling_worst_ms);
        snprintf(core_line, sizeof(core_line), "CORE %.2F MS GSU %.2F MS",
                 last_profile.core_ms, last_profile.gsu_ms);
        snprintf(video_line, sizeof(video_line), "VIDEO %.2F MS DRAW %.2F MS",
                 last_profile.video_ms, last_profile.render_ms);
        snprintf(pipeline_line, sizeof(pipeline_line), "DECODE %.2F EDGE %.2F UPLOAD %.2F",
                 last_profile.decode_ms, last_profile.edge_ms, last_profile.upload_ms);
        snprintf(blend_line, sizeof(blend_line), "INTERP %.2F SPRITE %.2F",
                 last_profile.interpolation_ms, last_profile.sprite_ms);
        snprintf(gpu_line, sizeof(gpu_line), "PRESENT %.2F WAIT %.2F %s",
                 last_profile.present_ms, last_profile.limiter_ms,
                 (renderer_backend_flags & SDL_RENDERER_SOFTWARE) ? "SOFTWARE" : "GPU");
        snprintf(work_line, sizeof(work_line), "GSU OPS %llu ALLOC %u",
                 static_cast<unsigned long long>(last_profile.gsu_instructions),
                 last_profile.allocations);
        snprintf(audio_line, sizeof(audio_line), "AUDIO %.2F MS %llu BYTES",
                 last_profile.audio_ms,
                 static_cast<unsigned long long>(last_profile.audio_bytes));
        const double cap = cap_fps();
        if (cap > 0.0) snprintf(cap_line, sizeof(cap_line), "CAP %.1F HZ", cap);
        else snprintf(cap_line, sizeof(cap_line), "CAP UNLIMITED");
        lines.emplace_back(fps_line);
        lines.emplace_back(scene_line);
        lines.emplace_back(frame_line);
        lines.emplace_back(low_line);
        lines.emplace_back(core_line);
        lines.emplace_back(video_line);
        lines.emplace_back(pipeline_line);
        lines.emplace_back(blend_line);
        lines.emplace_back(gpu_line);
        lines.emplace_back(work_line);
        lines.emplace_back(audio_line);
        lines.emplace_back(cap_line);
        snprintf(gsu_line, sizeof(gsu_line), "SUPER FX %d%%", gsu_clock_percent);
        lines.emplace_back(gsu_line);
        lines.emplace_back(filter_name());
        lines.emplace_back(aspect_name());
        lines.emplace_back(draw_distance_name());
        lines.emplace_back(render_path_name());
        if (render_path != RENDER_COMPATIBILITY) {
            char cache_line[64], visibility_line[64], fov_line[64], width_line[64], distance_line[64], dynamic_line[64], mode_line[64], warmup_line[64];
            snprintf(cache_line, sizeof(cache_line), "TRACK CACHE %u NODES",
                     static_cast<unsigned>(native_track.nodes.size()));
            snprintf(visibility_line, sizeof(visibility_line), "SEGMENTS %d DRAWN %d CULLED",
                     native_segments_drawn, native_segments_culled);
            snprintf(fov_line, sizeof(fov_line), "VERTICAL FOV %.0F", native_vertical_fov);
            snprintf(width_line, sizeof(width_line), "ROAD WIDTH %.1FX", native_road_width_scale);
            snprintf(distance_line, sizeof(distance_line), "NATIVE FAR %.0F", native_draw_distance);
            snprintf(dynamic_line, sizeof(dynamic_line), "DYNAMIC OBJECTS %u",
                     static_cast<unsigned>(native_vehicles_current.size()));
            snprintf(mode_line, sizeof(mode_line), "GAME MODES %02X %02X NMI %02X",
                     live_display_mode & 255, live_runtime_mode & 255, live_nmi_mode & 255);
            snprintf(warmup_line, sizeof(warmup_line), "NATIVE WARMUP %u",
                     native_race_warmup_frames);
            lines.emplace_back(cache_line);
            lines.emplace_back(visibility_line);
            lines.emplace_back(fov_line);
            lines.emplace_back(width_line);
            lines.emplace_back(distance_line);
            lines.emplace_back(dynamic_line);
            if (render_path == RENDER_HYBRID_COMPARE || native_debug_bounds || native_debug_ids) {
                lines.emplace_back(mode_line);
                lines.emplace_back(warmup_line);
            }
            lines.emplace_back(native_fog ? "NATIVE FOG ON" : "NATIVE FOG OFF");
            if (race_scene_active && !native_race_motion_ready)
                lines.emplace_back("NATIVE WAITING FOR RACE MOTION");
            if (native_debug_bounds) lines.emplace_back("DEBUG WIREFRAME ON");
            if (native_debug_ids) lines.emplace_back("DEBUG SEGMENT IDS ON");
        }
        if (n64_materials) lines.emplace_back("N64 MATERIALS ON");
        if (render_path == RENDER_HYBRID_COMPARE || native_debug_bounds || native_debug_ids) {
            char chunks_line[64];
            snprintf(chunks_line, sizeof(chunks_line), "TRACK CHUNKS %d", live_track_chunks);
            lines.emplace_back(chunks_line);
        }
        if (motion_smoothing) lines.emplace_back("SPRITE-SAFE MOTION ON");
        if (runahead_enabled) lines.emplace_back("RUNAHEAD ON");
        if (replay_recording) lines.emplace_back("RECORDING");
        if (replay_playing) lines.emplace_back("REPLAY");
    }
    if (reconstruction_recording) lines.emplace_back("RECONSTRUCTION DATA RECORDING");
    if (paused) lines.emplace_back("PAUSED");
    if (show_toast) lines.push_back(toast_text);
    size_t longest = 0;
    for (const std::string& line : lines) if (line.size() > longest) longest = line.size();
    const int padding = 4 * scale;
    const bool show_graph = show_fps && frame_time_history_count > 1;
    const size_t graph_samples = std::min<size_t>(120, frame_time_history_count);
    const int graph_height = 24 * scale;
    SDL_Rect box{16, 16, static_cast<int>(longest * 6 * scale + padding * 2),
                 static_cast<int>(lines.size() * 9 * scale + padding +
                                  (show_graph ? graph_height + padding : 0))};
    if (show_graph)
        box.w = std::max(box.w, static_cast<int>(graph_samples * scale + padding * 2));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 185);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 105, 235, 255, 255);
    int y = box.y + padding;
    for (const std::string& line : lines) {
        draw_text(line, box.x + padding, y, scale);
        y += 9 * scale;
    }
    if (show_graph) {
        SDL_Rect graph_background{box.x + padding, y, static_cast<int>(graph_samples * scale),
                                  graph_height};
        SDL_SetRenderDrawColor(renderer, 16, 28, 38, 230);
        SDL_RenderFillRect(renderer, &graph_background);
        const size_t first = (frame_time_history_cursor + frame_time_history.size() - graph_samples) %
                             frame_time_history.size();
        for (size_t index = 0; index < graph_samples; ++index) {
            const double milliseconds = frame_time_history[(first + index) % frame_time_history.size()];
            const int height = std::max(1, std::min(graph_height,
                static_cast<int>(milliseconds * graph_height / 33.3)));
            if (milliseconds > 25.0) SDL_SetRenderDrawColor(renderer, 255, 70, 70, 255);
            else if (milliseconds > 20.0) SDL_SetRenderDrawColor(renderer, 255, 205, 70, 255);
            else SDL_SetRenderDrawColor(renderer, 80, 230, 150, 255);
            SDL_Rect bar{graph_background.x + static_cast<int>(index * scale),
                         graph_background.y + graph_height - height, scale, height};
            SDL_RenderFillRect(renderer, &bar);
        }
    }
}

#include "compat_wide_present.h"
#include "session_recorder.h"

static void update_compat_hd_overlay() {
    compat_hd_center_ready=false;
    if(!compat_hd_center || source_width<232 || source_height<160 ||
       compat_wide_world_reference.size()!=208*128*4) return;
    const uint32_t* reference=reinterpret_cast<const uint32_t*>(compat_wide_world_reference.data());
    compat_hd_overlay_pixels.resize(208*128);
    std::array<uint8_t,208*128> missing{},eroded{},cleaned{};
    // Reproduce the captured face coverage at the original pixel grid. This
    // lets the overlay retain GSU bitmap primitives that do not travel through
    // the polygon packet path (notably the wheel sprites) without retaining
    // the low-resolution road and vehicle polygons around them.
    uint64_t prediction_key=wide_palette_hash();
    for(uint8_t value:compat_wide_display_bytes) prediction_key=(prediction_key^value)*1099511628211ull;
    if(prediction_key!=compat_hd_predicted_key) {
      compat_hd_predicted.fill(0);
      for(size_t offset=0;offset+sizeof(CompatWideCameraFace)<=compat_wide_display_bytes.size();offset+=sizeof(CompatWideCameraFace)) {
        CompatWideCameraFace face;
        memcpy(&face,compat_wide_display_bytes.data()+offset,sizeof(face));
        int count=face.count, clipped=0;
        if(count<3 || count>32) continue;
        WidePoint points[64];
        for(int i=0;i<count;++i) {
            const float* p=&face.xyz[i*3];
            const float* q=&face.xyz[((i+1)%count)*3];
            if(p[2]>=1) points[clipped++]={face.center_x+128*p[0]/p[2],face.center_y+128*p[1]/p[2],0,0};
            if((p[2]>=1)!=(q[2]>=1)) {
                const float t=(1-p[2])/(q[2]-p[2]);
                points[clipped++]={face.center_x+128*(p[0]+t*(q[0]-p[0])),
                    face.center_y+128*(p[1]+t*(q[1]-p[1])),0,0};
            }
        }
        if(clipped<3) continue;
        float minx=208,maxx=0,miny=128,maxy=0;
        for(int i=0;i<clipped;++i) {minx=std::min(minx,points[i].x);maxx=std::max(maxx,points[i].x);miny=std::min(miny,points[i].y);maxy=std::max(maxy,points[i].y);}
        const int x0=std::max(0,static_cast<int>(floorf(minx))),x1=std::min(207,static_cast<int>(ceilf(maxx)));
        const int y0=std::max(0,static_cast<int>(floorf(miny))),y1=std::min(127,static_cast<int>(ceilf(maxy)));
        const uint32_t face_color=face.textured?0xffffffffu:compat_wide_colors[face.color&255];
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            const float px=x+0.5f,py=y+0.5f;
            bool inside=false;
            for(int i=0,j=clipped-1;i<clipped;j=i++) {
                const float yi=points[i].y,yj=points[j].y;
                if(((yi>py)!=(yj>py)) && px<(points[j].x-points[i].x)*(py-yi)/(yj-yi)+points[i].x)
                    inside=!inside;
            }
            if(inside) compat_hd_predicted[y*208+x]=face_color;
        }
      }
      compat_hd_predicted_key=prediction_key;
    }
    for(unsigned y=0;y<128;++y) for(unsigned x=0;x<208;++x) {
            const uint32_t original=source_pixels[static_cast<size_t>(y+32)*source_width+x+24];
            const uint32_t world=reference[y*208+x];
            const uint32_t expected=compat_hd_predicted[y*208+x];
            const bool ppu_overlay=((original^world)&0x00ffffffu)!=0;
            const bool unsupported=(world&0xff000000u) &&
                (!expected || (expected!=0xffffffffu && color_distance(world,expected)>24));
            missing[y*208+x]=unsupported;
            compat_hd_overlay_pixels[y*208+x]=ppu_overlay?original:0;
    }
    // Polygon quantization creates isolated one-pixel disagreement lines.
    // A 3x3 opening removes those stair steps while retaining compact missing
    // primitives such as wheels and scenery that never reached face capture.
    for(unsigned y=1;y<127;++y) for(unsigned x=1;x<207;++x) {
        bool solid=true;
        for(int dy=-1;dy<=1 && solid;++dy) for(int dx=-1;dx<=1;++dx)
            if(!missing[(y+dy)*208+x+dx]) {solid=false;break;}
        eroded[y*208+x]=solid;
    }
    for(unsigned y=0;y<128;++y) for(unsigned x=0;x<208;++x) {
        bool has_neighbor=false;
        for(int dy=-1;dy<=1 && !has_neighbor;++dy) for(int dx=-1;dx<=1;++dx) {
            const int xx=static_cast<int>(x)+dx,yy=static_cast<int>(y)+dy;
            if(xx>=0&&xx<208&&yy>=0&&yy<128&&eroded[yy*208+xx]) {has_neighbor=true;break;}
        }
        cleaned[y*208+x]=has_neighbor;
        if(has_neighbor) compat_hd_overlay_pixels[y*208+x]=
            source_pixels[static_cast<size_t>(y+32)*source_width+x+24];
    }
    if(!compat_hd_overlay_texture) {
        compat_hd_overlay_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,208,128);
        if(compat_hd_overlay_texture) {
            SDL_SetTextureBlendMode(compat_hd_overlay_texture,SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(compat_hd_overlay_texture,SDL_ScaleModeNearest);
            ++active_profile.texture_recreates;
        }
    }
    if(compat_hd_overlay_texture && SDL_UpdateTexture(compat_hd_overlay_texture,nullptr,
       compat_hd_overlay_pixels.data(),208*4)==0) {
        ++active_profile.uploads;
        compat_hd_center_ready=true;
    }
}

static void draw_compatibility_frame() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (!texture) return;
    SDL_Rect destination{};
    SDL_Rect* destination_ptr = nullptr;
    if (aspect_mode == ASPECT_AMBIENT && !compat_wide) {
        int output_width = 0, output_height = 0;
        SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
        SDL_SetTextureColorMod(texture, 72, 82, 96);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        ++active_profile.render_copies;
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 105);
        SDL_Rect shade{0, 0, output_width, output_height};
        SDL_RenderFillRect(renderer, &shade);
    }
    if (aspect_mode != ASPECT_STRETCH || compat_wide) {
        int output_width = 0, output_height = 0;
        SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
        const double target_aspect = 4.0 / 3.0;
        if (output_width > static_cast<int>(output_height * target_aspect)) {
            destination.h = output_height;
            destination.w = static_cast<int>(output_height * target_aspect + 0.5);
            destination.x = (output_width - destination.w) / 2;
        } else {
            destination.w = output_width;
            destination.h = static_cast<int>(output_width / target_aspect + 0.5);
            destination.y = (output_height - destination.h) / 2;
        }
        destination_ptr = &destination;
    }
    SDL_SetTextureAlphaMod(texture, 255);
    SDL_RenderCopy(renderer, texture, nullptr, destination_ptr);
    ++active_profile.render_copies;
    if (compat_wide && destination_ptr) draw_compat_wide_sides(destination);
    if (compat_draw_stream_debug && render_path == RENDER_COMPATIBILITY &&
        compat_draw_stream_age < 12 && source_width && source_height) {
        int ow = 0, oh = 0;
        SDL_GetRendererOutputSize(renderer, &ow, &oh);
        const SDL_Rect area = destination_ptr ? destination : SDL_Rect{0, 0, ow, oh};
        const float sx = static_cast<float>(area.w) / source_width;
        const float sy = static_cast<float>(area.h) / source_height;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 64, 255, 240, 170);
        const auto& stream = compat_draw_stream_debug == 2 ? compat_draw_stream_previous : compat_draw_stream;
        for (size_t offset = 0; offset + 68 <= stream.size(); offset += 68) {
            const uint16_t* polygon = stream.data() + offset;
            const unsigned count = polygon[0];
            if (count < 3 || count > 32) continue;
            SDL_FPoint points[33];
            for (unsigned i = 0; i < count; ++i) {
                // Diagnostic placement for the first-course 208x128 game view.
                // Layer/window relocation and display-buffer ownership remain
                // unverified; this overlay never replaces the original image.
                points[i] = {area.x + ((polygon[4 + i * 2] & 255) + 24) * sx,
                             area.y + ((polygon[5 + i * 2] & 255) + 32) * sy};
            }
            points[count] = points[0];
            SDL_RenderDrawLinesF(renderer, points, count + 1);
        }
        draw_text(compat_draw_stream_debug == 2 ? "DRAW STREAM DEBUG - PREVIOUS SUBMISSION" :
                  "DRAW STREAM DEBUG - LATEST SUBMISSION", area.x + 8, area.y + 8, 1);
    }
}

static bool present_capture_selected(uint64_t frame) {
    const char* directory = getenv("SRF_PRESENT_CAPTURE_DIR");
    if (!directory || !*directory) return false;
    const char* from_text = getenv("SRF_PRESENT_CAPTURE_FROM");
    const char* to_text = getenv("SRF_PRESENT_CAPTURE_TO");
    const char* step_text = getenv("SRF_PRESENT_CAPTURE_STEP");
    const uint64_t from = from_text && *from_text ? _strtoui64(from_text, nullptr, 0) : 0;
    const uint64_t to = to_text && *to_text ? _strtoui64(to_text, nullptr, 0) : UINT64_MAX;
    uint64_t step = step_text && *step_text ? _strtoui64(step_text, nullptr, 0) : 1;
    if (!step) step = 1;
    return frame >= from && frame <= to && ((frame - from) % step) == 0;
}

static void capture_presented_frame() {
    if (!present_capture_selected(emulated_frame)) return;
    int width = 0, height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0)
        return;
    std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(),
                             width * static_cast<int>(sizeof(uint32_t))) != 0)
        return;
    const char* directory = getenv("SRF_PRESENT_CAPTURE_DIR");
    char path[1024];
    snprintf(path, sizeof(path), "%s\\present_f%06llu.ppm", directory,
             static_cast<unsigned long long>(emulated_frame));
    FILE* file = fopen(path, "wb");
    if (!file) return;
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (uint32_t pixel : pixels) {
        const uint8_t rgb[3] = {
            static_cast<uint8_t>(pixel >> 16),
            static_cast<uint8_t>(pixel >> 8),
            static_cast<uint8_t>(pixel),
        };
        fwrite(rgb, 1, sizeof(rgb), file);
    }
    fclose(file);
}

static void video_callback(const void* pixels, unsigned width, unsigned height, size_t pitch) {
    const Uint64 video_start = profile_now();
    if (compat_wide && render_path == RENDER_COMPATIBILITY && core_srf_get_wide_capture) {
        size_t bytes = 0;
        if(core_srf_get_wide_colors) {
            const uint8_t* colors=core_srf_get_wide_colors(&bytes);
            if(colors && bytes==sizeof(compat_wide_colors)) memcpy(compat_wide_colors.data(),colors,bytes);
        }
        if(core_srf_get_wide_sprites) {
            const uint8_t* sprites=core_srf_get_wide_sprites(&bytes);
            if(sprites && bytes==768*128*4) {
                const Uint64 start=profile_now();
                const bool changed=compat_wide_sprite_bytes.size()!=bytes ||
                    memcmp(compat_wide_sprite_bytes.data(),sprites,bytes)!=0;
                bool visible=false;
                if(changed) {
                    for(size_t i=0;i<bytes;i+=4)
                        if(sprites[i] || sprites[i+1] || sprites[i+2] || sprites[i+3]) {visible=true;break;}
                }
                // Most race frames have no eligible OBJ outside the original
                // world window. Avoid creating and uploading a transparent
                // 384 KiB texture until it is actually needed.
                if(changed && (visible || compat_wide_sprite_texture) && !compat_wide_sprite_texture) {
                    compat_wide_sprite_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,768,128);
                    if(compat_wide_sprite_texture) {
                        SDL_SetTextureBlendMode(compat_wide_sprite_texture,SDL_BLENDMODE_BLEND);
                        SDL_SetTextureScaleMode(compat_wide_sprite_texture,SDL_ScaleModeNearest);
                        ++active_profile.texture_recreates;
                    }
                }
                if(changed) {
                    compat_wide_sprite_bytes.assign(sprites,sprites+bytes);
                    if(compat_wide_sprite_texture && SDL_UpdateTexture(compat_wide_sprite_texture,nullptr,sprites,768*4)==0)
                        ++active_profile.uploads;
                }
                active_profile.upload_ms+=profile_elapsed_ms(start,profile_now());
            }
        }
        if (core_srf_get_wide_display_camera) {
            const uint8_t* faces=core_srf_get_wide_display_camera(&bytes);
            compat_wide_display_bytes.clear();
            if (faces && bytes && bytes%sizeof(CompatWideCameraFace)==0 && bytes<=2048*sizeof(CompatWideCameraFace))
                compat_wide_display_bytes.assign(faces,faces+bytes);
        }
        if(core_srf_get_wide_background) {
            const uint8_t* bg=core_srf_get_wide_background(&bytes);
            if(bg && bytes==768*128*4) {
                const Uint64 upload_start = profile_now();
                bg=prepare_compat_recorded_background(bg);
                if(!compat_wide_background_texture) {
                    compat_wide_background_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,768,128);
                    ++active_profile.texture_recreates;
                }
                if(compat_wide_background_texture) {
                    SDL_SetTextureScaleMode(compat_wide_background_texture,
                        filter_mode==FILTER_NEAREST ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
                    SDL_UpdateTexture(compat_wide_background_texture,nullptr,bg,768*4);
                    ++active_profile.uploads;
                }
                active_profile.upload_ms += profile_elapsed_ms(upload_start, profile_now());
            }
        }
        if(core_srf_get_wide_world_reference) {
            const uint8_t* world=core_srf_get_wide_world_reference(&bytes);
            if(world && bytes==208*128*4)
                compat_wide_world_reference.assign(world,world+bytes);
            else compat_wide_world_reference.clear();
        }
        const uint8_t* data = core_srf_get_wide_capture(&bytes);
        if (bytes && bytes % 136 == 0 && bytes <= 2048 * 136) {
            compat_wide_previous = compat_wide_polygons;
            compat_wide_polygons.resize(bytes / 2);
            memcpy(compat_wide_polygons.data(), data, bytes);
            compat_wide_age = 0;
        } else if (compat_wide_age < 120) ++compat_wide_age;
        if (core_srf_get_wide_camera) {
            const uint8_t* camera = core_srf_get_wide_camera(&bytes);
            if (camera && bytes && bytes % sizeof(CompatWideCameraFace) == 0 && bytes <= 2048*sizeof(CompatWideCameraFace)) {
                compat_wide_camera_previous = compat_wide_camera;
                compat_wide_camera.resize(bytes/sizeof(CompatWideCameraFace));
                memcpy(compat_wide_camera.data(), camera, bytes);
            }
        }
        if (core_srf_get_cgram) {
            const uint8_t* palette = core_srf_get_cgram(&bytes);
            if (palette && bytes == 512) memcpy(compat_wide_palette.data(), palette, 512);
        }
    }
    if (compat_draw_stream_debug && render_path == RENDER_COMPATIBILITY && core_srf_get_geometry_probe) {
        size_t bytes = 0;
        const uint8_t* data = core_srf_get_geometry_probe(&bytes, 3);
        if (bytes && bytes % 136 == 0 && bytes <= 2048 * 136) {
            compat_draw_stream_previous = compat_draw_stream;
            compat_draw_stream.resize(bytes / sizeof(uint16_t));
            memcpy(compat_draw_stream.data(), data, bytes);
            compat_draw_stream_age = 0;
        } else if (compat_draw_stream_age < 12) ++compat_draw_stream_age;
    }
    if (pixels && width && height) {
        const Uint64 decode_start = profile_now();
        decode_source_frame(pixels, width, height, pitch);
        active_profile.decode_ms += profile_elapsed_ms(decode_start, profile_now());
        update_compat_hd_overlay();
        const Uint64 materials_start = profile_now();
        apply_n64_materials();
        active_profile.materials_ms += profile_elapsed_ms(materials_start, profile_now());
        if (filter_mode == FILTER_EDGE) {
            edge_upscale(width, height);
            const Uint64 upload_start = profile_now();
            if (ensure_texture(width * 2, height * 2, SDL_PIXELFORMAT_ARGB8888)) {
                SDL_UpdateTexture(texture, nullptr, filtered_pixels.data(),
                                  static_cast<int>(width * 2 * sizeof(uint32_t)));
                ++active_profile.uploads;
            }
            active_profile.upload_ms += profile_elapsed_ms(upload_start, profile_now());
        } else if (ensure_texture(width, height, SDL_PIXELFORMAT_ARGB8888)) {
            const Uint64 upload_start = profile_now();
            SDL_UpdateTexture(texture, nullptr, source_pixels.data(),
                              static_cast<int>(width * sizeof(uint32_t)));
            ++active_profile.uploads;
            active_profile.upload_ms += profile_elapsed_ms(upload_start, profile_now());
        }
        const Uint64 native_state_start = profile_now();
        update_native_live_state();
        active_profile.native_state_ms += profile_elapsed_ms(native_state_start, profile_now());
    }

    const Uint64 render_start = profile_now();
    if (render_path == RENDER_NATIVE_TRACK) {
        if (!draw_native_track(false)) draw_compatibility_frame();
    } else if (render_path == RENDER_HYBRID_COMPARE) {
        draw_compatibility_frame();
        draw_native_track(true);
    } else {
        draw_compatibility_frame();
    }
    active_profile.render_ms += profile_elapsed_ms(render_start, profile_now());
    const Uint64 overlay_start = profile_now();
    draw_status_overlay();
    active_profile.overlay_ms += profile_elapsed_ms(overlay_start, profile_now());
    const Uint64 capture_start = profile_now();
    capture_presented_frame();
    active_profile.capture_ms += profile_elapsed_ms(capture_start, profile_now());
    const Uint64 present_start = profile_now();
    SDL_RenderPresent(renderer);
    active_profile.present_ms += profile_elapsed_ms(present_start, profile_now());
    active_profile.video_ms += profile_elapsed_ms(video_start, profile_now());
}

static void audio_sample_callback(int16_t left, int16_t right) {
    const Uint64 audio_start = profile_now();
    const int16_t samples[2] = {left, right};
    if (audio_device) {
        SDL_QueueAudio(audio_device, samples, sizeof(samples));
        active_profile.audio_bytes += sizeof(samples);
        ++active_profile.audio_callbacks;
    }
    active_profile.audio_ms += profile_elapsed_ms(audio_start, profile_now());
}

static size_t audio_batch_callback(const int16_t* samples, size_t frames) {
    if (!audio_device || !samples || !frames) return frames;
    const Uint64 audio_start = profile_now();
    if (SDL_GetQueuedAudioSize(audio_device) > 48000u * 8u)
        SDL_ClearQueuedAudio(audio_device);
    const Uint32 bytes = static_cast<Uint32>(frames * 4);
    SDL_QueueAudio(audio_device, samples, bytes);
    active_profile.audio_bytes += bytes;
    ++active_profile.audio_callbacks;
    active_profile.audio_ms += profile_elapsed_ms(audio_start, profile_now());
    return frames;
}

static void input_poll_callback() {}

static void open_controller() {
    if (controller) return;
    for (int index = 0; index < SDL_NumJoysticks(); ++index) {
        if (SDL_IsGameController(index)) {
            controller = SDL_GameControllerOpen(index);
            if (controller) return;
        }
    }
}

static int16_t input_state_callback(unsigned port, unsigned device,
                                    unsigned index, unsigned id) {
    (void)index;
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;

    if (!input_script.empty()) {
        const uint16_t bit = joypad_bit(id);
        return (scripted_input_mask & bit) != 0;
    }
    const uint16_t replay_bit = joypad_bit(id);
    if (replay_playing)
        return replay_position < replay_masks.size() &&
               (replay_masks[replay_position] & replay_bit) != 0;

    SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
    SDL_GameControllerButton button = SDL_CONTROLLER_BUTTON_INVALID;
    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_B:      key = SDL_SCANCODE_Z;      button = SDL_CONTROLLER_BUTTON_A; break;
        case RETRO_DEVICE_ID_JOYPAD_Y:      key = SDL_SCANCODE_A;      button = SDL_CONTROLLER_BUTTON_X; break;
        case RETRO_DEVICE_ID_JOYPAD_A:      key = SDL_SCANCODE_X;      button = SDL_CONTROLLER_BUTTON_B; break;
        case RETRO_DEVICE_ID_JOYPAD_X:      key = SDL_SCANCODE_S;      button = SDL_CONTROLLER_BUTTON_Y; break;
        case RETRO_DEVICE_ID_JOYPAD_L:      key = SDL_SCANCODE_C;      button = SDL_CONTROLLER_BUTTON_LEFTSHOULDER; break;
        case RETRO_DEVICE_ID_JOYPAD_R:      key = SDL_SCANCODE_V;      button = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER; break;
        case RETRO_DEVICE_ID_JOYPAD_START:  key = SDL_SCANCODE_RETURN; button = SDL_CONTROLLER_BUTTON_START; break;
        case RETRO_DEVICE_ID_JOYPAD_SELECT: key = SDL_SCANCODE_RSHIFT; button = SDL_CONTROLLER_BUTTON_BACK; break;
        case RETRO_DEVICE_ID_JOYPAD_UP:     key = SDL_SCANCODE_UP;     button = SDL_CONTROLLER_BUTTON_DPAD_UP; break;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:   key = SDL_SCANCODE_DOWN;   button = SDL_CONTROLLER_BUTTON_DPAD_DOWN; break;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:   key = SDL_SCANCODE_LEFT;   button = SDL_CONTROLLER_BUTTON_DPAD_LEFT; break;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:  key = SDL_SCANCODE_RIGHT;  button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT; break;
        default: return 0;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (id == RETRO_DEVICE_ID_JOYPAD_START && (SDL_GetModState() & KMOD_ALT)) return 0;
    bool pressed = key != SDL_SCANCODE_UNKNOWN && keys[key];
    if (controller && button != SDL_CONTROLLER_BUTTON_INVALID &&
        SDL_GameControllerGetButton(controller, button)) pressed = true;
    if (controller) {
        constexpr int dead_zone = 16000;
        const int x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        const int y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (id == RETRO_DEVICE_ID_JOYPAD_LEFT && x < -dead_zone) pressed = true;
        if (id == RETRO_DEVICE_ID_JOYPAD_RIGHT && x > dead_zone) pressed = true;
        if (id == RETRO_DEVICE_ID_JOYPAD_UP && y < -dead_zone) pressed = true;
        if (id == RETRO_DEVICE_ID_JOYPAD_DOWN && y > dead_zone) pressed = true;
    }
    if (pressed) reported_input_mask |= replay_bit;
    return pressed ? 1 : 0;
}

static std::string state_path(int slot) {
    return app_directory + "state-" + std::to_string(slot) + ".bin";
}

static void save_state(int slot) {
    const size_t size = core_retro_serialize_size();
    if (!size) return;
    std::vector<uint8_t> state(size);
    if (!core_retro_serialize(state.data(), state.size())) return;
    FILE* file = fopen(state_path(slot).c_str(), "wb");
    if (!file) return;
    fwrite(state.data(), 1, state.size(), file);
    fclose(file);
}

static void load_state(int slot) {
    FILE* file = fopen(state_path(slot).c_str(), "rb");
    if (!file) return;
    fseek(file, 0, SEEK_END);
    const long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    const size_t expected = core_retro_serialize_size();
    if (file_size == static_cast<long>(expected)) {
        std::vector<uint8_t> state(expected);
        if (fread(state.data(), 1, state.size(), file) == state.size()) {
            if (core_retro_unserialize(state.data(), state.size()) && compat_wide &&
                render_path==RENDER_COMPATIBILITY) {
                if(core_srf_reset_wide_capture) core_srf_reset_wide_capture();
                compat_wide_display_bytes.clear();
                compat_wide_age=120;
            }
        }
    }
    fclose(file);
}

static std::string save_ram_path() {
    return app_directory + "Stunt Race FX.srm";
}

static void load_save_ram() {
    void* memory = core_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    const size_t size = core_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (!memory || !size) return;
    FILE* file = fopen(save_ram_path().c_str(), "rb");
    if (!file) return;
    fread(memory, 1, size, file);
    fclose(file);
}

static void write_save_ram() {
    void* memory = core_retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    const size_t size = core_retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    if (!memory || !size) return;
    FILE* file = fopen(save_ram_path().c_str(), "wb");
    if (!file) return;
    fwrite(memory, 1, size, file);
    fclose(file);
}

static bool env_enabled(const char* name) {
    const char* value = getenv(name);
    return value && value[0] && strcmp(value, "0") != 0;
}

static void profile_initialize() {
    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        renderer_backend = info.name ? info.name : "UNKNOWN";
        renderer_backend_flags = info.flags;
    }
    SDL_GetRendererOutputSize(renderer, &profile_output_width, &profile_output_height);
    fprintf(stderr, "SDL renderer: %s flags=0x%X output=%dx%d%s\n",
            renderer_backend.c_str(), static_cast<unsigned>(renderer_backend_flags),
            profile_output_width, profile_output_height,
            (renderer_backend_flags & SDL_RENDERER_SOFTWARE) ? " SOFTWARE FALLBACK" : " GPU");

    const char* requested_log = getenv("SRF_PROFILE_LOG");
    if (!requested_log || !*requested_log) return;
    profile_log_path = requested_log;
    profile_log = fopen(profile_log_path.c_str(), "wb");
    if (!profile_log) {
        fprintf(stderr, "Profiler: could not open %s\n", profile_log_path.c_str());
        return;
    }
    setvbuf(profile_log, nullptr, _IOFBF, 1 << 20);
    fprintf(profile_log,
        "frame,race,scene_changed,scene_interval,scene_age,total_ms,processing_ms,wait_ms,core_ms,gsu_ms,core_other_ms,video_ms,decode_ms,materials_ms,interpolation_ms,sprite_ms,edge_ms,upload_ms,native_state_ms,render_ms,overlay_ms,present_ms,audio_ms,allocations,uploads,texture_recreates,render_copies,audio_bytes,audio_callbacks,gsu_instructions,gsu_calls,changed_pixels,broad_update,track_chunks,filter,aspect,output_width,output_height,input_mask,spike_level\n");
    fprintf(stderr, "Profiler: logging frames to %s\n", profile_log_path.c_str());
}

static void profile_shutdown() {
    update_rolling_profile();
    if (profile_log) {
        fflush(profile_log);
        fclose(profile_log);
        profile_log = nullptr;
    }
    if (profile_log_path.empty() || !profile_frame_count) return;
    const std::string summary_path = profile_log_path + ".summary.txt";
    FILE* summary = fopen(summary_path.c_str(), "wb");
    if (!summary) return;
    const double count = static_cast<double>(profile_frame_count);
    fprintf(summary, "Stunt Race FX compatibility performance summary\n");
    fprintf(summary, "frames=%llu\n", static_cast<unsigned long long>(profile_frame_count));
    fprintf(summary, "renderer=%s\n", renderer_backend.c_str());
    fprintf(summary, "renderer_flags=0x%X\n", static_cast<unsigned>(renderer_backend_flags));
    fprintf(summary, "output=%dx%d\n", profile_output_width, profile_output_height);
    fprintf(summary, "average_frame_ms=%.4f\n", profile_totals.total_ms / count);
    fprintf(summary, "rolling_1_percent_low_fps=%.2f\n", rolling_one_percent_low);
    fprintf(summary, "worst_frame_ms=%.4f\n", profile_worst_ms);
    fprintf(summary, "average_processing_ms=%.4f\n", profile_totals.processing_ms / count);
    fprintf(summary, "average_wait_ms=%.4f\n", profile_totals.limiter_ms / count);
    fprintf(summary, "average_core_ms=%.4f\n", profile_totals.core_ms / count);
    fprintf(summary, "average_gsu_ms=%.4f\n", profile_totals.gsu_ms / count);
    fprintf(summary, "average_video_ms=%.4f\n", profile_totals.video_ms / count);
    fprintf(summary, "average_decode_ms=%.4f\n", profile_totals.decode_ms / count);
    fprintf(summary, "average_materials_ms=%.4f\n", profile_totals.materials_ms / count);
    fprintf(summary, "average_interpolation_ms=%.4f\n", profile_totals.interpolation_ms / count);
    fprintf(summary, "average_sprite_ms=%.4f\n", profile_totals.sprite_ms / count);
    fprintf(summary, "average_edge_ms=%.4f\n", profile_totals.edge_ms / count);
    fprintf(summary, "average_upload_ms=%.4f\n", profile_totals.upload_ms / count);
    fprintf(summary, "average_native_state_ms=%.4f\n", profile_totals.native_state_ms / count);
    fprintf(summary, "average_render_ms=%.4f\n", profile_totals.render_ms / count);
    fprintf(summary, "average_present_ms=%.4f\n", profile_totals.present_ms / count);
    fprintf(summary, "average_audio_ms=%.4f\n", profile_totals.audio_ms / count);
    fprintf(summary, "audio_bytes=%llu\n", static_cast<unsigned long long>(profile_totals.audio_bytes));
    fprintf(summary, "allocations=%u\n", profile_totals.allocations);
    fprintf(summary, "texture_uploads=%u\n", profile_totals.uploads);
    fprintf(summary, "texture_recreates=%u\n", profile_totals.texture_recreates);
    fclose(summary);
    fprintf(stderr, "Profiler: summary written to %s\n", summary_path.c_str());
}

int main(int argc, char** argv) {
    char executable_path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, executable_path, MAX_PATH);
    char* final_slash = strrchr(executable_path, '\\');
    if (final_slash) final_slash[1] = '\0';
    else executable_path[0] = '\0';
    app_directory = executable_path;
    SetCurrentDirectoryA(app_directory.c_str());

    const std::string fast_core_path = app_directory + "snes9x_libretro.dll";
    const std::string fallback_core_path = app_directory + "bsnes_libretro.dll";
    const std::string core_path =
        GetFileAttributesA(fast_core_path.c_str()) != INVALID_FILE_ATTRIBUTES
            ? fast_core_path : fallback_core_path;
    const std::string default_rom = app_directory + "Stunt Race FX (USA) (Rev 1).sfc";
    const char* rom_path = argc > 1 ? argv[1] : default_rom.c_str();
    if (GetFileAttributesA(core_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        show_error("The SNES compatibility core is missing. Re-extract the complete build.");
        return 1;
    }
    if (GetFileAttributesA(rom_path) == INVALID_FILE_ATTRIBUTES) {
        show_error("Put your legally obtained ROM beside this executable and name it:\n\nStunt Race FX (USA) (Rev 1).sfc");
        return 1;
    }

    core_module = LoadLibraryA(core_path.c_str());
    if (!core_module || !load_core_functions()) {
        if (!core_module) show_error("Windows could not load the SNES compatibility core.");
        return 2;
    }

    core_retro_set_environment(environment_callback);
    core_retro_init();
    core_retro_set_video_refresh(video_callback);
    core_retro_set_audio_sample(audio_sample_callback);
    core_retro_set_audio_sample_batch(audio_batch_callback);
    core_retro_set_input_poll(input_poll_callback);
    core_retro_set_input_state(input_state_callback);

    retro_system_info system_info{};
    core_retro_get_system_info(&system_info);
    retro_game_info game_info{};
    game_info.path = rom_path;
    if (!system_info.need_fullpath) {
        FILE* file = fopen(rom_path, "rb");
        if (!file) { show_error("The ROM could not be opened."); return 3; }
        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        game_rom.resize(size);
        fread(game_rom.data(), 1, game_rom.size(), file);
        fclose(file);
        game_info.data = game_rom.data();
        game_info.size = game_rom.size();
    }
    if (!core_retro_load_game(&game_info)) {
        show_error("The emulator could not load this ROM. Use the USA Rev 1 ROM.");
        core_retro_deinit();
        FreeLibrary(core_module);
        return 4;
    }
    core_retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    load_save_ram();

    retro_system_av_info av_info{};
    core_retro_get_system_av_info(&av_info);
    const double frames_per_second = av_info.timing.fps > 1.0 ? av_info.timing.fps : 60.0988;
    native_fps = frames_per_second;
    const int sample_rate = av_info.timing.sample_rate > 1.0
        ? static_cast<int>(av_info.timing.sample_rate + 0.5) : 48000;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        show_error(SDL_GetError());
        return 5;
    }
    if (!load_materials()) n64_materials = false;
    if (const char* requested_filter = getenv("SRF_FILTER")) {
        if (!_stricmp(requested_filter, "nearest")) filter_mode = FILTER_NEAREST;
        else if (!_stricmp(requested_filter, "linear")) filter_mode = FILTER_LINEAR;
        else filter_mode = FILTER_EDGE;
    } else if (env_enabled("SRF_NEAREST")) {
        filter_mode = FILTER_NEAREST;
    }
    show_fps = env_enabled("SRF_SHOW_FPS");
    if (const char* requested_motion = getenv("SRF_MOTION"))
        motion_smoothing = requested_motion[0] && strcmp(requested_motion, "0") != 0;
    else
        motion_smoothing = true;
    runahead_enabled = env_enabled("SRF_RUNAHEAD");
    if (const char* requested_materials = getenv("SRF_MATERIALS"))
        n64_materials = requested_materials[0] && strcmp(requested_materials, "0") != 0;
    if (const char* requested_distance = getenv("SRF_DRAW_DISTANCE")) {
        if (!_stricmp(requested_distance, "original") || !_stricmp(requested_distance, "off") ||
            !_stricmp(requested_distance, "0")) draw_distance_mode = 0;
        else if (!_stricmp(requested_distance, "far") || atof(requested_distance) >= 2.0)
            draw_distance_mode = 2;
        else draw_distance_mode = 1;
    }
    if (const char* requested_renderer = getenv("SRF_RENDERER")) {
        if (!_stricmp(requested_renderer, "native") || !_stricmp(requested_renderer, "track"))
            render_path = RENDER_NATIVE_TRACK;
        else if (!_stricmp(requested_renderer, "hybrid") || !_stricmp(requested_renderer, "compare"))
            render_path = RENDER_HYBRID_COMPARE;
        else
            render_path = RENDER_COMPATIBILITY;
    }
    if (const char* requested_fov = getenv("SRF_NATIVE_FOV"))
        native_vertical_fov = std::max(40.0f, std::min(85.0f, static_cast<float>(atof(requested_fov))));
    if (const char* requested_width = getenv("SRF_NATIVE_ROAD_SCALE"))
        native_road_width_scale = std::max(1.0f, std::min(6.0f,
            static_cast<float>(atof(requested_width))));
    if (const char* requested_far = getenv("SRF_NATIVE_DISTANCE"))
        native_draw_distance = std::max(8000.0f, std::min(100000.0f,
            static_cast<float>(atof(requested_far))));
    if (const char* requested_fog = getenv("SRF_NATIVE_FOG"))
        native_fog = requested_fog[0] && strcmp(requested_fog, "0") != 0;
    native_debug_bounds = env_enabled("SRF_NATIVE_WIREFRAME");
    native_debug_ids = env_enabled("SRF_NATIVE_IDS");
    if (motion_smoothing) filter_mode = FILTER_EDGE;
    if (const char* requested_cap = getenv("SRF_CAP")) {
        if (!_stricmp(requested_cap, "unlimited")) speed_mode = 3;
        else {
            const double cap = atof(requested_cap);
            speed_mode = cap > 105.0 ? 2 : (cap > 65.0 ? 1 : 0);
        }
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                filter_mode == FILTER_NEAREST ? "0" : "2");
    open_controller();
    load_input_script();

    int window_width = 960, window_height = 720;
    if (const char* dimensions = getenv("SRF_WINDOW"))
        sscanf(dimensions, "%dx%d", &window_width, &window_height);
    compat_draw_stream_debug = env_enabled("SRF_COMPAT_DRAW_STREAM") ?
        std::min(2, std::max(1, atoi(getenv("SRF_COMPAT_DRAW_STREAM")))) : 0;
    compat_wide = env_enabled("SRF_COMPAT_WIDE");
    compat_hd_center = env_enabled("SRF_COMPAT_HD_CENTER");
    if(compat_hd_center) compat_wide=true;
    const bool start_reconstruction_recording=env_enabled("SRF_RECORD_SESSION");
    if(start_reconstruction_recording) compat_wide=true;
    if (env_enabled("SRF_STRETCH")) aspect_mode = ASPECT_STRETCH;
    else if (env_enabled("SRF_AMBIENT")) aspect_mode = ASPECT_AMBIENT;
    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (env_enabled("SRF_HIDDEN")) window_flags |= SDL_WINDOW_HIDDEN;
    if (env_enabled("SRF_FULLSCREEN")) window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    window = SDL_CreateWindow("Stunt Race FX Enhanced v4.14 - First Track Materials",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              window_width, window_height, window_flags);
    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    if (env_enabled("SRF_VSYNC")) renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    renderer = SDL_CreateRenderer(window, -1, renderer_flags);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!window || !renderer) {
        show_error(SDL_GetError());
        return 5;
    }
    profile_frequency = SDL_GetPerformanceFrequency();
    profile_initialize();
    // Create the two recording-validated surface maps before gameplay begins,
    // preventing a first-use upload hitch when the race palette appears.
    if(n64_materials && compat_hd_center) {
        ensure_compat_recorded_material(MATERIAL_ASPHALT);
        ensure_compat_recorded_material(MATERIAL_GRASS);
    }
    if(start_reconstruction_recording && !srf_start_recording())
        show_error("The reconstruction recording folder could not be created.");

    SDL_AudioSpec requested{}, obtained{};
    requested.freq = sample_rate;
    requested.format = AUDIO_S16SYS;
    requested.channels = 2;
    requested.samples = 1024;
    if (!env_enabled("SRF_NO_AUDIO"))
        audio_device = SDL_OpenAudioDevice(nullptr, 0, &requested, &obtained, 0);
    if (audio_device) SDL_PauseAudioDevice(audio_device, 0);

    bool running = true;
    const Uint64 timer_frequency = profile_frequency;
    const bool precise_timer_period = timeBeginPeriod(1) == TIMERR_NOERROR;
    double next_frame_deadline = static_cast<double>(SDL_GetPerformanceCounter());
    Uint64 fps_window_start = static_cast<Uint64>(next_frame_deadline);
    uint32_t fps_window_frames = 0;
    uint32_t scene_window_frames = 0;
    const char* max_frames_text = getenv("SRF_MAX_FRAMES");
    const uint64_t max_frames = max_frames_text && *max_frames_text
        ? _strtoui64(max_frames_text, nullptr, 0) : 0;
    const bool fast_capture = env_enabled("SRF_FAST");
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            else if (event.type == SDL_CONTROLLERDEVICEADDED) open_controller();
            else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (controller) SDL_GameControllerClose(controller);
                controller = nullptr;
                open_controller();
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                const SDL_Scancode key = event.key.keysym.scancode;
                if (key == SDL_SCANCODE_ESCAPE) running = false;
                else if (key == SDL_SCANCODE_F11 ||
                         (key == SDL_SCANCODE_RETURN && (event.key.keysym.mod & KMOD_ALT))) {
                    const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
                    SDL_SetWindowFullscreen(window, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    set_toast(fullscreen ? "WINDOWED" : "FULLSCREEN");
                } else if (key == SDL_SCANCODE_F10) {
                    aspect_mode = static_cast<AspectMode>((static_cast<int>(aspect_mode) + 1) % 3);
                    set_toast(aspect_name());
                } else if (key == SDL_SCANCODE_W && (event.key.keysym.mod & KMOD_CTRL) &&
                           render_path == RENDER_COMPATIBILITY) {
                    compat_wide = !compat_wide;
                    if(core_srf_reset_wide_capture) core_srf_reset_wide_capture();
                    compat_wide_display_bytes.clear();
                    compat_wide_age = 120;
                    compat_wide_camera.clear();
                    compat_wide_camera_previous.clear();
                    set_toast(compat_wide ? "EXPANDED SIDES PREVIEW ON" : "EXPANDED SIDES OFF");
                } else if (key == SDL_SCANCODE_H && (event.key.keysym.mod & KMOD_CTRL) &&
                           render_path == RENDER_COMPATIBILITY) {
                    compat_hd_center=!compat_hd_center;
                    if(compat_hd_center) compat_wide=true;
                    compat_hd_center_ready=false;
                    set_toast(compat_hd_center ? "HD WORLD VIEW PREVIEW ON" : "HD WORLD VIEW PREVIEW OFF");
                } else if (key == SDL_SCANCODE_F8 && render_path == RENDER_COMPATIBILITY) {
                    compat_draw_stream_debug = (compat_draw_stream_debug + 1) % 3;
                    if (!compat_draw_stream_debug) {
                        compat_draw_stream.clear();
                        compat_draw_stream_previous.clear();
                        compat_draw_stream_age = 12;
                    }
                    set_toast(compat_draw_stream_debug == 2 ? "PREVIOUS POLYGON SUBMISSION" :
                        compat_draw_stream_debug ? "LATEST POLYGON SUBMISSION" : "ORIGINAL POLYGON OUTLINES OFF");
                } else if (key == SDL_SCANCODE_F12) {
                    show_fps = !show_fps;
                    set_toast(show_fps ? "FPS ON" : "FPS OFF");
                } else if (key == SDL_SCANCODE_G) {
                    filter_mode = static_cast<FilterMode>((static_cast<int>(filter_mode) + 1) % 3);
                    if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
                    set_toast(filter_name());
                } else if (key == SDL_SCANCODE_R &&
                           (event.key.keysym.mod & KMOD_CTRL) &&
                           (event.key.keysym.mod & KMOD_SHIFT)) {
                    if(reconstruction_recording) {
                        srf_stop_recording();
                        set_toast("RECONSTRUCTION RECORDING SAVED");
                    } else {
                        compat_wide=true;
                        if(srf_start_recording()) set_toast("RECONSTRUCTION RECORDING ON");
                        else set_toast("RECORDING START FAILED");
                    }
                } else if (key == SDL_SCANCODE_R && (event.key.keysym.mod & KMOD_CTRL)) {
                    if (replay_recording) {
                        replay_recording = false;
                        save_replay();
                        set_toast("REPLAY SAVED");
                    } else {
                        replay_masks.clear();
                        replay_position = 0;
                        replay_playing = false;
                        replay_recording = true;
                        set_toast("RECORDING");
                    }
                } else if (key == SDL_SCANCODE_R) {
                    render_path = static_cast<RenderPath>((static_cast<int>(render_path) + 1) % 3);
                    set_toast(render_path_name());
                } else if (key == SDL_SCANCODE_B) {
                    native_debug_bounds = !native_debug_bounds;
                    set_toast(native_debug_bounds ? "DEBUG WIREFRAME ON" : "DEBUG WIREFRAME OFF");
                } else if (key == SDL_SCANCODE_I) {
                    native_debug_ids = !native_debug_ids;
                    set_toast(native_debug_ids ? "SEGMENT IDS ON" : "SEGMENT IDS OFF");
                } else if (key == SDL_SCANCODE_F) {
                    native_fog = !native_fog;
                    set_toast(native_fog ? "NATIVE FOG ON" : "NATIVE FOG OFF");
                } else if (key == SDL_SCANCODE_LEFTBRACKET) {
                    native_vertical_fov = std::max(40.0f, native_vertical_fov - 2.0f);
                    char message[64];
                    snprintf(message, sizeof(message), "VERTICAL FOV %.0F", native_vertical_fov);
                    set_toast(message);
                } else if (key == SDL_SCANCODE_RIGHTBRACKET) {
                    native_vertical_fov = std::min(85.0f, native_vertical_fov + 2.0f);
                    char message[64];
                    snprintf(message, sizeof(message), "VERTICAL FOV %.0F", native_vertical_fov);
                    set_toast(message);
                } else if (key == SDL_SCANCODE_SEMICOLON || key == SDL_SCANCODE_APOSTROPHE) {
                    native_road_width_scale = std::max(1.0f, std::min(6.0f,
                        native_road_width_scale + (key == SDL_SCANCODE_APOSTROPHE ? 0.1f : -0.1f)));
                    if (!native_track.nodes.empty())
                        rebuild_native_track(native_track.source_bank, native_track.source_address);
                    char message[64];
                    snprintf(message, sizeof(message), "ROAD WIDTH %.1FX", native_road_width_scale);
                    set_toast(message);
                } else if (key == SDL_SCANCODE_MINUS) {
                    native_draw_distance = std::max(8000.0f, native_draw_distance * 0.75f);
                    char message[64];
                    snprintf(message, sizeof(message), "NATIVE FAR %.0F", native_draw_distance);
                    set_toast(message);
                } else if (key == SDL_SCANCODE_EQUALS) {
                    native_draw_distance = std::min(100000.0f, native_draw_distance * 1.333333f);
                    char message[64];
                    snprintf(message, sizeof(message), "NATIVE FAR %.0F", native_draw_distance);
                    set_toast(message);
                } else if (key == SDL_SCANCODE_P && (event.key.keysym.mod & KMOD_CTRL)) {
                    replay_recording = false;
                    if (replay_playing) {
                        replay_playing = false;
                        set_toast("REPLAY STOPPED");
                    } else if (load_replay()) {
                        replay_playing = true;
                        set_toast("REPLAY STARTED");
                    } else {
                        set_toast("NO REPLAY");
                    }
                } else if (key == SDL_SCANCODE_P) {
                    apply_visual_preset(visual_preset + 1);
                } else if (key == SDL_SCANCODE_M) {
                    motion_smoothing = !motion_smoothing;
                    if (motion_smoothing && filter_mode != FILTER_EDGE) {
                        filter_mode = FILTER_EDGE;
                        if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
                    }
                    motion_previous.clear();
                    motion_target.clear();
                    motion_pixels.clear();
                    set_toast(motion_smoothing ? "SPRITE-SAFE MOTION ON" : "MOTION OFF");
                } else if (key == SDL_SCANCODE_T) {
                    n64_materials = !n64_materials;
                    set_toast(n64_materials ? "N64 MATERIALS ON" : "N64 MATERIALS OFF");
                } else if (key == SDL_SCANCODE_D) {
                    draw_distance_mode = (draw_distance_mode + 1) % 3;
                    native_draw_distance = draw_distance_mode == 2 ? 60000.0f
                        : (draw_distance_mode == 1 ? 30000.0f : 12000.0f);
                    set_toast(draw_distance_name());
                } else if (key == SDL_SCANCODE_L) {
                    runahead_enabled = !runahead_enabled;
                    set_toast(runahead_enabled ? "RUNAHEAD ON" : "RUNAHEAD OFF");
                } else if (key == SDL_SCANCODE_SPACE) {
                    paused = !paused;
                    set_toast(paused ? "PAUSED" : "RESUMED");
                } else if (key == SDL_SCANCODE_PERIOD && paused) {
                    frame_advance = true;
                } else if (key == SDL_SCANCODE_PAGEUP) {
                    if (speed_mode < 3) ++speed_mode;
                    const double cap = cap_fps();
                    char message[64];
                    if (cap > 0.0) snprintf(message, sizeof(message), "CAP %.1F HZ", cap);
                    else snprintf(message, sizeof(message), "CAP UNLIMITED");
                    set_toast(message);
                    next_frame_deadline = static_cast<double>(SDL_GetPerformanceCounter());
                } else if (key == SDL_SCANCODE_PAGEDOWN) {
                    if (speed_mode > 0) --speed_mode;
                    char message[64];
                    snprintf(message, sizeof(message), "CAP %.1F HZ", cap_fps());
                    set_toast(message);
                    next_frame_deadline = static_cast<double>(SDL_GetPerformanceCounter());
                } else if (key == SDL_SCANCODE_HOME) {
                    speed_mode = 0;
                    set_toast("CAP 60.1 HZ");
                    next_frame_deadline = static_cast<double>(SDL_GetPerformanceCounter());
                } else if (key == SDL_SCANCODE_F5 && !(event.key.keysym.mod & KMOD_SHIFT)) {
                    restore_track_visibility();
                    core_retro_reset();
                } else if (key >= SDL_SCANCODE_F1 && key <= SDL_SCANCODE_F9) {
                    const int slot = static_cast<int>(key - SDL_SCANCODE_F1) + 1;
                    if (event.key.keysym.mod & KMOD_SHIFT) save_state(slot);
                    else load_state(slot);
                }
            }
        }

        if (paused && !frame_advance) {
            video_callback(nullptr, 0, 0, 0);
            SDL_Delay(10);
            next_frame_deadline = static_cast<double>(SDL_GetPerformanceCounter());
            continue;
        }
        frame_advance = false;
        profile_begin_frame();
        update_scripted_input();
        const Uint64 scene_state_start = profile_now();
        update_scene_and_draw_distance();
        active_profile.scene_state_ms += profile_elapsed_ms(scene_state_start, profile_now());
        if (core_srf_reset_gsu_profile) core_srf_reset_gsu_profile();
        else if (core_srf_reset_gsu_trace) core_srf_reset_gsu_trace();
        if (core_srf_reset_gsu_trace &&
            (reconstruction_recording || capture_frame_selected(emulated_frame + 1)))
            core_srf_reset_gsu_trace();
        if (core_srf_start_geometry_probe && getenv("SRF_GEOMETRY_PROBE") &&
            capture_frame_selected(emulated_frame + 1)) {
            const char* watch = getenv("SRF_GEOMETRY_WATCH");
            core_srf_start_geometry_probe(watch ? strtoul(watch, nullptr, 16) : UINT32_MAX);
        } else if (compat_draw_stream_debug && render_path == RENDER_COMPATIBILITY &&
                   core_srf_start_geometry_probe) {
            core_srf_start_geometry_probe(0xfffffffeu);
        }
        reported_input_mask = 0;
        const bool observe_wide=compat_wide && render_path==RENDER_COMPATIBILITY;
        if (observe_wide!=compat_wide_observing) {
            if(core_srf_reset_wide_capture) core_srf_reset_wide_capture();
            compat_wide_display_bytes.clear();
            compat_wide_age=120;
            compat_wide_observing=observe_wide;
        }
        if (compat_wide && render_path == RENDER_COMPATIBILITY && core_srf_start_wide_capture)
            core_srf_start_wide_capture();
        const Uint64 core_start = profile_now();
        if (runahead_enabled) {
            const size_t state_size = core_retro_serialize_size();
            active_profile.allocations += 2;
            std::vector<uint8_t> current_state(state_size);
            std::vector<uint8_t> advanced_state(state_size);
            if (state_size && core_retro_serialize(current_state.data(), state_size)) {
                core_av_enable = 2;
                core_retro_run();
                const bool saved_advanced = core_retro_serialize(advanced_state.data(), state_size);
                core_av_enable = 1;
                core_retro_run();
                if (saved_advanced) core_retro_unserialize(advanced_state.data(), state_size);
                else core_retro_unserialize(current_state.data(), state_size);
                core_av_enable = 3;
            } else {
                core_av_enable = 3;
                core_retro_run();
            }
        } else {
            core_av_enable = 3;
            core_retro_run();
        }
        active_profile.core_ms += profile_elapsed_ms(core_start, profile_now());
        if (core_srf_get_gsu_time_ns)
            active_profile.gsu_ms = static_cast<double>(core_srf_get_gsu_time_ns()) / 1000000.0;
        if (core_srf_get_gsu_instruction_count)
            active_profile.gsu_instructions = core_srf_get_gsu_instruction_count();
        if (core_srf_get_gsu_exec_calls)
            active_profile.gsu_calls = core_srf_get_gsu_exec_calls();
        ++emulated_frame;
        srf_record_frame();
        if (replay_recording) replay_masks.push_back(reported_input_mask);
        if (replay_playing) {
            ++replay_position;
            if (replay_position >= replay_masks.size()) {
                replay_playing = false;
                set_toast("REPLAY COMPLETE");
            }
        }
        const Uint64 capture_state_start = profile_now();
        capture_gsu_state();
        active_profile.capture_ms += profile_elapsed_ms(capture_state_start, profile_now());
        if (max_frames && emulated_frame >= max_frames) running = false;
        ++fps_window_frames;
        if (visual_frame_changed) ++scene_window_frames;
        const Uint64 fps_now = SDL_GetPerformanceCounter();
        if (fps_now - fps_window_start >= timer_frequency / 2) {
            measured_fps = static_cast<double>(fps_window_frames) * timer_frequency /
                           static_cast<double>(fps_now - fps_window_start);
            measured_scene_fps = static_cast<double>(scene_window_frames) * timer_frequency /
                                 static_cast<double>(fps_now - fps_window_start);
            fps_window_frames = 0;
            scene_window_frames = 0;
            fps_window_start = fps_now;
        }
        const double current_cap = fast_capture ? 0.0 : cap_fps();
        if (current_cap <= 0.0) {
            next_frame_deadline = static_cast<double>(fps_now);
            profile_commit_frame();
            continue;
        }
        const double frame_ticks = static_cast<double>(timer_frequency) / current_cap;
        const Uint64 limiter_start = profile_now();
        next_frame_deadline += frame_ticks;
        const double late_reset_threshold = frame_ticks * 2.0;
        double now_ticks = static_cast<double>(SDL_GetPerformanceCounter());
        if (now_ticks > next_frame_deadline + late_reset_threshold)
            next_frame_deadline = now_ticks;
        while (running) {
            const Uint64 now = SDL_GetPerformanceCounter();
            const double remaining_ticks = next_frame_deadline - static_cast<double>(now);
            if (remaining_ticks <= 0.0) break;
            const double remaining_ms = remaining_ticks * 1000.0 /
                                         static_cast<double>(timer_frequency);
            if (remaining_ms > 1.5) {
                SDL_Delay(static_cast<Uint32>(remaining_ms - 0.75));
            } else if (remaining_ms > 0.2) {
                SwitchToThread();
            }
        }
        active_profile.limiter_ms += profile_elapsed_ms(limiter_start, profile_now());
        profile_commit_frame();
    }

    srf_stop_recording();
    profile_shutdown();
    if (precise_timer_period) timeEndPeriod(1);
    restore_track_visibility();
    write_save_ram();
    if (replay_recording) save_replay();
    if (audio_device) SDL_CloseAudioDevice(audio_device);
    if (controller) SDL_GameControllerClose(controller);
    if (texture) SDL_DestroyTexture(texture);
    for (SDL_Texture*& material_texture : native_material_textures) {
        if (material_texture) SDL_DestroyTexture(material_texture);
        material_texture = nullptr;
    }
    for (SDL_Texture*& material_texture : compat_recorded_material_textures) {
        if (material_texture) SDL_DestroyTexture(material_texture);
        material_texture = nullptr;
    }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    core_retro_unload_game();
    core_retro_deinit();
    SDL_Quit();
    FreeLibrary(core_module);
    return 0;
}

