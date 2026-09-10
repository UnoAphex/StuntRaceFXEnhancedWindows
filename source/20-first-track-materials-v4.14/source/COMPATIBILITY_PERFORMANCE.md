# Compatibility renderer performance — v4.5

## Result

The repeatable first-track bridge trace shows that the Windows presentation
pipeline is not overloaded at the dip/underpass. At 300% Super FX speed with
sprite-safe edge smoothing enabled, PC processing remains around 4 ms and every
bridge phase stays well below the 16.67 ms budget. Real-time phase averages are
16.58–16.61 ms after the limiter wait, with bridge 1% lows near 59.4–59.7 FPS.

The visible hitch originates earlier: the original game/Super FX renderer
produces broad 3D scene updates irregularly at that camera angle. Around tunnel
entry and exit, a fresh broad scene can be separated by seven 60 Hz presentation
frames. Under the bridge, completed GSU jobs also become larger than on the
approach. Sprite and HUD updates continue independently; this is why output FPS
can remain 60 while the track itself appears to advance at a lower cadence.

## Changes

- Added per-frame profiling for core, Super FX, video, decode, materials,
  interpolation, sprite-safe marking, edge filtering, upload, native-state,
  draw, overlay, capture, present, audio, limiter wait, allocations, uploads,
  renderer copies, changed pixels, and GSU work.
- Added a rolling F12 frame-time graph, rolling average, 1% low, and worst frame.
- Added optional CSV logging through `SRF_PROFILE_LOG`, plus a shutdown summary
  and automatic 20/25/33.3 ms spike classification.
- Removed steady-state decode/interpolation allocations by retaining reusable
  buffers. The bridge path falls from two reported allocations per frame to zero.
- Separated lightweight GSU counters from full diagnostic tracing. The 64 KiB
  trace and watched RAM-write trace are armed only for requested capture frames.
- Replaced a drifting relative limiter with an absolute QPC deadline and a
  Windows 1 ms timer-period request. A waitable-timer variant was measured and
  rejected because it produced worse wakeup outliers on the test system.
- Logs the actual SDL backend, flags, output size, and software-renderer fallback.

## Feature isolation

Measured average PC processing time through frames 3060–3360 on the development
machine (diagnostic captures and audio disabled):

| Configuration | Average | 99th percentile | Worst |
| --- | ---: | ---: | ---: |
| Linear, no motion, 512x384 | 1.99 ms | 5.13 ms | 6.49 ms |
| Edge + motion, 2560x1440 | 4.12 ms | 6.15 ms | 6.50 ms |
| Edge + motion, 3440x1440 | 4.09 ms | 5.97 ms | 7.56 ms |
| Edge + motion, 3840x2160 | 4.14 ms | 5.99 ms | 7.34 ms |
| N64 materials, edge + motion, 3440x1440 | 4.45 ms | 6.57 ms | 6.98 ms |

Final scaling is handled by the accelerated Direct3D backend, so output
resolution has little CPU impact. The source-sized edge pass costs about 1 ms,
motion analysis/blending about 1 ms, and N64 material processing about 0.34 ms.

## Super FX clock validation

100%, 200%, 300%, 400%, and 500% were tested using the same deterministic input.
Higher clocks did not make broad scene delivery uniformly regular; they moved
the gaps to different parts of the route. At frame 3400, 400% differed from the
300% baseline in 1,980 bytes of system work RAM and 500% differed in 3,645 bytes.
The build therefore retains the established 300% setting instead of risking
physics, scripting, collision, AI, or timing changes.

## Repeatable benchmark

Run from `source/tools/perf` with a legally obtained USA Rev 1 ROM:

```powershell
.\run_first_track_benchmark.ps1 -RomPath "C:\path\Stunt Race FX (USA) (Rev 1).sfc"
py -3 .\analyze_profile.py "$env:TEMP\StuntRaceFXWE-benchmark-YYYYMMDD-HHMMSS\first-track.csv"
```

The runner copies the build to a disposable temporary runtime so save RAM never
enters the repository. Profile results are also outside the repository by
default. Do not add captures, saves, states, replays, ROMs, or ROM-derived data.

## Remaining limitation

This pass makes the 60 Hz presentation stable and exposes the real bottleneck,
but it cannot manufacture authoritative future camera geometry that the original
renderer has not produced. Eliminating every seven-frame track hold requires a
scene-aware 3D interpolation/reprojection path or the persistent native renderer;
blindly raising GSU speed is neither reliable nor gameplay-safe.
