# Treaded Tire Presentation Test

Stunt Race FX draws the player's large rear tires through the Super FX vehicle
geometry path; they are not ordinary SNES OBJ sprites. Version 4.18 therefore
implements an optional, renderer-only replacement pass for the HD compatibility
preview instead of modifying gameplay data or the emulated renderer.

The generated tire is alpha-composited over two measured source-space regions:

- left: x 87, y 102, width 28, height 41
- right: x 138, y 102, width 28, height 41

At the captured 1280x720 presentation this maps to approximately 105x132 output
pixels per tire. The same source-space placement scales proportionally with the
window and keeps the original 4:3 center framing.

Use `Play Treaded Tire Test 1920x1080.cmd` to enable the experiment. Press
Ctrl+Y while a race is active to compare the treaded replacements with the
original wheels. This pass does not change vehicle transforms, handling,
collision, timing, animation, or the native renderer.

The runtime texture is a 420x528 RGBA image stored as a raw companion file so
SDL can retain the generated alpha channel without adding a new image-decoding
dependency. The PNG is included as the editable visual source.
