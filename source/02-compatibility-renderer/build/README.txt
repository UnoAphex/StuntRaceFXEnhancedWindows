STUNT RACE FX - PLAYABLE COMPATIBILITY BUILD
============================================

This is the first fully playable Windows build from the porting project.
It uses the accurate bsnes compatibility core while the native static-recompile
backend is still being repaired.

SETUP
-----
1. Extract the entire folder.
2. Put your legally obtained USA Rev 1 ROM in this folder.
3. Name it exactly: Stunt Race FX (USA) (Rev 1).sfc
4. Double-click StuntRaceFX.exe, or use one of the Play shortcuts.

The ROM is not included.

DISPLAY PRESETS
---------------
- StuntRaceFX.exe: 960x720 window, smoothed scaling, correct 4:3 display.
- Play Fullscreen 4x3.cmd: borderless fullscreen with correct aspect ratio.
- Play Ultrawide 3440x1440.cmd: borderless fullscreen stretched to fill 21:9.
- F10 switches between correct 4:3 and stretch-to-fill while playing.
- F11 or Alt+Enter switches fullscreen on or off.

The core outputs at the SNES NTSC rate of about 60.0988 Hz. The original game's
low internal polygon-update cadence is unchanged; this compatibility build does
not invent 60 unique game-animation frames. Smoothed scaling softens the jagged
low-resolution framebuffer, but it is not native polygon anti-aliasing.

CONTROLS
--------
Keyboard:
  Arrow keys = D-pad
  Z = SNES B
  X = SNES A
  A = SNES Y
  S = SNES X
  C / V = SNES L / R
  Enter = Start
  Right Shift = Select

Xbox-style controller:
  D-pad or left stick = D-pad
  A / B / X / Y = SNES B / A / Y / X
  LB / RB = SNES L / R
  Menu / View = Start / Select

Hotkeys:
  F5 = reset game
  F1-F9 = load state
  Shift+F1-F9 = save state
  F10 = toggle aspect mode
  F11 or Alt+Enter = toggle fullscreen
  Esc = quit

Save RAM is written to Stunt Race FX.srm in this folder. Save states are named
state-1.bin through state-9.bin.

VERIFIED
--------
The release executable was tested through title, game selection, car/name
selection, track/class selection, and into an active Speed Trax race. Keyboard
input, Xbox controller discovery, video, audio initialization, and sustained
runtime past the title attract sequence were exercised.

LEGAL / SOURCE
--------------
No Nintendo ROM or ROM-derived data is included. bsnes and this frontend are
distributed under GPLv3; SDL2 uses the zlib license; libretro.h uses MIT.
See COPYING-GPLv3.txt, SDL2-LICENSE.txt, and SOURCE.txt. Complete corresponding
source is included as source-code.zip.

