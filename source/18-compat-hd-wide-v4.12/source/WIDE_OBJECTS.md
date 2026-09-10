# Compatibility wide objects v4.11

This iteration restores two original rendering inputs that the first expanded
view did not carry: texture-mapped Super FX faces and eligible SNES OBJ pixels
crossing the original race-window boundary. All observation is restricted to
the verified unmodified USA Rev 1 ROM (CRC32 `380C2635`).

## Textured geometry

The observer now records the texture header and per-vertex UV values at the
same pre-clip face-submission point used for flat polygons. The packet includes
the original ROM bank, texture base, texture mask, color mode, initial color,
scroll values and camera-space vertices. The frontend samples the live user-
provided ROM into reusable GPU textures and reproduces the Super FX GETC color
mode and transparency rules. No ROM texture is exported or stored in the build.

Draw commands preserve original face order across flat and textured polygons.
The texture cache is bounded and only uploads a texture when its key or live
palette changes.

## Edge-crossing sprites

The core uses Snes9x's completed per-scanline OBJ selection and original tile,
flip, palette and priority state. Only pixels outside the central race window
are exported. Priority-3 pixels are currently accepted because they are the
only OBJ priority that is unconditionally above the common race BG1 depths.
Transparent pixels, color-math cases and unsafe lower priorities fail closed.

The fixed 16-pixel race-window bezel strips at X=16 and X=224 are explicitly
excluded. Those were the only eligible OBJ pixels in the deterministic first-
track checkpoint; repeating them produced the vertical side bars seen during
development. Empty sprite buffers are not uploaded each frame.

This pass can complete a sprite already selected by the original PPU renderer
when it crosses the old edge. It cannot invent a sprite or 3D object that the
game never submitted.

## Why fully offscreen cars are still absent

Retail Super FX code performs an object-level combined-outcode test before it
walks the object's face commands. At the verified `01:8D2C` branch, objects
outside a common original viewport edge jump to the rejection path at
`01:924D`. Consequently, no face packet exists at the later `01:918E` capture
point for some rival cars and scenery in the extended view.

Changing that branch in the authoritative Super FX run would change graphics
work and cycle behavior. A reliable continuation should instead decode the
original object command stream against the already prepared camera vertices,
or run that graphics section on isolated cloned state. Either route must retain
original materials, normals, animation and face order. Caching whichever faces
happened to be visible earlier is insufficient because it loses hidden faces
and state-dependent materials.

## Matching the center to the side quality

The quality difference is structural. The center is the original 208x128 Super
FX bitmap scaled to the output, while the sides are polygons rasterized by the
PC GPU at output resolution. Filtering cannot give the center equally straight
polygon edges.

The correct next architecture is a complete GPU 3D layer spanning center and
sides, followed by the original HUD and sprite layers at their intended scale.
Before enabling that as normal play, the capture must cover every flat,
textured, special and early-culled object path and preserve their draw/depth
order. The PPU output also needs a reliable 3D-versus-2D separation so the GPU
layer does not cover the car, gauges, text or animated overlays. Version 17
keeps the original center because those prerequisites are not complete yet.

## Validation

- ROM-free background, sprite and geometry-capture tests pass.
- At deterministic frame 2500, the fixed OBJ bezel extension is empty after
  filtering and the expanded image has no repeated vertical sprite strips.
- At deterministic frame 3300, v4.11 and v4.10 produce identical hashes for
  the 4:3 presented image, Super FX RAM, CPU/register snapshot and WRAM.
- The benchmark uses the Direct3D SDL backend and the existing deterministic
  first-track input. Generated captures remain outside the repository.

The native-track renderer was not changed in this iteration.
