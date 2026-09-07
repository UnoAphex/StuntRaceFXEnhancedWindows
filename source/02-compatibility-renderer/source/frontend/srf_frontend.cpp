/*
 * Stunt Race FX compatibility frontend
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
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

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static SDL_GameController* controller;
static SDL_AudioDeviceID audio_device;
static retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
static int texture_width;
static int texture_height;
static bool stretch_video;
static std::string app_directory;

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
    return true;
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

static bool ensure_texture(unsigned width, unsigned height) {
    if (texture && texture_width == static_cast<int>(width) &&
        texture_height == static_cast<int>(height)) return true;
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, sdl_pixel_format(),
                                SDL_TEXTUREACCESS_STREAMING, width, height);
    texture_width = static_cast<int>(width);
    texture_height = static_cast<int>(height);
    return texture != nullptr;
}

static void video_callback(const void* pixels, unsigned width, unsigned height, size_t pitch) {
    if (pixels && width && height && ensure_texture(width, height))
        SDL_UpdateTexture(texture, nullptr, pixels, static_cast<int>(pitch));

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (texture) {
        SDL_Rect destination{};
        SDL_Rect* destination_ptr = nullptr;
        if (!stretch_video) {
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
    if (key != SDL_SCANCODE_UNKNOWN && keys[key]) return 1;
    if (controller && button != SDL_CONTROLLER_BUTTON_INVALID &&
        SDL_GameControllerGetButton(controller, button)) return 1;
    if (controller) {
        constexpr int dead_zone = 16000;
        const int x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        const int y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
        if (id == RETRO_DEVICE_ID_JOYPAD_LEFT && x < -dead_zone) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_RIGHT && x > dead_zone) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_UP && y < -dead_zone) return 1;
        if (id == RETRO_DEVICE_ID_JOYPAD_DOWN && y > dead_zone) return 1;
    }
    return 0;
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

    const std::string core_path = app_directory + "bsnes_libretro.dll";
    const std::string default_rom = app_directory + "Stunt Race FX (USA) (Rev 1).sfc";
    const char* rom_path = argc > 1 ? argv[1] : default_rom.c_str();
    if (GetFileAttributesA(core_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        show_error("bsnes_libretro.dll is missing. Re-extract the complete build.");
        return 1;
    }
    if (GetFileAttributesA(rom_path) == INVALID_FILE_ATTRIBUTES) {
        show_error("Put your legally obtained ROM beside this executable and name it:\n\nStunt Race FX (USA) (Rev 1).sfc");
        return 1;
    }

    core_module = LoadLibraryA(core_path.c_str());
    if (!core_module || !load_core_functions()) {
        if (!core_module) show_error("Windows could not load bsnes_libretro.dll.");
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
    const int sample_rate = av_info.timing.sample_rate > 1.0
        ? static_cast<int>(av_info.timing.sample_rate + 0.5) : 48000;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        show_error(SDL_GetError());
        return 5;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, env_enabled("SRF_NEAREST") ? "0" : "2");
    open_controller();

    int window_width = 960, window_height = 720;
    if (const char* dimensions = getenv("SRF_WINDOW"))
        sscanf(dimensions, "%dx%d", &window_width, &window_height);
    stretch_video = env_enabled("SRF_STRETCH");
    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (env_enabled("SRF_FULLSCREEN")) window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    window = SDL_CreateWindow("Stunt Race FX - Playable Compatibility Build",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              window_width, window_height, window_flags);
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
    const double frame_ticks = static_cast<double>(timer_frequency) / frames_per_second;
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
                } else if (key == SDL_SCANCODE_F10) {
                    stretch_video = !stretch_video;
                } else if (key == SDL_SCANCODE_F5) {
                    core_retro_reset();
                } else if (key >= SDL_SCANCODE_F1 && key <= SDL_SCANCODE_F9) {
                    const int slot = static_cast<int>(key - SDL_SCANCODE_F1) + 1;
                    if (event.key.keysym.mod & KMOD_SHIFT) save_state(slot);
                    else load_state(slot);
                }
            }
        }

        core_retro_run();
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
