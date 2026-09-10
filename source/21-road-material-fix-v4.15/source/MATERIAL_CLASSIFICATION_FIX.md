# Road/material classification fix v4.15

The first recording-driven material pass correctly identified recurring color
families, but occurrence alone did not prove semantic ownership. Runtime play
showed that the green polygon family at palette slots 212–215 is reused by
road/shoulder geometry. Treating every one of those faces as terrain could put
the grass detail map on part of the driving surface.

Version 21 removes all green polygon-face material assignment. The geometry
pass now assigns asphalt only to the verified road colors at palette slots 60
and 61. Grass detail is confined to green pixels in the recorded BG2 landscape
below the horizon, which is the layer responsible for the broad first-course
turf. Road geometry is drawn over that background normally.

This correction changes presentation classification only. It does not alter
course data, collision, physics, input, timing, interpolation, sprites, the HUD,
or the experimental native renderer.
