# Sprite-safe smoothing v4.4

## Problem

The v4.3 responsive blend reduced steering delay, but its new-frame detector
sampled one pixel in four and required more than 2% of those samples to change.
Small animation updates often did not replace the smoothing target at all. This
could hold vehicle bobbing and collapse the animated in-race pause drawing to
only a few visible poses.

## Classification and composition

The compatibility framebuffer does not expose a ready-made 3D-versus-2D layer
mask. Version 4.4 therefore separates update behavior using information that is
reliable at the final framebuffer:

- Non-race scenes are treated as 2D presentation and shown directly.
- Every pixel is checked for changes during a race.
- A change affecting at most 2% of the frame is classified as local animation.
- Changed pixels mark their complete 8x8 SNES tiles for immediate presentation.
- A change affecting more than 2% is classified as broad scene motion and keeps
  responsive temporal smoothing.
- The HUD and player/control-feedback region continue to update immediately.

Broad scene updates begin 75% toward the newest frame, or 87.5% while steering.
The following presentation completes the blend. Controller polling, physics,
collision, game animation timing, Super FX execution, and gameplay speed are
unchanged.

## Regression test

A deterministic race opened the animated RETIRE? pause screen and captured 81
consecutive presented frames with smoothing enabled. Version 09 left long gaps
between accepted animation states. Version 10 restored a regular five-frame
source-animation cadence and visibly captured the distinct mechanic/car poses.

The tests and captures were generated outside the repository and are not part
of the source or runtime package.