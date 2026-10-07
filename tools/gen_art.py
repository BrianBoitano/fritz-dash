#!/usr/bin/env python3
"""Generate the PS4 package art for Fritz Dash.

Writes icon0.png (512x512) and pic1.png (1920x1080) into the given folder,
drawn with the same 5x7 bitmap font and neon look as the game itself.

Usage: python3 gen_art.py <output_sce_sys_dir>
"""
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

# Same glyphs as src/draw.c: one byte per row, bit 4 is the leftmost column.
FONT = {
    "A": [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
    "D": [0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E],
    "F": [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10],
    "H": [0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11],
    "I": [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
    "R": [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
    "S": [0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E],
    "T": [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04],
    "Z": [0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F],
    " ": [0, 0, 0, 0, 0, 0, 0],
}

LIME = (90, 255, 40)
CYAN = (0, 230, 255)
PINK = (255, 50, 200)
WHITE = (255, 255, 255)


def text_width(text, scale):
    return (len(text) * 6 - 1) * scale


def draw_text(draw, x, y, scale, text, fill):
    for ch in text:
        for r, bits in enumerate(FONT[ch]):
            for c in range(5):
                if bits & (0x10 >> c):
                    draw.rectangle(
                        [x + c * scale, y + r * scale, x + (c + 1) * scale - 1, y + (r + 1) * scale - 1],
                        fill=fill,
                    )
        x += 6 * scale


def gradient(w, h, top, bottom):
    img = Image.new("RGB", (w, h))
    px = ImageDraw.Draw(img)
    for y in range(h):
        t = y / (h - 1)
        col = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        px.line([(0, y), (w, y)], fill=col)
    return img


def add_glow(base, layer_fn, radius):
    """Draw with layer_fn on a black layer, blur it, and add it to base as light."""
    layer = Image.new("RGB", base.size, (0, 0, 0))
    layer_fn(ImageDraw.Draw(layer))
    glow = layer.filter(ImageFilter.GaussianBlur(radius))
    out = ImageChops.add(base, glow)
    return ImageChops.add(out, glow)


def cube(draw, cx, cy, size):
    h = size / 2
    draw.rectangle([cx - h, cy - h, cx + h, cy + h], fill=LIME)
    i = h - size * 0.11
    draw.rectangle([cx - i, cy - i, cx + i, cy + i], fill=(6, 30, 10))
    c = size * 0.23
    draw.rectangle([cx - c, cy - c, cx + c, cy + c], fill=CYAN)
    w = size * 0.09
    draw.rectangle([cx - w, cy - w, cx + w, cy + w], fill=WHITE)


def spike(draw, x, ground, size):
    draw.polygon([(x, ground), (x + size / 2, ground - size), (x + size, ground)], fill=WHITE)
    inset = size * 0.14
    draw.polygon(
        [(x + inset, ground - 3), (x + size / 2, ground - size + inset * 1.6), (x + size - inset, ground - 3)],
        fill=(10, 4, 22),
    )


def scene(w, h, ground, grid, title_scale, title_y, two_lines, cube_xy, cube_size, spikes, spike_size):
    img = gradient(w, h, (10, 4, 40), (62, 10, 92))
    d = ImageDraw.Draw(img)
    for x in range(0, w, grid):
        d.rectangle([x, 0, x + max(1, grid // 64), ground], fill=(40, 30, 110))
    for y in range(grid // 2, ground, grid):
        d.rectangle([0, y, w, y + max(1, grid // 64)], fill=(40, 30, 110))
    d.rectangle([0, ground, w, h], fill=(20, 6, 52))
    for x in range(0, w, grid):
        d.rectangle([x, ground, x + max(2, grid // 48), h], fill=(30, 60, 110))

    # Neon ground line, cube trail, cube and spikes, each with a soft glow.
    def ground_line(dd):
        dd.rectangle([0, ground - max(2, h // 180), w, ground + max(2, h // 180)], fill=CYAN)

    img = add_glow(img, ground_line, h / 90)
    d = ImageDraw.Draw(img)
    ground_line(d)

    cx, cy = cube_xy
    for i in range(1, 6):
        s = cube_size * (0.42 - i * 0.06)
        tx = cx - cube_size * 0.55 * i
        col = tuple(int(LIME[k] * (1 - i / 6) + 40 * (i / 6)) for k in range(3))
        d.rectangle([tx - s / 2, cy - s / 2, tx + s / 2, cy + s / 2], fill=col)

    img = add_glow(img, lambda dd: cube(dd, cx, cy, cube_size * 1.05), cube_size / 6)
    for sx in spikes:
        img = add_glow(img, lambda dd, sx=sx: spike(dd, sx, ground, spike_size), spike_size / 7)
    d = ImageDraw.Draw(img)
    cube(d, cx, cy, cube_size)
    for sx in spikes:
        spike(d, sx, ground, spike_size)

    # Title.
    words = ["FRITZ", "DASH"]
    colors = [(LIME, (210, 255, 180)), (PINK, (255, 200, 240))]
    positions = []
    if two_lines:
        for i, word in enumerate(words):
            positions.append(((w - text_width(word, title_scale)) // 2, title_y + i * 9 * title_scale))
    else:
        total = text_width("FRITZ DASH", title_scale)
        x0 = (w - total) // 2
        positions.append((x0, title_y))
        positions.append((x0 + 6 * 6 * title_scale, title_y))
    for (x, y), word, (glow_col, core_col) in zip(positions, words, colors):
        img = add_glow(img, lambda dd, x=x, y=y, word=word, g=glow_col: draw_text(dd, x, y, title_scale, word, g),
                       title_scale * 1.2)
        d = ImageDraw.Draw(img)
        draw_text(d, x, y, title_scale, word, core_col)
    return img


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    out.mkdir(parents=True, exist_ok=True)

    icon = scene(512, 512, ground=440, grid=64, title_scale=13, title_y=34, two_lines=True,
                 cube_xy=(150, 330), cube_size=96, spikes=[300, 380], spike_size=72)
    icon.save(out / "icon0.png", optimize=True)

    pic = scene(1920, 1080, ground=840, grid=192, title_scale=22, title_y=150, two_lines=False,
                cube_xy=(700, 600), cube_size=110, spikes=[1000, 1090, 1500], spike_size=90)
    pic.save(out / "pic1.png", optimize=True)

    for name in ("icon0.png", "pic1.png"):
        with Image.open(out / name) as im:
            print(f"{out / name}: {im.size[0]}x{im.size[1]} {im.mode}")


if __name__ == "__main__":
    main()
