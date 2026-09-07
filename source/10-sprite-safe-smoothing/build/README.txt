STUNT RACE FX - SPRITE-SAFE SMOOTHING v4.4
=========================================

QUICK START
-----------
1. Extract the complete folder.
2. Put your legally obtained USA Rev 1 ROM beside StuntRaceFX.exe.
3. Name it exactly: Stunt Race FX (USA) (Rev 1).sfc
4. Double-click "Play Sprite-Safe Smooth 60 FPS.cmd".

Use "Play Compatibility Renderer.cmd" if you want the known-good original
rendering path. No ROM, save, state, replay, capture, or ROM-derived track data
is included in this package.

v4.4 SPRITE-SAFE SMOOTHING
--------------------------
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

v4.2 ROAD / VEHICLE ALIGNMENT
-----------------------------
- Applies the missing visible-road expansion to the decoded collision widths.
  The default native road is now 2.0x the raw course-table half-width.
- Press semicolon/apostrophe to narrow or widen the road in 0.1x steps while
  using Hybrid Comparison. The supported calibration range is 1.0x to 2.5x.
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
F12         Show/hide FPS, scene updates, renderer, cache, and culling status.
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
