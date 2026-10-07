#!/usr/bin/env python3
"""Fritz Dash level compiler: ASCII maps -> level JSON.

    gen_levels.py c OUT.c MAP.txt...      built-in levels as a C file (BUILTIN_*)
    gen_levels.py lab OUT.c MAP.txt...    MODE LAB's demos (samples/mode_*.txt) as LAB_*
    gen_levels.py json OUTDIR MAP.txt...  one OUTDIR/<stem>.json per map

A map file has header lines, then one or more "map:" sections. Each section is
a block of rows, top row first; the bottom row is y = 0 (the ground). Sections
are placed left to right, bottom-aligned, so a level can be written in short
readable pieces. Lines starting with "#" before the first section are comments.

    name: BASICS
    author: FRITZ DASH
    speed: 1                 0 = 0.5x, 1 = 1x, 2 = 2x, 3 = 3x
    mode: cube               or ship, ball, ufo, wave, or a newer mode's name (robot, spider, ...)
    colors: #0a0428 #3e0a5c #140634 #00f0ff     top bottom ground line
    stars: 2                 earned by finishing the level (0 = none)
    signs: 0 5 9             words for the signs (!), left to right: SIGN_TEXTS numbers
    song: 1                  built-in song number
    egg: 1 SECRET HALL | RUN INTO THE WALL THAT LOOKS TOO CLEAN
                             easter egg 1: its name and the hint the SECRETS book
                             shows once the level is beaten (one line per egg)

Map characters:
    .        empty                  #        block
    D        dirt (the drill digs it)        Z     water (the surfer rides it)
    ^ v > <  spike up/down/right/left
    , '      half spike up/down     = _      platform top half/bottom half
    y Y      yellow pad floor/ceiling        p P   pink pad floor/ceiling
    o *      yellow orb, pink orb   G g      gravity portal up/down
    1 2 3 4  speed portal 0.5x/1x/2x/3x      C S   cube/ship portal
    B U W    ball/UFO/wave portal   n N      mini/big portal
    O @      saw, big saw           & H      pulse, pulse bars
    ~ + )    glow line, sparkle, arrow       % w   back block, grass
    $        secret coin            !        sign (words from the signs: header)
    |        end of the level
    a b c d e f h i j k l m   colour trigger: fade to palette 0..11

Lines inside a section that start with a word are directives, in that
section's cells (x from its left edge, y up from the ground). Map characters
are nearly used up, so newer objects are placed by name:
    group G X0 Y0 X1 Y1       objects in the rectangle join group G (1..8)
    move G X Y DX DY FRAMES   a move trigger at (X, Y) moving group G
    portal MODE X Y           a mode's portal by its name: portal robot 12 1
    portal shifter X Y M...   a SHIFTER portal cycling through modes M... on the
                              beat (up to 4): portal shifter 30 1 ship ufo robot
    portal mystery X Y M...   a MYSTERY portal ("?") picking one of modes M...
    obj TYPE X Y [R] [K=V...] any object by its level-file type, turned R quarter
                              turns, with fields c, g, dx, dy, f (numbers) and
                              m (mode names, comma separated):
                              obj mirror 40 0 c=1   obj morph_blocks 50 0 g=2 c=1 f=2
    fake X0 Y0 X1 Y1          FAKE WALLs filling the rectangle: they look like
                              blocks and he goes straight through
Every name must be one the game knows (the simulator checks that no object
of a built-in level or sample is skipped).

Easter eggs (docs/EGGS.md). Every egg has a number N (1..8) in its level; its
id is the level's two-digit number and N ("03:2"), so never renumber one.
Each egg needs one `egg: N NAME | HINT` header line (name up to 20 letters,
hint up to 60; capitals, digits, spaces and ! : - . % / ? , ( ) + &) and one
object with c=N:
    obj egg X Y c=N           the EGG pickup, taken on touch
    obj fake_wall X Y [c=N]   looks like a block (#), passes through; with c=N
                              egg N hides inside it (taken passing through)
    obj fake_floor X Y [R] [c=N]  looks like a platform (=), falls through
    obj egg_nojump X Y c=N dx=L   EGG ZONE: pays egg N for crossing the L
    obj egg_mini X Y c=N dx=L     cells from column X with no jump and no orb,
    obj egg_upside X Y c=N dx=L   at mini size, or upside down all the way;
    obj egg_orbs X Y c=N dx=L     egg_orbs pays on the third orb hit inside them
The simulator proves every egg can be taken on a run that finishes the level,
on the 100 ms press grid, and that no hazard sits within 2 cells behind a fake
wall (or under a fake floor).
"""
import re
import json
import sys
from pathlib import Path

LEGEND = {
    "#": ("block", 0),
    "D": ("dirt", 0), "Z": ("water", 0),
    "^": ("spike", 0), ">": ("spike", 1), "v": ("spike", 2), "<": ("spike", 3),
    ",": ("half_spike", 0), "'": ("half_spike", 2),
    "=": ("platform", 0), "_": ("platform", 2),
    "y": ("pad_yellow", 0), "Y": ("pad_yellow", 2),
    "p": ("pad_pink", 0), "P": ("pad_pink", 2),
    "o": ("orb_yellow", 0), "*": ("orb_pink", 0),
    "G": ("portal_gravity_up", 0), "g": ("portal_gravity_down", 0),
    "1": ("portal_speed_05", 0), "2": ("portal_speed_1", 0),
    "3": ("portal_speed_2", 0), "4": ("portal_speed_3", 0),
    "C": ("portal_cube", 0), "S": ("portal_ship", 0),
    "B": ("portal_ball", 0), "U": ("portal_ufo", 0), "W": ("portal_wave", 0),
    "n": ("portal_mini", 0), "N": ("portal_big", 0),
    "O": ("saw", 0), "@": ("saw_big", 0), "&": ("pulse", 0), "H": ("pulse_bars", 0),
    "~": ("deco_glow", 0), "+": ("deco_star", 0), ")": ("deco_arrow", 0),
    "%": ("deco_block", 0), "w": ("deco_spikes", 0), "$": ("coin", 0), "!": ("sign", 0),
    "|": ("end", 0),
}
COLOR_TRIGGERS = "abcdefhijklm"


def compile_map(path: Path) -> dict:
    """Parses one map file into the level JSON structure."""
    level = {"format": "fritzdash-level", "version": 1, "name": path.stem.upper(),
             "author": "", "speed": 1, "mode": "cube", "objects": []}
    sections, rows, directives, signs, eggs = [], None, [], [], []
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.rstrip()
        if line.strip() == "map:":
            rows = []
            sections.append(rows)
            directives.append([])
            continue
        if rows is not None and line.split() and line.split()[0] in ("group", "move", "portal", "obj", "fake"):
            directives[-1].append((lineno, line.split()))
            continue
        if rows is None:
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            key, _, value = line.partition(":")
            key, value = key.strip(), value.strip()
            if key in ("speed", "stars", "song"):
                level[key] = int(value)
            elif key == "signs":
                signs = [int(v) for v in value.split()]
            elif key == "colors":
                top, bottom, ground, line_col = value.split()
                level["colors"] = {"top": top, "bottom": bottom, "ground": ground, "line": line_col}
            elif key in ("name", "author", "mode"):
                level[key] = value
            elif key == "egg":
                eggs.append(egg_header(path, lineno, value))
            else:
                sys.exit(f"{path}:{lineno}: unknown header '{key}'")
        elif line:
            rows.append((lineno, line))

    x0 = 0
    for rows, dirs in zip(sections, directives):
        width = max(len(r) for _, r in rows)
        first = len(level["objects"])
        for i, (lineno, row) in enumerate(rows):
            y = len(rows) - 1 - i
            for col, ch in enumerate(row):
                if ch in ". ":
                    continue
                if ch in COLOR_TRIGGERS:
                    level["objects"].append({"t": "color_trigger", "x": x0 + col, "y": y,
                                             "c": COLOR_TRIGGERS.index(ch)})
                    continue
                if ch not in LEGEND:
                    sys.exit(f"{path}:{lineno}:{col + 1}: unknown map character '{ch}'")
                name, rot = LEGEND[ch]
                obj = {"t": name, "x": x0 + col, "y": y}
                if rot:
                    obj["r"] = rot
                level["objects"].append(obj)
        for lineno, words in dirs:
            if words[0] in ("portal", "obj"):
                level["objects"].append(named_object(path, lineno, words, x0))
                continue
            try:
                nums = [int(w) for w in words[1:]]
            except ValueError:
                sys.exit(f"{path}:{lineno}: directive numbers expected")
            if words[0] == "fake" and len(nums) == 4:
                for fx in range(nums[0], nums[2] + 1):
                    for fy in range(nums[1], nums[3] + 1):
                        level["objects"].append({"t": "fake_wall", "x": x0 + fx, "y": fy})
            elif words[0] == "group" and len(nums) == 5:
                g, gx0, gy0, gx1, gy1 = nums
                for obj in level["objects"][first:]:
                    if gx0 <= obj["x"] - x0 <= gx1 and gy0 <= obj["y"] <= gy1 and obj["t"] != "color_trigger":
                        obj["g"] = g
            elif words[0] == "move" and len(nums) == 6:
                g, mx, my, dx, dy, frames = nums
                level["objects"].append({"t": "move_trigger", "x": x0 + mx, "y": my, "g": g,
                                         "dx": dx, "dy": dy, "f": frames})
            else:
                sys.exit(f"{path}:{lineno}: bad directive")
        x0 += width
    moving = sum(1 for o in level["objects"] if o.get("g") and o["t"] not in ("move_trigger", "color_trigger", "end"))
    if moving > 512:
        sys.exit(f"{path}: {moving} objects in groups, but only 512 (MAX_MOVING) can move")
    posts = sorted((o for o in level["objects"] if o["t"] == "sign"), key=lambda o: (o["x"], o["y"]))
    if len(posts) != len(signs):
        sys.exit(f"{path}: {len(posts)} signs on the map but {len(signs)} in the signs: header")
    for post, words in zip(posts, signs):
        if words:
            post["c"] = words
    check_eggs(path, level, eggs)
    return level


EGG_TYPES = ("egg", "egg_nojump", "egg_mini", "egg_upside", "egg_orbs")
EGG_TEXT = re.compile(r"^[A-Z0-9 !:\-.%/?,()+&]*$")


def egg_header(path: Path, lineno: int, value: str) -> dict:
    """An `egg: N NAME | HINT` header line."""
    m = re.match(r"^(\d+)\s+([^|]+)\|(.+)$", value)
    if not m:
        sys.exit(f"{path}:{lineno}: egg: N NAME | HINT expected")
    n, name, hint = int(m.group(1)), m.group(2).strip().upper(), m.group(3).strip().upper()
    if not 1 <= n <= 8:
        sys.exit(f"{path}:{lineno}: egg numbers are 1..8")
    if not name or len(name) > 20 or len(hint) > 60 or not EGG_TEXT.match(name) or not EGG_TEXT.match(hint):
        sys.exit(f"{path}:{lineno}: egg name (1..20) or hint (up to 60) too long or has a letter the font lacks")
    return {"n": n, "name": name, "hint": hint}


def check_eggs(path: Path, level: dict, eggs: list) -> None:
    """Every egg number has exactly one header and one egg object or zone."""
    heads = [e["n"] for e in eggs]
    objs = [o.get("c", 0) for o in level["objects"]
            if o["t"] in EGG_TYPES or (o["t"] in ("fake_wall", "fake_floor") and o.get("c", 0))]
    if len(set(heads)) != len(heads) or sorted(heads) != sorted(objs):
        sys.exit(f"{path}: egg headers {sorted(heads)} and egg objects {sorted(objs)} must match one to one")
    for o in level["objects"]:
        if o["t"].startswith("egg_") and not 1 <= o.get("dx", 0) <= 40:
            sys.exit(f"{path}: an egg zone needs dx=1..40 (its length in cells)")
    if eggs:
        level["eggs"] = sorted(eggs, key=lambda e: e["n"])


def named_object(path: Path, lineno: int, words: list, x0: int) -> dict:
    """An object from a `portal` or `obj` directive."""
    if len(words) < 4:
        sys.exit(f"{path}:{lineno}: {words[0]} NAME X Y expected")
    name, rest = words[1], words[4:]
    try:
        obj = {"t": name, "x": x0 + int(words[2]), "y": int(words[3])}
    except ValueError:
        sys.exit(f"{path}:{lineno}: {words[0]} NAME X Y expected")
    if words[0] == "portal":
        if name in ("shifter", "mystery"):
            modes = [w for w in rest if "=" not in w]
            if not 1 <= len(modes) <= 4:
                sys.exit(f"{path}:{lineno}: a {name} portal needs 1 to 4 modes")
            obj["m"] = modes
            rest = [w for w in rest if "=" in w]
        else:
            obj["t"] = f"portal_{name}"
    elif rest and "=" not in rest[0]:
        obj["r"] = int(rest[0]) & 3
        rest = rest[1:]
    for kv in rest:
        key, _, value = kv.partition("=")
        if key == "m":
            obj["m"] = value.split(",")
        elif key in ("c", "g", "dx", "dy", "f", "r"):
            try:
                obj[key] = int(value)
            except ValueError:
                sys.exit(f"{path}:{lineno}: {key}= needs a number")
        else:
            sys.exit(f"{path}:{lineno}: unknown field '{key}'")
    return obj


def to_json(level: dict) -> str:
    """Level JSON with one object per line, so diffs and C strings stay short."""
    head = {k: v for k, v in level.items() if k not in ("objects", "eggs")}
    objs = ",\n".join("  " + json.dumps(o, separators=(",", ":")) for o in level["objects"])
    text = json.dumps(head, separators=(",", ":"))[:-1]
    if "eggs" in level:
        text += ',\n"eggs":' + json.dumps(level["eggs"], separators=(",", ":"))
    return f'{text},\n"objects":[\n{objs}\n]}}\n'


def c_string(text: str) -> str:
    out = []
    for line in text.splitlines(keepends=True):
        esc = line.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
        out.append(f'    "{esc}"')
    return "\n".join(out)


def main() -> None:
    if len(sys.argv) < 4 or sys.argv[1] not in ("c", "json", "lab"):
        sys.exit(__doc__)
    mode, out, maps = sys.argv[1], Path(sys.argv[2]), [Path(p) for p in sorted(sys.argv[3:])]
    levels = [(p.stem, to_json(compile_map(p))) for p in maps]
    if mode == "json":
        out.mkdir(parents=True, exist_ok=True)
        for stem, text in levels:
            (out / f"{stem}.json").write_text(text)
        return
    ids = ", ".join(f'"{stem}"' for stem, _ in levels)
    bodies = ",\n".join(c_string(text) for _, text in levels)
    name, src = ("LAB", "samples/mode_*.txt") if mode == "lab" else ("BUILTIN", "levels/*.txt")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        f"// Generated by tools/gen_levels.py from {src}. Do not edit.\n"
        '#include "game.h"\n\n'
        f"const int {name}_COUNT = {len(levels)};\n"
        f"const char *const {name}_IDS[] = {{ {ids} }};\n"
        f"const char *const {name}_JSON[] = {{\n{bodies},\n}};\n"
    )


if __name__ == "__main__":
    main()
