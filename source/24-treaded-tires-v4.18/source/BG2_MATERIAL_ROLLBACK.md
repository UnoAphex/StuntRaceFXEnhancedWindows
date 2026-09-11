# BG2 material rollback v4.16

Runtime testing showed that the broad green BG2 layer is not a reliable terrain
identifier. It can appear beneath, or participate in the presentation of, the
near driving surface. Applying grass detail to every green pixel below the
horizon therefore allowed grass texture to appear where asphalt was expected.

Version 22 removes the BG2 grass pass completely. It also retains the v4.15
removal of green polygon-face classification. The only enhanced surface mapping
left enabled is asphalt on the two verified road-face palette signatures.

This intentionally favors correct classification over broader texturing. Grass
can be reintroduced after a reliable road/terrain ownership mask is recovered
from course or collision data. Pixel color and render layer alone are not enough.

Gameplay, collision, timing, input, interpolation, sprites, HUD composition, and
the experimental native renderer are unchanged.
