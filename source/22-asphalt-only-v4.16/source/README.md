# Stunt Race FX Verified Asphalt Only v4.16 — source bundle

Version 22 removes the automatic BG2 grass pass after runtime testing showed
that the green background layer can also underlie or participate in the near
road presentation. Only the two verified asphalt face signatures remain
textured. See BG2_MATERIAL_ROLLBACK.md.

## Inherited v4.15 classification work

Version 21 corrects the first-track material classification after runtime play
showed that a recurring green polygon family is shared with road/shoulder
geometry. Green polygon faces are no longer labeled as grass. Asphalt remains
limited to verified road colors, and grass detail is confined to the BG2
landscape layer below the horizon. See MATERIAL_CLASSIFICATION_FIX.md.

## Inherited v4.14 material work

Version 20 applies the first complete recorded race to the compatibility
renderer's visuals. Recurrent first-course road faces receive output-resolution
asphalt detail, while recorded BG2 behavior is used to texture the broad green
landscape without touching the sky, cars, signs, barriers, HUD, or sprites. The
rules are deliberately evidence-based instead of applying the old broad color
classifier to every face. See FIRST_TRACK_MATERIALS.md for findings and tests.
The session recorder remains available and the experimental native renderer is
unchanged.

## Inherited v4.13 recording work

Version 19 adds opt-in continuous recording of the compatibility renderer's
runtime reconstruction evidence. The dedicated recording launcher creates an
ignored root-level session directory containing a lossless compressed binary
container, frame index, and manifest. Geometry, camera packets, palette, VRAM,
sprites, framebuffer layers, Super FX state/traces, WRAM, input, timing, and
periodic exact checkpoints are retained. The included inspection tool verifies
the stream and reconstructs complete channel state at any chosen frame. See
RECONSTRUCTION_RECORDING.md. Normal launchers do not record.

## Inherited v4.12 work

Version 18 adds an opt-in full-width HD presentation path to the compatibility
renderer. The same observed original Super FX polygon packets used by the side
preview are now GPU-rasterized across the center and expanded sides at the
selected output resolution. A reconstructed 208x128 world reference separates
the original HUD, text, animated wheel sprites, and unsupported bitmap
primitives into a correctly proportioned overlay. This removes the previous
center/side sharpness mismatch without changing simulation or Super FX timing.
See COMPATIBILITY_HD_WIDE.md for design, validation, performance, and limits.
The experimental native renderer is unchanged.

## Inherited v4.11 work

Version 17 decodes original texture-mapped polygon packets for the compatibility
wide view and adds a conservative compositor for eligible original OBJ pixels
crossing the old race-window edge. It also records the verified early object-cull
boundary and the work required for a sharp central 3D layer. See WIDE_OBJECTS.md.
The experimental native renderer is unchanged. The following sections describe
the inherited implementation and history.

Version 15 adds opt-in GPU-rasterized side viewports from observed original
camera-space polygons and a scanline-accurate BG2 tile extension. It reveals
additional world without stretching the original center image or HUD.
See COMPATIBILITY_WIDE.md for implementation, validation and limitations.
This is a first expanded-view preview, not a complete replacement rasterizer.
The experimental native implementation remains unchanged from version 12.

## Inherited history

Version 14 captures original projected polygons and adds an optional F8 outline
comparison to the compatibility frontend. See COMPATIBILITY_DRAW_STREAM.md.
Full replacement rasterization and true ultrawide remain pending.
The following notes describe the inherited versions.

Version 13 adds opt-in read-only GSU instruction histograms and register watches.
See COMPATIBILITY_GEOMETRY_PROBE.md for the format, tests and pending work.
This milestone does not implement true ultrawide or higher-resolution polygons.
Native renderer implementation is unchanged. The following v4.6 notes describe
the inherited baseline, not additional v4.7 work.

This is the corresponding source for the ROM-free v4.6 build. It preserves the
v4.5 compatibility-performance baseline and advances the optional native path
using verified XLR8 development-source structures as private research reference.

The shipped source contains no XLR8/Nintendo source or extracted game assets.
The implementation uses the findings to interpret runtime state: authored road
scale, ordered wheel/axle pose, and source-consistent vehicle dimensions.

- `frontend/`: Windows SDL/libretro frontend and its build script.
- `core/`: complete libretro Snes9x source at revision
  `890b5d445538fe790aa3add3d5702c80f551e0ae`, with the local Super FX timing,
  scene-inspection, and frontend bridge changes used by the included binary.
- `materials/`: generated source art, 64x64 runtime tiles, processing script,
  and exact generation prompts.
- `tools/`: live-scene extraction and GSU disassembly research helpers.
- `tools/track/`: offline course-centerline inspector and diagnostic OBJ export.
- `NATIVE_TRACK_RESEARCH.md`: traced course format, architecture, validation,
  controls, and candid first-milestone limitations.
- `PORTING.md`: enhanced-port roadmap and compatibility constraints.
- `COMPATIBILITY_PERFORMANCE.md`: bridge investigation, feature-isolation
  results, optimizations, and remaining source-cadence limitation.
- `SOURCE_INFORMED_RENDERER.md`: v4.6 evidence, implementation changes, and
  remaining retail-model/scene-decoding work.
- `tools/perf/`: deterministic first-track input, benchmark runner, and CSV
  analyzer. Generated results are intentionally kept outside the repository.

No game ROM, save RAM, save state, captured framebuffer, or ROM-derived game
asset is included.

The frontend can be rebuilt from the parent workspace with
`frontend/build-playable-backend.cmd`. The core was built under MSYS2 MinGW64:

```text
make -C core/libretro -j4 platform=win LTO=
```

See the Snes9x, SDL2, and frontend license files in the runtime package.

Additional design and validation notes: SPRITE_SAFE_SMOOTHING.md and
COMPATIBILITY_PERFORMANCE.md
