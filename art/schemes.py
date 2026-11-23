"""The colour schemes: every COL_* value of a scheme, derived from the yellow original in ui_layout.h and popup_band.h
by one rule and a few seeds per scheme. art.py draws each scheme's images from the result and writes the colours the
application draws by code as a table (ui_art.h, ART_COLOUR).

The rule, per kind of colour (the kind is decided by name, below):
  fixed     copied: what says what a thing is or does (the install green, the selection's blue, the grey bar with its
            white label, the glass styles' own colours), and white and black used as light and shade on them
  surface   the grounds and the held look, two families with their own seeds: the hue turns to the family's hue (a
            colour keeps its distance to its family's own hue, at most HUE_SPAN degrees), the chroma is scaled, the
            lightness follows the scheme's tone curve (none: every surface keeps its luminance, and with it every
            contrast between two surfaces)
  ink       text and lines: the luminance that keeps the original's contrast on the grounds the ink is drawn on
            (INK_GROUNDS); hue and chroma turn with the ground family. An ink that has more contrast than it needs
            is not moved
  meaning   inks that carry meaning (teal symbols, orange and red warnings, the folder): hue and chroma stay, the
            luminance follows the ink rule, so on every light scheme they stay exactly as they are
Notation: Y is the WCAG relative luminance, l = Y ** (1/3) the lightness, (C, h) chroma and hue in OKLCH (OKLab by
Bjorn Ottosson, https://bottosson.github.io/posts/oklab/, its matrices are in the public domain).

Python 3 standard library only; the same input gives the same values."""
import math

# The schemes in the settings' order; the first is the standard. Seeds:
# ground: the hue (OKLCH degrees) the yellow family turns to, its chroma factor
# held:   the held look's hue, chroma factor and lightness factor (below 1: darker, to carry the colour); the
#         scheme's own pressed tone, near its ground's hue (Weiss a cool light grey, Nacht a lighter night blue)
# tone:   None, or the lightnesses the ground and the upper button face go to, (g', f'): one power curve through
#         both, for a dark scheme; with a third value a the curve bends at the ground: l' = g' * (l / g) ** a below
#         it (a above 1 deepens shades, rims and shadows: a ground lighter than the original's)
# loader: the loader's lit dot (COL_LOADER), in the scheme's own hue, at least LOADER_MIN against the bottom screen
# pour:   the installing screen's balls, drops and liquid (COL_POUR): the loader's hue, darkened until it has at
#         least POUR_MIN against the top screen's card and the glass box (glass()), as the loader's tone alone has
#         not on every scheme (Gelb 2.84, Minze 2.99 against the card); Nacht's loader tone has it already
SCHEMES = (
    dict(name="WEISS", ground=(255.0, 0.12), held=(255.0, 0.15, 0.88), tone=(1.0, 1.0, 1.6), loader=0x5E7A99,
         pour=0x5B7795),
    dict(name="GELB", loader=0xA86E00,     # the yellow original as it is: the rule does not reproduce it (HUE_SPAN)
         pour=0x976200),
    dict(name="SAND", ground=(82.0, 0.36), held=(96.0, 0.92, 0.98), tone=None, loader=0x9A6B2E, pour=0x926326),
    dict(name="MINZE", ground=(162.0, 0.58), held=(160.0, 0.70, 0.96), tone=None, loader=0x3F8458, pour=0x357A4F),
    dict(name="HIMMEL", ground=(232.0, 0.75), held=(236.0, 0.85, 0.90), tone=None, loader=0x1F7AB8, pour=0x1272B0),
    dict(name="FLIEDER", ground=(302.0, 0.70), held=(300.0, 0.80, 0.91), tone=None, loader=0x7A5FC0, pour=0x775CBC),
    dict(name="ROSE", ground=(8.0, 0.65), held=(6.0, 0.75, 0.92), tone=None, loader=0xB4507A, pour=0xAE4B75),
    dict(name="NACHT", ground=(258.0, 0.28), held=(258.0, 0.60, 1.50), tone=(0.265, 0.345), loader=0x7AA9D6,
         pour=0x7AA9D6),
)
LOADER_MIN = 3.0        # WCAG 2 contrast of a graphic that carries meaning
POUR_MIN = 3.4          # the pour tone's, with room for the glass in front of the liquid (white, 16 %)

FIXED_NAMES = {
    # the green of "install", "progress" and "done"
    "COL_GREEN", "COL_GO_TOP", "COL_GO_LOW", "COL_GO_SHADE", "COL_GO_EDGE_TOP", "COL_GO_EDGE_LOW",
    "COL_TUBE_FILL_TOP", "COL_TUBE_FILL_LOW",
    # the selection's blue corners
    "COL_SELECT_IN", "COL_SELECT_OUT",
    # violet accents and the glass symbols' own colours (the glass style is a setting of its own)
    "COL_BADGE_TOP", "COL_BADGE_LOW", "COL_SWATCH_A", "COL_SWATCH_A_SHADE", "COL_SWATCH_B", "COL_SWATCH_B_SHADE",
    "COL_SWATCH_C", "COL_SWATCH_C_SHADE", "COL_SEAL", "COL_SEAL_SHADE",
    # the grey bar with its white engraved label: the same chrome everywhere
    "COL_BAR_TOP", "COL_BAR_MID", "COL_BAR_LOW", "COL_BAR_SHADOW", "COL_BAR_LIGHT", "COL_LABEL", "COL_LABEL_SHADOW",
    # white and black used as light and shade on fixed colours, the dim behind a popup, the camera's place
    "COL_WHITE", "COL_STRIPE", "COL_DIM", "COL_DLG_SHADOW", "COL_DLG_HELD_INK", "COL_CAMERA",
}
FIXED_PREFIXES = ("COL_GLASS_", "COL_SHARD_", "COL_PLATIN_")
MEANING = {"COL_TEAL", "COL_ORANGE", "COL_FOLDER", "COL_ALERT"}
HELD_PREFIXES = ("COL_PRESS_", "COL_BAR_PRESS_", "COL_THUMB_PRESS_")
# The grounds each ink is drawn on (read from ui.c and the application's screens). The first is the one whose contrast the ink
# keeps exactly; on the others it keeps the original's or BODY_MIN, whichever is lower. TRAY: the start screen's tray
# floor as art.py mixes it. An ink drawn on a new ground gets that ground here.
INK_GROUNDS = {
    "COL_INK": ("COL_TOP_BG_A", "COL_CARD", "COL_BOT_BG_B"),
    "COL_MUTED": ("COL_TOP_BG_A", "COL_CARD", "COL_BOT_BG_B", "COL_FACE_TOP", "COL_FACE_LOW", "COL_OFF"),
    "COL_TILE_INK": ("COL_FACE_TOP", "COL_FACE_LOW", "COL_OFF"),
    "COL_OFF_INK": ("COL_OFF",),
    "COL_PRESS_INK": ("COL_PRESS_TOP", "COL_PRESS_LOW", "COL_BAR_PRESS_04"),
    "COL_DLG_INK": ("COL_DLG_TOP_30", "COL_DLG_FACE_10", "COL_DLG_FACE_26"),
    "COL_ENGRAVE_FLOOR": ("TRAY",),
    "COL_DOTS": ("COL_TOP_BG_A",),
    "COL_ORANGE": ("COL_TOP_BG_A", "COL_CARD", "COL_BOT_BG_B", "COL_FACE_TOP"),
    "COL_ALERT": ("COL_TOP_BG_B",),
    "COL_TEAL": ("COL_FACE_TOP", "COL_TOP_BG_A"),
    "COL_FOLDER": ("COL_FACE_TOP",),
}
BODY_MIN = 4.5
HUE_SPAN = 15.0


def kind_of(name):
    if name in FIXED_NAMES or name.startswith(FIXED_PREFIXES):
        return "fixed"
    if name in MEANING:
        return "meaning"
    if name in INK_GROUNDS:
        return "ink"
    return "held" if name.startswith(HELD_PREFIXES) else "ground"


# ---------------------------------------------------------------- colour arithmetic


def rgb(v):
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


def lin(v):
    v /= 255.0
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def unlin(v):
    v = min(1.0, max(0.0, v))
    return 255.0 * (12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055)


def luminance(c):
    r, g, b = (lin(x) for x in c)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    """WCAG 2 contrast ratio, 1 to 21."""
    ya, yb = luminance(a), luminance(b)
    return (max(ya, yb) + 0.05) / (min(ya, yb) + 0.05)


def cbrt(v):
    return math.copysign(abs(v) ** (1 / 3), v)


def oklch(c):
    r, g, b = (lin(x) for x in c)
    l = cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b)
    m = cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b)
    s = cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b)
    L = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s
    a = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s
    b = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s
    return L, math.hypot(a, b), math.degrees(math.atan2(b, a)) % 360.0


def linear_of(L, C, h):
    a, b = C * math.cos(math.radians(h)), C * math.sin(math.radians(h))
    l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3
    m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3
    s = (L - 0.0894841775 * a - 1.2914855480 * b) ** 3
    return (4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
            -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
            -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s)


def _at(C, h, Y):
    """The linear sRGB colour of chroma C and hue h whose luminance is Y (lightness found by bisection)."""
    lo, hi = 0.0, 1.2
    for _ in range(40):
        L = (lo + hi) / 2
        r, g, b = linear_of(L, C, h)
        if 0.2126 * r + 0.7152 * g + 0.0722 * b < Y:
            lo = L
        else:
            hi = L
    return linear_of((lo + hi) / 2, C, h)


def colour_at(h, C, Y):
    """The sRGB colour of hue h and relative luminance Y with chroma C, or with the largest chroma below C that
    sRGB can show at that hue and luminance."""
    Y = min(1.0, max(0.0, Y))
    inside = lambda p: all(-0.0005 <= v <= 1.0005 for v in p)
    p = _at(C, h, Y)
    if not inside(p):
        lo, hi = 0.0, C
        for _ in range(24):
            mid = (lo + hi) / 2
            if inside(_at(mid, h, Y)):
                lo = mid
            else:
                hi = mid
        p = _at(lo, h, Y)
    return tuple(int(round(unlin(v))) for v in p)


# ---------------------------------------------------------------- the rule


def lightness(c):
    return luminance(c) ** (1.0 / 3.0)


def tone_curve(scheme, default):
    """The scheme's lightness curve for surfaces, on l = Y ** (1/3)."""
    if not scheme.get("tone"):
        return lambda l: l
    g0, f0 = lightness(default["COL_BG"]), lightness(default["COL_FACE_TOP"])
    g1, f1 = scheme["tone"][:2]
    b = math.log(f1 / g1) / math.log(f0 / g0)
    if len(scheme["tone"]) == 2:
        return lambda l: min(1.0, f1 * (l / f0) ** b)
    a = scheme["tone"][2]
    return lambda l: min(1.0, g1 * (l / g0) ** (a if l < g0 else b))


def ground_of(palette, key, tray_floor):
    if key == "TRAY":       # the tray's floor: the card, tray_floor % towards the shadow colour
        card, dark = palette["COL_CARD"], palette["COL_SHADOW"]
        return tuple(round(card[i] + (dark[i] - card[i]) * tray_floor / 100.0) for i in range(3))
    return palette[key]


def derive(scheme, default, tray_floor):
    """A scheme's palette, {name: (r, g, b)}, from the original's (default, every COL_* name)."""
    if "ground" not in scheme:
        return dict(default)
    tone = tone_curve(scheme, default)
    anchor = {"ground": oklch(default["COL_BG"])[2], "held": oklch(default["COL_PRESS_TOP"])[2]}
    seeds = {"ground": scheme["ground"] + (1.0,), "held": scheme["held"]}
    cache, out = {}, {}

    def turned(c, family):
        _, C, h = oklch(c)
        H, k, _ = seeds[family]
        return (H + max(-HUE_SPAN, min(HUE_SPAN, h - anchor[family]))) % 360.0, C * k

    def surface(c, family):
        if (c, family) not in cache:
            h, C = turned(c, family)
            cache[(c, family)] = colour_at(h, C, min(1.0, tone(lightness(c)) * seeds[family][2]) ** 3)
        return cache[(c, family)]

    for name, c in default.items():
        k = kind_of(name)
        if k == "fixed":
            out[name] = c
        elif k in ("ground", "held"):
            out[name] = surface(c, k)
    for name, c in default.items():
        kind = kind_of(name)
        if kind not in ("ink", "meaning"):
            continue
        y0 = luminance(c)
        want, side, side0 = None, 0, 0
        for i, g in enumerate(INK_GROUNDS[name]):
            g0, g1 = ground_of(default, g, tray_floor), ground_of(out, g, tray_floor)
            cr = contrast(c, g0)
            # the first pair exactly; the others at least BODY_MIN (a hundredth more, for the rounding to 8 bits)
            cr = cr if i == 0 else min(cr, BODY_MIN * 1.01)
            yg = luminance(g1)
            if side == 0:           # the ink stays on its side of the ground unless that side has no room
                side = side0 = 1 if y0 > luminance(g0) else -1
                if side < 0 and (yg + 0.05) / cr - 0.05 < 0:
                    side = 1
                elif side > 0 and cr * (yg + 0.05) - 0.05 > 1:
                    side = -1
            y = cr * (yg + 0.05) - 0.05 if side > 0 else (yg + 0.05) / cr - 0.05
            if i == 0:
                want = y
            elif side > 0 and yg < y:
                want = max(want, y)
            elif side < 0 and yg > y:
                want = min(want, y)
        want = min(1.0, max(0.0, want))
        # moved only when it would fall short: an ink that stays on its side and has clearly more than the wanted
        # contrast on every ground keeps its luminance; "clearly": by more than rounding to 8 bits accounts for
        if side == side0 and ((side < 0 and y0 < want) or (side > 0 and y0 > want)) \
                and abs(want - y0) >= 0.02 * (y0 + 0.05):
            want = y0
        if kind == "meaning":
            if abs(want - y0) < 0.02 * (y0 + 0.05):
                out[name] = c       # the grounds kept their luminance: the colour stays as it is
                continue
            _, C, h = oklch(c)
        else:
            h, C = turned(c, "held" if name.startswith(HELD_PREFIXES) else "ground")
        out[name] = colour_at(h, C, want)
    return out


def glass(card):
    """The installing screen's glass box on the card (r, g, b): white over it, little on a dark card."""
    w = 0.06 if luminance(card) < 0.2 else 0.5
    return tuple(round(c + (255 - c) * w) for c in card)


def palette(index, values, tray_floor):
    """The scheme's COL_* values as numbers, from the original's ({name: 0xRRGGBB}, every COL_* name), and its
    own tones that have no original (COL_LOADER, COL_POUR)."""
    default = {n: rgb(v) for n, v in values.items()}
    out = {n: (c[0] << 16) | (c[1] << 8) | c[2] for n, c in derive(SCHEMES[index], default, tray_floor).items()}
    out["COL_LOADER"] = SCHEMES[index]["loader"]
    out["COL_POUR"] = SCHEMES[index]["pour"]
    return out
