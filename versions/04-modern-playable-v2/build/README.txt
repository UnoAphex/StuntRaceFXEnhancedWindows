STUNT RACE FX - MODERN PLAYABLE BUILD v2
========================================

SETUP
-----
1. Extract the complete folder.
2. Put your legally obtained USA Rev 1 ROM beside StuntRaceFX.exe.
3. Name it exactly: Stunt Race FX (USA) (Rev 1).sfc
4. Run StuntRaceFX.exe or one of the Play shortcuts.

No ROM or ROM-derived data is included.

MODERN DISPLAY CONTROLS
-----------------------
P           Cycle four presets: Original, Crisp, Ultrawide, Smooth Motion.
G           Cycle Edge Smooth, Linear, and Nearest filtering.
M           Toggle experimental polygon-motion smoothing.
F10         Cycle correct 4:3, ambient ultrawide, and stretch-to-fill.
F12         Show/hide output FPS, estimated scene FPS, cap, and status.
Alt+Enter   Toggle borderless fullscreen and windowed mode.
F11         Alternate fullscreen toggle.

Ambient ultrawide keeps the actual game at its correct 4:3 geometry, replacing
black pillars with a dark, softly scaled extension. It avoids making cars and
circles unnaturally wide. Stretch remains available for anyone who prefers the
screen completely filled.

The FPS display separates OUTPUT FPS from SCENE FPS. Output is the emulator's
refresh rate. Scene is an estimate of how often the central 3D picture changes,
which makes the original game's low polygon update rate visible.

PERFORMANCE AND LATENCY
-----------------------
Page Up     Raise cap: 60.1 -> 90.1 -> 120.2 -> Unlimited.
Page Down   Lower the cap.
Home        Return to the correct 60.1 Hz cap.
L           Toggle one-frame runahead for lower controller latency.

The normal cap is the correct SNES NTSC rate (about 60.0988 Hz). Higher caps
are turbo modes: they speed up game logic and audio, not just animation.
Runahead is optional because it roughly doubles emulation work. Leave it off
if a computer cannot hold 60 FPS consistently.

QUALITY-OF-LIFE AND REPLAYS
---------------------------
Space       Pause/resume.
Period      Advance one frame while paused.
Ctrl+R      Start/stop recording controller input to replay-last.srf.
Ctrl+P      Start/stop playback of replay-last.srf.
F5          Reset game.
F1-F9       Load state slot 1-9 (F5 remains reset).
Shift+F1-F9 Save state slot 1-9.
Esc         Quit.

Replays record input rather than video, so files are tiny. Start from the same
game state when playing one back for deterministic results. Save RAM is written
to Stunt Race FX.srm; save states are state-1.bin through state-9.bin.

GAME CONTROLS
-------------
Keyboard:
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

WHAT IS NATIVE VS EXPERIMENTAL
------------------------------
Edge Smooth and ambient ultrawide are presentation enhancements applied to the
finished SNES picture. Motion smoothing estimates transitions and can ghost.
Runahead reduces latency but costs CPU time. A true high-resolution 3D renderer,
real horizontal FOV expansion, extended draw distance, and interpolated object
transforms are under active development using the live Super FX scene bridge;
they are not claimed as complete in this v2 package.

TECHNICAL / LEGAL
-----------------
This playable build uses Snes9x as its compatibility core while the static
recompile/native renderer is developed. Snes9x is freeware for personal,
non-commercial use; see SNES9X-LICENSE.txt. The frontend is MIT licensed and
SDL2 uses zlib. Corresponding source is included as source-code.zip.
