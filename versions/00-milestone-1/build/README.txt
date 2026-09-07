STUNT RACE FX — ENHANCED PORT, MILESTONE 1

This package does not contain Nintendo game data.

SETUP
1. Copy your verified US Rev 1 ROM into this folder.
2. Name it exactly: Stunt Race FX (USA) (Rev 1).sfc
3. Double-click "Play Enhanced.cmd".

CONTROLS
F11 or Alt+Enter  Toggle borderless fullscreen
1                 Corrected 4:3 aspect (default)
2                 Raw framebuffer aspect
3                 Stretch to fill the window
Escape            Quit
F5 / F8            Save / load state

ULTRAWIDE
"Play Ultrawide 3440x1440.cmd" opens an ultrawide window while preserving the
correct game aspect. This milestone does not yet render additional world at the
sides. That requires the planned native Super FX geometry renderer.

STATUS
- Verified ROM MD5: 128b316a74caf17fdc216b3ab46d4a9a
- 300-frame reference checksum: EED02566
- 300-frame enhanced/interception checksum: EED02566
- Scaling smoothing: implemented
- True polygon MSAA: planned with native renderer
- True 60 FPS polygon motion: planned with transform interpolation/native render
- True ultrawide world view: planned with wider native camera and culling

See PORTING.md for the technical audit and implementation roadmap.
