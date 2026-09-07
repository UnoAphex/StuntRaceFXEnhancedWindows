# Native Track Geometry Research

## Prototype status

The enhanced frontend now contains a first-course native road prototype.  The
compatibility renderer remains the default and its Super FX execution is not
patched.  During a race the frontend observes the live course pointer, decodes
the immutable course centerline from the user's ROM, builds a persistent
world-space mesh, and renders it independently at the SDL output resolution.

This milestone reconstructs the authoritative road surface, shoulders, and a
terrain corridor.  It does **not** yet decode the original building, sign,
barrier, or scenery models.  Those remain visible in compatibility/hybrid mode
and are the next static-geometry decoding milestone.

## Course construction findings

During the first race, Super FX RAM `$70:1092` contains `$88B7` and
`$70:1094` contains bank `$05`.  The referenced LoROM table begins with a
little-endian node count, followed by fixed eight-byte records:

```
u16 node_count
repeat node_count:
    s16 world_x
    s16 world_y
    s16 world_z
    u16 road_half_width
```

The first course has 64 records and forms a closed loop.  Coordinates and road
widths remain stable throughout the race.  GSU code around `$04:CC06` reads the
same pointer, walks the records, finds the nearby course node, and returns its
position and width.  This verifies that the table is authoritative course and
collision data rather than a display-only approximation.

The 65816 dispatcher at `$08:801C` handles the stream that supplies the GSU:

- command `$04` writes the active ROM course pointer to `$70:1092/$1094`;
- command `$02`, parsed near `$08:826E`, describes objects with position,
  heading, callback, offsets/extents, flags, and a model-related field.

The 42-byte structures in GSU RAM `$210C..$2304` are transient working/render
slots.  They are recycled for different course nodes and list roles while the
vehicle advances.  They are not safe static-cache keys.  Experiments that made
all of those slots active changed gameplay/timing, so the enhanced path only
observes them and never changes the original lists.

## Static and dynamic boundary

Verified static for the current prototype:

- course centerline and road half-widths;
- the road ribbon derived from adjacent centerline tangents;
- diagnostic gravel shoulders and terrain corridor derived on the PC side.

Known dynamic:

- player and opponent vehicles;
- camera transform;
- effects, animation, moving objects, and state-dependent scene objects;
- transient GSU working/render lists.

Vehicle roots use record kind `abs($0098)` and model pointer `$B1A1`, followed
by four `abs($0052)` child records.  The frontend reads these live records to
make interpolated diagnostic vehicle proxies.  Their source state remains
owned by the original game.

Static scenery cannot be classified solely by appearance.  Command `$02`
objects must be correlated with callbacks, flags, and observed transforms over
a race before being promoted into the persistent cache.  Destructible,
animated, or race-state-dependent instances must remain dynamic.

## Persistent PC-side cache

`NativeTrackCache` stores:

- source ROM bank/address and a generation number;
- center nodes and widths;
- derived left/right road edges and outer terrain edges;
- world-space bounds and closed/open-course metadata.

The cache is rebuilt only when the live course pointer changes.  No camera or
projection data is baked into it.  The included `tools/track/extract_track_cache.py`
can export the same table as JSON and a diagnostic OBJ for offline inspection;
generated exports are ROM-derived research artifacts and must not be shipped.

The intended scenery extension is a second cache layer containing decoded
model vertices/indices, material IDs, per-instance world transforms, bounds,
segment ownership, and verified static/dynamic classification.

## Native rendering path

The frontend reads the live camera position and 3x3 matrix, interpolates it for
presentation, and applies a PC-side perspective projection.  The projection
preserves vertical framing while naturally widening horizontal visibility at
16:9 and 21:9.  Near clipping, frustum tests, far-distance tests, and fog are
performed against the native camera.  The original framebuffer is not
stretched into the extra view.

The current SDL geometry path provides:

- true output-resolution road rasterization;
- adjustable vertical FOV and far distance;
- PC-side segment culling;
- grass, gravel, and asphalt materials;
- distance fog;
- 60 Hz camera/vehicle visual interpolation;
- a centered 4:3-safe original HUD strip;
- wireframe, segment IDs, and cache/culling counters.

SDL's 2D geometry API has no depth buffer or multisample control.  The
prototype therefore uses material layers plus far-to-near sorting.  A later
GPU backend (D3D11/OpenGL/Vulkan) should consume the same world-space cache to
add a real depth buffer, MSAA, stable intersecting geometry, lighting, and
shadows.  FXAA can remain an optional presentation pass.

## Controls and fallback

- `R`: compatibility -> native track -> hybrid comparison
- `B`: native mesh wireframe
- `I`: track segment IDs
- `F`: native distance fog
- `[` / `]`: vertical FOV
- `-` / `=`: native far distance
- `D`: original/extended/far distance presets
- `Alt+Enter` or `F11`: fullscreen/windowed

Startup environment options:

- `SRF_RENDERER=compat|native|hybrid`
- `SRF_NATIVE_FOV=58`
- `SRF_NATIVE_DISTANCE=60000`
- `SRF_NATIVE_FOG=0|1`
- `SRF_MATERIALS=0|1`
- `SRF_NATIVE_WIREFRAME=0|1`
- `SRF_NATIVE_IDS=0|1`

Compatibility mode continues to use the known-good original renderer.  Hybrid
mode overlays translucent native geometry for camera/track alignment checks.

## Validation completed and remaining

Completed on the first race:

- live pointer decoded consistently as a 64-node closed course;
- offline extractor and runtime decoder agree on count and bounds;
- native road follows the live camera through scripted race input;
- native output renders at 1280x720 and ultrawide output dimensions;
- road/terrain layers no longer obscure each other on sharp turns;
- compatibility remains the default and can be restored instantly.

Still required before claiming general course parity:

- enumerate course pointers for every mode/course;
- decode command `$02` scenery/model data and original polygon materials;
- observe each candidate instance across race states before caching it;
- compare checkpoints, jumps, bridges, tunnels, destructibles, and alternate
  camera modes;
- replace diagnostic vehicle boxes with decoded dynamic models;
- use a depth-buffered GPU backend and add MSAA/optional FXAA;
- build automated image/geometry comparisons for missing polygons, cracks,
  transforms, materials, culling, and state-dependent geometry.

The evidence favors direct course-data decoding over capturing the transient
GSU display lists.  Direct decoding keeps geometry persistent and world-space,
while display-list capture inherits per-frame slot reuse and original viewport
decisions.  Selective capture may still help identify model/material formats,
but it should not become the cache's authoritative representation.
