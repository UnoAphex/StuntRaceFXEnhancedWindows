# Unified compatibility timing — v4.17

All compatibility-renderer presets use the same emulation tick, input polling,
high-resolution deadline accumulator, frame limiter, interpolation state, and
diagnostic profiler. Presets only select presentation features.

## Duplicate-presentation correction

The preset matrix found that the core supplies two video callbacks during
startup frame 2. The old frontend presented both callbacks immediately. v4.17
now consumes every callback so the newest source image remains authoritative,
but defers compatibility presentation until the emulation tick completes. It
then presents exactly once. The native and hybrid paths retain their existing
callback/presentation behavior.

This changes neither the number of emulation ticks nor their contents. Input is
still sampled before the tick, and WRAM, Super FX RAM, and register hashes match
v4.16 exactly at deterministic first-race frame 2600.

## Preset policy

Every compatibility launcher explicitly selects:

- the compatibility renderer;
- the native ~60.0988 Hz game cadence (`SRF_CAP=60`);
- 300% Super FX clock using the core's canonical `300%` value;
- no runahead; and
- no SDL VSync double limiter.

Fullscreen/windowed changes, output resolution, aspect treatment, expanded
sides, HD center, materials, edge filtering, and smoothing cannot select a
different timing loop. Manual speed controls remain available for deliberate
testing, but are not preset defaults.

There is only one active pacing authority. With the normal 60 Hz cap, the
high-resolution deadline accumulator owns pacing and SDL VSync is not enabled,
even if inherited environment settings request it. SDL VSync can own pacing
only when the user deliberately selects the unlimited cap. This prevents the
16/33 ms oscillation measured when both mechanisms were active together.

## Instrumentation

Performance CSV files now distinguish core `video_callbacks` from actual
`presentation_callbacks`. Summaries report the active compatibility preset,
the shared timing architecture, target FPS, and counts of missing or multiple
presentations.

`tools/perf/run_compatibility_preset_matrix.ps1` tests the five interactive
visual presets, every player-facing compatibility launcher configuration, an
explicit VSync case, and a fullscreen ultrawide case. It checks frame pacing,
one presentation per tick, and deterministic WRAM, Super FX RAM, and register
state across presets. Test output is written outside the repository by default.

## v4.17 validation

- 15/15 compatibility configurations passed a real-time 180-frame test.
- Post-warmup average was 16.60 ms for every configuration.
- Post-warmup p99 was 16.60–16.62 ms in the final build.
- Missing presentations: zero.
- Multiple presentations: zero.
- 15/15 configurations passed through first-race frame 2600.
- The slowest uncapped first-race p99 was 14.33 ms at 3440x1440 HD wide.
- All preset state hashes matched one another.
- v4.17 state matched v4.16 at frame 2600.
- Wide background, sprite, and capture unit tests passed.

The polygon-stream inspector and reconstruction recorder inherit the same
timing policy, but are developer tools rather than visual presets; their own
debug capture/I/O overhead is intentionally not judged as gameplay pacing.
