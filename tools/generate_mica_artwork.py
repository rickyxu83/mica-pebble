#!/usr/bin/env python3
"""Generate Mica's original Pebble-style monochrome mark.

A deliberately small pixel grid, hard edges and one ink colour keep the artwork
consistent with Pebble OS icons. No upstream mascot artwork is used.
"""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "app" / "resources"
INK = "#172B33"


def mark(size: int = 24) -> Image.Image:
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # Open speech bubble with a compact M. Every coordinate stays on the grid.
    d.rectangle((2, 3, 21, 17), outline=INK, width=2)
    d.polygon([(6, 17), (5, 22), (11, 17)], fill=INK)
    d.line((7, 14, 7, 8, 10, 8, 12, 11, 14, 8, 17, 8, 17, 14), fill=INK, width=2, joint="curve")
    return im


def fold_mark() -> Image.Image:
    im = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.polygon([(12, 1), (22, 11), (12, 23), (2, 11)], fill="#FFF1D2", outline=INK, width=2)
    d.line((12, 2, 12, 22), fill=INK, width=1)
    d.line((7, 14, 7, 9, 9, 9, 11, 12, 13, 9, 15, 9, 15, 14), fill=INK, width=1)
    return im


def main() -> None:
    (OUT / "images").mkdir(parents=True, exist_ok=True)
    (OUT / "icons").mkdir(parents=True, exist_ok=True)
    mascot = mark()
    mascot.resize((64, 64), Image.Resampling.NEAREST).save(OUT / "images" / "mica_mark.png")
    mascot.resize((24, 24), Image.Resampling.NEAREST).save(OUT / "icons" / "mica_general_menu.png")
    mascot.resize((96, 96), Image.Resampling.NEAREST).save(OUT / "images" / "mica_mark_about.png")
    fold_mark().resize((64, 64), Image.Resampling.NEAREST).save(OUT / "images" / "mica_fold.png")
    print("Generated original monochrome Mica mark")


if __name__ == "__main__":
    main()
