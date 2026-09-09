STUNT RACE FX - ORIGINAL POLYGON DRAW STREAM v4.8
====================================================

v4.8 STATUS
-----------
Adds optional cyan outlines of captured original polygons: press F8 in
compatibility mode to cycle latest submission / previous submission / off.
Or use "Inspect Original Polygon Stream.cmd". This is an alignment diagnostic;
timing and screen placement are still experimental, and lines may cross HUD.
Normal play defaults to OFF. No true ultrawide or sharper normal rasterization
is included yet. Native renderer implementation is unchanged.
See ../source/COMPATIBILITY_DRAW_STREAM.md for findings and limitations.

INHERITED v4.7 STATUS
-----------
Playable compatibility baseline with optional read-only developer diagnostics.
This build does NOT yet add true ultrawide or higher-resolution polygons.
Normal play uses the existing graphics, motion and sprite timing. The geometry
probe is disabled unless explicitly enabled by the developer benchmark tool.
The experimental native renderer is inherited unchanged from v4.6.
See ../source/COMPATIBILITY_GEOMETRY_PROBE.md in the repository for findings.

QUICK START
-----------
1. Extract the complete folder.
2. Put your legally obtained USA Rev 1 ROM beside StuntRaceFX.exe.
3. Name it exactly: Stunt Race FX (USA) (Rev 1).sfc
4. Double-click "Play Compatibility Performance 60 FPS.cmd".
5. To test the v4.6 renderer, use "Play Native Track Prototype Windowed.cmd"
   or the 3440x1440 ultrawide launcher.

Use "Play Compatibility Renderer.cmd" if you want the known-good original
rendering path. No ROM, save, state, replay, capture, or ROM-derived track data
is included in this package.

v4.6 SOURCE-INFORMED NATIVE RENDERER
------------------------------------
- Replaces the old 2.0x guessed road width with a 4.0x default calibrated from
  XLR8's track_scale=5 authored road-piece dimensions. Hybrid mode can still
  adjust the conversion from 1.0x through 6.0x.
- Derives vehicle heading from ordered front/rear wheel pairs instead of an
  undirected principal-axis estimate, eliminating the major 180-degree pose
  ambiguity and making steering follow the live car state.
- Derives pitch, roll, wheelbase, and track width from the four live wheel
  points. The native player body is now world-space geometry rather than a
  screen-pinned overlay.
- The chase camera follows the authoritative player chassis heading when a
  verified four-wheel pose is available, with the course tangent retained as
  a safe fallback.
- Sorts individual native triangles by camera depth instead of swapping entire
  long course segments at their midpoint, reducing painter-order flicker.
- Keeps Compatibility as the default and preserves all v4.5 performance and
  sprite-safe smoothing behavior.

v4.5 PERFORMANCE / FRAME PACING
-------------------------------
- F12 now opens a detailed performance overlay with frame time, rolling
  average, 1% low, worst recent frame, core/Super FX/video/present timings,
  workload counters, and a lightweight frame-time graph.
- Uses an absolute 60.0988 Hz presentation schedule with 1 ms Windows timer
  precision to avoid timing drift and reduce uneven limiter wakeups.
- Reuses compatibility decode and interpolation work buffers. Steady-state
  bridge testing performs no per-frame allocations in these paths.
- Full GSU diagnostic traces are recorded only on explicitly requested capture
  frames, so developer instrumentation does not burden ordinary gameplay.
- High output resolutions continue to use the Direct3D GPU for final scaling.
  Tested 1080p, 1440p, 3440x1440, and 4K modes remain far below the 16.67 ms
  CPU budget on the development machine.
- Keeps the existing 300% Super FX setting. Higher values changed authoritative
  work RAM and merely moved the original renderer's irregular scene-update
  gaps, so they are not used as a gameplay-unsafe shortcut to 60 FPS.

v4.4 SPRITE-SAFE SMOOTHING (PRESERVED)
--------------------------------------
- Non-race menus and presentation screens now bypass temporal smoothing so every
  original 2D animation frame is shown.
- Race frames are compared at every source pixel instead of sparse sampling.
- Small local changes are treated as SNES tile/sprite animation. Their affected
  8x8 tiles update immediately while the track/background finishes smoothing.
- Broad 3D scene changes retain the responsive 75% blend, or 87.5% while
  steering.
- Restores the intended pause-menu drawing sequence and vehicle bob/engine
  animation without changing game logic or animation timing.

WHAT THIS PROTOTYPE DOES
------------------------
- Reads the active course pointer from the running original game.
- Decodes the first race's authoritative 64-node course/collision centerline
  and road widths directly from your ROM.
- Builds a persistent PC-side world-space mesh for road, gravel shoulders, and
  a grass terrain corridor. Camera projection is not baked into the cache.
- Rasterizes that mesh at the actual selected output size, including 3440x1440.
- Preserves vertical framing and reveals genuine additional world geometry on
  the sides at widescreen and ultrawide aspect ratios.
- Uses PC-side near clipping, frustum/far culling, adjustable FOV, draw distance,
  material textures, and distance fog.
- Reads live camera and vehicle state from the original game and interpolates
  presentation while original gameplay timing remains authoritative.
- Keeps the original HUD centered in a 4:3 safe area.
- Keeps the compatibility renderer intact and available instantly.

v4.1 STABILITY FIXES
--------------------
- Menus, loading screens, grid views, and the countdown now remain on the full
  compatibility renderer. A cached course is presented only after the verified
  race display cycle has remained active continuously.
- Replaced the unsafe shared Super FX matrix sample that caused whole-screen
  track flips with a stable course-tangent chase camera.
- Restricts submission to a continuous section of track ahead of the player and
  sorts complete track segments, reducing painter-order flicker on closed loops.
- Adds a visible low-poly player car presentation. Opponents still use live
  diagnostic proxies until their original model format is decoded.
- Reduced the recommended native far distance to a more stable 30000 units.

v4.2 ROAD / VEHICLE ALIGNMENT (SUPERSEDED BY v4.6)
--------------------------------------------------
- Applies the missing visible-road expansion to the decoded collision widths.
  The v4.2 default was 2.0x the raw course-table half-width.
- Press semicolon/apostrophe to narrow or widen the road in 0.1x steps while
  using Hybrid Comparison. v4.6 expands the supported range to 1.0x through
  6.0x and defaults to 4.0x.
- The player proxy now reads live vehicle heading plus the pitch and roll plane
  formed by its four authoritative child points. It visibly steers, banks, and
  pitches instead of remaining rigidly pinned to the screen.
- Stabilizes the direction sign of the extracted vehicle axis, preventing
  occasional 180-degree car flips caused by principal-axis ambiguity.

This is a first-course research milestone, not full scenery parity. The native
path does not yet decode original buildings, signs, barriers, trees, or exact
vehicle models. Vehicles in native mode are temporary low-poly diagnostic
proxies. Use compatibility mode to play with all original scenery, or hybrid
mode to compare the reconstructed road against the original view.

NATIVE RENDERER CONTROLS
------------------------
R           Cycle Compatibility, Native Track, and Hybrid Comparison.
B           Toggle native polygon wireframe.
I           Toggle track segment IDs.
F           Toggle native distance fog.
[ and ]     Decrease/increase vertical FOV.
; and '     Narrow/widen the native road calibration by 0.1x.
- and =     Decrease/increase native draw distance.
D           Cycle Original, Extended, and Far distance presets.
F12         Show/hide detailed FPS, frame-time graph, and subsystem timings.
Alt+Enter   Toggle borderless fullscreen and windowed mode.
F11         Alternate fullscreen toggle.

DISPLAY / PERFORMANCE
---------------------
P           Cycle Original, Crisp, Ultrawide, Smooth, and N64 Style presets.
T           Toggle material textures.
G           Cycle Edge Smooth, Linear, and Nearest filtering.
M           Toggle sprite-safe motion/frame blending.
F10         Cycle correct 4:3, ambient ultrawide, and stretch presentation.
Page Up     Raise the emulation cap (turbo modes).
Page Down   Lower the cap.
Home        Return to the correct 60.1 Hz cap.
L           Toggle one-frame runahead.

The native camera and dynamic proxies use the existing 60 Hz visual
interpolation. Original physics, collision, race logic, timers, and gameplay
speed are not changed to create extra frames.

The current SDL geometry prototype has no depth buffer or explicit MSAA.
Material layering and painter sorting are used for this milestone. A future
depth-buffered GPU backend can consume the same cache to add MSAA, improved
depth precision, lighting, and shadows without changing course extraction.

GENERAL CONTROLS
----------------
Space       Pause/resume.
Period      Advance one frame while paused.
Ctrl+R      Start/stop input recording to replay-last.srf.
Ctrl+P      Start/stop replay playback.
F5          Reset game.
F1-F9       Load state slot 1-9 (F5 remains reset).
Shift+F1-F9 Save state slot 1-9.
Esc         Quit.

Keyboard game controls:
  Arrow keys = D-pad
  Z / X / A / S = SNES B / A / Y / X
  C / V = SNES L / R
  Enter = Start
  Right Shift = Select

Xbox-style controller:
  D-pad or left stick = D-pad
  A / B / X / Y = SNES B / A / Y / X
  LB / RB = SNES L / R
  Menu / View = Start / Select

FILES CREATED WHILE PLAYING
---------------------------
Save RAM: Stunt Race FX.srm
Save states: state-1.bin through state-9.bin
Replay: replay-last.srf

SOURCE / LEGAL
--------------
The research report, extraction utility, frontend source, core source snapshot,
material sources, and build notes are in the adjacent expanded source directory
in the repository. The package uses a source-built Snes9x compatibility core
while the static recompilation/native renderer is developed. See the included
license files and SOURCE.txt.
