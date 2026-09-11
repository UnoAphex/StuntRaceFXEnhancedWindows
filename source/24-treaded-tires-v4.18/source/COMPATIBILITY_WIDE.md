# Compatibility expanded-view preview — v4.9

This is the first opt-in expanded-world compatibility preview. It is not the
experimental native track renderer, a stretched framebuffer, or a complete
high-resolution replacement for the original center viewport.

## What is implemented

- Capture original face vertices before the original screen-edge rejection.
- Read the original camera-space vertex table and object translation. Project
  a presentation copy with the verified focal scale of 128 and center (104,64).
- Near-clip and screen-clip that copy, triangulate, and rasterize the **side
  regions only** using SDL_RenderGeometry on the accelerated SDL renderer.
- Preserve the complete original center image, including its sprite-safe
  interpolation, car, HUD, menus and text. Preserve the original vertical scale
  and SNES pixel aspect; wider windows add horizontal visibility.
- Extend the original mode-3 BG2 tiles using each scanline's scroll, palette and
  brightness. A persistent 768x128 streaming texture is GPU-scaled at original
  texel density. This is background tiling, not stretching the finished frame.
- Bypass side drawing at 4:3, outside a recognized race, or without a recognized
  displayed bitmap. Normal compatibility play remains opt-out/default.
- Keep experimental native drawing functions, interpolation routines, input,
  simulation and GSU cycle scheduling unchanged.

Use the two `Play True Wide Preview ...cmd` launchers. Ctrl+W switches the
extension in compatibility mode; Alt+Enter/F11 switches fullscreen. F12 toggles
the FPS/performance overlay. The supported ROM CRC32 is 380C2635
(unmodified USA Rev 1). Other ROMs do not enable the instruction observer.

## Original pipeline findings

The original 208x128 race viewport sits at (24,32) in the 256x224 SNES image.
Signed projected coordinates exist beyond the original horizontal clip bounds
0..207. At GSU bank 01, prefetched PC 918E / opcode B5, the face's vertex list
has been assembled but the accumulated screen outcode has not yet rejected it.
The ROM face indices address the camera-coordinate table at RAM 0340. The
original object translation is at 0026/0028/002A; projection center is at
0034/0036. These are inspected, never rewritten.

The original reciprocal projection table is consistent with a focal scale of
128. PC projection removes the final integer divide/rasterization staircase
for the sides; it does **not** undo earlier integer rotation/vertex quantization.
High-resolution central geometry and selectable 1x/2x/3x/4x remain pending.

### Display ownership — important correction during this iteration

A GSU picture can span several SNES frames. Clearing geometry every video
callback discarded partial submissions. A fixed previous-submission delay
also selected the wrong picture during turns. Both approaches were tested and
replaced for the displayed side geometry.

The observer now accumulates whole pictures in two bounded presentation-only
buffers keyed by SCBR 0B / 25. Retail framebuffer bases are 2C00 / 9400.
Read-only DMA observation freezes a finished picture when the original game
starts transferring it to VRAM:

| Picture | First observed transfer | Byte count |
| --- | --- | --- |
| A | 70:2C00 to VRAM word 0000 | 2200 |
| B | 70:B800 to VRAM word 3400 | 2200 |

The transfers continue over subsequent SNES frames. The first visible BG1
tile, read at scanline 32 with its actual scroll/tilemap state, selects A when
its physical tile address is 0000, and B when it is 4400. Side drawing selects
that frozen picture instead of the currently executing GSU picture. Unsupported
tile mappings fail closed. DMA behavior and data are unchanged.

The per-SNES-frame signed/camera captures remain diagnostic outputs; they are
not the final displayed list. `wide_display_camera` records the selected list.
`wide_dma` contains 8 u16 words per event: source bank/address, destination VRAM
word address, byte count, transfer mode, SCBR, current partial camera count,
observer frame serial. Diagnostic files are written only to the requested
external capture directory, never included in the release.

## Known limitations / next work

- Earlier object-level culling can still omit scenery outside the original
  view. This preview recovers pre-screen-clip faces, not every world object.
- Special/textured polygon paths are not decoded. No fabricated scenery is
  substituted for missing geometry.
- The center remains the original low-resolution renderer. Side edges are
  GPU-rasterized at output resolution, so differences at the join are visible.
- Original center motion smoothing is unchanged; side geometry follows the
  displayed GSU picture. It does not yet interpolate matching polygon vertices.
  Some join motion/quantization differences remain. No new input delay is added.
- Material replacement presets affect the inherited center, not the sides.
  New launchers disable material replacement for the closest color match.
- The original bezel and HUD stay centered; the wide world is confined to the
  original race view's vertical band. This is not an edge-to-edge HUD redesign.
- Race-state/tilemap guards and bitmap addresses are retail-specific. Every
  course, pause transition, replay, save-state load, fullscreen switch and GPU
  backend have not yet been visually validated.
- No new MSAA, central high-resolution polygon replacement, or guarantee of
  sustained 60 FPS on all hardware is claimed.

## Build and test

Use `frontend/build-playable-backend.cmd` with SDL2_ROOT set to an x64 SDL2
installation; the script uses MSVC BuildTools. Build the core in `core/libretro`
with MinGW `make platform=win LTO=` and copy the resulting DLL into build/.
The inherited makefile lacks complete local-header dependencies: after editing
the observation headers force fxemu.c/gfx.cpp to rebuild (or do a clean build).

`tools/perf/run_first_track_benchmark.ps1` accepts `-Wide`, `-OutputSize`,
`-CaptureFrame`, `-CaptureFrames`, `-MaxFrames` and `-RealTime`. It runs from a
temporary ROM-free runtime copy with deterministic input and no inherited saves.
The historical input route is a repeatable stress route, not proof of a clean
lap or complete bridge coverage. Hidden/uncapped tests measure processing cost,
not visible scanout latency or guaranteed real-world frame pacing.

See WIDE_VALIDATION.md for the final packaged-binary checks. No private original
source, ROM, captures, save data or ROM-derived exports are shipped.
