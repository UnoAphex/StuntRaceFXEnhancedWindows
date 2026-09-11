# v4.9 validation — 2026-09-08

Tested binaries (SHA-256):

- StuntRaceFX.exe: `D55D3C3D32917678FCFBBBB588D1A628E049184CE8B69DE78FBEE099DBCD60F9`
- snes9x_libretro.dll: `4C93024B287040CA547E018606BD28AFF32A813E20E1818C9A3D7EBB280F3E18`

These are the final binaries in build/. Packaging checks compare ZIP entries
against these same files. Original native rendering functions were not edited;
changes in the shared core are opt-in read-only observers, not execution changes.

## Regression evidence

- Deterministic 3,400-frame input run boots, passes menus, loads the first race
  and drives. Uses USA Rev 1, GSU 300%, edge filter, sprite-safe motion, materials
  off, no audio, isolated temporary runtime with no inherited save RAM.
- At frame 3300, wide-enabled 3440x1440 and wide-enabled 512x384 runs match the
  earlier clean baseline byte-for-byte for GSU RAM, WRAM and GSU registers.
- The wide-enabled 4:3 frame-3300 PPM also matches that baseline byte-for-byte:
  SHA-256 `DD3392112D53F483444B997B7C8C3F347E4B3361C6A32CBF3FD9BC607E8458C8`.
- A final frame-900 menu capture has centered, unstretched car selection and
  black side margins, with no track painted over the menu.
- Inspected 1080p frame 2600 with correctly scaled car/HUD and genuinely extended
  ground/background; inspected turning frame 2900 after transfer-based picture
  selection. Earlier fixed-delay results showed a larger join mismatch and
  were superseded. Inspected ultrawide frame 3300 during development.
- No claim of exhaustive all-track, all-preset, audio, fullscreen, pause-menu or
  sprite-animation visual testing. The inherited sprite/interpolation/input
  routines are unchanged; the state checks are evidence, not a formal proof
  of every possible gameplay state.

## Timing samples — Direct3D SDL backend

| Run, 3400 frames | Average frame | Average processing | Rolling 1% low | Worst frame |
| --- | ---: | ---: | ---: | ---: |
| Timed 1920x1080, with capture | 16.6241 ms | 4.4405 ms | 59.94 FPS | 108.5049 ms |
| Uncapped 3440x1440, with capture | 4.4110 ms | 4.4110 ms | 118.14 FPS | 253.3775 ms |
| Timed 3440x1440, no capture | 16.5963 ms | 4.3714 ms | 59.63 FPS | 34.4339 ms |

The capture runs include diagnostic file work and should not be treated as
clean worst-frame benchmarks. The no-capture ultrawide run still had spikes:
frames 1759 (34.43 ms), 1720 (31.29 ms), 2878 (30.37 ms), 2884 (26.76 ms).
Do not advertise locked 60 FPS or zero hitching. During frames 2200 onward,
average frame time was 16.5912 ms and worst was 30.3720 ms in this run.

The timed no-capture run averaged 0.6402 ms GSU, 0.4271 ms interpolation,
0.0044 ms sprite handling, 1.0178 ms edge processing, 0.0992 ms uploads, and
0.2377 ms present. It reused two textures; no per-frame texture creation.
The inherited allocation counter is instrumented events, not a heap census.
All runs were hidden and audio-disabled. These tests demonstrate CPU/presentation
headroom, not monitor-visible latency, fullscreen pacing or audio correctness.

## Local evidence locations (not shipped)

- Final wide/ultrawide state test: TEMP/StuntRaceFXWE-benchmark-20260908-135836
- Timed 1080p/capture: TEMP/StuntRaceFXWE-benchmark-20260908-135852
- Final 4:3 regression: TEMP/StuntRaceFXWE-benchmark-20260908-135949
- Final menu: TEMP/StuntRaceFXWE-benchmark-20260908-140117
- Timed ultrawide/no capture: TEMP/StuntRaceFXWE-benchmark-20260908-140150
- Transfer trace investigation: TEMP/StuntRaceFXWE-benchmark-20260908-135438
- Display-owned turn comparison: TEMP/StuntRaceFXWE-benchmark-20260908-135726

The release contains no ROM, captured image, RAM dump or private source material.
