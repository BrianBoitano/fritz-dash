#!/usr/bin/env python3
"""Fritz Dash stock icon compiler: the pixel art in tools/icon_art.py -> C.

    gen_icons.py c OUT.c              STOCK[], PETS[] and the set names as a C file
    gen_icons.py sheet OUT.png [N]    contact sheet of every icon and pet at N x (default 4)

Every icon is 16 rows of 16 letters, one letter per pixel; "." is clear (the
cube's colour shows through). A row of 8 letters is the left half of a
symmetric row: it is mirrored to make the right half. Letters are colours from PALETTE in
icon_art.py, or from the icon's own pal= overrides; "1" and "2" are the
cube's two Customize colours. An icon may use at most 8 colours.

Icons are saved in custom.txt by their position in ICONS, so new icons go at
the end of the list (any set), never in the middle.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import icon_art  # noqa: E402

N = 16
MAX_COLOURS = 8
FONT = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!:-.%/?,()<>+_&= ")
PLAYER = {"1": "STOCK_COL1", "2": "STOCK_COL2"}
SHEET_PLAYER = {"1": (90, 255, 40), "2": (0, 230, 255)}   # the default look: GREEN and CYAN


def fail(msg):
    sys.exit(f"gen_icons.py: {msg}")


def rgb(hexstr):
    v = int(hexstr, 16)
    return (v >> 16) & 255, (v >> 8) & 255, v & 255


def check_name(name, what):
    if not name or len(name) > 12 or set(name) - FONT:
        fail(f"{what}: name {name!r} must be 1-12 characters the game font has")


def grid(rows, what):
    """16 rows of 16 letters; a row of 8 is the left half, mirrored to make the right."""
    lines = [l.strip() for l in rows.strip().splitlines()]
    lines = [l + l[::-1] if len(l) == N // 2 else l for l in lines]
    if len(lines) != N or any(len(l) != N for l in lines):
        fail(f"{what}: needs {N} rows of {N} letters, got {[len(l) for l in lines]}")
    return lines


def palette_of(frames, pal, what):
    """Letters used, in first-seen order, and their colours (RGB, or "1"/"2")."""
    colours = dict(icon_art.PALETTE)
    colours.update(pal or {})
    used = []
    for lines in frames:
        for line in lines:
            for ch in line:
                if ch == ".":
                    continue
                if ch not in colours and ch not in PLAYER:
                    fail(f"{what}: letter {ch!r} is not a colour")
                if ch not in used:
                    used.append(ch)
    if not used:
        fail(f"{what}: is empty")
    if len(used) > MAX_COLOURS:
        fail(f"{what}: uses {len(used)} colours ({''.join(used)}), at most {MAX_COLOURS}")
    return used, colours


def digits(lines, used):
    return "".join("0" if ch == "." else str(1 + used.index(ch)) for line in lines for ch in line)


def c_colour(ch, colours):
    if ch in PLAYER:
        return PLAYER[ch]
    r, g, b = rgb(colours[ch])
    return f"RGB({r}, {g}, {b})"


def unlock(spec, what):
    """'' (free), 'stars N', 'coins N' or 'badge B_NAME' -> (stars, coins, badge expr)."""
    if not spec:
        return 0, 0, "0"
    kind, _, val = spec.partition(" ")
    if kind == "stars" and val.isdigit():
        return int(val), 0, "0"
    if kind == "coins" and val.isdigit():
        return 0, int(val), "0"
    if kind == "badge" and val.startswith("B_"):
        return 0, 0, f"{val} + 1"
    fail(f"{what}: unlock {spec!r} must be 'stars N', 'coins N' or 'badge B_NAME'")


def load():
    sets = list(icon_art.SETS)
    icons = []
    for ic in icon_art.ICONS:
        what = f"icon {ic['name']}"
        check_name(ic["name"], what)
        if ic["set"] not in sets:
            fail(f"{what}: set {ic['set']!r} is not in SETS")
        lines = grid(ic["rows"], what)
        used, colours = palette_of([lines], ic.get("pal"), what)
        icons.append(dict(ic, lines=lines, used=used, colours=colours))
    pets = []
    for pt in icon_art.PETS:
        what = f"pet {pt['name']}"
        check_name(pt["name"], what)
        frames = [grid(f, f"{what} frame {i + 1}") for i, f in enumerate(pt["frames"])]
        if len(frames) != 2:
            fail(f"{what}: needs 2 frames")
        used, colours = palette_of(frames, pt.get("pal"), what)
        pets.append(dict(pt, frames_l=frames, used=used, colours=colours))
    names = [ic["name"] for ic in icons]
    if len(set(names)) != len(names):
        fail("two icons share a name")
    secret = [ic for ic in icons if ic.get("secret")]
    if len(icons) - len(secret) != icon_art.REGULAR or len(secret) != icon_art.SECRET or len(pets) != icon_art.PET_COUNT:
        fail(f"need {icon_art.REGULAR} icons, {icon_art.SECRET} secret ones and {icon_art.PET_COUNT} pets, "
             f"got {len(icons) - len(secret)}, {len(secret)}, {len(pets)}")
    for s in sets:
        n = sum(1 for ic in icons if ic["set"] == s)
        if s != icon_art.SECRET_SET and not 8 <= n <= 16:
            fail(f"set {s} has {n} icons (8 to 16)")
        if s == icon_art.SECRET_SET and any(not ic.get("secret") for ic in icons if ic["set"] == s):
            fail(f"every icon in {s} is secret")
    return sets, icons, pets


def write_c(out):
    sets, icons, pets = load()
    o = ["// Generated by tools/gen_icons.py from tools/icon_art.py. Do not edit.",
         '#include "badges.h"', '#include "stock.h"', "",
         f"const int STOCK_COUNT = {len(icons)};",
         f"const int STOCK_SET_COUNT = {len(sets)};",
         f"const int PET_COUNT = {len(pets)};",
         "const char *const STOCK_SET_NAMES[] = { " + ", ".join(f'"{s}"' for s in sets) + " };", "",
         "const StockIcon STOCK[] = {"]
    for ic in icons:
        stars, coins, badge = unlock(ic.get("unlock", ""), f"icon {ic['name']}")
        pal = ", ".join(c_colour(ch, ic["colours"]) for ch in ic["used"])
        o.append(f'    {{ {{ "{ic["name"]}", {stars}, {coins}, {badge} }}, {sets.index(ic["set"])}, '
                 f'{1 if ic.get("secret") else 0},')
        o.append(f"      {{ {pal} }},")
        o.append(f'      "{digits(ic["lines"], ic["used"])}" }},')
    o += ["};", "", "const StockPet PETS[] = {"]
    for pt in pets:
        pal = ", ".join(c_colour(ch, pt["colours"]) for ch in pt["used"])
        o.append(f'    {{ "{pt["name"]}", {{ {pal} }},')
        o.append("      { " + ", ".join(f'"{digits(f, pt["used"])}"' for f in pt["frames_l"]) + " } },")
    o.append("};")
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    Path(out).write_text("\n".join(o) + "\n")


def write_sheet(out, scale):
    from PIL import Image, ImageDraw
    sets, icons, pets = load()
    cell, cols, pad = N * scale + 24, 16, 14
    label_h, head_h = 16, 22
    groups = [(s, [ic for ic in icons if ic["set"] == s]) for s in sets]
    rows = sum((len(g) + cols - 1) // cols for _, g in groups) + 1
    w = pad * 2 + cols * cell
    h = pad * 2 + rows * (cell + label_h) + (len(groups) + 1) * head_h
    img = Image.new("RGB", (w, h), (16, 12, 30))
    d = ImageDraw.Draw(img)

    def colour(ch, colours):
        return SHEET_PLAYER[ch] if ch in PLAYER else rgb(colours[ch])

    def sprite(x0, y0, lines, colours, back):
        if back:
            d.rectangle([x0 - 6, y0 - 6, x0 + N * scale + 5, y0 + N * scale + 5], fill=back)
        for y, line in enumerate(lines):
            for x, ch in enumerate(line):
                if ch != ".":
                    d.rectangle([x0 + x * scale, y0 + y * scale, x0 + (x + 1) * scale - 1, y0 + (y + 1) * scale - 1],
                                fill=colour(ch, colours))

    y = pad
    for s, group in groups:
        d.text((pad, y + 4), f"{s} ({len(group)})", fill=(255, 235, 40))
        y += head_h
        for i, ic in enumerate(group):
            cx, cy = pad + (i % cols) * cell + 12, y + (i // cols) * (cell + label_h) + 8
            sprite(cx, cy, ic["lines"], ic["colours"], SHEET_PLAYER["1"])
            tag = ic["name"] + (" *" if ic.get("unlock") else "")
            d.text((cx - 8, cy + N * scale + 8), tag[:14], fill=(230, 230, 255))
        y += ((len(group) + cols - 1) // cols) * (cell + label_h)
    d.text((pad, y + 4), f"PETS ({len(pets)}, two frames each)", fill=(255, 235, 40))
    y += head_h
    for i, pt in enumerate(pets):
        cx = pad + (i % 8) * cell * 2 + 12
        for f in range(2):
            sprite(cx + f * cell, y + 8, pt["frames_l"][f], pt["colours"], (40, 34, 70))
        d.text((cx - 8, y + 8 + N * scale + 8), pt["name"], fill=(230, 230, 255))
    img.save(out)


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "c":
        write_c(sys.argv[2])
    elif len(sys.argv) >= 3 and sys.argv[1] == "sheet":
        write_sheet(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 4)
    else:
        fail(__doc__)


if __name__ == "__main__":
    main()
