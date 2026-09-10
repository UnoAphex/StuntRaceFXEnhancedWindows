# Enhanced Port Plan

## Verified starting point

- The supplied US Rev 1 ROM is 1,048,576 bytes and matches MD5
  `128b316a74caf17fdc216b3ab46d4a9a`.
- Release builds succeed with MSVC, CMake, SDL2, `snesrecomp`, and LakeSnes.
- A 300-frame reference run and the built-in two-function interception run both
  finish with WRAM checksum `EED02566`.
- The current project is a hybrid recompilation. LakeSnes still executes the
  original 65816 and Super FX programs for nearly all gameplay. The C sources
  are useful reverse-engineering work, but they are not yet a standalone port.

## What “60 FPS” means here

The SNES host already presents about 60 video frames per second. Stunt Race FX's
polygon scene changes much less often because the Super FX renderer needs
multiple host frames to finish its work. Merely raising the SDL refresh rate
duplicates old images and does not make steering, physics, or geometry smoother.

The safe route is:

1. Keep the original simulation timestep as the compatibility reference.
2. Capture previous/current camera and object transforms at simulation ticks.
3. Render interpolated transforms at 60 Hz with a native GPU renderer.
4. Only after visual parity, audit and convert frame-counted physics, timers,
   particles, animation, audio commands, and input sampling to explicit time.

This avoids the classic “double the frame rate, double the game speed” failure.

## What “ultrawide” means here

The first enhanced presentation milestone supports ultrawide windows without
distorting the original picture. It centers an aspect-correct 4:3 image and uses
the extra area as pillarbox space. This is implemented now.

True additional world visibility requires replacing the Super FX framebuffer
renderer. The native renderer must widen the camera frustum, extend horizontal
culling, and keep 2D HUD elements in a safe 4:3 region. Stretching or revealing
uninitialized framebuffer columns is not true ultrawide.

## Rendering milestones

### Native track research prototype

The first-race course pointer and centerline/width table have now been decoded
and rendered as a persistent world-space PC mesh.  The prototype has a native,
compatibility, and hybrid comparison path, PC-side FOV/frustum/draw-distance
controls, materials/fog/debug overlays, and a centered original HUD.  See
`NATIVE_TRACK_RESEARCH.md` for the traced addresses, cache architecture,
validation status, and limitations.  Static scenery/model decoding and a
depth-buffered GPU backend remain future milestones.

### Milestone 1 — deterministic hybrid baseline (complete)

- Reproducible SDL2 dependency manifest
- Validated ROM revision
- Release build
- Reference/interception checksums
- Aspect-correct resize and ultrawide-window handling
- Linear/best texture filtering and fullscreen controls

### Milestone 2 — capture and inspect the Super FX scene (in progress)

- Corrected the inherited GSU opcode decoder and pipeline control-flow bugs
- Added deterministic controller scripts (`SRF_INPUT_SCRIPT`)
- Added JSONL GSU launch tracing (`SRF_GSU_TRACE`)
- Instrument GSU program launches and framebuffer swaps
- Identify camera matrices, model pointers, primitive streams, palette/material
  state, depth ordering, and viewport data
- Export deterministic frame captures for comparison

The corrected first launch at `01:CF42` now stops after 379,681 modeled cycles.
Its final work-RAM hash (`5567A6CD`) and all general registers match an
independent accuracy-oriented Super FX implementation; its internal PC differs
only by the one-byte representation used by this interpreter's pipeline. A
1,100-frame scripted regression reached 20 GSU jobs across four entry points,
all of which stopped normally, and repeated with WRAM checksum `4E00DF8B`.

### Milestone 3 — native geometry renderer

- Decode models and tracks from the user's ROM at runtime
- Reproduce original projection, clipping, flat shading, and draw order
- Composite the original PPU-rendered HUD and menus over the native 3D scene
- Add MSAA for polygon edges and an optional post-process pass

### Milestone 4 — smooth 60 Hz presentation

- Snapshot object and camera transforms at canonical simulation ticks
- Interpolate visual state at 60 Hz
- Decouple presentation pacing from emulated SNES VBlank
- Validate input latency, audio synchronization, replay determinism, and timers

### Milestone 5 — true widescreen and ultrawide

- Horizontal-plus field-of-view policy for 16:9, 16:10, 21:9, and 32:9
- Wider culling bounds and corrected projection constants
- HUD safe area, split-screen policy, and configurable FOV
- Regression captures for every course, camera, and scene transition

## Current enhanced controls

- `F11` or `Alt+Enter`: toggle borderless fullscreen
- `1`: corrected 4:3 aspect
- `2`: raw framebuffer pixel aspect
- `3`: stretch to the window

Environment options:

- `SRF_WINDOW=3440x1440`
- `SRF_FULLSCREEN=1`
- `SRF_ASPECT=4:3`, `raw`, or `stretch`
- `SRF_FILTER=nearest`, `linear`, or `best`
- `SRF_VSYNC=0` or `1`
- `SRF_INPUT_SCRIPT=path` for deterministic frame-range controller input
- `SRF_GSU_TRACE=path` for one JSON object per GSU launch

The `linear` and `best` modes smooth scaling of the original framebuffer. True
polygon-edge antialiasing comes in Milestone 3; it cannot be recovered from the
finished low-resolution image without blurring detail.

## Distribution rule

Never commit or package the ROM, derived ROM data, save states, or frame dumps.
The executable should require the user to select their own matching ROM at
runtime. The source project and its dependencies are MIT/Zlib-compatible, but
Nintendo game data is not part of that license.
