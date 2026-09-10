# v4.10 — widescreen cleanup

## Implemented

- Side color conversion now reads the core's brightness-adjusted RGB565 palette,
  matching the center's channel expansion. Geometry uses the scanline-96 palette
  snapshot; BG2 uses each line's palette. Arbitrary per-line polygon palette
  effects/color math are not claimed to be fully reproduced.
- Forced blanking blanks the side background and polygon palette.
- Background sampler mode explicitly follows nearest vs filtered mode, including
  filter changes after texture creation. The original center filter is untouched.
- The side pass replaces the one-original-texel vertical bezel columns at the
  original viewport edges. This removes the thin black dividers visible in the
  user's screenshot without stretching the view or painting over central HUD.
- Clipping overflow discards the affected face, rather than silently truncating
  its vertex list and drawing a malformed polygon.
- Compatibility material classification samples the unmodified current frame.
  Previously its left support pixel could already contain the material applied
  earlier in the same loop, making classification depend on traversal order.
  Native/hybrid retain their previous material-support behavior.
- Compatibility material application is restricted to the original race image,
  protecting the surrounding frame/bezel from asphalt/stone classification.
- Wide cache reset clears both accumulating and display-ready pictures after
  toggles, render-path transitions, and successful compatibility save-state loads.
  Unserialize behavior itself, input, simulation and interpolation are unchanged.

No private original source or derived assets were added. Earlier snapshots are
unchanged. New artwork, complete textured polygon extraction, and material
replacement on the side geometry are not part of this cleanup.

## Why the center is still softer

The side pass submits original camera-space polygons to the GPU at output
resolution. The center still presents the complete original SNES image, after
the existing sprite-safe interpolation/filter. Sharpening that finished image
cannot reproduce the side polygon precision.

Inspection confirms the GPU capture does not cover all original draw paths;
the original implementation has separate texture-mapped polygon routines.
The SNES compositor also draws OBJ and BG layers with priority/color-math rules.
Not every apparent 2D feature can safely be treated as a separate SNES sprite.
Therefore drawing the current polygon list over the center is not a safe
quality upgrade: original vehicle details, scenery and messages could vanish.

The appropriate next implementation is to complete original textured/special
draw coverage, expose authoritative overlay coverage/priority, then rasterize
the complete 3D viewport in one GPU pass and composite the original overlays.
This stays in the execution-driven compatibility enhancement path, not the
experimental native track reconstruction. Keep the existing center as fallback
until vehicle details, pause/rank messages and HUD are verified.

**Not implemented here:** crisp high-resolution center geometry. Resolution and
motion differences can still make a join visible even though the black divider
columns are gone. Do not advertise this release as seamless HD throughout.

## Validation

- Built the Windows frontend and core locally from this expanded source.
- ROM-free tests include the actual production headers, checking RGB565 primary
  colors, palette refresh, forced blanking, displayed-bitmap selection, reset,
  multi-SNES-frame geometry assembly, DMA freezing and restart after reset.
  Run `tools/perf/run-wide-unit-tests.cmd`; output binaries go to TEMP.
- Final 4:3 frame 3300 matches v4.9 byte-for-byte for the screenshot, WRAM, GSU
  RAM and GSU registers. Wide enabled at 4:3 still bypasses side drawing.
- Inspected the 1920x1080 frame-2600 wide capture: the vertical black dividers
  are removed, the original car/HUD remain, and the center remains lower-res.
- Ran the material-enabled first-track route and inspected frame 2600.
- Save/load and toggle cache logic has header-level lifecycle coverage, not an
  exhaustive interactive save-state/renderer-switch test.
- No all-course, audio, fullscreen, or complete sprite-animation QA claim.

Test artifacts are external to the repo:

- TEMP/StuntRaceFXWE-benchmark-20260908-184153: wide 1080p frame 2600.
- TEMP/StuntRaceFXWE-benchmark-20260908-184209: 4:3 state/pixel regression.
- TEMP/StuntRaceFXWE-benchmark-20260908-184224: materials enabled.
- TEMP/StuntRaceFXWE-benchmark-20260908-184626: final material/bezel capture.
- TEMP/StuntRaceFXWE-benchmark-20260908-184638: final executable 4:3 regression.

A 3,400-frame timed 3440x1440 run before the last material-only bezel guard
(TEMP/StuntRaceFXWE-benchmark-20260908-184410) averaged 16.5927 ms per frame,
4.3812 ms processing, and 59.62 FPS rolling 1% low. Worst frame was 47.9175 ms;
this is not a locked-60/no-hitch claim. It used a hidden window with audio off
and materials off. Visible fullscreen, audio latency and all tracks need further
testing. The final change only adds the compatibility material bezel guard.

Final binary SHA-256:

- StuntRaceFX.exe: `3CA2213B5C46EA5C7382003A5991AED207B77D07A9E4C9877005808CC2D1487F`
- snes9x_libretro.dll: `DCD3A63B71489AE2839F77EE696CA2193D5604F28D1726BF1421340AE7E42D56`

WIDE_VALIDATION.md is retained as **historical v4.9 evidence**, not the hashes
or final timings of this version.
