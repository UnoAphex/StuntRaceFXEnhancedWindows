STUNT RACE FX - ENHANCED PLAYABLE BUILD
========================================

SETUP
-----
1. Extract the complete folder.
2. Put your legally obtained USA Rev 1 ROM beside StuntRaceFX.exe.
3. Name it exactly: Stunt Race FX (USA) (Rev 1).sfc
4. Run StuntRaceFX.exe or one of the Play shortcuts.

No ROM or ROM-derived data is included.

NEW GRAPHICS AND PERFORMANCE CONTROLS
-------------------------------------
F12         Show or hide the FPS/status counter.
G           Cycle Edge Smooth, Linear, and Nearest filtering.
M           Toggle experimental motion smoothing.
Page Up     Raise cap: 60.1 -> 90.1 -> 120.2 -> Unlimited.
Page Down   Lower the cap.
Home        Return to the correct 60.1 Hz cap.
F10         Toggle correct 4:3 aspect and stretch-to-fill.
Alt+Enter   Toggle fullscreen and windowed mode.
F11         Alternate fullscreen toggle.

Edge Smooth is the default. It performs a custom edge-directed 2x pass before
GPU scaling, reducing staircase edges without making the whole image as blurry
as ordinary linear scaling.

Motion smoothing blends transitions between coarse polygon updates. It stays at
the correct game speed and may look smoother in motion, but it can introduce
slight ghosting. Press M to compare it live.

The default cap is the correct SNES NTSC rate (about 60.0988 Hz). This build was
measured holding 60.1 FPS with Edge Smooth enabled. The 90/120/Unlimited modes
run the entire emulation faster, including game logic and audio; they are turbo
modes, not generated animation frames. Press Home to restore normal speed.

DISPLAY PRESETS
---------------
- Play Enhanced 60 FPS.cmd: windowed, Edge Smooth, counter initially visible.
- Play Fullscreen 4x3.cmd: correct aspect in borderless fullscreen.
- Play Ultrawide 3440x1440.cmd: fullscreen 21:9 stretch-to-fill.
- Play Experimental Motion Smooth.cmd: correct-speed frame blending enabled.
- Play 120 FPS Turbo.cmd: double-speed experiment; press Home for normal speed.

CONTROLS
--------
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

Other hotkeys:
  F5 = reset game
  F1-F9 = load state (F5 remains reset)
  Shift+F1-F9 = save state
  Esc = quit

Save RAM is written to Stunt Race FX.srm. Save states are state-1.bin through
state-9.bin.

TECHNICAL / LEGAL
-----------------
This remains a compatibility build while the native static-recompile backend is
being repaired. It uses Snes9x for substantially lower CPU cost than the earlier
accuracy-focused bsnes package. Snes9x is freeware for personal/non-commercial
use; see SNES9X-LICENSE.txt. The frontend is MIT licensed and SDL2 uses zlib.
Complete corresponding source is included as source-code.zip.

