"""Fritz Dash stock icons: 16x16 pixel art as code (compiled by gen_icons.py).

Rows are top first, one letter per pixel, "." clear. A row of 8 letters is
the left half of a symmetric row (gen_icons.py mirrors it). Letters are the
PALETTE colours below, an icon's own pal={...} overrides, or "1" and "2" for
the cube's two Customize colours. At most 8 colours per icon.

unlock: "" (free), "stars N", "coins N" or "badge B_NAME" (src/badges.h).
secret=True: shown as "?" until an easter egg finds it (stock_find).
Icons are saved by their position in ICONS: add new ones at the end.
"""

SETS = ["ROBOTS", "MONSTERS", "ANIMALS", "SPOOKY", "FOOD", "SPACE", "VEHICLES", "SPORTS", "FACES", "SECRET"]
SECRET_SET = "SECRET"
REGULAR, SECRET, PET_COUNT = 80, 16, 8

PALETTE = {
    "K": "181424",  # outline
    "W": "ffffff", "w": "cdd2e1", "g": "8c91a5", "d": "4e5064",
    "R": "eb2d2d", "r": "961923",
    "O": "ff911e", "o": "be5a14",
    "Y": "ffe132", "y": "d7a014", "L": "fff5aa",
    "G": "50d746", "v": "1e8232", "l": "aaf06e",
    "B": "3773ff", "b": "1e32a0",
    "C": "46d7ff", "c": "198caf",
    "P": "ff7dc3", "p": "c83282",
    "U": "9b50eb", "u": "55288c",
    "N": "965a2d", "n": "5f371e",
    "T": "ffcda0", "t": "e1a06e",
}

ICONS = []
PETS = []


def icon(name, set_, rows, pal=None, unlock="", secret=False):
    ICONS.append(dict(name=name, set=set_, rows=rows, pal=pal, unlock=unlock, secret=secret))


def pet(name, frame1, frame2, pal=None):
    PETS.append(dict(name=name, frames=[frame1, frame2], pal=pal))


# --- ROBOTS -------------------------------------------------------------------

icon("BOLT BOT", "ROBOTS", """
.......R
.......R
.......K
..KKKKKK
..Kwwwww
..KwWCCw
KKKwCCCw
KgKwcccw
KKKwwwww
..Kwwwww
..Kwwddd
..KwwdWd
..Kwwddd
..Kggggg
..KKKKKK
........
""")

icon("ORB BOT", "ROBOTS", """
........
......KK
....KKWW
...KWWWW
..KWWWWW
..KWKKKK
.KWKKCCK
.KWKKCCK
.KWWKKKK
.KwWWWWW
.KwwWWWW
..KwwWWW
..Kgwwww
...Kggww
....KKKK
........
""")

icon("TV BOT", "ROBOTS", """
...K....
....K...
.....K..
.KKKKKKK
.Kgggggg
.Kgbbbbb
.KgbbCCb
.KgbbCCb
.Kgbbbbb
.KgbCbbb
.KgbbCCC
.Kgbbbbb
.KggggggggRgYgK.
.KKKKKKK
...KK...
........
""")

icon("BINO BOT", "ROBOTS", """
..KKKK..
.KggggK.
KgKKKKgK
KgKWCKgK
KgKCCKgK
KgKKKKgK
.KggggK.
..KKKK.K
.......K
..KKKKKK
..KYYYYY
..KYYKKK
..KYYYYY
.KKKKKKK
.Kdgdgdg
..KKKKKK
""")

icon("VISOR BOT", "ROBOTS", """
........
.....KKK
...KKggg
..Kggwww
.Kggwwww
.Kgwwwww
KKKKKKKK
KrRRRYYRRRRRRRrK
KrRRRYYRRRRRRRrK
KKKKKKKK
.Kgggggg
.Kgdgdgd
.Kgdgdgd
..Kggggg
...KKKKK
........
""")

icon("MEGA MECH", "ROBOTS", """
.Y......
.YY.....
..YY....
...YY.KR
....YYKR
..KKKKYR
.KwwwwwY
.Kwwwwww
.KbKKKKw
.KbKGGKw
.KbbKKww
.KwbbwwK
.KwwwwKR
..KwwwKR
...KKKKK
........
""")

icon("ROBO CAT", "ROBOTS", """
.K......
.KK.....
.KgK....
.KgCK...
.KgCgKKK
.Kgggggg
.KgYYYgg
.KgYKYgg
.KgYYYgg
KKgggggg
.KgggggK
KKgggKgg
.KggggKK
..Kggggg
...KKKKK
........
""")

icon("MY BOT", "ROBOTS", """
...KK...
...K2K..
....KK..
..KKKKKK
.K222222
.K2KKKK2
.K2KWWK2
.K2KWKK2
.K2KKKK2
.K222222
.K222KKK
.K2222KW
.K222222
..KKKKKK
....KK..
...KKKK.
""")

icon("GOLD BOT", "ROBOTS", """
.......Y
......YY
.......Y
..KKKKKK
..KLYYYY
..KYKKKY
KKKYKRKY
KyKYKKKY
KKKYYYYY
..KYYYYY
..KYYKKK
..KYYKLK
..KYYKKK
..Kyyyyy
..KKKKKK
........
""", unlock="badge B_STARS10")

icon("CYCLO BOT", "ROBOTS", """
........
......KK
....KKuu
...KuuUU
..KuUUUU
..KuUKKK
.KuUKRWRRRRKUuK.
.KuUKRRK
.KuUKRRK
.KuUKRRR
..KuUKKK
..KuUUUU
...KuuUU
..KKKKKK
..Kg.Kg.
..KK.KK.
""", unlock="stars 6")

# --- POCKET MONSTERS (fan art for the family console, and a few originals) ---

icon("PIKACHU", "MONSTERS", """
K.......
KK......
KKK.....
.KYK....
.KYYK...
..KYYKKK
..KYYYYY
.KYYYYYY
.KYKWYYY
.KYKKYYY
.KRRYYYY
.KRRYYKY
.KYYYYYK
..KYYYYY
...KKKYY
......KK
""")

icon("CHARMANDER", "MONSTERS", """
................
....KKKKKK......
...KOOOOOOK.....
..KOOOOOOOOK....
..KOKWOOKWOK..R.
..KOKKOOKKOK.RR.
..KOOOOOOOOK.RYR
..KOKOOOOKOK.RYR
...KOKKKKOK..KOK
..KOOLLLLOOK.KOK
.KOOLLLLLLOOKOK.
.KOOLLLLLLOOOK..
..KOOLLLLOOK....
..KOOKKKKOOK....
..KKK....KKK....
................
""")

icon("SQUIRTLE", "MONSTERS", """
........
.....KKK
...KKCCC
..KCCCCC
.KCCCCCC
.KCCKKCC
.KCKWNKC
.KCKNNKC
.KCCKKCC
.KCCCCCC
..KCCKCC
..KCCCKK
.KNKKCCC
KNLNNKKK
KNNLLNNN
.KKKKKKK
""")

icon("BULBASAUR", "MONSTERS", """
......KK
....KKGG
...KGvGG
..KGGvGG
.K.KGGvG
KXK.KKKK
KXXKXXXX
KXXXXXXX
KXKKKXXX
KXKWRRXX
KXKRRKXX
KXXKKXXx
KXXXXXXx
.KXXKXXX
..KXXKKK
...KKKKK
""", pal={"X": "5ec8a0", "x": "3c9678"})

icon("EEVEE", "MONSTERS", """
K.......
KK......
KNK.....
KnNK....
.KnNKKKK
.KNNNNNN
KNNNNNNN
KNKKNNNN
KNKWKNNN
KNKKKNNN
KNNNNNNK
.KNNNNKN
KLKNNNNK
KLLKNNNN
KLLLKKKK
.KLLLLLL
""")

icon("JIGGLYPUFF", "MONSTERS", """
......KK
.K...KPP
.KPKKPPP
.KPPPPPP
KPPPPPPP
KPPPPPPP
KPKKKKKP
KKWWCCKP
KKWCCCKP
KKCCccKP
KPKKKKKP
KPPPPPPP
KPPPPPPK
.KPPPPPP
..KKPPPP
....KKKK
""")

icon("GENGAR", "MONSTERS", """
..K....K
.KUK..KU
.KUUKKUU
KUUUUUUU
KUUUUUUU
KUURRUUU
KUURRRUU
KUUUUUUU
KUUUUUUU
KUKKKKKK
KUKWWWWW
KUUKWWWW
KUUUKKKK
KuUUUUUU
.KuuUUUU
..KKKKKK
""")

icon("SNORLAX", "MONSTERS", """
..K.....
..KK....
..KXK...
.KXXXKKK
.KXXXXXX
KXXXXXXX
KXXLLLLL
KXLLLLLL
KXLKKKLL
KXLLLLLL
KXLLLLLL
KXLLKKKK
KXLLLWLL
KXXLLLLL
.KXXXXXX
..KKKKKK
""", pal={"X": "2a6478"})

icon("POKE BALL", "MONSTERS", """
......KK
....KKRR
...KRRRR
..KRWWRR
.KRWRRRR
.KRRRRKK
KRRRRKWW
KKKKKKWW
KKKKKKWW
KwwwwKWW
.KwwwwKK
.Kwwwwww
..Kwwwww
...Kwwww
....KKww
......KK
""")

icon("DITTO", "MONSTERS", """
........
........
....KKKK
..KKXXXX
.KXXXXXX
.KXXXXXX
KXXXKXXX
KXXXKXXX
KXXXXXXX
KXXKXXXX
KXXXKKKK
KXXXXXXX
.KXXXXXX
KXXXXXXX
KxxKxxxx
.KK.KKKK
""", pal={"X": "c8a0e6", "x": "9a74c0"})

icon("SPROUTLE", "MONSTERS", """
................
...KKK....KKK...
..KGGlK..KlGGK..
..KGGGGKKGGGGK..
...KKGGvvGGKK...
.....KKvvKK.....
....KKKKKKKK....
..KKXXXXXXXXKK..
.KXXXXXXXXXXXXK.
.KXXKWXXXXWKXXK.
KXXXKKXXXXKKXXXK
KXPPXXXXXXXXPPXK
KXXXXXKXXKXXXXXK
.KXXXXXKKXXXXXK.
..KKXXXXXXXXKK..
....KKKKKKKK....
""", pal={"X": "96e6aa"})

icon("ZAPPUP", "MONSTERS", """
................
.KK..KKKKKK..KK.
KbbKKBBBYYBBKbbK
KbbKBBBYYBBBKbbK
KbbKBBBBYBBBKbbK
KbbKBBBBBBBBKbbK
KbbBBKKBBKKBBbbK
.KbBKWKBBKWKBbK.
.KbBKKKBBKKKBbK.
..KBBBBKKBBBBK..
..KBBBBKKBBBBK..
..KBBBKBBKBBBK..
...KBBBKKBBBK...
...KBBBRRBBBK...
....KKKRRKKK....
................
""", unlock="coins 3")

# --- ANIMALS ------------------------------------------------------------------

icon("CAT", "ANIMALS", """
.K......
.KK.....
.KOK....
.KOPK...
.KOOOKKK
.KOOoOoO
KOOOOOOO
KOGKGOOO
KOGKGOOO
KOOOOOOO
KOOOOOOP
KKOOOOKO
KOOOOOOK
.KOOOOOO
..KKOOOO
....KKKK
""")

icon("PUPPY", "ANIMALS", """
........
...KKKKK
.KKttttt
KNNKtttt
KNNKtttt
KNNtKKtt
KNNtKWtt
.KNttttt
.KNtttKK
..KttttK
..KtttKt
..KttttR
...KtttR
....KKKK
........
........
""")

icon("PANDA", "ANIMALS", """
........
.KKK....
KKKKKKKK
KKKWWWWW
.KWWWWWW
KWWWWWWW
KWWKKWWW
KWKWKWWW
KWKKWWWW
KWWWWWWK
KWWWWWKW
KWWWWWWK
.KWWWWWW
..KKWWWW
....KKKK
........
""")

icon("FROG", "ANIMALS", """
........
.KKKK...
KWWWWK..
KWKKWK..
KWKKWKKK
KGWWGGGG
KGGGGvGG
KGGGGGGG
KPPGGGGG
KGKGGGGG
KGGKGGGG
KGGGKKKK
KGGGGGGG
.KGGGGGG
..KKKKKK
........
""")

icon("PENGUIN", "ANIMALS", """
........
....KKKK
..KKbbbb
.Kbbbbbb
.KbbWWbb
KbbWWWWb
KbWWWWWW
KbWKWWWW
KbWKWWWW
KbWWWWWO
KbWWWWOO
KbbWWWWW
.KbbWWWW
.KbbbWWW
..KKKKKK
...OOO..
""")

icon("FOX", "ANIMALS", """
K.......
KK......
KOK.....
KOOK....
KOWOK...
KOWOOKKK
KOOOOOOO
KOOKOOOO
KOOKKOOO
KWWOOOOO
.KWWWOOO
..KWWWWW
...KWWWW
....KWWK
.....KKK
........
""")

icon("OWL", "ANIMALS", """
K.......
KNK.....
KNNKKKKK
KNNNNNNN
KNKKKKNN
KKYYYYKN
KKYKKYKN
KKYKKYKN
KKYYYYKN
KNKKKKNO
KNNNNNNO
KNTNTNTN
KNNTNTNN
.KNNNNNN
..KKKKKK
...OO...
""")

icon("BUNNY", "ANIMALS", """
...KK...
..KWWK..
..KWPK..
..KWPK..
..KWPK..
..KWWKKK
.KWWWWWW
KWWWWWWW
KWWKKWWW
KWWKKWWW
KPWWWWWP
KWWWWWKK
KWWWWWKW
.KWWWWWK
..KKKKKK
........
""")

icon("PIG", "ANIMALS", """
........
.KK.....
.KPK.KKK
.KpPKPPP
..KPPPPP
.KPPPPPP
KPPKPPPP
KPPKPPPP
KPPPKKKK
KPPPKppp
KPPPKpKp
KPPPKppp
KPPPKKKK
.KPPPPPP
..KKKKKK
........
""")

icon("LION", "ANIMALS", """
..KKK.KK
.KoOoKoO
KoOoooOo
KoooKKKK
KooKYYYY
KoKYYYYY
KoKYKKYY
KoKYKKYY
KoKYYYYY
KoKYYLLK
KoKYLLLL
KooKLLLK
KoooKKKK
.KoOoooo
..KKoOoO
....KKKK
""", unlock="stars 10")

# --- SPOOKY -------------------------------------------------------------------

icon("GHOST", "SPOOKY", """
........
.....KKK
...KKWWW
..KWWWWW
.KWWWWWW
.KWWKKWW
KWWWKKWW
KWWWKKWW
KWWWWWWW
KWWWWWWK
KWWWWWWK
KWWWWWWW
KwWWWWWW
KwwWWWWW
KWWKWWWK
.KK.KKK.
""")

icon("PUMPKIN", "SPOOKY", """
........vv......
.......vv.......
...KKKKv
..KOOOoK
.KOOOOoO
.KOOKOoO
KOOKYKoO
KOKYYYKO
KOKKKKKO
KOKKKKKK
KOKYYKYY
KOOKYYYY
KOOOKKKK
.KOOOOOO
..KKOOOO
....KKKK
""")

icon("SKULL", "SPOOKY", """
........
....KKKK
..KKWWWW
.KWWWWWW
.KWWWWWW
KWWWWWWW
KWWKKKWW
KWKKRKWW
KWKKKKWW
KWWKKWWW
.KWWWWWK
..KWWWWW
..KWKWKW
..KWKWKW
...KKKKK
........
""")

icon("BAT", "SPOOKY", """
........
........
....K...
....KK..
K...KUKK
KK.KUUUU
KuKKUURU
KuuKUUUU
KuuuKUWU
KuuuuKUU
KuKuuuKK
KK.KuKKK
K...KK..
........
........
........
""")

icon("ZOMBIE", "SPOOKY", """
................
...KKKKKKKKKK...
..KnnnnnnnnnnK..
.KnnnnnnnnnnnnK.
.KnlnnlllnnlnnK.
.KllllllllllllK.
.KlKKKllllKKKlK.
.KlKWKllllKRKlK.
.KlKKKllllKKKlK.
.KllllllllllllK.
.KlllllGGlllllK.
.KlKKKKKKKKKKlK.
.KlKlKlKlKlKKlK.
.KlKKKKKKKKKKlK.
..KllllllllllK..
...KKKKKKKKKK...
""", pal={"l": "a5d28c"})

icon("MUMMY", "SPOOKY", """
................
....KKKKKKKK....
..KKWWWWWWWWKK..
.KggggWWWWWWWWK.
.KWWWWWgggggggK.
.KWWWWWWWWWWWWK.
.KKKKKKKKKKKKKK.
.KdYYddddddYYdK.
.KKKKKKKKKKKKKK.
.KWWWWWWWWWWWWK.
.KgggggWWWWWWWK.
.KWWWWWWggggggK.
.KWWWWWWWWWWWWK.
..KggggWWWWWWK..
...KKKKKKKKKK...
................
""", unlock="stars 4")

icon("SPIDER", "SPOOKY", """
........
.....KKK
K..KKuuu
.KKuuuuu
...KRuRu
KKKuuuuu
...Kuuuu
.KKuUUuu
K..KuUUu
KKKuuuuu
...Kuuuu
.KKKuuuK
K..KKKWK
...K....
..K.....
........
""")

icon("EYEBALL", "SPOOKY", """
................
.....KKKKKK.....
...KKWWWWWWKK...
..KWWWWWWWWWRK..
.KWRWWBBBBWWRWK.
.KWWRBBBBBBWRWK.
KWWWBBKKKKBBWWWK
KWWWBKWKKKKBWWWK
KRWWBKKKKKKBWWRK
KWRWBBKKKKBBWRWK
.KWWWBBBBBBWWWK.
.KWRWWBBBBWWWWK.
..KWWWWWWWWRWK..
...KKWWWWWWKK...
.....KKKKKK.....
................
""", unlock="badge B_ATTEMPTS100")

# --- FOOD ---------------------------------------------------------------------

icon("PIZZA", "FOOD", """
........
KKKKKKKK
KNNNNNNN
KNtNNtNN
KYYRRYYY
.KYRRYYY
.KYYYYRR
..KYYYRR
..KYYYYY
...KYRRY
...KYRRY
....KYYY
....KYYY
.....KYY
......KY
.......K
""")

icon("BURGER", "FOOD", """
........
....KKKK
..KKOOOO
.KOOOLOO
.KOLOOOO
KOOOOOOO
KKKKKKKK
KGvGGvGG
KRRRRRRR
KYYYYYYY
KnnnnnnY
KnnnnnnY
KKKKKKKK
KOOOOOOO
.KOOOOOO
..KKKKKK
""")

icon("DONUT", "FOOD", """
........
.....KKK
...KKPPP
..KPYPPP
.KPPPPBP
.KPWPKKK
KPPPK...
KPGPK...
KPPPK...
KtPPPKKK
KttPPPPY
.KttPWPP
.KtttPPP
..KKtttt
....KKKK
........
""")

icon("ICE CREAM", "FOOD", """
........
......KK
....KKPP
...KPPWP
..KPPPPP
..KPPPPP
.KPPPPPP
.KPpPPpP
..KKPKKP
..KOOOOO
...KOoOo
...KoOoO
....KOoO
....KoOo
......KO
.......K
""")

icon("TACO", "FOOD", """
................
................
................
...GlGGlGGlGG...
..GlRGGnnGGRlG..
.GnnnYnnnnYnnnG.
KYYYYYYYYYYYYYYK
KYyYYYYYYYYYYyYK
.KYYYYYYYYYYYYK.
.KyYYYYYYYYYYyK.
..KYYYYYYYYYYK..
...KKyYYYYyKK...
.....KKKKKK.....
................
................
................
""")

icon("WATERMELON", "FOOD", """
........
........
........
KKKKKKKK
KGWRRRRR
KGWRKRRR
.KGWRRRK
.KGWRKRR
..KGWRRR
..KGGWWR
...KGGWW
....KKGG
......KK
........
........
........
""")

icon("CUPCAKE", "FOOD", """
........
......RR
....KKRR
...KPPPP
..KPPWPP
.KPPPPPP
.KPpPPPP
KPPPPpPP
KppppPPP
KKKKKKKK
.KcCcCcC
.KcCcCcC
..KcCcCc
..KcCcCc
...KKKKK
........
""")

icon("FRIES", "FOOD", """
.......KK.......
....KKKYyK......
...KYyKYyKKK....
.KKKYyKYyKYyKKK.
KYyKYyKYyKYyKYyK
KYyKYyKYyKYyKYyK
KYyKYyKYyKYyKYyK
KKKKKKKKKKKKKKKK
KRRRRRRRRRRRRRRK
.KRRKRRRRRRKRRK.
.KRRKRRRRRRKRRK.
.KRRRRRRRRRRRRK.
..KRRKRRRRKRRK..
..KRRRKKKKRRRK..
..KrrrrrrrrrrK..
...KKKKKKKKKK...
""", unlock="coins 6")

# --- SPACE --------------------------------------------------------------------

icon("ROCKET", "SPACE", """
.......K
......KR
.....KRR
.....KWW
....KWWW
....KWKK
....KWKC
....KWKC
....KWWK
....KWWW
...KRKWW
..KRRKWW
..KRRKKK
..KK..OY
......OY
.......O
""")

icon("SATURN", "SPACE", """
................
................
......KKKK......
....KKOOOOKK....
...KttttttttK...
..KOOOOOOOOOOK..
KKKttttttttttKKK
KLLKKKKKKKKKKLLK
KLLLLLLLLLLLLLLK
KKKOOOOOOOOOOKKK
..KttttttttttK..
...KOOOOOOOOK...
....KKttttKK....
......KKKK......
................
................
""")

icon("ALIEN", "SPACE", """
...Y....
....K...
.....KKK
...KKGGG
..KGGGGG
.KGGGGGG
.KGKKGGG
KGKWKKGG
KGKKKKKG
.KGKKKKG
.KGGKKGG
..KGGGGG
..KGGGGK
...KGGGG
....KKKK
........
""")

icon("ASTRONAUT", "SPACE", """
................
.....KKKKKK.....
...KKWWWWWWKK...
..KWWWWWWWWWWK..
.KWWKKKKKKKKWWK.
.KWKbCCbbbbbKWK.
KWWKbCbbbbbbKWWK
KWWKbbbbbbbbKWWK
KWWKbbbbbbCbKWWK
.KWWKKKKKKKKWWK.
.KWWWWWWWWWWWWK.
..KwwWWWWWWwwK..
.KKKKKKKKKKKKKK.
.KwwwRwYwBwwwwK.
.KwwwwwwwwwwwwK.
.KKKKKKKKKKKKKK.
""")

icon("UFO", "SPACE", """
........
........
......KK
.....KCC
....KCGG
....KGKG
..KKKKKK
.Kwwwwww
KgYgYgYg
.KKddddd
...KKKKK
......L.
.....L..
....L...
........
........
""")

icon("MOON", "SPACE", """
................
.....KKKKK......
...KKYYYYK......
..KYYYYYK.......
.KYYYYYK.....Y..
.KYYYYK.....YLY.
KYYKYYK......Y..
KYYYYYK.........
KYYYYYK.........
KYYYKYYK........
.KYYYKYYK...L...
.KYYYYYYYKK.....
..KYYYYYYYYKK...
...KKYYYYYYYYK..
.....KKKKKKKK...
................
""", unlock="stars 3")

icon("SUPERSTAR", "SPACE", """
.......K
......KY
......KY
.....KYY
KKKKKKYY
KYYYYYYY
.KYYYKYY
..KYYKYY
...KYYKY
...KYYYK
..KYYYYY
..KYYYKK
.KYYYK..
.KYYK...
KYKK....
KK......
""")

icon("COMET", "SPACE", """
O...............
.OO....Y........
..OOO...........
..YOOO..........
...YYOOO.....Y..
....YYYOO.......
.....YYYOOKKK...
......YYYKCCCK..
......YYKCWWCCK.
.......KCWCCCCK.
.......KCCCCCcK.
........KCCCcK..
.........KKKK...
................
...Y............
................
""", unlock="badge B_HOUR")

# --- VEHICLES -----------------------------------------------------------------

icon("RACE CAR", "VEHICLES", """
................
................
................
................
......KKK.......
.....KYCCK......
KK..KRRRRRKKKK..
KRK.KRRRRRRRRRK.
KRRKRRWWWRRRRRRK
KRRRRRWRWRRRRRRK
.KKKKKKKKKKKKKK.
..KggK....KggK..
..KgdK....KgdK..
...KK......KK...
................
................
""", unlock="badge B_RACER")

icon("MEGA TRUCK", "VEHICLES", """
................
................
......KKKKK.....
.....KCCKBBK....
..KKKBCCKBBBKK..
.KBBBBBBBBBBBBK.
.KBBYBBBBBBYBBK.
..KKKKKKKKKKKK..
...K.K....K.K...
.KKKKK....KKKKK.
KddgddK..KddgddK
KdgggdK..KdgggdK
KddgddK..KddgddK
.KKKKK....KKKKK.
................
................
""")

icon("TRAIN", "VEHICLES", """
................
..KK............
.KwwK...........
..KK...KKKKKKK..
..KK...KRRRRRK..
..KdK..KRCCCRK..
.KKdKKKKRCCCRK..
KBBBBBBBRRRRRK..
KBYBBBBBRRRRRRK.
KBBBBBBBRRRRRRK.
KKKKKKKKKKKKKKKK
.KddK.KddK.KddK.
KdgddKdgddKdgddK
.KddK.KddK.KddK.
................
................
""")

icon("HELICOPTER", "VEHICLES", """
................
.KKKKKKKKKKKKK..
.......KK.......
.....KKKKKKK....
....KCCYYYYYK...
...KCCCYYYYYYKKK
...KCCYYYYYYYYYK
...KYYYYYYYYKKKK
....KYYYYYYK..K.
.....KKKKKK.....
....K.....K.....
...KKKKKKKKK....
................
................
................
................
""")

icon("FIRE TRUCK", "VEHICLES", """
................
................
.....KK.........
.....KRK........
..KKKKKKKKKK....
..KwKwKwKwKK....
..KKKKKKKKKKKK..
.KRRRRRRRRKCCK..
.KRRRRRRRRKCCRK.
.KRWWRRRRRRRRRK.
.KRRRRRRRRRRRRK.
.KKKKKKKKKKKKKK.
..KggK....KggK..
..KgdK....KgdK..
...KK......KK...
................
""")

icon("SUBMARINE", "VEHICLES", """
................
................
.........K......
.........K......
.......KKKK.....
......KYYYYK....
..KKKKKYYYYKKK..
.KYYYYYYYYYYYYK.
KYYKCKYYKCKYYYYK
KYYKCKYYKCKYYYYK
.KYYYYYYYYYYYYK.
..KyyyyyyyyyyK..
...KKKKKKKKKK...
................
................
................
""")

icon("JET PLANE", "VEHICLES", """
.......K
......KW
......KW
......KC
.....KWC
....KWWW
..KKWWWW
.KWWWWWW
KWWWWWWW
KKKKKWWW
.....KWW
.....KWW
....KWWW
...KWWWR
...KKKKR
.......O
""")

icon("SCHOOL BUS", "VEHICLES", """
................
................
................
.KKKKKKKKKKKKKK.
KYYYYYYYYYYYYYYK
KYKKKKKKKKKKYKKK
KYKCCKCCKCCKYCCK
KYKCCKCCKCCKYCCK
KYKKKKKKKKKKYKKK
KYYYYYYYYYYYYYYK
KKKKKKKKKKKKKKKK
KYYYYYYYYYYYYYWK
.KKKKKKKKKKKKKK.
..KKKK....KKKK..
..KdgK....KdgK..
...KK......KK...
""", unlock="coins 9")

# --- SPORTS -------------------------------------------------------------------

icon("SOCCER BALL", "SPORTS", """
........
.....KKK
...KKWKK
..KWWWWK
.KWWWWWW
.KKWWWWK
KKKKWWKK
KKKWWWKK
KKWWWWWK
KWWWWWWW
KWWWKWWW
.KWKKKWW
.KWKKKWW
..KWKWWW
...KKWWW
.....KKK
""")

icon("BASKETBALL", "SPORTS", """
........
.....KKK
...KKOOK
..KOOOOK
.KKOOOOK
.KOKOOOK
KOOKOOOK
KKKKKKKK
KOOKOOOK
KOOKOOOK
.KOKOOOK
.KKOOOOK
..KOOOOK
...KKOOK
.....KKK
........
""")

icon("FOOTBALL", "SPORTS", """
........
........
........
.....KKK
...KKNNN
..KNWNNN
.KNWNNWN
KNNNWWWW
.KNWNNWN
..KNWNNN
...KKNNN
.....KKK
........
........
........
........
""")

icon("BASEBALL", "SPORTS", """
........
.....KKK
...KKWWW
..KWRWWW
.KWRWWWW
.KRWWWWW
KWRWWWWW
KWRWWWWW
KWRWWWWW
KWRWWWWW
.KRWWWWW
.KWRWWWW
..KWRWWW
...KKWWW
.....KKK
........
""")

icon("8 BALL", "SPORTS", """
................
.....KKKKKK.....
...KKgddddddKK..
..KgddWWWWdddK..
.KdddWWKKWWdddK.
.KddWWKWWKWWddK.
.KddWWKWWKWWddK.
.KddWWWKKWWWddK.
.KddWWKWWKWWddK.
.KddWWKWWKWWddK.
.KdddWWKKWWdddK.
..KdddWWWWdddK..
..KddddddddddK..
...KKddddddKK...
.....KKKKKK.....
................
""")

icon("BOXING", "SPORTS", """
................
....KKKKKK......
...KRRRRRRK.....
..KRRRWRRRRK....
.KRRRWRRRRRRK...
.KRRRRRRRRRRRK..
.KRRRRKKKRRRRK..
.KRRRRRRRKRRRK..
.KRRRRRRRRRRK...
..KRRRRRRRRRK...
...KKKKKKKKK....
...KWWWWWWWK....
...KWWWWWWWK....
...KKKKKKKKK....
................
................
""")

icon("TROPHY", "SPORTS", """
........
..KKKKKK
..KYYYYY
KKKYYYWY
KYKYYYYY
KYKYYYYY
.KKYYYYY
..KyYYYY
...KyYYY
....KKyY
.....KYY
.....KYy
....KKKK
...KyyyY
...KKKKK
........
""", unlock="badge B_ALL_LEVELS")

icon("GOLD MEDAL", "SPORTS", """
..KBBK....KRRK..
...KBBK..KRRK...
....KBBKKRRK....
.....KBBRRK.....
......KKKK......
.....KKKKKK.....
....KYYYYYYK....
...KYYYKKYYYK...
...KYYKKKYYYK...
...KYYYKKYYYK...
...KYYYKKYYYK...
...KYYKKKKYYK...
....KYYYYYYK....
.....KKKKKK.....
................
................
""", unlock="stars 15")

# --- FACES (worn on the cube's own colour) ------------------------------------

icon("NINJA", "FACES", """
................
................
RRRRRRRRRRRRRRRR
rrrrrrrrrrrrrRRr
............RrR.
KKKKKKKK
KKWWWWKK
KKWWKWKK
KKKWWWKK
KKKKKKKK
................
................
................
................
................
................
""")

icon("PIRATE", "FACES", """
RRRRRRRR
RWRRRWRR
RRRRRRRR
rrrrrrrr
................
.KKKK..........K
KKKKKK....KKK.K.
KKKKKK...KWWWK..
KKKKKK...KWKWK..
.KKKK.....KKK...
................
...KKKKKKKKKK...
....KWWYYWWK....
.....KKKKKK.....
................
................
""")

icon("SHADES", "FACES", """
................
................
................
................
KKKKKKKKKKKKKKKK
KKKKKKKKKKKKKKKK
.KCCWKKKKKCCWKK.
.KCCCKK..KCCCKK.
..KKK......KKK..
................
................
..........KK....
...KKKKKKKK.....
................
................
................
""")

icon("SILLY", "FACES", """
................
................
..KKKK....KKKK..
.KWWWWK..KWWWWK.
.KWWWKK..KKWWWK.
.KWWWKK..KKWWWK.
..KKKK....KKKK..
................
................
..K..........K..
...KKKKKKKKKK...
.....KRRRK......
.....KRrRK......
......KKK.......
................
................
""")

icon("HEART EYES", "FACES", """
................
................
.KK.KK..
KRRKRRK.
KRWRRRK.
KRRRRRK.
.KRRRK..
..KRK...
...K....
................
.K......
.KK.....
..KKKKKK
...KWWWW
....KKKK
................
""")

icon("DEMON", "FACES", """
K.......
KK......
KRK.....
.KRK....
..KKK...
................
.KK.........KK..
..KKK......KKK..
...KKK....KKK...
..KKRKK..KKRKK..
..KKKK....KKKK..
................
.KKKKKKKKKKKKKK.
.KWKWKWKKWKWKWK.
..KKKKKKKKKKKK..
................
""", unlock="badge B_STAR_STORM")

icon("SLEEPY", "FACES", """
..........KKKK..
............K...
...........K....
..........KKKK..
................
................
................
.KKKKK....KKKKK.
..KKK......KKK..
................
................
................
.....KKKKKK.....
.....KCCCCK.....
......KKKK......
................
""")

icon("GENTLEMAN", "FACES", """
....KKKKKKKK....
....KKKKKKKK....
....KKKKKKKK....
....KRRRRRRK....
..KKKKKKKKKKKK..
................
..........YYYY..
...KK....Y.KK.Y.
...KK....Y.KK.Y.
..........YYYY..
............Y...
.KK..KKKKKK..KY.
KK.KKKKKKKKKK.KK
K..KKKK..KKKK..K
................
................
""")

# --- SECRET (found in easter eggs) --------------------------------------------

icon("KING", "SECRET", """
KK.....K
KYK...KY
KYYK.KYY
KYYYKYYY
KYRYYYBY
KYYYYYYY
KKKKKKKK
........
..KKK...
..KWK...
..KKK...
........
.K......
..KKKKKK
........
........
""", secret=True)

icon("RAINBOW", "SECRET", """
KKKKKKKKKKKKKKKK
KRRRRRRRRRRRRRRK
KRRRRRRRRRRRRRRK
KOOOOOOOOOOOOOOK
KOOOKKOOOOKKOOOK
KYYYKKYYYYKKYYYK
KYYYKKYYYYKKYYYK
KGGGGGGGGGGGGGGK
KGGKGGGGGGGGKGGK
KCCCKCCCCCCKCCCK
KCCCCKKKKKKCCCCK
KBBBBBBBBBBBBBBK
KBBBBBBBBBBBBBBK
KUUUUUUUUUUUUUUK
KUUUUUUUUUUUUUUK
KKKKKKKKKKKKKKKK
""", secret=True)

icon("DIAMOND", "SECRET", """
........
........
...KKKKK
..KWWCCC
.KWCCCCC
KKKKKKKK
.KCCcCCC
..KCcCCC
...KcCCC
....KcCC
.....KcC
......Kc
.......K
........
........
........
""", secret=True)

icon("MEW", "SECRET", """
........
.K......
.KK.....
.KXK....
.KXXKKKK
.KXXXXXX
KXXXXXXX
KXXKKXXX
KXKWBKXX
KXKBBKXX
KXXKKXXX
KXXXXXXX
.KXXXXXK
..KXXXXX
...KKKKK
........
""", pal={"X": "ffafd7"}, secret=True)

icon("DRAGON", "SECRET", """
K.......
KyK.....
.KyK....
.KyyKKKK
..KRRRRR
.KRRRRRR
KRKKKRRR
KRRYKKRR
KRRYYKRR
KRRRRRRR
KRRROOOO
.KROKOOO
.KRROOOO
..KWKWKW
...KKKKK
........
""", secret=True)

icon("UNICORN", "SECRET", """
.......Y
......KY
..K...Ky
..KK..KY
..KWKKKK
.PKWWWWW
PPKWWWWW
PUKWKKWW
PUKWKWWW
UPKWWWWW
PUKWWWWW
.PKWTTTT
..KTKTTT
...KTTTT
....KKKK
........
""", secret=True)

icon("T-REX", "SECRET", """
................
....KKKKKKKK....
...KGGGGGGGGK...
..KGGGKKGGGGGK..
..KGGGKWGGGGGGK.
.KGGGGGGGGGGGGGK
.KGGGGGGGGGGGKGK
.KGGGGGGKKKKKKKK
KGGGGGGKWKWKWKW.
KGGGGGGK........
KGGGGGGKWKWKWK..
KGGGGGGGKKKKKKK.
KGGGGGGGGGGGGK..
.KGGGGGGGGGKK...
..KvvKKvvK......
................
""", secret=True)

icon("INVADER", "SECRET", """
........
........
...K....
...KGK..
....KGKK
...KGGGG
..KGGKKG
.KGGGKKG
KGGGGGGG
KGKGGGGG
KGKGKKKK
KKKGK...
....KGGK
....KKK.
........
........
""", secret=True)

icon("CHOMPER", "SECRET", """
................
.....KKKKK......
...KKYYYYYKK....
..KYYYYYYYYYK...
.KYYYYYKKYYK....
.KYYYYYKKYK.....
KYYYYYYYYK......
KYYYYYYYK...WW.W
KYYYYYYYK...WW.W
KYYYYYYYYK......
.KYYYYYYYYK.....
.KYYYYYYYYYK....
..KYYYYYYYYYK...
...KKYYYYYKK....
.....KKKKK......
................
""", secret=True)

icon("GALAXY", "SECRET", """
................
.....KKKKKK.....
...KKuuuuuuKK...
..KuuUUUUUuuuK..
.KuuUuuuuuUuWuK.
.KuUuuPPPuuUuuK.
KuuUuPuuuPuUuuuK
KuUuPuWWuPuUuuuK
KuUuPuWWuPuUuuuK
KuuUuPuuuuPuuuuK
KuuuUuPPPPuuuuuK
.KuWuUuuuuuuUuK.
.KuuuuUUUUUUuuK.
..KuuuuuuuuuuK..
...KKuuuuuuKK...
.....KKKKKK.....
""", secret=True)

icon("FIRE SKULL", "SECRET", """
..C.....
.CC...C.
.CBC.CBC
CBBCCBBC
CBWBBBWB
KKKWWWWW
.KWWWWWW
KWWWWWWW
KWKKKKWW
KWKBCKWW
KWKKKKWW
.KWWWWWK
..KWKWKW
..KWKWKW
...KKKKK
........
""", secret=True)

icon("LAVA BLOB", "SECRET", """
........
........
........
.....KKK
...KKOOO
..KOOYYO
.KOOYOOO
.KOOOOOO
KOOKKOOO
KOOKWOOO
KOOOOOOO
KROOOKKK
KRROOOOO
KRRRRRRR
KRKRRKRR
.K.KK.KK
""", secret=True)

icon("DEMON KING", "SECRET", """
K.......
KwK.....
KwwK....
.KwwKKKK
.KRRRRRR
KRRRRRRR
KRKKKRRR
KRRYKKRR
KRRYYKRR
KRRRRRRR
KRKKKKKK
KRKWKKKK
KRRKKKKK
.KRRRRRR
..KKRRRR
....KKKK
""", secret=True)

icon("FRITZ", "SECRET", """
KKKKKKKKKKKKKKKK
KYYYYYYYYYYYYYYK
KYKKKKKKKKKKKKYK
KYK..........KYK
KYK.KKKKKKK..KYK
KYK.KWWWWWK..KYK
KYK.KWKKKKK..KYK
KYK.KWK......KYK
KYK.KWKKKK...KYK
KYK.KWWWWK.Y.KYK
KYK.KWKKKKYYYKYK
KYK.KWK....Y.KYK
KYK.KKK......KYK
KYKKKKKKKKKKKKYK
KYYYYYYYYYYYYYYK
KKKKKKKKKKKKKKKK
""", secret=True)

icon("OMEGA BOT", "SECRET", """
......KK
.....KYY
..KKKKYY
.KUUUKKK
.KUUUUUU
KUUKKKKK
KUKCCCCC
KUKCWCCC
KUUKKKKK
KUUUUUUU
.KUUyyyy
.KUyKyKy
.KUUyyyy
..KUUUUU
...KKKKK
........
""", secret=True)

icon("GLITCH", "SECRET", """
................
..KKKKKKKKKKK...
..KCCCCCCCCCK...
...KPPPPPPPPPK..
..KCCKKCCCKKCK..
.KCCCKKCCCKKCK..
..KWWWWWWWWWWK..
....KPPPPPPPPPK.
..KCCCCCCCCCCK..
..KCKCCCCCCKCK..
.KCCCKKKKKKCK...
..KPPPPPPPPPPK..
...KCCCCCCCCCK..
..KKKKKKKKKKK...
................
................
""", secret=True)

# --- PETS (two frames each; the easter eggs make them follow the cube) --------

pet("ROBO BUDDY", """
................
.......R
.......K
....KKKK
...Kwwww
...KwCCw
...KwCCw
...Kwwww
...KwKKK
...Kwwww
....KKKK
....Kg..
....KK..
................
................
................
""", """
................
.......Y
.......K
....KKKK
...Kwwww
...KwCCw
...KwCCw
...Kwwww
...KwKKK
...Kwwww
....KKKK
....Kg..
....KK..
................
................
................
""")

pet("BIRDY", """
................
................
................
.....KKKK.......
....KYYYYK......
...KYYKYYYKK....
...KYYYYYKOOK...
..KKKYYYYYKK....
.KYYYKYYYYK.....
..KKKYYYYYK.....
....KYYYYK......
.....KKKK.......
......K.K.......
................
................
................
""", """
................
................
................
.....KKKK.......
....KYYYYK......
...KYYKYYYKK....
...KYYYYYKOOK...
...KYYYYYYKK....
..KKKYYYYYK.....
.KYYYKYYYK......
..KKKKYYK.......
.....KKKK.......
......K.K.......
................
................
................
""")

pet("BOO", """
................
................
.....KKK
....KWWW
...KWWWW
...KWKWW
...KWKWW
...KWWWW
...KWWWW
...KWWWW
...KWKWW
...KK.KK
................
................
................
................
""", """
................
................
................
.....KKK
....KWWW
...KWWWW
...KWKWW
...KWKWW
...KWWWW
...KWWWW
...KWWWW
...KWWKW
....KK.K
................
................
................
""")

pet("DRAGGY", """
................
..K.........K...
..KK.......KK...
..KGK.KKK.KGK...
...KGKGGGKGK....
....KGGGGGK.....
...KGKGGGKGK....
...KGWKGKWGK....
...KGGGGGGGK....
....KGGOOGK.....
....KGGGGGK.....
.....KKKKK......
.....K...K......
................
................
................
""", """
................
................
................
......KKK.......
.....KGGGK......
....KGGGGGK.....
.KKKGKGGGKGKKK..
KGGGKWKGKWKGGGK.
.KKKGGGGGGGKKK..
....KGGOOGK.....
....KGGGGGK.....
.....KKKKK......
.....K...K......
................
................
................
""")

pet("MINI UFO", """
................
................
................
......KK
.....KCC
.....KCC
...KKKKK
..Kggggg
..KgYgYg
...KKKKK
................
................
................
................
................
................
""", """
................
................
................
......KK
.....KCC
.....KCC
...KKKKK
..Kggggg
..KgRgRg
...KKKKK
......L.
.....L..
................
................
................
................
""")

pet("KITTY", """
................
................
................
....K....K......
....KK..KK......
....KOKKOK......
...KOOOOOOK.....
...KOKOOKOK.....
...KOOKKOOK.....
....KOOOOK......
...KOOLLOOK.....
...KOOLLOOK.KK..
...KOOOOOOK.KOK.
...KOKOOKOKKOK..
...KKKKKKKKKK...
................
""", """
................
................
................
....K....K......
....KK..KK......
....KOKKOK......
...KOOOOOOK.KK..
...KOKOOKOK.KOK.
...KOOKKOOK..KOK
....KOOOOK...KOK
...KOOLLOOK..KOK
...KOOLLOOK.KOK.
...KOOOOOOKKOK..
...KOKOOKOKKK...
...KKKKKKKKK....
................
""")

pet("SLIMEY", """
................
................
................
................
......KK
....KKGG
...KGlGG
..KGGGGG
..KGKGGG
..KGKGGG
..KGGGGG
...KKKKK
................
................
................
................
""", """
................
................
................
................
................
................
....KKKK
..KKGlGG
.KGGGGGG
.KGGKGGG
.KGGGGGG
..KKKKKK
................
................
................
................
""")

pet("STARRY", """
................
.......K
......KY
.....KYY
..KKKKYY
..KYYYYY
...KYKYY
....KYYY
....KYYY
...KYYKK
...KYK..
...KK...
................
................
................
................
""", """
..W.............
.......K......W.
......KY
.....KYY
..KKKKYY
..KYYYYY
...KYKYY
....KYYY
....KYYY
...KYYKK
...KYK..
...KK...
................
W...............
..........W.....
................
""")
