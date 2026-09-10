# Compatibility HD Wide v4.12

## Result

This iteration introduces the first opt-in compatibility-renderer mode in which
the captured Super FX polygon world is rasterized continuously across the
original center and expanded left/right view at the selected PC output
resolution. It replaces neither the game simulation nor the experimental native
renderer.

The two included HD launchers target 1920x1080 and 3440x1440. The vertical
framing and original projection scale remain fixed; increasing aspect ratio
increases the horizontal camera-space range. The HUD remains centered and is not
stretched.

## Pipeline

The original game and Super FX execution remain authoritative. During each race
frame the compatibility core exposes read-only copies of:

1. the ordered original camera-space polygon packets;
2. live palette and texture state;
3. the 768-pixel scanline background used by the wide extension; and
4. a 208x128 reference reconstructed from the active Mode 3 BG1 world bitmap,
   falling through to BG2 where the BG1 pixel is transparent.

The frontend GPU-rasterizes the ordered polygon packets over the background at
the actual output size. It compares the original PPU result with the reconstructed
world reference to isolate HUD, text, and OBJ graphics. That overlay is then
composited at its original proportions over the high-resolution world.

Some Super FX-generated bitmap primitives, notably animated wheels, do not pass
through the observed polygon packet path. A low-resolution coverage prediction
identifies compact missing primitives and restores them from the authoritative
frame. A 3x3 morphological opening rejects isolated polygon-edge disagreement,
preventing the old jagged raster boundary from being painted over the new GPU
edge. The predicted coverage is cached until polygon or palette data changes.

This design was selected instead of fixed 1x/2x/3x/4x internal multipliers: the
geometry is directly rasterized at the current output resolution, so 1080p,
1440p, ultrawide, and 4K naturally receive the available edge precision without
an extra CPU upscale or intermediate framebuffer.

## Controls and fallback

- `Ctrl+H` toggles the full HD world preview and enables the wide observer.
- `Ctrl+W` toggles the inherited side-only expanded view.
- The standard compatibility launchers do not enable the HD preview.
- 4:3 bypasses the wide pass and remains the unchanged baseline.

If required state is unavailable, the HD path fails closed and the ordinary
compatibility image remains visible.

## Validation

ROM-free unit tests cover RGB565 conversion, BG1 world-reference decoding,
transparent BG1 fall-through to BG2, scanline refresh, blanking, bitmap
selection, reset, wide sprite safety, and polygon-capture assembly.

Deterministic first-track comparison at frame 3299 between v4.11 and v4.12 in
4:3 produced identical SHA-256 hashes for the presented frame, Super FX RAM,
register snapshot, and WRAM. This confirms that the opt-in presentation work did
not change the baseline image or emulated state.

At 3440x1440 with real-time 60 Hz pacing over 1800 deterministic frames:

- average delivered frame time: 16.5877 ms;
- rolling 1% low: 59.57 FPS;
- average processing time: 4.0260 ms;
- average core time: 4.0090 ms; and
- average presentation time: 0.2371 ms.

Captured checks at the beginning of the first race and through a later turn show
continuous center/side roads, barriers, buildings, player geometry, rival
geometry when submitted by the game, textured decals, HUD, timers, and animated
wheel primitives. The former vertical divider lines are no longer present.

## Known limitations

- Geometry and objects that the original Super FX program rejects before face
  submission remain unavailable to this passive observer. Fully offscreen rival
  cars can therefore still be absent until the original object-cull stage is
  decoded or a safe graphics-only execution strategy is developed.
- Compact restored bitmap primitives retain original-resolution pixel edges.
- BG2 scenery keeps its original texture resolution even though polygon edges
  are rasterized at the PC output resolution.
- The compatibility path has no depth buffer; it preserves original submission
  order. Rare packet-order or unsupported primitive cases may need further
  per-track correction.

No game ROM, save data, capture, or ROM-derived export belongs in this source
tree or the release archive.
