# Compatibility geometry probe v4.7

This is a playable diagnostic milestone, not the completed ultrawide/geometry
quality update. No projection, culling, rasterization, simulation, input, sprite
or interpolation behavior is intentionally changed. Native renderer functions
and launchers are unchanged from v4.6.

## Findings

Compatibility receives a composed SNES image. STRETCH scales that image;
AMBIENT stretches a dark backdrop behind a centered 4:3 image. edge_upscale
filters neighbors to produce a 2x image; it does not rasterize polygons.
The sprite timing fix uses sparse changed tiles rather than separate layers.

Super FX PLOT writes integer, eight-bit pixel positions into tiled bitplanes.
Enlarging the SDL texture cannot recover rejected geometry or fractional
vertices. Development source MOBJ.MC, MCLIP.MC, MDRAWC.MC and MDRAWP.MC identify
candidate projection, outcode, clipping and drawing stages. Their retail
addresses and conditional source variants still require execution validation.

## New observer

The core has an opt-in bounded histogram of bank/R15 visits and PLOT/RPIX counts.
An optional exact bank/R15 watch records up to 256 sets of sixteen registers,
opcode, status and cache base. These are reads only, before instruction fetch.
No game instructions are skipped, timing costs changed or game RAM written.
Normal gameplay leaves the observer disabled. Capture incurs diagnostic CPU
and disk overhead and is unsuitable for frame-pacing measurements.

R15 is the pre-fetch pipe register, NOT a verified instruction address. Branch
delay slots/cache execution require careful interpretation. Pixel visits are
not polygon counts. Hash collisions are resolved with at most 32 probes;
discarded samples and truncated snapshots are reported explicitly.

Run tools/perf/run_first_track_benchmark.ps1 with a ROM path, -CaptureFrame 3299
and -GeometryProbe. Add -GeometryWatch 07F716 to record a sampled drawing site.
Generated data belongs in the temporary output directory, never the repo.
The runner clears inherited SRF settings and copies only runtime file types,
excluding user saves and ROM copies, for repeatable clean-start tests.

Analyze snes9x_f003299_geometry_histogram.bin with
tools/perf/analyze_geometry_probe.py (Python 3.8+).

Binary formats (Windows little-endian, schema version 1):

- histogram: 16384 records of four u32 values: bank/R15, visits, PLOT, RPIX.
  Zero visits marks an empty slot.
- registers: up to 256 records of twenty u16 values: bank, pipe opcode,
  status, cache base, R0 through R15. An empty capture creates no file.
- status: four u32 values: version, missed histogram samples, missed watched
  snapshots, observed instructions. No ROM or code bytes are in these formats.

## Validation

The 3400-frame scripted compatibility run completes with the rebuilt core.
At frame 3300, observer-on and observer-off runs have identical SHA-256 hashes
for the presented image, GSU RAM, WRAM and exposed GSU registers. That sample
contains 1503 instructions and no PLOT visits. Frame 3299 contains 181120
instructions and sixteen PLOT sites in bank 07, with 64 visits each; no
histogram samples were dropped. These sites are observations, not yet
identified polygon entry points.

The watch at 07:F716 also captured 64 register snapshots without truncation.
A clean-start comparison against the shipped v4.6 executable/core matched all
four frame-3300 hashes as well. The first cross-version comparison was invalid
because the old folder's save files were inherited; the runner now excludes
them. This correction does not modify any user saves.

The uncapped 512x384 diagnostic-off capture run averaged 3.2468 ms and peaked
at 30.7990 ms. Capture/readback/disk activity is included, so this is not proof
of locked 60 FPS. Real-time pacing, audio and the full visual regression matrix
are not certified by this milestone.

## Remaining work

Map pre-clipping polygon submissions and object rejection, validate fractional
projection inputs and frame ownership, then reconstruct the original draw
stream at 4:3 before expanding horizontal visibility. Resolve SNES layer/window
composition for menus/HUD. Only expose geometry multipliers after real polygon
rasterization exists. No new visual quality claims are made for this build.
