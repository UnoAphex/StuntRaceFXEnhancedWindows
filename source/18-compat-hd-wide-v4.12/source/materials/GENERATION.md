# N64-style material asset generation

The image-generation skill influenced the nine terrain tiles. It supplied
stylized top-down source art; `process_texture_assets.py` then center-cropped,
mirrored for periodic edges, reduced each source to 64x64, adjusted color and
contrast, and quantized it to a 24–32 color palette. The runtime uses the BMP
files and preserves the game's original hue and lighting through luma-only
modulation.

## Exact prompts

### Grass

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: fresh and slightly dry green grass viewed perfectly
top-down. Chunky stylized blades and mottling, neutral diffuse color, limited
palette, deliberately retro but clean. Even lighting, no directional shadows,
no perspective, no flowers, no rocks, no tire marks, no paths, no borders, no
objects, no text, no logos. The left/right and top/bottom edges must tile
naturally. Fill the entire image.

### Asphalt

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: dark asphalt viewed perfectly top-down. Chunky stylized
charcoal and slate aggregate, low-detail hand-painted diffuse color, limited
palette, deliberately retro but clean. Even lighting, no directional shadows,
no perspective, no painted lines, no cracks, no tire marks, no borders, no
objects, no text, no logos. The left/right and top/bottom edges must tile
naturally. Fill the entire image.

### Gravel

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: compact gravel road viewed perfectly top-down. Chunky
stylized gray and warm beige pebbles, low-detail hand-painted diffuse color,
limited 32-color palette, subtle broad variation, deliberately retro but clean.
Even lighting, no shadows, no perspective, no tire marks, no painted lines, no
borders, no objects, no text, no logos. The left/right and top/bottom edges must
tile naturally. Fill the entire image.

### Dirt

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: compact reddish-brown dirt and dry mud viewed perfectly
top-down. Chunky stylized soil clumps and soft mottling, low-detail hand-painted
diffuse color, limited 32-color palette, subtle broad variation, deliberately
retro but clean. Even lighting, no directional shadows, no perspective, no tire
tracks, no grass, no stones larger than small specks, no borders, no objects, no
text, no logos. The left/right and top/bottom edges must tile naturally. Fill the
entire image.

### Sand

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: pale golden tan sand viewed perfectly top-down. Chunky
stylized granular mottling with a few softly curved wind patterns, low-detail
hand-painted diffuse color, limited 32-color palette, subtle broad variation,
deliberately retro but clean. Even lighting, no directional shadows, no
perspective, no footprints, no plants, no borders, no objects, no text, no
logos. The left/right and top/bottom edges must tile naturally. Fill the entire
image.

### Stone / concrete

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: pale gray concrete and compact stone paving viewed
perfectly top-down. Chunky stylized aggregate and broad slab-like tonal
variation without visible grout lines, low-detail hand-painted diffuse color,
limited 32-color palette, deliberately retro but clean. Even lighting, no
directional shadows, no perspective, no cracks, no road markings, no borders,
no objects, no text, no logos. The left/right and top/bottom edges must tile
naturally. Fill the entire image.

### Snow / ice

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: packed snow with faint icy blue patches viewed perfectly
top-down. Chunky stylized crystalline mottling, low-detail hand-painted diffuse
color, limited 32-color palette, subtle broad variation, deliberately retro but
clean. Even lighting, no directional shadows, no perspective, no footprints, no
tire tracks, no borders, no objects, no text, no logos. The left/right and
top/bottom edges must tile naturally. Fill the entire image.

### Water

Create one square seamless tileable surface texture for a late-1990s Nintendo
64 style racing game: calm deep blue water viewed perfectly top-down. Chunky
stylized cyan and navy ripples, low-detail hand-painted diffuse color, limited
32-color palette, subtle broad repeating wave variation, deliberately retro but
clean. Even lighting, no directional shadows, no perspective, no shoreline, no
foam, no objects, no borders, no text, no logos. The left/right and top/bottom
edges must tile naturally. Fill the entire image.

### Mud

Create one square seamless tileable terrain texture for a late-1990s Nintendo
64 style racing game: dark wet brown mud viewed perfectly top-down. Chunky
stylized soft puddled mottling and compact earth clumps, low-detail hand-painted
diffuse color, limited 32-color palette, subtle broad variation, deliberately
retro but clean. Even lighting, no directional shadows, no perspective, no tire
tracks, no footprints, no grass, no borders, no objects, no text, no logos. The
left/right and top/bottom edges must tile naturally. Fill the entire image.

## Intended use

These tiles are post-process terrain-detail masks for the compatibility
frontend. The classifier selects a material from the original framebuffer color
and broad-polygon support, then applies only the tile's brightness variation.
This gives flat terrain a restrained N64-like surface while retaining the SNES
palette, depth shading, silhouettes, and collision behavior.
