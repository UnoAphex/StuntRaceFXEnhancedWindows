# Stunt Race FX Compatibility Geometry Probe v4.7 — source bundle

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
