# First-track recording-driven materials v4.14

## Evidence used

The complete local session `session-20260909-191002-p22012` was inspected in
place and was not copied into this source snapshot. It contains 7,366 frames,
including 5,991 race frames, 2,464 polygon updates, 1,595 display-camera
updates, and 25 complete checkpoints. Representative states were reconstructed
at frames 1,500, 2,100, 2,500, 4,000, 5,500, and 7,000 to cover the grid,
open pavement, buildings, wooded sections, and the finish.

The display-camera samples show that the recurring first-course paved faces use
palette slots 60 and 61 (`526152` and `4A594A`). The green 212–215 family is
also consistently terrain-colored, but the broad near-field turf is primarily
carried by the recorded `BKGD` channel rather than the polygon stream. A broad
color classifier was rejected: it falsely classifies vehicle reds as mud,
vehicle blues as water, and signs as sand or gravel.

## Rendering application

- Palette-verified road faces receive a neutral asphalt detail map in the
  compatibility renderer's output-resolution geometry pass.
- Verified green face colors receive the grass detail map.
- Green BG2 landscape pixels below the horizon receive restrained turf detail,
  covering the large flat areas which are absent from the face stream.
- Original polygon colors remain authoritative. Material detail modulates those
  colors rather than replacing the palette.
- Original texture-mapped faces, vehicles, barriers, signs, sky, HUD, sprites,
  physics, timing, and the experimental native renderer are unchanged.
- Material textures are created before gameplay begins, avoiding a first-use
  upload during a race. `T` still toggles materials for direct comparison.

The session remains useful evidence rather than a shipped dependency. No
recording, captured framebuffer, ROM data, or derived course asset is included
in the source or build.

## Validation

- Deterministic 4:3 material-off v4.14 and v4.13 captures match exactly for the
  presented frame, GSU RAM, GSU registers, and WRAM at frame 2,500.
- The 3,200-frame 3440x1440 material-on test completed on the Direct3D backend
  with 7.0400 ms average processing time and 63.78 FPS rolling 1% low while
  running uncapped. The single 268.42 ms worst sample is startup resource
  creation, before racing, rather than an in-race material upload.
- The final 3,400-frame real-time 3440x1440 pass averaged 16.5979 ms per frame,
  delivered a 59.92 FPS rolling 1% low, and had a 17.0177 ms worst frame.
  Processing averaged 7.2398 ms, leaving 9.3580 ms of limiter headroom.
- Wide background, sprite, and geometry-capture unit tests pass.

This is a conservative first-course visual pass, not a complete material map
for every course. Additional tracks should be recorded and validated before
their palette and background rules are added.
