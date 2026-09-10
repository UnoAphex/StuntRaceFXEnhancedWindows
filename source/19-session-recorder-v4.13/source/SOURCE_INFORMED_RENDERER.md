# Source-informed native renderer — v4.6

## Scope and provenance boundary

The private XLR8 source tree was used only as a research reference. It identifies
the racing project internally as XLR8 and retains substantial Star Fox engine
material. No source file, binary asset, model, map, or generated export from
that tree is copied into this repository or the Windows package.

The playable build continues to read the user's retail USA Rev 1 ROM at run
time. Original emulation remains authoritative for simulation, collision, AI,
race state, animation, and timing. Compatibility rendering remains the default.

## Verified structure findings

- `track_scale` is 5, so authored track-model coordinates use a 32x scale.
- Authored road pieces commonly extend 30–80 source units from their local
  origin, corresponding to 960–2560 world units; wider structures reach 120
  source units.
- The retail first-course table's final words range from 330 to 605. They are
  collision/search radii and are narrower than the visible authored surface.
- Car definitions emit four wheel offsets in ordered axle pairs. The offsets
  are scaled by `car_scale` and contain front/rear flags.
- Vehicle body headers and wheel definitions establish plausible model bounds,
  but the v4.6 public build does not embed those proprietary model vertices.

## v4.6 implementation

The native road presentation scale now defaults to 4.0. On the first course
this converts the 330–605 radii into 1320–2420-unit half widths, matching the
authored road-piece range substantially better than v4.2's 660–1210 result.
The value remains a visual-only calibration and never changes collision.

When four valid child wheel records are present, the frontend computes front
and rear axle centers directly. Their directed difference supplies vehicle
heading and pitch. The two side centers supply roll and track width. Previous
pose or the current GSU view resolves the one possible axle-order reversal.
PCA remains available only as a fallback for incomplete records.

The native chase camera now follows this live chassis heading. The native
player is submitted in world space with live position, heading, pitch, and roll
instead of being drawn as a screen-pinned overlay. Geometry is still an
original low-poly proxy until the retail model stream is decoded at runtime.

SDL's renderer still lacks a depth buffer. v4.6 reduces visible ordering swaps
by sorting each submitted triangle by its own camera-space depth instead of
sorting an entire long track segment by one midpoint. A depth-buffered backend
remains the correct long-term solution for complete bridges and intersecting
scenery.

## Next renderer milestone

1. Trace the retail model pointer and shape-header stream using the verified
   XLR8 header/face layouts.
2. Decode retail vertices, faces, groups, normals, and palette indices directly
   from the user's ROM into reusable meshes.
3. Correlate command `$02` instances with runtime callbacks and transforms.
4. Add a depth-buffered GPU path consuming the same world-space cache.
5. Validate static/dynamic classification and parity across every course.
