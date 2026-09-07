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
#include <windows.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
static srf_get_blob_t core_srf_get_gsu_ram;
static srf_get_blob_t core_srf_get_gsu_registers;

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
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
static bool paused;
static bool frame_advance;
static int core_av_enable = 3;
static int visual_preset = 1;
static bool replay_recording;
static bool replay_playing;
static size_t replay_position;
static uint16_t reported_input_mask;
static std::vector<uint16_t> replay_masks;
static int motion_step = 4;
static Uint64 toast_until;
static std::string toast_text;
static std::vector<uint32_t> source_pixels;
static std::vector<uint32_t> filtered_pixels;
static std::vector<uint32_t> motion_previous;
static std::vector<uint32_t> motion_target;
static std::vector<uint32_t> motion_pixels;
static std::vector<uint32_t> previous_scene_pixels;
static bool visual_frame_changed;
static std::string app_directory;
static uint64_t emulated_frame;
static unsigned source_width;
static unsigned source_height;

struct ScriptedInput {
    uint64_t first_frame;
    uint64_t last_frame;
    uint16_t mask;
};
static std::vector<ScriptedInput> input_script;
static uint16_t scripted_input_mask;

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
    core_srf_get_gsu_registers = reinterpret_cast<srf_get_blob_t>(
        GetProcAddress(core_module, "srf_get_gsu_registers"));
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
    const unsigned inverse = static_cast<unsigned>(4 - step);
    const unsigned amount = static_cast<unsigned>(step);
    const unsigned r = (((previous >> 16) & 255u) * inverse + ((target >> 16) & 255u) * amount) / 4u;
    const unsigned g = (((previous >> 8) & 255u) * inverse + ((target >> 8) & 255u) * amount) / 4u;
    const unsigned b = ((previous & 255u) * inverse + (target & 255u) * amount) / 4u;
    return 0xff000000u | (r << 16) | (g << 8) | b;
}

static const std::vector<uint32_t>& apply_motion_smoothing() {
    if (!motion_smoothing) {
        motion_previous.clear();
        motion_target.clear();
        motion_pixels.clear();
        motion_step = 4;
        return source_pixels;
    }
    if (motion_target.size() != source_pixels.size()) {
        motion_previous = source_pixels;
        motion_target = source_pixels;
        motion_pixels = source_pixels;
        motion_step = 4;
        return motion_pixels;
    }

    size_t samples = 0, changed = 0;
    for (size_t index = 0; index < source_pixels.size(); index += 4) {
        ++samples;
        if (color_distance(source_pixels[index], motion_target[index]) > 48) ++changed;
    }
    if (changed * 100 > samples * 2) {
        motion_previous = motion_target;
        motion_target = source_pixels;
        motion_step = 2;
    } else if (motion_step < 4) {
        ++motion_step;
    }
    motion_pixels.resize(source_pixels.size());
    for (size_t index = 0; index < source_pixels.size(); ++index)
        motion_pixels[index] = blend_motion(motion_previous[index], motion_target[index], motion_step);
    return motion_pixels;
}

static void edge_upscale(const void* pixels, unsigned width, unsigned height, size_t pitch) {
    source_width = width;
    source_height = height;
    std::vector<uint32_t> next_pixels(static_cast<size_t>(width) * height);
    for (unsigned y = 0; y < height; ++y) {
        const uint8_t* row = static_cast<const uint8_t*>(pixels) + y * pitch;
        for (unsigned x = 0; x < width; ++x)
            next_pixels[static_cast<size_t>(y) * width + x] = read_source_pixel(row, x);
    }

    visual_frame_changed = false;
    if (previous_scene_pixels.size() == next_pixels.size()) {
        size_t tested = 0, changed = 0;
        const unsigned first_y = height / 10;
        const unsigned last_y = height * 4 / 5;
        for (unsigned y = first_y; y < last_y; y += 2)
            for (unsigned x = width / 12; x < width * 11 / 12; x += 2) {
                const size_t index = static_cast<size_t>(y) * width + x;
                ++tested;
                if (color_distance(next_pixels[index], previous_scene_pixels[index]) > 30)
                    ++changed;
            }
        visual_frame_changed = tested && changed * 200 > tested;
    }
    previous_scene_pixels = next_pixels;
    source_pixels.swap(next_pixels);

    const std::vector<uint32_t>& input_pixels = apply_motion_smoothing();
    const unsigned output_width = width * 2;
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
    if (aspect_mode == ASPECT_AMBIENT) return "AMBIENT 21:9";
    if (aspect_mode == ASPECT_STRETCH) return "STRETCH";
    return "ASPECT 4:3";
}

static void set_toast(const std::string& text);

static void recreate_texture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

static void apply_visual_preset(int preset) {
    visual_preset = (preset % 4 + 4) % 4;
    motion_smoothing = false;
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
    } else {
        filter_mode = FILTER_EDGE;
        aspect_mode = ASPECT_AMBIENT;
        motion_smoothing = true;
        set_toast("PRESET SMOOTH MOTION");
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
        char fps_line[64], scene_line[64], cap_line[64];
        snprintf(fps_line, sizeof(fps_line), "OUTPUT %.1F FPS", measured_fps);
        snprintf(scene_line, sizeof(scene_line), "SCENE %.1F FPS", measured_scene_fps);
        const double cap = cap_fps();
        if (cap > 0.0) snprintf(cap_line, sizeof(cap_line), "CAP %.1F HZ", cap);
        else snprintf(cap_line, sizeof(cap_line), "CAP UNLIMITED");
        lines.emplace_back(fps_line);
        lines.emplace_back(scene_line);
        lines.emplace_back(cap_line);
        lines.emplace_back(filter_name());
        lines.emplace_back(aspect_name());
        if (motion_smoothing) lines.emplace_back("MOTION ON");
        if (runahead_enabled) lines.emplace_back("RUNAHEAD ON");
        if (replay_recording) lines.emplace_back("RECORDING");
        if (replay_playing) lines.emplace_back("REPLAY");
    }
    if (paused) lines.emplace_back("PAUSED");
    if (show_toast) lines.push_back(toast_text);
    size_t longest = 0;
    for (const std::string& line : lines) if (line.size() > longest) longest = line.size();
    const int padding = 4 * scale;
    SDL_Rect box{16, 16, static_cast<int>(longest * 6 * scale + padding * 2),
                 static_cast<int>(lines.size() * 9 * scale + padding)};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 185);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 105, 235, 255, 255);
    int y = box.y + padding;
    for (const std::string& line : lines) {
        draw_text(line, box.x + padding, y, scale);
        y += 9 * scale;
    }
}

static void video_callback(const void* pixels, unsigned width, unsigned height, size_t pitch) {
    if (pixels && width && height) {
        if (filter_mode == FILTER_EDGE) {
            edge_upscale(pixels, width, height, pitch);
            if (ensure_texture(width * 2, height * 2, SDL_PIXELFORMAT_ARGB8888))
                SDL_UpdateTexture(texture, nullptr, filtered_pixels.data(),
                                  static_cast<int>(width * 2 * sizeof(uint32_t)));
        } else if (ensure_texture(width, height, sdl_pixel_format())) {
            SDL_UpdateTexture(texture, nullptr, pixels, static_cast<int>(pitch));
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (texture) {
        SDL_Rect destination{};
        SDL_Rect* destination_ptr = nullptr;
        if (aspect_mode == ASPECT_AMBIENT) {
            int output_width = 0, output_height = 0;
            SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
            SDL_SetTextureColorMod(texture, 72, 82, 96);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_SetTextureColorMod(texture, 255, 255, 255);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 105);
            SDL_Rect shade{0, 0, output_width, output_height};
            SDL_RenderFillRect(renderer, &shade);
        }
        if (aspect_mode != ASPECT_STRETCH) {
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
        SDL_RenderCopy(renderer, texture, nullptr, destination_ptr);
    }
    draw_status_overlay();
    SDL_RenderPresent(renderer);
}

static void audio_sample_callback(int16_t left, int16_t right) {
    const int16_t samples[2] = {left, right};
    if (audio_device) SDL_QueueAudio(audio_device, samples, sizeof(samples));
}

static size_t audio_batch_callback(const int16_t* samples, size_t frames) {
    if (!audio_device || !samples || !frames) return frames;
    if (SDL_GetQueuedAudioSize(audio_device) > 48000u * 8u)
        SDL_ClearQueuedAudio(audio_device);
    SDL_QueueAudio(audio_device, samples, static_cast<Uint32>(frames * 4));
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
        if (fread(state.data(), 1, state.size(), file) == state.size())
            core_retro_unserialize(state.data(), state.size());
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
    std::vector<uint8_t> rom_data;
    if (!system_info.need_fullpath) {
        FILE* file = fopen(rom_path, "rb");
        if (!file) { show_error("The ROM could not be opened."); return 3; }
        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        rom_data.resize(size);
        fread(rom_data.data(), 1, rom_data.size(), file);
        fclose(file);
        game_info.data = rom_data.data();
        game_info.size = rom_data.size();
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
    if (const char* requested_filter = getenv("SRF_FILTER")) {
        if (!_stricmp(requested_filter, "nearest")) filter_mode = FILTER_NEAREST;
        else if (!_stricmp(requested_filter, "linear")) filter_mode = FILTER_LINEAR;
        else filter_mode = FILTER_EDGE;
    } else if (env_enabled("SRF_NEAREST")) {
        filter_mode = FILTER_NEAREST;
    }
    show_fps = env_enabled("SRF_SHOW_FPS");
    motion_smoothing = env_enabled("SRF_MOTION");
    runahead_enabled = env_enabled("SRF_RUNAHEAD");
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
    if (env_enabled("SRF_STRETCH")) aspect_mode = ASPECT_STRETCH;
    else if (env_enabled("SRF_AMBIENT")) aspect_mode = ASPECT_AMBIENT;
    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (env_enabled("SRF_HIDDEN")) window_flags |= SDL_WINDOW_HIDDEN;
    if (env_enabled("SRF_FULLSCREEN")) window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    window = SDL_CreateWindow("Stunt Race FX - Playable Compatibility Build",
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

    SDL_AudioSpec requested{}, obtained{};
    requested.freq = sample_rate;
    requested.format = AUDIO_S16SYS;
    requested.channels = 2;
    requested.samples = 1024;
    if (!env_enabled("SRF_NO_AUDIO"))
        audio_device = SDL_OpenAudioDevice(nullptr, 0, &requested, &obtained, 0);
    if (audio_device) SDL_PauseAudioDevice(audio_device, 0);

    bool running = true;
    const Uint64 timer_frequency = SDL_GetPerformanceFrequency();
    Uint64 previous_frame = SDL_GetPerformanceCounter();
    Uint64 fps_window_start = previous_frame;
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
                } else if (key == SDL_SCANCODE_F12) {
                    show_fps = !show_fps;
                    set_toast(show_fps ? "FPS ON" : "FPS OFF");
                } else if (key == SDL_SCANCODE_G) {
                    filter_mode = static_cast<FilterMode>((static_cast<int>(filter_mode) + 1) % 3);
                    if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
                    set_toast(filter_name());
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
                    set_toast(motion_smoothing ? "MOTION ON" : "MOTION OFF");
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
                    previous_frame = SDL_GetPerformanceCounter();
                } else if (key == SDL_SCANCODE_PAGEDOWN) {
                    if (speed_mode > 0) --speed_mode;
                    char message[64];
                    snprintf(message, sizeof(message), "CAP %.1F HZ", cap_fps());
                    set_toast(message);
                    previous_frame = SDL_GetPerformanceCounter();
                } else if (key == SDL_SCANCODE_HOME) {
                    speed_mode = 0;
                    set_toast("CAP 60.1 HZ");
                    previous_frame = SDL_GetPerformanceCounter();
                } else if (key == SDL_SCANCODE_F5 && !(event.key.keysym.mod & KMOD_SHIFT)) {
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
            previous_frame = SDL_GetPerformanceCounter();
            continue;
        }
        frame_advance = false;
        update_scripted_input();
        reported_input_mask = 0;
        if (runahead_enabled) {
            const size_t state_size = core_retro_serialize_size();
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
        ++emulated_frame;
        if (replay_recording) replay_masks.push_back(reported_input_mask);
        if (replay_playing) {
            ++replay_position;
            if (replay_position >= replay_masks.size()) {
                replay_playing = false;
                set_toast("REPLAY COMPLETE");
            }
        }
        capture_gsu_state();
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
            previous_frame = fps_now;
            continue;
        }
        const double frame_ticks = static_cast<double>(timer_frequency) / current_cap;
        while (running) {
            const Uint64 now = SDL_GetPerformanceCounter();
            const double elapsed = static_cast<double>(now - previous_frame);
            if (elapsed >= frame_ticks) {
                previous_frame = now;
                break;
            }
            const double remaining_ms = (frame_ticks - elapsed) * 1000.0 /
                                        static_cast<double>(timer_frequency);
            if (remaining_ms > 1.5) SDL_Delay(static_cast<Uint32>(remaining_ms - 1.0));
        }
    }

    write_save_ram();
    if (replay_recording) save_replay();
    if (audio_device) SDL_CloseAudioDevice(audio_device);
    if (controller) SDL_GameControllerClose(controller);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    core_retro_unload_game();
    core_retro_deinit();
    SDL_Quit();
    FreeLibrary(core_module);
    return 0;
}
