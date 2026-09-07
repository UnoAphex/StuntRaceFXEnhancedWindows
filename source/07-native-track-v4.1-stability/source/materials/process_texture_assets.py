from pathlib import Path
from PIL import Image, ImageEnhance, ImageOps


ROOT = Path(__file__).parent / "stuntrace-main" / "assets" / "textures"

MATERIALS = {
    "grass": ("grass-source-v1.png", 32, 0.92, 0.88),
    "asphalt": ("asphalt-source-v1.png", 24, 0.90, 0.84),
    "gravel": ("gravel-source-v1.png", 32, 0.96, 0.86),
    "dirt": ("dirt-source-v1.png", 28, 0.94, 0.86),
    "sand": ("sand-source-v1.png", 24, 0.94, 0.82),
    "stone": ("stone-source-v1.png", 28, 0.92, 0.84),
    "snow": ("snow-source-v1.png", 24, 0.88, 0.80),
    "water": ("water-source-v1.png", 28, 0.96, 0.86),
    "mud": ("mud-source-v1.png", 24, 0.94, 0.86),
}


def make_periodic_tile(source: Path, palette_colors: int, saturation: float,
                       contrast: float) -> Image.Image:
    image = Image.open(source).convert("RGB")
    side = min(image.size)
    left = (image.width - side) // 2
    top = (image.height - side) // 2
    image = image.crop((left, top, left + side, top + side))

    # A mirrored 2x2 construction makes every edge periodic. Build it at a
    # slightly larger working size, then reduce to a deliberately tiny N64 tile.
    quarter = image.resize((64, 64), Image.Resampling.LANCZOS)
    periodic = Image.new("RGB", (128, 128))
    periodic.paste(quarter, (0, 0))
    periodic.paste(ImageOps.mirror(quarter), (64, 0))
    periodic.paste(ImageOps.flip(quarter), (0, 64))
    periodic.paste(ImageOps.mirror(ImageOps.flip(quarter)), (64, 64))
    periodic = periodic.resize((64, 64), Image.Resampling.LANCZOS)
    periodic = ImageEnhance.Color(periodic).enhance(saturation)
    periodic = ImageEnhance.Contrast(periodic).enhance(contrast)
    return periodic.quantize(colors=palette_colors, method=Image.Quantize.MEDIANCUT).convert("RGB")


tiles = {}
for name, (source_name, colors, saturation, contrast) in MATERIALS.items():
    tile = make_periodic_tile(ROOT / source_name, colors, saturation, contrast)
    tiles[name] = tile
    tile.save(ROOT / f"{name}-n64-v1.png", optimize=True)
    tile.save(ROOT / f"{name}-n64-v1.bmp")
    print(f"{name}: {tile.size[0]}x{tile.size[1]}, <= {colors} colors")

preview = Image.new("RGB", (192, 192))
for index, name in enumerate(MATERIALS):
    preview.paste(tiles[name], ((index % 3) * 64, (index // 3) * 64))
preview.save(Path(__file__).parent / "material-preview.png")
