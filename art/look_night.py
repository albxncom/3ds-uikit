"""Nachtglas's images (art.py --look night; the look's table: look_night.c, its numbers: look_night.h): a dark look
made for the Shard icons. The ground is drawn by code (look_night.c: a deep gradient with a few slow lights), so
nothing here is opaque: every surface is smoked glass the ground shows through, with a thin light along the edges that
face the light (the upper left, as the glass icons') and a faint highlight inside them. What can be pressed is cut
crystal: a facet round its edge, and a light behind it in the colour of its Shard icon (the ring of art.py's
GLASS_ICONS: bring in teal, the house blue, storage violet, remove coral, badges amber), which fills the crystal while
it is held. Loaders and the installing screen's marks fill with light.

art.py calls in here through its look hooks (art.py, LOOK): layout (the colours: text in white and ice on every ground,
the meanings' colours lifted for a dark ground, see palette; and look_night.h's numbers), colours (the ground's lights,
its own colours), sprites (the common list with every surface redrawn under its own name, so ui.c draws them as before,
and the look's own sprites after them, OWN_SPRITES) and sheet (the glass sheet with the carousel's frames turned). The
colour schemes turn the ground's hue (grounds), and every tone of the look's own turns with it: the night blue of the
glass, its light, the selection, the loader, the scroll bar, the carousel's frames; see own. The tones that say what
something does keep their colour on every ground (GLOW, the inks of meaning). So each scheme has an atlas of its own.

Python 3 standard library only; the same input gives the same bytes."""
import math

NAME = "night"
A = None            # art.py's module (set by art.py before anything here is drawn)
# The look's own sprites, after the common ones (ui_art.h: ART_NG_*).
GLOWS = ("TEAL", "VIOLET", "CORAL", "AMBER", "LIGHT", "DEEP")      # LIGHT, DEEP: the design's own, with the ground
TONES = ("BLUE", "TEAL", "CORAL")       # a popup button's light: goes back, goes on, removes (each then its _DOWN)
NAMES = tuple("NG_GLOW_" + c + d for c in GLOWS for d in ("", "_DOWN")) + (
    "NG_RIM_R0", "NG_RIM_R10", "NG_RIM_R16", "NG_LINE", "NG_TAB", "NG_TAB_DOWN") + tuple(
    "NG_DLG_" + c + d for c in TONES for d in ("", "_DOWN")) + ("NG_LOADER_BODY", "NG_LOADER_LIGHT", "NG_LOADER_FRONT")
OWN_SPRITES = NAMES
# The ground's lights (grounds: its two lights and the small third one), read by look_night.c as SC(COL_NG_LAMP_*)
OWN_COLOURS = ("COL_NG_LAMP_A", "COL_NG_LAMP_B", "COL_NG_LAMP_C")

LIGHT = (-0.5, -0.866)          # towards the light: the upper left, as the glass icons'
WHITE = (255.0, 255.0, 255.0)
BLACK = (0.0, 0.0, 0.0)
ICE = (226.0, 236.0, 255.0)     # light as glass gives it back: a cool white
SMOKE = (10.0, 10.0, 26.0)      # smoked glass: nearly black, a breath of the night's blue
# what a crystal is lit with, by its icon's place in the Shard ring: the ring's tones as light
GLOW = {"TEAL": (58.0, 232.0, 206.0), "BLUE": (96.0, 150.0, 255.0), "VIOLET": (164.0, 124.0, 255.0),
        "CORAL": (255.0, 116.0, 96.0), "AMBER": (255.0, 188.0, 64.0)}
# The inks that stand on the bare ground: light enough for 4.5 to 1 even where two of its lights lie over each other
INK, MUTED, TEAL_INK, ORANGE_INK, ALERT_INK = 0xF6F8FF, 0xD2DAF5, 0x70EEDB, 0xFFBA7C, 0xFFB5AD
PAD = (6, 4, 6, 10)             # how far a pane's shadow and a crystal's light reach past the element

# The eight grounds, by colour scheme (schemes.py's order): per ground the hues (OKLCH degrees) at the top screen's
# top and at the bottom screen's foot, the chroma and the lightness l = Y ** (1/3) at both, and the hues of its two
# lights. The first is the design's own: violet into blue, the start word's colours.
GROUNDS = (
    dict(name="WEISS", hue=(302.0, 266.0), chroma=(0.115, 0.125), light=(0.300, 0.262), lamps=(318.0, 255.0)),
    dict(name="GELB", hue=(72.0, 40.0), chroma=(0.074, 0.088), light=(0.300, 0.255), lamps=(80.0, 36.0)),
    dict(name="SAND", hue=(58.0, 18.0), chroma=(0.028, 0.036), light=(0.305, 0.258), lamps=(70.0, 355.0)),
    dict(name="MINZE", hue=(172.0, 232.0), chroma=(0.075, 0.095), light=(0.300, 0.255), lamps=(160.0, 245.0)),
    dict(name="HIMMEL", hue=(236.0, 276.0), chroma=(0.100, 0.120), light=(0.305, 0.258), lamps=(222.0, 290.0)),
    dict(name="FLIEDER", hue=(326.0, 292.0), chroma=(0.115, 0.125), light=(0.305, 0.260), lamps=(340.0, 280.0)),
    dict(name="ROSE", hue=(8.0, 328.0), chroma=(0.105, 0.110), light=(0.300, 0.255), lamps=(22.0, 310.0)),
    dict(name="NACHT", hue=(272.0, 250.0), chroma=(0.030, 0.045), light=(0.225, 0.190), lamps=(285.0, 235.0)),
)
LAMP_CHROMA, LAMP_LIGHT = 0.19, 0.50        # a light's own colour; ui.c lays it over the ground at NG_LAMP_PCT


def grounds(schemes):
    """Per scheme six colours as numbers: the ground at the top screen's top, where the two screens meet and at the
    bottom screen's foot, then its two lights and the tone between them (the third, smaller light)."""
    def number(c):
        return (c[0] << 16) | (c[1] << 8) | c[2]
    rows = []
    for g in GROUNDS:
        (h0, h1), (c0, c1), (l0, l1) = g["hue"], g["chroma"], g["light"]
        turn = (h1 - h0 + 180.0) % 360.0 - 180.0        # the short way round
        mid = schemes.colour_at((h0 + turn / 2) % 360.0, (c0 + c1) / 2, ((l0 + l1) / 2) ** 3)
        a, b = g["lamps"]
        between = (a + ((b - a + 180.0) % 360.0 - 180.0) / 2) % 360.0
        rows.append(tuple(number(c) for c in (
            schemes.colour_at(h0, c0, l0 ** 3), mid, schemes.colour_at(h1, c1, l1 ** 3),
            schemes.colour_at(a, LAMP_CHROMA * g["chroma"][0] / 0.115, LAMP_LIGHT ** 3),
            schemes.colour_at(b, LAMP_CHROMA * g["chroma"][1] / 0.125, LAMP_LIGHT ** 3),
            schemes.colour_at(between, LAMP_CHROMA * g["chroma"][0] / 0.115, (LAMP_LIGHT * 1.06) ** 3))))
    return rows


def turning(index):
    """How the design's own tones follow "Farbe" onto ground index: the turn of the hue in degrees and the factor of
    the chroma, from the first ground (the design's own, violet into blue) to this one, at the bottom screen's foot,
    where most of the interface stands."""
    first, g = GROUNDS[0], GROUNDS[index]
    return (g["hue"][1] - first["hue"][1] + 180.0) % 360.0 - 180.0, g["chroma"][1] / first["chroma"][1]


def turned(schemes, c, index):
    """A tone of the design's own as drawn for the first ground, on ground index: its hue turned and its chroma
    scaled as the ground's, its luminance kept, so every contrast stays what it is on the first ground."""
    by, k = turning(index)
    if not by and k == 1.0:
        return c
    _, chroma, hue = schemes.oklch(c)
    return tuple(float(v) for v in schemes.colour_at((hue + by) % 360.0, chroma * k, schemes.luminance(c)))


def own(c):
    """turned() for the scheme art.py draws (each scheme's atlas is drawn by a process of its own)."""
    return turned(A.schemes, c, A.SCHEME)


def night(look):
    """A look for glass() written with the first ground's tones (body and tint), on the scheme art.py draws."""
    out = dict(look)
    for key in ("top", "low", "tint"):
        if out.get(key):
            out[key] = (own(out[key][0]),) + tuple(out[key][1:])
    return out


def palette(schemes, index, cols, tray_floor):
    """Every COL_* of the design: the night scheme's values (so whatever the kit still draws by a colour comes out
    dark), the ground's own colours, and over them the design's text colours, turned with the ground (luminance kept,
    so their contrast is the same on every ground), and its meaning colours, the same on all grounds."""
    night_scheme = [k for k, s in enumerate(schemes.SCHEMES) if s["name"] == "NACHT"][0]
    out = schemes.palette(night_scheme, cols, tray_floor)
    top, mid, low = grounds(schemes)[index][:3]

    def tone(v):
        c = turned(schemes, ((v >> 16) & 255, (v >> 8) & 255, v & 255), index)
        return (int(c[0]) << 16) | (int(c[1]) << 8) | int(c[2])
    out.update(
        COL_TOP_BG_A=top, COL_TOP_BG_B=mid, COL_BOT_BG_A=mid, COL_BOT_BG_B=low, COL_BG=mid,
        COL_INK=tone(INK), COL_MUTED=tone(MUTED), COL_TILE_INK=tone(0xF0F3FF), COL_PRESS_INK=0xFFFFFF,
        COL_OFF_INK=tone(0x98A2C4), COL_DOTS=tone(0x8C98C8), COL_DLG_INK=tone(0xF6F8FF), COL_DLG_LABEL_LIGHT=0x000000,
        COL_LABEL_HELD_SHADOW=0x000000, COL_TEAL=TEAL_INK, COL_ORANGE=ORANGE_INK, COL_ALERT=ALERT_INK,
        COL_FOLDER=0xF7B955, COL_GREEN=0x3FDCC2, COL_SYMBOL_LIGHT=0x000000, COL_PATTERN_LIGHT=0xFFFFFF,
        COL_PATTERN_DARK=tone(0xB9C8FF), COL_CARD=tone(0x15142B))
    return out


def layout(index):
    """What scheme index is drawn from (art.py's layout hook): layout.h and look_night.h's numbers, the look's
    colours (palette), and the pattern fainter than the other looks' on the dark ground."""
    out = {n: v for n, v in vars(A.D).items() if not n.startswith("__")}
    A.read_layout(A.HEADERS / "look_night.h", out)
    out.update(palette(A.schemes, index, {n: v for n, v in out.items() if n.startswith("COL_")}, A.D.TRAY_FLOOR))
    loader = loader_tone(A.schemes, index, out["NG_LAMP_PCT"] / 100.0)
    # the installing screen's liquid, balls and drops are light, as the loader's (in the look's material)
    out.update(PATTERN_ALPHA=9, COL_LOADER=loader, COL_POUR=loader)
    return type("Layout", (), out)


def loader_tone(schemes, index, lamp):
    """The scheme's loader tone (schemes.py) on the look's dark ground: its hue and chroma, as light as it takes
    for LOADER_MIN against the ground's three colours, also under one of its lights at its strongest (lamp)."""
    def three(v):
        return ((v >> 16) & 255, (v >> 8) & 255, v & 255)
    c = three(schemes.SCHEMES[index]["loader"])
    _, chroma, hue = schemes.oklch(c)
    row = grounds(schemes)[index]
    behind = [three(v) for v in row[:3]]
    behind += [tuple(u + (w - u) * lamp for u, w in zip(g, three(v))) for g in list(behind) for v in row[3:]]
    y = schemes.luminance(c)
    while min(schemes.contrast(c, g) for g in behind) < schemes.LOADER_MIN and y < 1.0:
        y = min(1.0, y * 1.03 + 0.001)
        c = schemes.colour_at(hue, chroma, y)
    return (int(c[0]) << 16) | (int(c[1]) << 8) | int(c[2])


def colours():
    """The ground's lights in the scheme art.py draws (look_night.c lays them over its gradient)."""
    a, b, c = grounds(A.schemes)[A.SCHEME][3:]
    return dict(COL_NG_LAMP_A=a, COL_NG_LAMP_B=b, COL_NG_LAMP_C=c)


# ---------------------------------------------------------------- glass


def facing(sd, x, y):
    """How far the edge nearest to x, y faces the light: 1 towards it, 0 away from it."""
    gx, gy = sd(x + 0.5, y) - sd(x - 0.5, y), sd(x, y + 0.5) - sd(x, y - 0.5)
    n = math.hypot(gx, gy) or 1.0
    return max(0.0, min(1.0, 0.5 + 0.575 * (gx * LIGHT[0] + gy * LIGHT[1]) / n))


def glass(img, sd, y0, y1, top=(SMOKE, 0.40), low=(SMOKE, 0.54), rim=(0.10, 0.66), shine=0.15, shade=0.16,
          shadow=(0.34, 8.0, 3.0), facet=0.0, turned=False, tint=None):
    """One piece of glass with the outline sd (a signed distance, negative inside) between the rows y0 and y1: what
    it casts (alpha, blur, drop), its body from top to low (each a colour and how solid), a light one pixel wide
    along its edge (rim: its alpha away from the light and towards it), inside it a highlight where the edge faces
    the light (shine) and a shade where it faces away (shade). facet px: a cut round the edge, lighter towards the
    light and darker away from it, with a fine line where it meets the face: crystal. turned: lit from below, as
    something pressed in. tint: (colour, alpha at the top, alpha at the foot), a light inside the glass."""
    cover, soft, lerp = A.cover, A.soft, A.lerp
    if shadow:
        a, blur, down = shadow
        img.paint(lambda x, y: (a * soft(sd(x, y - down), blur) * (1.0 - cover(sd(x, y))), BLACK))

    def body(x, y):
        c = cover(sd(x, y))
        if c <= 0.0:
            return None
        t = min(1.0, max(0.0, (y - y0) / (y1 - y0)))
        return (c * (top[1] + (low[1] - top[1]) * t), lerp(top[0], low[0], t))
    img.paint(body)
    if tint:
        img.paint(lambda x, y: (cover(sd(x, y)) * (tint[1] + (tint[2] - tint[1]) *
                                                    min(1.0, max(0.0, (y - y0) / (y1 - y0)))), tint[0]))

    def lights(x, y):
        d = sd(x, y)
        c = cover(d)
        if c <= 0.0 or d < -14.0:
            return None
        f = facing(sd, x, y)
        f = 1.0 - f if turned else f
        deep = max(0.0, -d - 1.0)
        white = shine * f * math.exp(-deep / 2.6)
        dark = shade * (1.0 - f) * math.exp(-deep / 3.2)
        if facet and -d < facet + 1.5:
            cut = cover(-d - 1.0 - facet) if -d > 1.0 else 0.0      # 1 on the facet, 0 on the face
            white += 0.17 * f * cut
            dark += 0.20 * (1.0 - f) * cut
            white += (0.05 + 0.13 * f) * max(0.0, 1.0 - abs(-d - 1.0 - facet))     # where the cut meets the face
        if white >= dark:
            return (c * (white - dark), WHITE)
        return (c * (dark - white), BLACK)
    img.paint(lights)

    def edge(x, y):
        d = sd(x, y)
        ring = cover(d) - cover(d + 1.0)
        if ring <= 0.0:
            return None
        f = facing(sd, x, y)
        f = 1.0 - f if turned else f
        return (ring * (rim[0] + (rim[1] - rim[0]) * f ** 1.5), ice)
    ice = own(ICE)
    img.paint(edge)


def pane(name, r, kind, pad=PAD, **look):
    """A surface of any size (both_ways) or any width (kind: a height) with corners r."""
    def draw(img, box):
        b = A.rounded(box, r)
        glass(img, b.sd, b.y0, b.y1, **turned_look)
    turned_look = night(look)
    most = max(r) if isinstance(r, tuple) else r
    if kind == "both":
        return A.both_ways(name, most + 6, pad, draw)
    return A.across(name, kind, most, pad, draw, 8)


HELP_GLOW = (0.34, 4.0, 1.0)    # the help button's: alpha, blur, drop (a pane's: 0.34, 8, 3)
CRYSTAL = dict(top=((54.0, 60.0, 112.0), 0.46), low=((14.0, 15.0, 40.0), 0.60), facet=3.0, rim=(0.14, 0.78),
               shine=0.10, shade=0.12)
HELD = dict(top=((20.0, 22.0, 52.0), 0.60), low=((60.0, 68.0, 124.0), 0.50), facet=3.0, rim=(0.12, 0.60),
            shine=0.10, shade=0.14, turned=True, shadow=None)
ROW = dict(top=((40.0, 44.0, 86.0), 0.44), low=(SMOKE, 0.56), rim=(0.10, 0.62), shadow=(0.28, 6.0, 2.0))
ROW_HELD = dict(top=((16.0, 18.0, 46.0), 0.60), low=((70.0, 86.0, 150.0), 0.52), rim=(0.14, 0.66), turned=True,
                shadow=None, tint=(GLOW["BLUE"], 0.04, 0.26))
FLAT = dict(top=(SMOKE, 0.22), low=(SMOKE, 0.26), rim=(0.09, 0.20), shine=0.0, shade=0.0, shadow=None)
# stretched both ways, so its body is one tone from top to foot
CARD = dict(top=(SMOKE, 0.47), low=(SMOKE, 0.47), rim=(0.09, 0.56), shine=0.13, shade=0.14, shadow=(0.30, 8.0, 3.0))


def crystal(name, h, r, held=False, tint=None):
    look = night(HELD if held else CRYSTAL)
    if tint:
        look["tint"] = tint

    def draw(img, box):
        b = A.rounded(box, r)
        if held:
            b = b.inset(1, 1, 1, 1)
        glass(img, b.sd, b.y0, b.y1, **look)
    return A.across(name, h, r, PAD, draw, 8)


def light(img, b, colour, held, r, well=(0.55, 16.0), fill=(0.50, 20.0, 0.13)):
    """The light behind the crystal b: it wells up from the lower edge (well: how strong, over how many px), runs
    along that edge as a bright line and spills out under the crystal. Held, it fills the crystal (fill: as well,
    and how strong everywhere)."""
    def inside(x, y):
        c = A.cover(b.sd(x, y))
        if c <= 0.0:
            return None
        up = b.y1 - y
        a = fill[0] * math.exp(-up / fill[1]) + fill[2] if held else well[0] * math.exp(-up / well[1])
        return (c * min(1.0, a), colour)
    img.paint(inside)

    def line(x, y):
        d = b.sd(x, y)
        ring = A.cover(d) - A.cover(d + 1.2)
        if ring <= 0.0:
            return None
        f = 1.0 - facing(b.sd, x, y)
        return (ring * min(1.0, 1.25 * f ** 2.2), A.lerp(colour, WHITE, 0.45))
    img.paint(line)

    def spill(x, y):
        d = b.sd(x, y)
        if d <= -0.5 or y < b.y1 - r:
            return None
        gx, gy = b.sd(x + 0.5, y) - b.sd(x - 0.5, y), b.sd(x, y + 0.5) - b.sd(x, y - 0.5)
        f = max(0.0, gy / (math.hypot(gx, gy) or 1.0))
        out = max(0.0, d)
        return ((0.60 if held else 0.48) * f ** 1.5 * math.exp(-out / 4.0) * max(0.0, 1.0 - out / (PAD[3] - 1.0)) *
                (1.0 - A.cover(d)), colour)
    img.paint(spill)


def glow(name, colour, held):
    """The light behind a crystal as an image of its own, laid over the crystal's glass (a tile's corners, any size
    from 66 px high)."""
    r, foot = A.L.TILE_R, 40
    cap = r + 6

    def draw(img, box):
        b = A.rounded(box, r)
        light(img, b.inset(1, 1, 1, 1) if held else b, colour, held, r)
    return A.sliced(name, 2 * cap + A.MID, r + 2 + A.MID + foot, PAD,
                    (PAD[0] + cap, PAD[1] + r + 2, cap + PAD[2], foot + PAD[3]), draw)


def go(name, held):
    """The button that goes on: a crystal of dark teal glass, the teal light behind it stronger than a tile's."""
    L = A.L
    r = L.GO_R
    look = dict(HELD if held else CRYSTAL)
    if held:
        look.update(top=((4.0, 30.0, 40.0), 0.70), low=((14.0, 84.0, 88.0), 0.66))
    else:
        look.update(top=((16.0, 70.0, 80.0), 0.62), low=((4.0, 22.0, 34.0), 0.72), rim=(0.16, 0.86))

    def draw(img, box):
        b = A.rounded(box, r)
        if held:
            b = b.inset(1, 1, 1, 1)
        glass(img, b.sd, b.y0, b.y1, **look)
        # held, the fill stays under the label's 4.5 to 1 (white on teal)
        light(img, b, GLOW["TEAL"], held, r, well=(0.80, 24.0), fill=(0.58, 28.0, 0.09))
    return A.across(name, L.GO_H, r, PAD, draw, 8)


def bar(name, x0, x1, w, rx, ry, held, colour):
    """The bar or a tab (art.py bar: the same outline, so it is touched where it shows): smoked glass with the light
    along its upper edge, and at the screen's edge under it the light of what its key does."""
    L = A.L
    pad = L.BAR_SHADOW
    img = A.Image(w, L.BAR_H + pad)
    shape = A.BarShape(x0, pad, x1, pad + L.BAR_H, rx, ry)
    look = dict(top=((34.0, 37.0, 80.0), 0.95), low=((12.0, 13.0, 34.0), 0.97), rim=(0.16, 0.74), shine=0.12,
                shade=0.0, shadow=(0.40, 7.0, 0.0))
    if held:
        look.update(top=((14.0, 16.0, 42.0), 0.96), low=((38.0, 46.0, 96.0), 0.96), rim=(0.14, 0.50), shine=0.0)
    glass(img, shape.sd, pad, pad + L.BAR_H, **night(look))
    foot = pad + L.BAR_H

    def under(x, y):
        c = A.cover(shape.sd(x, y))
        if c <= 0.0:
            return None
        a = (0.80 if held else 0.34) * math.exp(-(foot - y) / (15.0 if held else 5.0))
        # the tab's light gathers under its label and thins out towards its ends
        mid, half = (max(x0, 0) + min(x1, w)) / 2.0, (min(x1, w) - max(x0, 0)) / 2.0
        return (c * a * (1.0 - 0.75 * min(1.0, abs(x - mid) / half) ** 2), colour)
    img.paint(under)
    return A.Sprite(name, img, (0, pad, 0, 0))


def keycap(name, letter, colour, held):
    """A key of the bar: a small disc of crystal with its letter in white, its rim in the colour of what it does."""
    L = A.L
    n, up = L.KEYCAP, L.LABEL_SHADOW_DY
    img = A.Image(n, n + up)
    s = n / 24.0
    disc = A.Box(0.5, up + 0.5, n - 0.5, up + n - 0.5, (n - 1) / 2.0)
    smoke = own(SMOKE)
    img.paint(lambda x, y: (A.cover(disc.sd(x, y)) * (0.55 if held else 0.34), colour if held else smoke))
    ring = A.coverage(img, A.shape_lines(A.KEY_RING), s, 1.7 * s, 0, up)
    mark = A.coverage(img, A.lines_of(A.KEY_LETTERS[letter]), s, 2.5 * s, 0, up)
    for k in range(n * (n + up)):
        if ring[k] > 0:
            img.over(k, A.lerp(colour, WHITE, 0.25 if not held else 0.7), ring[k])
        if mark[k] > 0:
            img.over(k, WHITE, mark[k])
    return A.Sprite(name, img, (0, up, 0, 0))


def start_key(name, held, empty=False):
    L = A.L

    def draw(img, box):
        if empty:
            return
        b = A.rounded(box, L.START_R)
        inner = b.inset(1.3, 1.3, 1.3, 1.3)
        img.paint(lambda x, y: (A.cover(b.sd(x, y)) * (0.50 if held else 0.30), blue if held else own(SMOKE)))
        img.paint(lambda x, y: (max(0.0, A.cover(b.sd(x, y)) - A.cover(inner.sd(x, y))),
                                A.lerp(blue, WHITE, 0.7 if held else 0.35)))
    blue = own(GLOW["BLUE"])
    return A.across(name, L.START_H, L.START_R, A.NO_PAD, draw)


def corner(name, state, tab=False):
    """The corner button (art.py corner: only its lower right corner is round, it stands in the screen's corner);
    tab: the settings button in that corner, at its own size (CORNER_W x NG_TAB_H), nothing of it stretched. Its
    shadow falls straight under the glass, so it is as wide along the free edge below as along the one at the right;
    the stretched one's fixed ends add up to no more than its smallest width with the shadow (CORNER_W + 6), or the
    shadow would lie twice where they overlap."""
    L = A.L
    r = L.CORNER_R
    look = dict(FLAT if state == "off" else HELD if state == "down" else CRYSTAL)
    look.update(facet=0.0)
    if state == "up":       # as the tabs: nearly solid, rows scroll under it
        look.update(top=((40.0, 44.0, 92.0), 0.95), low=((14.0, 15.0, 40.0), 0.97), shadow=(0.40, 6.0, 0.0))
    elif state == "down":
        look.update(top=((14.0, 16.0, 42.0), 0.96), low=((44.0, 52.0, 104.0), 0.96), shadow=None,
                    tint=(GLOW["VIOLET"], 0.08, 0.50))
    pad = A.NO_PAD if state == "off" else (0, 0, 6, 6)

    def draw(img, box):
        # the outline goes on past the screen's edges, so no light runs along them
        b = A.Box(box.x0 - 20, box.y0 - 20, box.x1, box.y1, (0, 0, r, 0))
        glass(img, b.sd, box.y0, box.y1, **turned_look)
    turned_look = night(look)
    if tab:
        return A.sliced(name, L.CORNER_W, L.NG_TAB_H, pad, (0, 0, 0, 0), draw)
    return A.across(name, L.CORNER_H, r, pad, draw, 6)


def disc_button(name, held):
    """The help button: a disc of crystal. Its glow below shorter and tighter than a pane's (HELP_GLOW), so that on
    "Datei loeschen?" rows stay clear between it and the file's box (help_box_gap, as in the other looks)."""
    L = A.L
    n = L.HELP_FACE
    img = A.Image(n + PAD[0] + PAD[2], n + PAD[1] + PAD[3])
    b = A.Box(PAD[0], PAD[1], PAD[0] + n, PAD[1] + n, n / 2.0)
    look = dict(HELD if held else dict(CRYSTAL, shadow=HELP_GLOW))
    look.update(facet=2.0)
    if held:
        b = b.inset(1, 1, 1, 1)
        look["tint"] = (GLOW["VIOLET"], 0.06, 0.34)
    glass(img, b.sd, b.y0, b.y1, **night(look))
    return A.Sprite(name, img, PAD)


def track():
    L = A.L

    def draw(img, box):
        b = A.rounded(box.inset(2, 0, 2, 0), L.SCROLL_R - 2)
        glass(img, b.sd, b.y0, b.y1, top=(smoke, 0.50), low=(smoke, 0.50), rim=(0.20, 0.10), shine=0.0,
              shade=0.0, shadow=None)
    smoke = own(SMOKE)
    return A.upright("TRACK", L.SCROLL_W, L.SCROLL_R, A.NO_PAD, draw)


def thumb(name, held):
    L = A.L

    def draw(img, box):
        b = A.rounded(box.inset(1, 0, 1, 0), L.SCROLL_R - 1)
        c = A.lerp(blue, WHITE, 0.55 if held else 0.30)
        img.paint(lambda x, y: ((0.50 if held else 0.26) * A.soft(b.sd(x, y), 5.0) * (1.0 - A.cover(b.sd(x, y))),
                                blue))
        glass(img, b.sd, b.y0, b.y1, top=(c, 0.95), low=(A.lerp(c, blue, 0.5), 0.90), rim=(0.3, 0.9),
              shine=0.2, shade=0.1, shadow=None)
    blue = own(GLOW["BLUE"])
    return A.upright(name, L.SCROLL_W, L.SCROLL_R, (4, 4, 4, 4), draw)


def orb(name, size, colour, alpha):
    """A soft light, strongest in its middle."""
    img = A.Image(size, size)
    half = size / 2.0

    def at(x, y):
        q = math.hypot(x - half, y - half) / half
        return (alpha * max(0.0, (math.exp(-3.2 * q * q) - math.exp(-3.2)) / (1.0 - math.exp(-3.2))), colour)
    img.paint(at)
    return A.Sprite(name, img)


# The loader (look_night.c loader; clear glass): a nearly clear tube of glass seen from the
# front as a ring, LOADER_BEAD_R round the middle and 2 * LOADER_HW wide, lit from above. Three images LOADER_BOX
# square with LOADER_PAD round it, drawn in this order: the body (the light shade beneath on the ground, and the
# glass's faint tint, the ground showing through it), the light (a small bright core with a wide soft glow and its
# tail, the head at the top; turned about the middle by the clock) and the front (the fine light edges where the glass
# turns away and the highlight along the top), which lies over the light, so the light is in the glass. Each layer is
# shaded at 4 x 4 points per pixel (supersampled); the tones are the look's own: smoked glass (SMOKE), its light
# (ICE), the scheme's loader tone (COL_LOADER), each turned to the scheme (own).
LOADER_HW = 4.5                 # half the tube's width
LOADER_PAD = 6                  # the shade reaches this far below the box (cut there), the rest stays inside it
LOADER_SHADE = (2.6, 1.7, 0.28)     # the shade: how far down, its softness (sigma, px), how dark
LOADER_ARC = 150.0              # the light's tail behind its head, degrees
LOADER_PALE = 0.20              # the light seen in the glass: the loader tone this far towards the glass's light


def loader_polar(x, y):
    """At x, y from the ring's middle: (u across the tube, -1 inner edge to 1 outer; the radial's y; v, 0 at the top
    to 1 at the foot), or None outside the tube."""
    r = A.L.LOADER_BEAD_R
    rho = math.hypot(x, y)
    if abs(rho - r) >= LOADER_HW or rho == 0.0:
        return None
    return (rho - r) / LOADER_HW, y / rho, (y / (r + LOADER_HW) + 1.0) / 2.0


def loader_image(name, *layers):
    """An image of the loader from shade(x, y) per layer, x, y from the ring's middle, each laid over the last."""
    L = A.L
    n = 2 * L.LOADER_BEAD_R + L.LOADER_BEAD + 2 * LOADER_PAD
    img, c = A.Image(n, n), n / 2.0
    for shade in layers:
        supersampled(img, lambda x, y: shade(x - c, y - c))
    return A.Sprite(name, img, (LOADER_PAD,) * 4)


def loader_body():
    r, (drop, sigma, dark) = A.L.LOADER_BEAD_R, LOADER_SHADE
    smoke, ice = own(SMOKE), own(ICE)

    def shade(x, y):
        """The tube's shape moved down by drop and blurred: the part of a Gaussian of sigma inside it across."""
        rho = math.hypot(x, y - drop)
        k = 0.5 * (math.erfc((r - LOADER_HW - rho) / (sigma * A.SQRT2)) - math.erfc((r + LOADER_HW - rho) /
                                                                                       (sigma * A.SQRT2)))
        return (dark * k, smoke) if k > 0.002 else None

    def tint(x, y):
        """A little dark, more at the foot and in the dark band inside each edge, where the glass turns away; and a
        faint veil of light in the upper body."""
        p = loader_polar(x, y)
        if not p:
            return None
        u, _, v = p
        nz = math.sqrt(1.0 - u * u)
        a = min(1.0, 0.10 * (0.55 + 0.9 * v) + 0.30 * (1.0 - nz) ** 1.5 * (0.5 + 0.5 * v))
        w = min(1.0, 0.17 * nz * nz * (1.0 - v) ** 1.5)
        return (a + (1.0 - a) * w, A.lerp(smoke, ice, w))
    return loader_image("NG_LOADER_BODY", shade, tint)


def loader_light():
    """The light, the loader tone seen through the glass (LOADER_PALE), its head at the top: a round soft head white
    in its middle, the glass lit round it, and the tail fading by the square over LOADER_ARC degrees behind it
    (counter-clockwise), with a soft middle across the tube and a little light caught in its walls; nothing of it
    outside the tube."""
    r, tone = A.L.LOADER_BEAD_R, A.lerp(A.rgb(A.L.COL_LOADER), own(ICE), LOADER_PALE)

    def shade(x, y):
        p = loader_polar(x, y)
        if not p:
            return None
        u = p[0]
        th = math.degrees(math.atan2(x, -y))        # clockwise from the top
        along = math.exp(-(th / 8.0) ** 2) if th > 0.0 else (1.0 + th / LOADER_ARC) ** 2 if th > -LOADER_ARC else 0.0
        d = math.hypot(x, y + r)                    # from the head's middle
        walls = 0.25 * abs(u) ** 5
        w = max(along * (0.80 * math.exp(-(u / 0.5) ** 2) + 0.40 * math.exp(-u * u) + walls),
                math.exp(-(d / 3.0) ** 2), 0.40 * math.exp(-(d / 7.5) ** 2) * (1.0 + 2.0 * walls))
        if w <= 0.004:
            return None
        return (w, A.lerp(tone, A.lerp(tone, WHITE, 0.60 * math.exp(-(d / 2.25) ** 2)), min(1.0, w)))
    return loader_image("NG_LOADER_LIGHT", shade)


def loader_front():
    """Over the light: the fine light edges where the glass turns away, stronger where they face up and at the top,
    and the highlight along the upper arc (faint along the lower)."""
    ice = own(ICE)

    def shade(x, y):
        p = loader_polar(x, y)
        if not p:
            return None
        u, ry, v = p
        nz = math.sqrt(1.0 - u * u)
        up = -ry * u                                # how far the glass faces up here
        w = (1.0 - nz) ** 2.4 * (0.42 + 0.58 * max(0.0, up)) * (1.0 - 0.40 * v)
        w += 0.95 * max(0.0, 0.434 * up + 0.901 * nz) ** 34 * (1.0 if ry < 0 else 0.18)
        return (w, ice) if w > 0.004 else None
    return loader_image("NG_LOADER_FRONT", shade)


def tube(name, h, pad, cap, r, light=None, front=0):
    """A groove of dark glass any width (the bar and the steps' line), or the light in it: from violet into blue,
    brightest at its front."""
    w = cap[0] + cap[1] + A.MID
    img = A.Image(w + pad[0] + pad[2], h + pad[1] + pad[3])
    b = A.Box(pad[0], pad[1], pad[0] + w, pad[1] + h, r)
    if not light:
        glass(img, b.sd, b.y0, b.y1, **night(dict(top=((4.0, 4.0, 14.0), 0.66), low=((24.0, 26.0, 56.0), 0.52),
              rim=(0.06, 0.36), shine=0.0, shade=0.22, shadow=None, turned=True)))
    else:
        a, c = light
        inner = b.inset(1, 1, 1, 1)
        if pad[1]:
            img.paint(lambda x, y: (0.42 * A.soft(inner.sd(x, y), 4.0) * (1.0 - A.cover(inner.sd(x, y))), a))

        def body(x, y):
            cv = A.cover(inner.sd(x, y))
            if cv <= 0.0:
                return None
            t = (y - inner.y0) / (inner.y1 - inner.y0)
            col = A.lerp(A.lerp(a, WHITE, 0.50), a, min(1.0, t * 1.6))      # a bright upper edge
            head = max(0.0, 1.0 - (inner.x1 - x) / max(1.0, front)) if front else 0.0
            return (cv, A.lerp(col, A.lerp(c, WHITE, 0.6), head))
        img.paint(body)
    return A.Sprite(name, img, pad, (pad[0] + cap[0], 0, cap[1] + pad[2], 0))


def stripes():
    """One period of the glints that run through the bar's light: a soft band at 45 degrees over the bar's whole height
    (art.py stripe_ends cuts it by the fill's outline)."""
    L = A.L
    w, h = L.STRIPE_W, L.PROGRESS_H
    img = A.Image(w, h)

    def at(x, y):
        u = ((x + y) % w) / w
        return (0.30 * max(0.0, math.cos(math.pi * (u - 0.5) * 2.0)) ** 2, WHITE)
    img.paint(at)
    return A.Sprite("STRIPES", img)


def dialog():
    """The popup: a pane of darker glass, solid enough to read on over the dimmed screen; the button band below is
    part of it (dialog_band), so its lower corners are the band's."""
    L = A.L
    r, low = L.DLG_R, L.DLG_BTN_R_10 / 10.0
    pad = (L.DLG_SHADOW_BLUR,) * 4
    reach = 24
    w, h = 2 * reach + A.MID, reach + A.MID + int(math.ceil(low)) + 2
    img = A.Image(w + 2 * pad[0], h + 2 * pad[1])
    b = A.Box(pad[0], pad[1], pad[0] + w, pad[1] + h, (r, r, low, low))
    glass(img, b.sd, b.y0, b.y1, **night(dict(top=((22.0, 23.0, 54.0), 0.965), low=((22.0, 23.0, 54.0), 0.965),
          rim=(0.14, 0.72), shine=0.12, shade=0.10, shadow=(0.50, 8.0, 2.0))))
    caps = (pad[0] + reach, pad[1] + reach, reach + pad[2], int(math.ceil(low)) + 2 + pad[3])
    return A.Sprite("DLG", img, pad, caps)


def dialog_band(name, held):
    """The popup's buttons: a band across the panel's foot, a fine light line above it; held: the light under it."""
    L = A.L
    h, r = L.DLG_BTN_H + 1, L.DLG_BTN_R_10 / 10.0
    end = int(math.ceil(r)) + 8
    w = 2 * end + A.MID
    img = A.Image(w, h)
    b = A.Box(0, 1, w, h, (0, 0, r, r))

    def at(x, y):
        c = A.cover(b.sd(x, y))
        if c <= 0.0:
            return None
        d = y - 1.0
        if d < 1.0:
            return (c * 0.34, ice)
        t = d / (h - 1.0)
        return (c * ((0.12 if held else 0.07) + 0.05 * t), ice)
    ice = own(ICE)
    img.paint(at)
    return A.Sprite(name, img, A.NO_PAD, (end, 0, end, 0))


def dialog_light(name, colour, held):
    """The light under a popup button, in the band's outline (ui.c draws the part under the button): it wells up
    from the panel's lower edge and runs along it as a line; held, it fills the button."""
    L = A.L
    h, r = L.DLG_BTN_H + 1, L.DLG_BTN_R_10 / 10.0
    end = int(math.ceil(r)) + 8
    w = 2 * end + A.MID
    img = A.Image(w, h)
    b = A.Box(0, 1, w, h, (0, 0, r, r))

    def at(x, y):
        c = A.cover(b.sd(x, y))
        if c <= 0.0 or y < 2.0:
            return None
        up = h - y
        return (c * ((0.46 * math.exp(-up / 18.0) + 0.12) if held else 0.52 * math.exp(-up / 11.0)), colour)
    img.paint(at)

    def line(x, y):
        d = b.sd(x, y)
        ring = A.cover(d) - A.cover(d + 1.2)
        if ring <= 0.0 or y < h - r:
            return None
        gx, gy = b.sd(x + 0.5, y) - b.sd(x - 0.5, y), b.sd(x, y + 0.5) - b.sd(x, y - 0.5)
        return (ring * max(0.0, gy / (math.hypot(gx, gy) or 1.0)) ** 1.5, A.lerp(colour, WHITE, 0.45))
    img.paint(line)
    return A.Sprite(name, img, A.NO_PAD, (end, 0, end, 0))


def dialog_divider(name):
    """Between the two buttons: a fine line of light that fades towards the band's foot (the held half's light is
    the band's own, so the three are one image)."""
    L = A.L
    img = A.Image(40, L.DLG_BTN_H)
    ice = own(ICE)
    img.paint(lambda x, y: (0.30 * (1.0 - y / L.DLG_BTN_H) ** 0.7 if 20.0 < x < 21.0 and y > 1 else 0.0, ice))
    return A.Sprite(name, img)


def rim(name, r):
    """The selection: a line of light round an element with corners r, a little outside it, with its glow. The
    three radii's images are alike but for their corners, so ui.c takes each corner from the image of its radius
    and the straight parts from any (NG_RIM_*)."""
    L = A.L
    out, cap, pad = L.NG_RIM_OUT, L.NG_RIM_CAP, L.NG_RIM_PAD

    def draw(img, box):
        b = A.Box(box.x0, box.y0, box.x1, box.y1, r).grown(out) if r else A.Box(
            box.x0 - out, box.y0 - out, box.x1 + out, box.y1 + out, 1.5)
        def at(x, y):
            d = abs(b.sd(x, y) + 1.0)           # from the middle of a line 2 px wide
            return (min(1.0, 0.50 * math.exp(-max(0.0, d - 1.0) / 2.0)), blue)
        img.paint(at)
        img.paint(lambda x, y: (max(0.0, min(1.0, 1.5 - abs(b.sd(x, y) + 1.0))), A.lerp(blue, WHITE, 0.78)))
    blue = own(GLOW["BLUE"])
    side = 2 * cap + A.MID
    return A.sliced(name, side, side, (pad,) * 4, (pad + cap, pad + cap, cap + pad, cap + pad), draw)


def line():
    """The line across the top screen where the other looks draw their dots: a hair of light that fades out at both ends."""
    L = A.L
    cap, up = L.NG_LINE_FADE, 2

    def draw(img, box):
        def at(x, y):
            along = min(1.0, min(x - box.x0, box.x1 - x) / cap)
            across_ = abs(y - (box.y0 + 0.5))
            return ((0.58 if across_ < 0.5 else 0.13 if across_ < 1.5 else 0.04) * along * along * (3 - 2 * along), ice)
        img.paint(at)
    ice = own(ICE)
    return A.sliced("NG_LINE", 2 * cap + A.MID, 1, (0, up, 0, up), (cap, 0, cap, 0), draw)


# ---------------------------------------------------------------- the sprites


def sheet(names):
    """The scheme's sheet (art.py's sheet hook): the Shard sheet."""
    made = {s.name: s for s in A.glass_sheet("shard")}
    return [made[name] for name in names]


def sprites(base, index):
    """The common sprites (base) with every surface redrawn in glass under its own name, and the
    look's own after them (art.py's sprites hook; index: the scheme, which A.L is drawn in). What is not named here
    keeps its drawing in the look's colours (symbols, glyphs, the Shard glass)."""
    L = A.L
    r, row = L.TILE_R, L.ROW_R
    w, edge, rx, ry = L.TAB_W + L.BAR_SHADOW, L.TAB_EDGE_10 / 10.0, L.TAB_RX, L.TAB_RY
    left, right = (-2 * rx, edge), (w - edge, w + 2 * rx)
    blue, teal = GLOW["BLUE"], GLOW["TEAL"]                 # back, go on: the same on every ground
    hue_light, hue_deep = own(GLOW["BLUE"]), own(GLOW["VIOLET"])  # the design's own light, with the ground
    # the bar's light: violet into blue turned about their middle to the scheme's bar tone (art.py bar_turned)
    bar_light = tuple(A.bar_turned(c, A.lerp(hue_deep, hue_light, 0.5)) for c in (hue_deep, hue_light))
    mine = [
        crystal("TILE", L.TILE_H, r), crystal("TILE_DOWN", L.TILE_H, r, True),
        crystal("DTILE", L.DTILE_H, r), crystal("DTILE_DOWN", L.DTILE_H, r, True),
        crystal("WIDE", L.WIDE_H, r), crystal("WIDE_DOWN", L.WIDE_H, r, True),
        pane("TILE_OFF", r, L.TILE_H, A.NO_PAD, **FLAT),
        crystal("CHOICE", L.CHOICE_H, r), crystal("CHOICE_DOWN", L.CHOICE_H, r, True),
        pane("ROW", row, L.ROW_H, **ROW), pane("ROW_DOWN", row, L.ROW_H, **ROW_HELD),
        pane("ROW_OFF", row, L.ROW_H, A.NO_PAD, **FLAT),
        pane("FOLDER_ROW", row, L.FOLDER_H, **ROW), pane("FOLDER_ROW_DOWN", row, L.FOLDER_H, **ROW_HELD),
        pane("SET_ROW", row, L.SET_H, **ROW), pane("SET_ROW_DOWN", row, L.SET_H, **ROW_HELD),
        go("GO", False), go("GO_DOWN", True),
        crystal("ALT", L.GO_H, L.GO_R), crystal("ALT_DOWN", L.GO_H, L.GO_R, True),
        bar("BAR", L.BAR_X, L.BOTTOM_W - L.BAR_X, L.BOTTOM_W, L.BAR_RX, None, False, blue),
        bar("BAR_DOWN", L.BAR_X, L.BOTTOM_W - L.BAR_X, L.BOTTOM_W, L.BAR_RX, None, True, blue),
        bar("TAB_L", *left, w, rx, ry, False, blue), bar("TAB_R", *right, w, rx, ry, False, teal),
        bar("TAB_L_DOWN", *left, w, rx, ry, True, blue), bar("TAB_R_DOWN", *right, w, rx, ry, True, teal),
        start_key("START", False), start_key("START_DOWN", True),
        start_key("START_SHADOW", False, True), start_key("START_DOWN_SHADOW", True, True),
        keycap("KEY_A", "A", teal, False), keycap("KEY_B", "B", blue, False),
        keycap("KEY_A_DOWN", "A", teal, True), keycap("KEY_B_DOWN", "B", blue, True),
        keycap("KEY_Y", "Y", hue_deep, False), keycap("KEY_Y_DOWN", "Y", hue_deep, True),      # preview, corner
        corner("CORNER", "up"), corner("CORNER_DOWN", "down"), corner("CORNER_OFF", "off"),
        pane("CARD", L.CARD_R, "both", **CARD),
        stripes(), track(), thumb("THUMB", False), thumb("THUMB_DOWN", True),
        orb("DROP_SUN", L.DROP_SUN, A.lerp(hue_deep, WHITE, 0.35), 0.20),
        disc_button("HELP", False), disc_button("HELP_DOWN", True),
        dialog(), dialog_band("DLG_BUTTON", False), dialog_band("DLG_BUTTON_DOWN", True),
        dialog_divider("DLG_DIVIDER"), dialog_divider("DLG_DIVIDER_LEFT_DOWN"),
        dialog_divider("DLG_DIVIDER_RIGHT_DOWN"),
        tube("PROGRESS_SHARD", L.PROGRESS_H, (3, 2, 3, 5), (14, 14), L.PROGRESS_R),
        tube("PROGRESS_FILL_SHARD", L.PROGRESS_H, (0, 0, 0, 0), (14, 14), L.PROGRESS_R, bar_light, 12),
        pane("CARD_GLASS_SHARD", L.CARD_R, "both", **dict(CARD, rim=(0.12, 0.70), shine=0.16)),
    ]
    tone = dict(GLOW, LIGHT=hue_light, DEEP=hue_deep)
    extra = [glow("NG_GLOW_" + c + d, tone[c], bool(d)) for c in GLOWS for d in ("", "_DOWN")] + [
        rim("NG_RIM_R0", 0), rim("NG_RIM_R10", row), rim("NG_RIM_R16", r), line(),
        corner("NG_TAB", "up", True), corner("NG_TAB_DOWN", "down", True),
        ] + [
        dialog_light("NG_DLG_" + c + d, GLOW[c], bool(d)) for c in TONES for d in ("", "_DOWN")] + [
        loader_body(), loader_light(), loader_front()]
    for s in base:          # Stock's loader beads: Nachtglas draws its own loader (NG_LOADER_*)
        if s.name in ("LOADER_BEAD", "LOADER_BEAD_ON"):
            mine.append(A.Sprite(s.name, A.Image(s.img.w, s.img.h), s.pad, s.cap))
    redrawn = {s.name: s for s in mine}
    missing = set(redrawn) - {s.name for s in base}
    if missing or tuple(s.name for s in extra) != NAMES:
        raise SystemExit("look_night.py: names that are not the atlas's: %s" % sorted(missing))
    return [redrawn.get(s.name, s) for s in base] + extra


def supersampled(img, at, n=4):
    """img.paint with at(x, y) (alpha, colour) averaged over n x n points in each pixel: graded forms a few px wide."""
    steps = [-0.5 + (i + 0.5) / n for i in range(n)]

    def pixel(u, v):
        a = r = g = b = 0.0
        for dy in steps:
            for dx in steps:
                got = at(u + dx, v + dy)
                if got and got[0] > 0.0:
                    w, c = min(got[0], 1.0), got[1]
                    a, r, g, b = a + w, r + w * c[0], g + w * c[1], b + w * c[2]
        return (a / (n * n), (r / a, g / a, b / a)) if a > 0.0 else None
    img.paint(pixel)
