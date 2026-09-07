/*
 * SDL2 platform layer — window, renderer, audio output, frame timing.
 */

#include "snesrecomp/platform.h"
#include "snesrecomp/menu_overlay.h"
#include "snesrecomp/snesrecomp.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window   *s_window   = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture  *s_texture  = NULL;
static SDL_AudioDeviceID s_audio_dev = 0;
static uint64_t      s_frame_start = 0;
static int           s_cur_scale  = 0;   /* tracks live window-scale changes */
static int           s_cur_filter = -1;  /* tracks live texture-filter changes */
static int           s_aspect_mode = 0;  /* 0=4:3, 1=raw pixels, 2=stretch */
static bool          s_fullscreen = false;

static bool env_enabled(const char *name, bool fallback) {
    const char *value = getenv(name);
    if (!value || !*value) return fallback;
    return strcmp(value, "0") != 0 && strcmp(value, "false") != 0 &&
           strcmp(value, "off") != 0 && strcmp(value, "no") != 0;
}

static void load_video_options(void) {
    const char *aspect = getenv("SRF_ASPECT");
    if (aspect && (!strcmp(aspect, "raw") || !strcmp(aspect, "8:7")))
        s_aspect_mode = 1;
    else if (aspect && !strcmp(aspect, "stretch"))
        s_aspect_mode = 2;
    else
        s_aspect_mode = 0;

    s_fullscreen = env_enabled("SRF_FULLSCREEN", false);
}

static void calculate_destination(int out_w, int out_h, int menu_h,
                                  int source_w, int source_h, SDL_Rect *dst) {
    int available_h = out_h - menu_h;
    if (available_h < 1) available_h = 1;

    if (s_aspect_mode == 2) {
        *dst = (SDL_Rect){ 0, menu_h, out_w, available_h };
        return;
    }

    double aspect = s_aspect_mode == 1
        ? (double)source_w / (double)source_h
        : 4.0 / 3.0;
    int width = out_w;
    int height = (int)((double)width / aspect + 0.5);
    if (height > available_h) {
        height = available_h;
        width = (int)((double)height * aspect + 0.5);
    }

    dst->x = (out_w - width) / 2;
    dst->y = menu_h + (available_h - height) / 2;
    dst->w = width;
    dst->h = height;
}

/* (Re)create the streaming game texture with the given filter (0=nearest,
 * 1=linear). Called at init and when the menu changes the filter. */
static void recreate_texture(int filter) {
    const char *quality = filter >= 2 ? "2" : (filter ? "1" : "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, quality);
    if (s_texture) SDL_DestroyTexture(s_texture);
    s_texture = SDL_CreateTexture(s_renderer,
        SDL_PIXELFORMAT_RGBX8888, SDL_TEXTUREACCESS_STREAMING,
        SNES_RENDER_WIDTH, SNES_RENDER_HEIGHT);
    s_cur_filter = filter;
}

/* NTSC frame time: ~16.6393 ms (60.098 Hz) */
static const double FRAME_TIME_MS = 1000.0 / 60.098;

bool platform_init(const char *window_title, int scale) {
    if (scale < 1) scale = 1;
    if (scale > 8) scale = 8;

    int win_w = SNES_RENDER_WIDTH * scale / 2;  /* 512 is 2x native, scale from 256 */
    int win_h = SNES_RENDER_HEIGHT * scale / 2;  /* 478 is ~2x native */

    load_video_options();
    const char *window_size = getenv("SRF_WINDOW");
    int requested_w = 0, requested_h = 0;
    if (window_size && sscanf(window_size, "%dx%d", &requested_w, &requested_h) == 2 &&
        requested_w >= 320 && requested_h >= 200) {
        win_w = requested_w;
        win_h = requested_h;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (s_fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    s_window = SDL_CreateWindow(
        window_title ? window_title : "snesrecomp",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        window_flags
    );
    if (!s_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    bool vsync = env_enabled("SRF_VSYNC", true);
    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    if (vsync) renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    s_renderer = SDL_CreateRenderer(s_window, -1, renderer_flags);
    if (!s_renderer) {
        /* The software fallback also makes automated/dummy-video tests work. */
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!s_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return false;
    }

    /* Smooth output by default; SRF_FILTER=nearest keeps hard pixel edges. */
    const char *filter = getenv("SRF_FILTER");
    int initial_filter = filter && !strcmp(filter, "nearest") ? 0 :
                         (filter && !strcmp(filter, "best") ? 2 : 1);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                initial_filter == 0 ? "0" : (initial_filter == 2 ? "2" : "1"));

    s_texture = SDL_CreateTexture(s_renderer,
        SDL_PIXELFORMAT_RGBX8888,
        SDL_TEXTUREACCESS_STREAMING,
        SNES_RENDER_WIDTH, SNES_RENDER_HEIGHT);
    if (!s_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return false;
    }

    /* Initialize audio */
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = 32040;       /* SNES native sample rate */
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 1024;
    want.callback = NULL;    /* Push mode */

    s_audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audio_dev > 0) {
        SDL_PauseAudioDevice(s_audio_dev, 0);  /* Start playback */
    }

    SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);
    SDL_RenderClear(s_renderer);
    SDL_RenderPresent(s_renderer);

    s_cur_scale  = scale;
    s_cur_filter = initial_filter;

    /* ImGui menu overlay (no-op in headless/scripted runs). Apply any persisted
     * scale/filter from its loaded config. */
    menu_overlay_init(s_window, s_renderer);
    if (menu_overlay_get_scale() != s_cur_scale) {
        s_cur_scale = menu_overlay_get_scale();
        SDL_SetWindowSize(s_window, SNES_RENDER_WIDTH * s_cur_scale / 2,
                                    SNES_RENDER_HEIGHT * s_cur_scale / 2);
    }
    if (getenv("SRF_FILTER") == NULL && menu_overlay_get_filter() != s_cur_filter)
        recreate_texture(menu_overlay_get_filter());

    printf("Video: aspect=%s, filter=%s, vsync=%s, fullscreen=%s\n",
           s_aspect_mode == 0 ? "4:3" : (s_aspect_mode == 1 ? "raw 8:7" : "stretch"),
           s_cur_filter == 0 ? "nearest" : (s_cur_filter == 2 ? "best" : "linear"),
           vsync ? "on" : "off", s_fullscreen ? "on" : "off");

    s_frame_start = SDL_GetPerformanceCounter();
    return true;
}

void platform_present_frame(const uint8_t *framebuffer) {
    if (!s_texture || !framebuffer) return;

    /* Apply live graphics-setting changes from the menu. */
    int want_scale = menu_overlay_get_scale();
    if (want_scale != s_cur_scale) {
        s_cur_scale = want_scale;
        SDL_SetWindowSize(s_window, SNES_RENDER_WIDTH * s_cur_scale / 2,
                                    SNES_RENDER_HEIGHT * s_cur_scale / 2);
    }
    if (getenv("SRF_FILTER") == NULL) {
        int want_filter = menu_overlay_get_filter();
        if (want_filter != s_cur_filter) recreate_texture(want_filter);
    }

    SDL_UpdateTexture(s_texture, NULL, framebuffer,
                      SNES_RENDER_WIDTH * 4);

    /* Crop the PPU's black overscan bands (the active picture is centred in the
     * 478-tall buffer), and place the game below the menu bar so the menu never
     * covers the top-of-screen HUD (lap timer/position). */
    int rx, ry, rw, rh;
    snesrecomp_active_video_rect(&rx, &ry, &rw, &rh);
    SDL_Rect src = { rx, ry, rw, rh };
    int menu_h = menu_overlay_get_menubar_height();
    int out_w = 0, out_h = 0;
    SDL_GetRendererOutputSize(s_renderer, &out_w, &out_h);
    SDL_Rect dst;
    calculate_destination(out_w, out_h, menu_h, rw, rh, &dst);

    /* Clear to the menu-bar colour so the strip above the game (which the ImGui
     * menu bar doesn't fully paint) blends with the menu instead of showing
     * black. Falls back to black when the menu is disabled. */
    int br = 0, bg = 0, bb = 0;
    if (menu_h > 0) menu_overlay_get_bar_color(&br, &bg, &bb);
    SDL_SetRenderDrawColor(s_renderer, (Uint8)br, (Uint8)bg, (Uint8)bb, 255);
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, &src, &dst);
    menu_overlay_render(s_renderer);   /* ImGui menu on top (no-op when disabled) */
    SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);   /* restore */
    SDL_RenderPresent(s_renderer);
}

void platform_queue_audio(const int16_t *samples, int sample_count) {
    if (s_audio_dev > 0 && samples && sample_count > 0) {
        /* Don't let audio buffer grow too large */
        uint32_t queued = SDL_GetQueuedAudioSize(s_audio_dev);
        if (queued < 32040 * 2 * 2 * 4) { /* ~4 frames max */
            float vol = menu_overlay_get_volume();   /* 1.0 when menu disabled */
            if (vol >= 0.999f) {
                SDL_QueueAudio(s_audio_dev, samples,
                               (uint32_t)(sample_count * 2 * sizeof(int16_t)));
            } else {
                /* Scale by master volume into a temp buffer. */
                static int16_t scaled[4096 * 2];
                int n = sample_count * 2;
                if (n > (int)(sizeof(scaled) / sizeof(scaled[0]))) n = sizeof(scaled) / sizeof(scaled[0]);
                for (int i = 0; i < n; i++) scaled[i] = (int16_t)((int)samples[i] * vol);
                SDL_QueueAudio(s_audio_dev, scaled, (uint32_t)(n * sizeof(int16_t)));
            }
        }
    }
}

/* Mouse motion accumulator (defined in input.c) */
extern void recomp_input_accumulate_mouse(int dx, int dy);

bool platform_poll_events(void) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        /* Let the ImGui menu see every event first; if it captured the event
         * (typing in a field, rebinding, clicking a menu) the game ignores it. */
        int consumed = menu_overlay_process_event(&ev);
        switch (ev.type) {
        case SDL_QUIT:
            return false;
        case SDL_KEYDOWN:
            if (!consumed) {
                /* Esc quits only when the menu isn't using it (rebind-cancel). */
                if (ev.key.keysym.sym == SDLK_ESCAPE) return false;
                /* Save-state hotkeys (mirror File -> Save / Load). */
                if (ev.key.keysym.sym == SDLK_F5) snesrecomp_save_state("srf_state.sav");
                if (ev.key.keysym.sym == SDLK_F8) snesrecomp_load_state("srf_state.sav");
                if (ev.key.keysym.sym == SDLK_F11 ||
                    (ev.key.keysym.sym == SDLK_RETURN && (ev.key.keysym.mod & KMOD_ALT))) {
                    s_fullscreen = !s_fullscreen;
                    SDL_SetWindowFullscreen(s_window,
                        s_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                }
                if (ev.key.keysym.sym == SDLK_1) s_aspect_mode = 0;
                if (ev.key.keysym.sym == SDLK_2) s_aspect_mode = 1;
                if (ev.key.keysym.sym == SDLK_3) s_aspect_mode = 2;
            }
            break;
        case SDL_MOUSEMOTION:
            if (!consumed)
                recomp_input_accumulate_mouse(ev.motion.xrel, ev.motion.yrel);
            break;
        default:
            break;
        }
    }
    if (menu_overlay_quit_requested()) return false;
    return true;
}

void platform_frame_sync(void) {
    uint64_t now = SDL_GetPerformanceCounter();
    double elapsed_ms = (double)(now - s_frame_start) * 1000.0
                        / (double)SDL_GetPerformanceFrequency();
    double remaining = FRAME_TIME_MS - elapsed_ms;
    if (remaining > 1.0) {
        SDL_Delay((uint32_t)(remaining - 0.5));
    }
    s_frame_start = SDL_GetPerformanceCounter();
}

void platform_shutdown(void) {
    menu_overlay_shutdown();
    if (s_audio_dev > 0) {
        SDL_CloseAudioDevice(s_audio_dev);
        s_audio_dev = 0;
    }
    if (s_texture)  SDL_DestroyTexture(s_texture);
    if (s_renderer) SDL_DestroyRenderer(s_renderer);
    if (s_window)   SDL_DestroyWindow(s_window);
    SDL_Quit();
    s_texture  = NULL;
    s_renderer = NULL;
    s_window   = NULL;
}
