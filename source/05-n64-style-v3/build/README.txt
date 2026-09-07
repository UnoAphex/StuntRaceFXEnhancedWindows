STUNT RACE FX - N64 STYLE PLAYABLE BUILD v3
============================================

QUICK START
-----------
1. Extract the complete folder.
2. Put your legally obtained USA Rev 1 ROM beside StuntRaceFX.exe.
3. Name it exactly: Stunt Race FX (USA) (Rev 1).sfc
4. Double-click "Play N64 Style Ultrawide 3440x1440.cmd".

StuntRaceFX.exe also launches directly when the correctly named ROM is beside it.
No ROM, save file, or ROM-derived game data is included in this package.

WHAT v3 ADDS
------------
- A 300% Super FX clock preset chosen from measured 100%, 300%, and 500% tests.
  This raises real polygon-scene updates; the game/output clock stays at 60.1 Hz.
- Frame blending is on by default to smooth the scene frames that still repeat.
- Nine N64-style, palette-limited material tiles: grass, asphalt, gravel, dirt,
  sand, stone/concrete, snow/ice, water, and mud.
- Automatic surface classification that keeps the original game palette and
  shading while protecting the vehicle, HUD, and white track markings.
- Original/Extended/Far surface-detail range. Far carries texture detail closest
  to the horizon; it does not alter physics or collision geometry.
- All v2 display, fullscreen, FPS, latency, save-state, and replay features.

The internal course list is shared by rendering and collision. A prototype that
forced extra course objects visible changed physics and race timing, so it is not
included. True extra world geometry remains future native-renderer work. The v3
range setting safely increases distant MATERIAL detail without risking gameplay.

DISPLAY AND ENHANCEMENT CONTROLS
--------------------------------
P           Cycle five presets: Original, Crisp, Ultrawide, Smooth, N64 Style.
T           Toggle the N64 material textures on/off for instant comparison.
D           Cycle Original, Extended, and Far surface-detail range.
G           Cycle Edge Smooth, Linear, and Nearest filtering.
M           Toggle motion/frame blending.
F10         Cycle correct 4:3, ambient ultrawide, and stretch-to-fill.
F12         Show/hide FPS, scene updates, cap, Super FX clock, and status.
Alt+Enter   Toggle borderless fullscreen and windowed mode.
F11         Alternate fullscreen toggle.

Ambient ultrawide keeps the actual game at correct 4:3 geometry and fills the
side area with a dark, softly scaled extension. Stretch is available, but it
makes cars and circles wider than intended.

FPS AND PERFORMANCE
-------------------
The F12 display separates OUTPUT FPS from SCENE FPS:
  OUTPUT FPS = frontend/emulator refresh rate (target: about 60.1).
  SCENE FPS  = estimate of how often the central 3D picture genuinely changes.

The default 300% Super FX setting substantially raises the actual 3D update rate,
but it does not rewrite the original game engine into mathematically perfect
60-FPS simulation. Motion blending presents the remaining repeated frames more
smoothly at 60.1 Hz. Use Play Original Look.cmd for the stock 100% chip budget.

Page Up     Raise cap: 60.1 -> 90.1 -> 120.2 -> Unlimited (turbo speed).
Page Down   Lower the cap.
Home        Return to the correct 60.1 Hz cap.
L           Toggle one-frame runahead for lower controller latency.

Higher display caps speed up game logic and audio; they are turbo modes, not
extra animation frames. Runahead roughly doubles emulation work.

QUALITY OF LIFE
---------------
Space       Pause/resume.
Period      Advance one frame while paused.
Ctrl+R      Start/stop controller-input recording to replay-last.srf.
Ctrl+P      Start/stop playback of replay-last.srf.
F5          Reset game.
F1-F9       Load state slot 1-9 (F5 remains reset).
Shift+F1-F9 Save state slot 1-9.
Esc         Quit.

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

FILES CREATED WHILE PLAYING
---------------------------
Save RAM: Stunt Race FX.srm
Save states: state-1.bin through state-9.bin
Replay: replay-last.srf

TECHNICAL / LEGAL
-----------------
The package uses a source-built Snes9x compatibility core while the static
recompile/native renderer is developed. Snes9x is freeware for personal,
non-commercial use; see SNES9X-LICENSE.txt. The frontend is MIT licensed and
SDL2 uses the zlib license. Corresponding source is in source-code.zip.
