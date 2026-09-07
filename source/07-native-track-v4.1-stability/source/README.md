# Stunt Race FX Native Track Prototype v4.1 — source bundle

This is the corresponding source for the ROM-free v4.1 native-track prototype.

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

No game ROM, save RAM, save state, captured framebuffer, or ROM-derived game
asset is included.

The frontend can be rebuilt from the parent workspace with
`frontend/build-playable-backend.cmd`. The core was built under MSYS2 MinGW64:

```text
make -C core/libretro -j4 platform=win LTO=
```

See the Snes9x, SDL2, and frontend license files in the runtime package.
