"""Prisma's images (art.py --look prism; the Glass design "Prisma" of ds-glass-prism behind the look boundary).
Light and angular, made for the Shard glass icons: a ground of large quiet facets that runs from pearl into the
scheme's tint, plates with cut corners, buttons as cut stones (a table and a bevel of facets lit from the upper left),
a dark setting for the bar, hairlines for separators, and the Shard ring's tones (teal, blue, violet, rose) as a thin
spectral line.

art.py calls in here through its look hooks (art.py, LOOK): layout() gives a scheme's colours in this look (a palette
of its own, laid over the neutral scheme's; the eight schemes are eight tints, Weiss the pearl to ice ground named
Perle, the last a dark crystal) with the look's numbers (look_prism.h); colours() its own colours (the spectrum);
sprites() replaces the images whose shape or material differs and adds the look's own (OWN_SPRITES); sheet() the
carousel panes' rims in the scheme's rim tones; blobs() the facets of both screens, one texture for all schemes
(white of varying strength; the application lays it over the scheme's two ground colours).

Every cut is at 45 degrees, where a pixel grid steps one by one, and is drawn with each pixel's exact share.
Python 3 standard library only; the same input gives the same bytes."""
import math

import schemes

NAME = "prism"
# The images this look adds to the atlas, after the common ones (ui_art.h): the hairline's lozenge, the selection's
# corners, the ground's last rows of each screen
OWN_SPRITES = ("PRISM_GEM", "PRISM_RING_BIG_TL", "PRISM_RING_BIG_TR", "PRISM_RING_BIG_BL", "PRISM_RING_BIG_BR",
               "PRISM_RING_SMALL_TL", "PRISM_RING_SMALL_TR", "PRISM_RING_SMALL_BL", "PRISM_RING_SMALL_BR",
               "PRISM_GROUND_END_TOP", "PRISM_GROUND_END_BOTTOM", "PRISM_LOADER_STONE",
               *("PRISM_LOADER_LIT_%d" % k for k in range(8)))
# The spectrum of the selection and the plates' line, per scheme (accent): Perle's are the Shard ring's tones
OWN_COLOURS = ("COL_SHARD_PRISM_0", "COL_SHARD_PRISM_1", "COL_SHARD_PRISM_2", "COL_SHARD_PRISM_3")
A = None            # art.py's module (set by art.py before anything here is drawn)

S2 = math.sqrt(2.0)
BLACK = (0.0, 0.0, 0.0)
NIGHT = 7                       # the dark crystal (schemes.SCHEMES' last)
# hue and chroma (OKLCH) of each scheme's tint, in schemes.SCHEMES' order: ice, citrine, champagne, mint, aquamarine,
# amethyst, rose quartz; the dark crystal has its own tones
TINTS = ((245.0, 0.030), (98.0, 0.075), (68.0, 0.045), (165.0, 0.055), (232.0, 0.062), (300.0, 0.055),
         (5.0, 0.050), (262.0, 0.040))
SLATE_HUE = 255.0
PEARL = 0                       # the first colour, named Perle in this look (the application's look facts): pearl to ice
# The accent: Perle keeps the Shard ring's tones (OWN_COLOURS, COL_SHARD_PRISM_0 to _3: teal, blue, violet, rose), the
# held stone's cyan into blue and its blue ink, the carousel rims' violet into teal and the loader's teal. Every
# other colour takes them in its own hue, turned these many degrees from it, at these lightness and chroma: so the
# spectral line, the selection, the beam, the held state, the rims and the loader follow "Farbe".
SPECTRUM_TURN = ((-30.0, 0.70, 0.12), (-10.0, 0.58, 0.15), (10.0, 0.62, 0.15), (30.0, 0.64, 0.14))


def lch(l, c, h):
    return tuple(int(round(schemes.unlin(v))) for v in schemes.linear_of(l, c, h))


def vivid(l, c, h):
    """lch() with the chroma lowered until the colour lies inside sRGB, so its hue holds."""
    while c > 0.0 and not all(-1e-4 <= v <= 1.0001 for v in schemes.linear_of(l, c, h)):
        c -= 0.002
    return lch(l, max(c, 0.0), h)


def accent(index, h):
    """A scheme's accent tones (see SPECTRUM_TURN): the spectrum, the held stone's two tones and its ink, the
    carousel rims' two tones, the loader's lit bead."""
    if index == PEARL:
        spectrum = [schemes.rgb(v) for v in (0x16B8A4, 0x3E6FE3, 0x8F6CF2, 0xD9557E)]
        return dict(spectrum=spectrum, lit=(lch(0.93, 0.050, 195.0), lch(0.88, 0.065, 270.0)),
                    lit_ink=lch(0.34, 0.09, 262.0), rim=(spectrum[2], schemes.rgb(0x16C2AC)),
                    bead=schemes.rgb(0x0F766D))
    spectrum = [vivid(l, c, h + turn) for turn, l, c in SPECTRUM_TURN]
    if index == NIGHT:
        lit, lit_ink = (vivid(0.50, 0.075, h - 30.0), vivid(0.42, 0.095, h + 20.0)), lch(0.97, 0.01, h)
    else:
        lit, lit_ink = (vivid(0.93, 0.050, h - 30.0), vivid(0.88, 0.065, h + 20.0)), vivid(0.34, 0.07, h)
    return dict(spectrum=spectrum, lit=lit, lit_ink=lit_ink, rim=(spectrum[2], spectrum[0]),
                bead=vivid(0.60, 0.13, h - 15.0))


def packed(c):
    return (c[0] << 16) | (c[1] << 8) | c[2]


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def tones(index):
    """A scheme's materials, as (r, g, b): the ground's two ends, the inks, a stone's table and its eight bevel
    facets (from the top clockwise), its edges, the plate, the bar's setting."""
    h, c = TINTS[index]
    if index == NIGHT:
        facet = [lch(l, 0.035, h) for l in (0.56, 0.44, 0.33, 0.25, 0.22, 0.29, 0.41, 0.50)]
        return dict(**accent(index, h), ground_a=lch(0.275, 0.040, 285.0), ground_b=lch(0.185, 0.045, 250.0),
                    facets=0.11,
                    ink=lch(0.94, 0.012, h), muted=lch(0.80, 0.025, h), tile_ink=lch(0.92, 0.015, h),
                    table=(lch(0.37, 0.035, h), lch(0.31, 0.040, h)), facet=facet,
                    edge_light=lch(0.66, 0.04, h), edge_dark=lch(0.12, 0.03, h),
                    plate=lch(0.33, 0.032, h), plate_alpha=0.86, plate_light=lch(0.60, 0.04, h),
                    plate_dark=lch(0.15, 0.03, h), shadow=lch(0.05, 0.02, h), shadow_alpha=0.42,
                    off=lch(0.27, 0.02, h), off_edge=lch(0.35, 0.025, h), off_ink=lch(0.70, 0.02, h),
                    bar=(lch(0.13, 0.025, h), lch(0.09, 0.02, h)), bar_edge=lch(0.40, 0.04, h),
                    rule=lch(0.72, 0.03, h), rule_alpha=0.50, clear=0.90, dlg_copy=lch(0.12, 0.03, h))
    facet = [lch(l, k * c, h) for l, k in ((1.0, 0.0), (0.955, 0.35), (0.905, 0.55), (0.84, 0.75), (0.865, 0.65),
                                           (0.925, 0.45), (0.972, 0.25), (0.992, 0.10))]
    return dict(**accent(index, h), ground_a=lch(0.950, 0.012, 75.0), ground_b=lch(0.872, c, h), facets=1.0,
                ink=lch(0.30, 0.030, SLATE_HUE), muted=lch(0.44, 0.030, SLATE_HUE),
                tile_ink=lch(0.36, 0.030, SLATE_HUE),
                table=(lch(0.988, 0.10 * c, h), lch(0.948, 0.32 * c, h)), facet=facet,
                edge_light=lch(0.80, 0.5 * c, h), edge_dark=lch(0.60, 0.035, SLATE_HUE),
                plate=lch(0.985, 0.08 * c, h), plate_alpha=0.80, plate_light=(255, 255, 255),
                plate_dark=lch(0.66, 0.035, SLATE_HUE), shadow=lch(0.30, 0.04, SLATE_HUE), shadow_alpha=0.24,
                off=lch(0.915, 0.3 * c, h), off_edge=lch(0.84, 0.4 * c, h), off_ink=lch(0.49, 0.025, SLATE_HUE),
                bar=(lch(0.36, 0.035, SLATE_HUE), lch(0.27, 0.035, SLATE_HUE)), bar_edge=lch(0.62, 0.04, SLATE_HUE),
                rule=lch(0.50, 0.035, SLATE_HUE), rule_alpha=0.42, clear=0.84, dlg_copy=(255, 255, 255),
                # the neutral scheme's symbol and warning colours are made for white; on the tinted ground they
                # are set deeper, so text in them keeps 4.5 to 1
                signal=dict(COL_TEAL=lch(0.455, 0.085, 185.0), COL_ORANGE=lch(0.47, 0.125, 48.0),
                            COL_ALERT=lch(0.46, 0.165, 28.0)))


def layout(index):
    """What scheme index is drawn from in this look: the neutral scheme's layout (the dark one's for the dark crystal)
    with the look's own colours laid over and its numbers (look_prism.h); the colours' names are ui_layout.h's, so the
    application reads them as ever."""
    base = A.scheme_layout(A.D, NIGHT if index == NIGHT else 0)
    t = tones(index)
    values = {n: v for n, v in vars(base).items() if not n.startswith("__")}
    values.update({n: v for n, v in vars(A.read_layout(A.HEADERS / "look_prism.h")).items() if not n.startswith("__")})
    own = dict(COL_TOP_BG_A=t["ground_a"], COL_TOP_BG_B=t["ground_b"], COL_BOT_BG_A=t["ground_a"],
               COL_BOT_BG_B=t["ground_b"], COL_INK=t["ink"], COL_MUTED=t["muted"], COL_TILE_INK=t["tile_ink"],
               COL_PRESS_INK=t["lit_ink"], COL_DOTS=t["rule"], COL_CARD=t["plate"], COL_SHADOW=t["shadow"],
               COL_OFF=t["off"], COL_OFF_EDGE=t["off_edge"], COL_OFF_INK=t["off_ink"],
               COL_FACE_TOP=t["table"][0], COL_FACE_LOW=t["table"][1], COL_DLG_INK=t["ink"],
               COL_DLG_LABEL_LIGHT=t["dlg_copy"],
               **{"COL_SHARD_PRISM_%d" % k: c for k, c in enumerate(t["spectrum"])})
    own.update(t.get("signal", {}))
    values.update({n: packed(tuple(int(round(v)) for v in c)) for n, c in own.items()})
    return type("Layout", (), values)


def colours():
    """The look's own colours in the scheme drawn: the spectrum (ui_layout.h's it draws in other values are layout()'s)."""
    return {n: getattr(A.L, n) for n in OWN_COLOURS}


# ---------------------------------------------------------------- cut shapes


class Cut:
    """A box whose corners are cut off at 45 degrees (the legs: top left, top right, bottom right, bottom left), as
    half planes n . p <= h; its edges are numbered from the top clockwise, 0 to 7 (odd: a cut)."""

    def __init__(self, x0, y0, x1, y1, cuts):
        tl, tr, br, bl = cuts
        self.box, self.cuts = (x0, y0, x1, y1), tuple(cuts)
        k = 1.0 / S2
        self.edges = [(0.0, -1.0, -y0, 0), (1.0, 0.0, x1, 2), (0.0, 1.0, y1, 4), (-1.0, 0.0, -x0, 6)]
        if tr:
            self.edges.append((k, -k, (x1 - tr - y0) * k, 1))
        if br:
            self.edges.append((k, k, (x1 - br + y1) * k, 3))
        if bl:
            self.edges.append((-k, k, (y1 - x0 - bl) * k, 5))
        if tl:
            self.edges.append((-k, -k, -(x0 + tl + y0) * k, 7))

    def inset(self, d):
        other = Cut.__new__(Cut)
        other.box, other.cuts = self.box, self.cuts
        other.edges = [(nx, ny, h - d, f) for nx, ny, h, f in self.edges]
        return other

    def moved(self, dx, dy):
        other = Cut.__new__(Cut)
        other.box, other.cuts = self.box, self.cuts
        other.edges = [(nx, ny, h + nx * dx + ny * dy, f) for nx, ny, h, f in self.edges]
        return other

    def at(self, x, y):
        """How far the point is outside (negative: inside), and the edge it is nearest to."""
        best, facet = -1e9, 0
        for nx, ny, h, f in self.edges:
            d = nx * x + ny * y - h
            if d > best:
                best, facet = d, f
        return best, facet

    def sd(self, x, y):
        return self.at(x, y)[0]


SUB = (-1.0 / 3, 0.0, 1.0 / 3)


def stone(A, img, shape, bevel, facet, table, edge_light, edge_dark, shadow=None, step=None, clear=1.0):
    """A cut stone: the table (table(x, y) gives its colour) inside a bevel of facets, one per edge of shape, in
    facet's colours; a hairline along the outline, light where the edge faces the light (top, left and the cuts
    between and beside them), dark elsewhere; shadow: (dy, blur, colour, alpha) under it. step: (inset, colour,
    alpha) draws a second, fine outline that far inside the table: the step of a step cut. clear: how solid the
    table is (the ground's facets show through the rest); the bevel is half as clear, the outline solid."""
    if shadow:
        dy, blur, colour, alpha = shadow
        low = shape.moved(0, dy)
        img.paint(lambda x, y: (alpha * A.soft(low.sd(x, y), blur) * (1.0 - A.cover(shape.sd(x, y))), colour))
    inner = shape.inset(bevel)
    lit = (0, 6, 7, 1)

    def tone(x, y):
        d, f = shape.at(x, y)
        if d < -bevel:
            return table(x, y)
        return facet[f]

    def body(x, y):
        d, f = shape.at(x, y)
        c = A.cover(d)
        if c <= 0.0:
            return None
        a = clear
        if d < -bevel - 1.0:
            col = table(x, y)
            if step:
                e = abs(-d - step[0])
                if e < 1.0:
                    col = mix(col, step[1], step[2] * (1.0 - e))
        else:
            a = (1.0 + clear) / 2.0
            acc = [0.0, 0.0, 0.0]
            for sy in SUB:
                for sx in SUB:
                    t = tone(x + sx, y + sy)
                    acc[0] += t[0]
                    acc[1] += t[1]
                    acc[2] += t[2]
            col = (acc[0] / 9.0, acc[1] / 9.0, acc[2] / 9.0)
            e = max(0.0, c - A.cover(d + 1.0)) / c
            if e > 0.0:
                col = mix(col, edge_light if f in lit else edge_dark, e * (0.55 if f in lit else 0.80))
                a += (1.0 - a) * e
        return (c * a, col)
    img.paint(body)
    return inner


def table_of(top, low, y0, y1):
    def at(x, y):
        t = min(1.0, max(0.0, (y - y0) / (y1 - y0)))
        return mix(top, low, t)
    return at


# ---------------------------------------------------------------- the faces


def face(A, name, h, cut, bevel, t, state="rest", kind="stone"):
    """A tile, row or button of height h and any width, as art.py's face(): rest, held ("down": one pixel smaller
    all round, the bevel's light turned over, the table lit) or greyed ("off": a flat plate)."""
    def draw(img, box):
        if state == "off":
            shape = Cut(box.x0, box.y0, box.x1, box.y1, (cut,) * 4)
            stone(A, img, shape, 1.0, [t["off"]] * 8, lambda x, y: t["off"], t["off_edge"], t["off_edge"])
            return
        if state == "down":
            shape = Cut(box.x0 + 1, box.y0 + 1, box.x1 - 1, box.y1 - 1, (cut,) * 4)
            a, b = t["lit"] if kind != "go" else (A.rgb(A.L.COL_GO_LOW), A.rgb(A.L.COL_GO_SHADE))
            facets = t["facet"][4:] + t["facet"][:4] if kind != "go" else go_facets(A)[4:] + go_facets(A)[:4]
            edge = t["edge_dark"] if kind != "go" else A.rgb(A.L.COL_GO_SHADE)
            stone(A, img, shape, bevel, facets, lambda x, y: mix(a, b, min(1.0, max(0.0, (y - box.y0) / (box.y1 - box.y0)))),
                  edge, edge)
            return
        shape = Cut(box.x0, box.y0, box.x1, box.y1, (cut,) * 4)
        shadow = (2.0, 4.0, t["shadow"], t["shadow_alpha"])
        if kind == "go":
            top, low = A.rgb(A.L.COL_GO_TOP), A.rgb(A.L.COL_GO_LOW)
            stone(A, img, shape, bevel, go_facets(A), table_of(top, low, box.y0 + bevel, box.y1 - bevel),
                  A.rgb(A.L.COL_GO_EDGE_TOP), A.rgb(A.L.COL_GO_SHADE), shadow,
                  step=(bevel + 2.0, (255, 255, 255), 0.10))
        else:
            stone(A, img, shape, bevel, t["facet"], table_of(t["table"][0], t["table"][1], box.y0 + bevel,
                                                              box.y1 - bevel),
                  t["edge_light"], t["edge_dark"], shadow, step=(bevel + 2.0, t["facet"][3], 0.35),
                  clear=t["clear"])
    pad = A.FACE_PAD if state == "rest" else (A.FACE_PAD if state == "down" else A.NO_PAD)
    return A.across(name, h, cut + bevel + 2, pad, draw, 2)


def go_facets(A):
    """The green button's bevel: an emerald, from its two greens."""
    top, low, shade = A.rgb(A.L.COL_GO_TOP), A.rgb(A.L.COL_GO_LOW), A.rgb(A.L.COL_GO_SHADE)
    white = (255, 255, 255)
    return [mix(top, white, 0.45), mix(top, white, 0.18), low, mix(low, shade, 0.75), mix(low, shade, 0.45),
            mix(top, low, 0.5), mix(top, white, 0.22), mix(top, white, 0.33)]


# ---------------------------------------------------------------- plates


def plate_body(A, t, shape, alpha):
    """A plate's material inside shape: (coverage times solidity, colour) per pixel, None outside."""
    lit = (0, 6, 7)

    def body(x, y):
        d, f = shape.at(x, y)
        c = A.cover(d)
        if c <= 0.0:
            return None
        a, col = alpha, t["plate"]
        if d > -1.0:            # the outline
            e = max(0.0, c - A.cover(d + 1.0)) / c
            col = mix(col, t["plate_dark"], e * (0.45 if f in lit else 0.9))
            a = alpha + (1.0 - alpha) * e
        elif d > -3.0:          # the bevel
            e = 1.0 if d > -2.5 else (d + 3.0) * 2.0
            if f in lit:
                col, a = mix(col, t["plate_light"], e), alpha + (1.0 - alpha) * e * 0.9
            elif f in (1, 5):
                pass
            else:
                col, a = mix(col, t["plate_dark"], 0.30 * e), alpha + (1.0 - alpha) * e * 0.5
        return (c * a, col)
    return body


def plate(A, name, t, pad=(3, 2, 3, 5), alpha=None, shadow=(2.0, 5.0, 0.75), even=False):
    """A plate of clear crystal (the cards): the upper left and the lower right corner cut, the other two nicked (even:
    all four cut), a bevel of two pixels (light above and left, shade below and right), a fine dark outline, a soft
    shadow (how far down, its blur, its share of the scheme's shadow); the ground's facets show through faintly."""
    alpha = t["plate_alpha"] if alpha is None else alpha
    cut, nick = A.L.PRISM_PLATE_CUT, A.L.PRISM_PLATE_NICK
    dy, blur, share = shadow
    corners = (cut,) * 4 if even else (cut, nick, cut, nick)

    def draw(img, box):
        shape = Cut(box.x0, box.y0, box.x1, box.y1, corners)
        low = shape.moved(0, dy)
        img.paint(lambda x, y: (min(1.0, t["shadow_alpha"] * share) * A.soft(low.sd(x, y), blur)
                                * (1.0 - A.cover(shape.sd(x, y))), t["shadow"]))
        img.paint(plate_body(A, t, shape, alpha))
    return A.both_ways(name, cut + 2, pad, draw)


# ---------------------------------------------------------------- the popup


def dialog_stone(A, img, t, x0, y0, x1, y1, held):
    """One of the popup's buttons: a stone as a tile's, lit and a pixel smaller when held."""
    cut, bevel = A.L.PRISM_CUT_BIG, A.L.PRISM_BEVEL_BIG
    if held:
        a, b = t["lit"]
        stone(A, img, Cut(x0 + 1, y0 + 1, x1 - 1, y1 - 1, (cut,) * 4), bevel, t["facet"][4:] + t["facet"][:4],
              table_of(a, b, y0, y1), t["edge_dark"], t["edge_dark"])
        return
    stone(A, img, Cut(x0, y0, x1, y1, (cut,) * 4), bevel, t["facet"], table_of(t["table"][0], t["table"][1],
                                                                             y0 + bevel, y1 - bevel),
          t["edge_light"], t["edge_dark"], (1.5, 3.0, t["shadow"], t["shadow_alpha"]),
          step=(bevel + 2.0, t["facet"][3], 0.35))


def dialog_panel(A, t):
    """The popup's panel: a plate, solid (the dimmed screen lies under it), with a deeper shadow; its four corners cut
    alike."""
    n = A.L.DLG_SHADOW_BLUR
    return plate(A, "DLG", t, (n, n, n, n), 1.0, (3.0, float(n), 1.6), even=True)


def dialog_button(A, name, t, held):
    """The popup's button band (art.py's dialog_button(): from one row above the band, any width): the panel's
    lower end again, solid, and on it one stone from side to side, PRISM_DLG_SIDE px in from the sides,
    PRISM_DLG_TOP and PRISM_DLG_LOW from the band's top and bottom (the band PRISM_DLG_GROW px taller below). Only
    pixels that lie wholly inside the panel are drawn, so its outline and cuts stay the panel's own."""
    L = A.L
    side, top, low = L.PRISM_DLG_SIDE, L.PRISM_DLG_TOP, L.PRISM_DLG_LOW
    end = max(side + L.PRISM_CUT_BIG + L.PRISM_BEVEL_BIG + 4, L.PRISM_PLATE_CUT + 4)
    w, h = 2 * end + A.MID, L.DLG_BTN_H + 1 + L.PRISM_DLG_GROW
    img = A.Image(w, h)
    panel = Cut(0, h - 400, w, h, (0, 0, L.PRISM_PLATE_CUT, L.PRISM_PLATE_NICK))
    body = plate_body(A, t, panel, 1.0)
    img.paint(lambda x, y: body(x, y) if panel.sd(x, y) <= -0.5 else None)
    stones = A.Image(w, h)
    dialog_stone(A, stones, t, side, 1 + top, w - side, h - low, held)
    for k in range(w * h):
        a = stones.p[4 * k + 3]
        if a > 0.0 and panel.sd(k % w + 0.5, k // w + 0.5) <= -0.5:
            img.over(k, [v / a for v in stones.p[4 * k:4 * k + 3]], a)
    return A.Sprite(name, img, A.NO_PAD, (end, 0, end, 0))


def dialog_divider(A, name, t, held):
    """Between the popup's two buttons, 40 px wide over the band's height: the panel, then the left stone's right
    end and the right stone's left end, PRISM_DLG_GAP px apart; held: -1 none, 0 the left one, 1 the right one."""
    L = A.L
    w, h, gap, top, low = 40, L.DLG_BTN_H + L.PRISM_DLG_GROW, L.PRISM_DLG_GAP, L.PRISM_DLG_TOP, L.PRISM_DLG_LOW
    img = A.Image(w, h)
    img.paint(plate_body(A, t, Cut(-200, h - 400, w + 200, h, (0, 0, 0, 0)), 1.0))
    for right in (0, 1):
        part = A.Image(w, h)
        x0, x1 = (w / 2.0 + gap / 2.0, w + 200.0) if right else (-200.0, w / 2.0 - gap / 2.0)
        dialog_stone(A, part, t, x0, top, x1, h - low, held == right)
        img.blit(part, 0, 0)
    return A.Sprite(name, img)


# ---------------------------------------------------------------- the bar and the tabs


TAB_CUT = 16


def bar_shape(A, x0, x1, pad):
    """The bar from x0 to x1 (an end beyond the image is the screen's edge), its upper corners cut."""
    L = A.L
    return Cut(x0, pad, x1, pad + L.BAR_H + 40, (TAB_CUT, TAB_CUT, 0, 0))


def bar(A, name, t, held, x0=None, x1=None, w=None):
    """The bar (or a tab) as art.py's bar(): the stones' dark setting, a light hairline along its top and its cut,
    its shadow; held: lit as a held stone."""
    L = A.L
    pad = L.BAR_SHADOW
    x0 = L.BAR_X if x0 is None else x0
    x1 = L.BOTTOM_W - L.BAR_X if x1 is None else x1
    w = L.BOTTOM_W if w is None else w
    img = A.Image(w, L.BAR_H + pad)
    shape = bar_shape(A, x0, x1, pad)
    if not held:
        img.paint(lambda x, y: (0.30 * A.soft(shape.sd(x, y) - 1.0, 5.0) * (1.0 - A.cover(shape.sd(x, y))), t["shadow"]))
    top, low = t["lit"] if held else t["bar"]

    def body(x, y):
        d, f = shape.at(x, y)
        c = A.cover(d)
        if c <= 0.0:
            return None
        col = mix(top, low, min(1.0, (y - pad) / L.BAR_H))
        if f in (0, 1, 7) and -2.0 < d <= -1.0:      # a light line one pixel inside the edge
            col = mix(col, (255, 255, 255) if held else t["bar_edge"], 0.9 if held else 0.7)
        return (c, col)
    img.paint(body)
    return A.Sprite(name, img, (0, pad, 0, 0))


def corner(A, name, t, state):
    """The corner button (art.py's corner()): a stone set into the screen's upper left corner, only its lower
    right corner cut, its bevel along the right and the lower edge."""
    L = A.L
    cut, bevel = L.PRISM_CUT_BIG, L.PRISM_BEVEL_BIG - 1

    def draw(img, box):
        if state == "off":
            shape = Cut(box.x0 - 9, box.y0 - 9, box.x1, box.y1, (0, 0, cut, 0))
            stone(A, img, shape, 1.0, [t["off"]] * 8, lambda x, y: t["off"], t["off_edge"], t["off_edge"])
        elif state == "down":
            shape = Cut(box.x0 - 9, box.y0 - 9, box.x1 - 1, box.y1 - 1, (0, 0, cut, 0))
            a, b = t["lit"]
            stone(A, img, shape, bevel, t["facet"][4:] + t["facet"][:4], table_of(a, b, box.y0, box.y1),
                  t["edge_dark"], t["edge_dark"])
        else:
            shape = Cut(box.x0 - 9, box.y0 - 9, box.x1, box.y1, (0, 0, cut, 0))
            stone(A, img, shape, bevel, t["facet"], table_of(t["table"][0], t["table"][1], box.y0, box.y1 - bevel),
                  t["edge_light"], t["edge_dark"], (2.0, 4.0, t["shadow"], t["shadow_alpha"]), clear=t["clear"])
    return A.across(name, L.PRISM_CORNER_H, cut + bevel + 2, A.NO_PAD if state == "off" else (0, 0, 4, 7), draw, 2)


def help_button(A, name, t, held):
    """The help button: an eight-sided stone; its mark is the glass sheet's, drawn over it."""
    L = A.L
    n, pad, cut, bevel = L.HELP_FACE, A.FACE_PAD, 9, 3
    img = A.Image(n + pad[0] + pad[2], n + pad[1] + pad[3])
    if held:
        shape = Cut(pad[0] + 1, pad[1] + 1, pad[0] + n - 1, pad[1] + n - 1, (cut,) * 4)
        a, b = t["lit"]
        stone(A, img, shape, bevel, t["facet"][4:] + t["facet"][:4], table_of(a, b, pad[1], pad[1] + n),
              t["edge_dark"], t["edge_dark"])
    else:
        shape = Cut(pad[0], pad[1], pad[0] + n, pad[1] + n, (cut,) * 4)
        stone(A, img, shape, bevel, t["facet"], table_of(t["table"][0], t["table"][1], pad[1], pad[1] + n),
              t["edge_light"], t["edge_dark"], (2.0, 4.0, t["shadow"], t["shadow_alpha"]), clear=t["clear"])
    return A.Sprite(name, img, pad)


def track(A, t):
    """The scroll bar's track: a hairline down its middle."""
    def draw(img, box):
        cx = (box.x0 + box.x1) / 2.0
        img.paint(lambda x, y: (t["rule_alpha"] * A.cover(abs(x - cx) - 1.0) * A.cover(max(box.y0 - y, y - box.y1)),
                                t["rule"]))
    return A.upright("TRACK", A.L.SCROLL_W, 3, A.NO_PAD, draw)


def thumb(A, name, t, held):
    """The scroll bar's thumb: a crystal with a point at each end, its left side in the light."""
    def draw(img, box):
        w = box.x1 - box.x0
        shape = Cut(box.x0, box.y0, box.x1, box.y1, (w / 2.0,) * 4)
        a, b = t["lit"] if held else t["table"]
        facet = t["facet"][4:] + t["facet"][:4] if held else t["facet"]
        stone(A, img, shape, 2.0, facet, lambda x, y: mix(a, b, (x - box.x0) / w), t["edge_light"], t["edge_dark"],
              None if held else (1.0, 3.0, t["shadow"], t["shadow_alpha"]))
    return A.upright(name, A.L.SCROLL_W, 7, (3, 1, 3, 4), draw)


def tabs(A, t):
    L = A.L
    w, edge = L.TAB_W + L.BAR_SHADOW, L.TAB_EDGE_10 / 10.0
    left, right = (-60.0, edge), (w - edge, w + 60.0)
    return [bar(A, "TAB_L", t, False, *left, w), bar(A, "TAB_R", t, False, *right, w),
            bar(A, "TAB_L_DOWN", t, True, *left, w), bar(A, "TAB_R_DOWN", t, True, *right, w)]


# ---------------------------------------------------------------- the ground


def lcg(seed):
    state = [seed & 0xFFFFFFFF]

    def nxt():
        state[0] = (state[0] * 1664525 + 1013904223) & 0xFFFFFFFF
        return (state[0] >> 8) / float(1 << 24)
    return nxt


class Facets:
    """A screen's ground: long shards that lean as the Shard icons' light bands do (x + LEAN y constant), each cut
    across into a few facets; every facet a strength of white between FAINT and BRIGHT, running a little along its
    shard. at(x, y) gives the facet's number and its white."""
    LEAN, FAINT, BRIGHT = 0.5, 0.0, 0.52

    def __init__(self, w, h, seed, widths):
        rnd = lcg(seed)
        self.w, self.h = w, h
        self.lines, u = [], -40.0
        while u < w + self.LEAN * h + 80:
            self.lines.append(u)
            u += widths[len(self.lines) % len(widths)] * (0.85 + 0.3 * rnd())
        self.cuts, self.slope, self.value = [], [], []
        last = 0.3
        for i in range(len(self.lines)):
            s = -0.55 + 0.9 * rnd()
            n = 1 + int(rnd() * 2.2)
            ds = sorted(h * (k + 0.25 + 0.5 * rnd()) / (n + 0.4) - 20 for k in range(n))
            vs = []
            for _ in range(n + 1):
                v = rnd()
                if abs(v - last) < 0.22:
                    v = (v + 0.45) % 1.0
                vs.append(v)
                last = v
            self.cuts.append(ds)
            self.slope.append(s)
            self.value.append(vs)

    def at(self, x, y):
        u = x + self.LEAN * y
        i = 0
        while i + 1 < len(self.lines) and self.lines[i + 1] <= u:
            i += 1
        t = y - self.slope[i] * (u - self.lines[i])
        j = 0
        for d in self.cuts[i]:
            if t >= d:
                j += 1
        v = self.value[i][j]
        run = (u - self.lines[i]) / max(1.0, self.lines[min(i + 1, len(self.lines) - 1)] - self.lines[i])
        white = self.FAINT + (self.BRIGHT - self.FAINT) * v + 0.05 * (0.5 - run) + 0.05 * (0.5 - y / self.h)
        return i * 8 + j, max(0.0, min(0.75, white))

    def edge(self, x, y):
        """A fine light line along each shard's left edge: how much of it lies on this pixel."""
        u = x + self.LEAN * y
        k = math.sqrt(1.0 + self.LEAN * self.LEAN)
        best = min(abs(u - line) for line in self.lines) / k
        return max(0.0, 1.0 - best)


def ground_screen(A, img, y0, w, h, seed, widths):
    f = Facets(w, h, seed, widths)
    ids = [[f.at(x + 0.5, y + 0.5) for x in range(w)] for y in range(h)]
    for y in range(h):
        for x in range(w):
            k, white = ids[y][x]
            mixed = False
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < w and 0 <= yy < h and ids[yy][xx][0] != k:
                    mixed = True
                    break
            if mixed:
                white = sum(f.at(x + 0.5 + sx * 0.75, y + 0.5 + sy * 0.75)[1] for sx in SUB for sy in SUB) / 9.0
            white = white + (0.80 - white) * 0.45 * f.edge(x + 0.5, y + 0.5)
            # eight levels a step keep the texture small and still smooth under the tints
            a = round(white * 255.0 / 2.0) * 2.0 / 255.0
            if a > 0.0:
                img.over((y0 + y) * img.w + x, (255.0, 255.0, 255.0), a)


GROUND = []         # the facets, drawn once per process (sprites() and blobs() both read them)


def ground(A):
    """Both screens' facets in one image PRISM_GROUND_W x PRISM_GROUND_H: white, its alpha the facet's strength."""
    L = A.L
    if not GROUND:
        img = A.Image(L.PRISM_GROUND_W, L.PRISM_GROUND_H)
        ground_screen(A, img, 0, L.TOP_W, L.SCREEN_H, 20261114, (96, 58, 132, 74, 110))
        ground_screen(A, img, L.PRISM_GROUND_ROW, L.BOTTOM_W, L.SCREEN_H, 7919, (84, 120, 62, 104))
        GROUND.append(img)
    return GROUND[0]


def ground_end(A, name, row, w):
    """The last PRISM_GROUND_END rows of a screen's facets (from row of the ground's image, w px wide), the same pixels
    as in the ground's texture, for the atlas: drawn from there they end the screen's ground on an atlas image."""
    L, img = A.L, ground(A)
    n = L.PRISM_GROUND_END
    out = A.Image(w, n)
    for y in range(n):
        at, to = 4 * (row + L.SCREEN_H - n + y) * img.w, 4 * y * w
        out.p[to:to + 4 * w] = img.p[at:at + 4 * w]
    return A.Sprite(name, out)


def blobs(index):
    """The ground's texture, the same in every scheme (named once): ART_PRISM_GROUND_Z[ART_SCHEMES][1]; each screen's
    last PRISM_GROUND_END rows empty, they are the atlas's (ground_end)."""
    import zlib
    L, img = A.L, ground(A)
    data = bytearray(img.rgba())
    for row in (0, L.PRISM_GROUND_ROW):
        at = 4 * img.w * (row + L.SCREEN_H - L.PRISM_GROUND_END)
        data[at:at + 4 * img.w * L.PRISM_GROUND_END] = bytes(4 * img.w * L.PRISM_GROUND_END)
    return {"ground": [zlib.compress(A.tile_order(bytes(data), L.PRISM_GROUND_W), 9)]}


# ---------------------------------------------------------------- keys


def key_cut(s):
    """A key's outline in a box of 24 units, s px a unit: a square with cut corners."""
    a, b, c = 1.5, 22.5, 6.5
    return [([(a + c, a), (b - c, a), (b, a + c), (b, b - c), (b - c, b), (a + c, b), (a, b - c), (a, a + c)], True)]


def keycap(A, name, letter, held=False):
    """A key of the bar as art.py's keycap(), its ring a square with cut corners: engraved (a dark copy
    LABEL_SHADOW_DY px above, only outside the key and above the letter's strokes), the key in white; held: in the
    held ink, the copy white."""
    L = A.L
    n, up = L.KEYCAP, L.LABEL_SHADOW_DY
    img = A.Image(n, n + up)
    s = n / 24.0
    ring, mark = key_cut(s), A.lines_of(A.KEY_LETTERS[letter])
    inside = Cut(s * 1.5, up + s * 1.5, s * 22.5, up + s * 22.5, (s * 6.5,) * 4)
    ring_up = A.coverage(img, ring, s, 2.1 * s, 0, 0)
    mark_up = A.coverage(img, mark, s, 2.4 * s, 0, 0)
    ring_at = A.coverage(img, ring, s, 2.1 * s, 0, up)
    mark_at = A.coverage(img, mark, s, 2.4 * s, 0, up)
    dark, a = ((A.rgb(L.COL_LABEL_HELD_SHADOW), L.LABEL_HELD_SHADOW_ALPHA / 100.0) if held
               else (A.rgb(L.COL_LABEL_SHADOW), L.LABEL_SHADOW_ALPHA / 100.0))
    ink = A.rgb(L.COL_PRESS_INK if held else L.COL_LABEL)
    for k in range(n * (n + up)):
        x, y = k % n + 0.5, k // n + 0.5
        shade = max(ring_up[k] * (1.0 - A.cover(inside.sd(x, y))), mark_up[k])
        if shade > 0:
            img.over(k, dark, a * shade)
        white = max(ring_at[k], mark_at[k])
        if white > 0:
            img.over(k, ink, white)
    return A.Sprite(name, img, (0, up, 0, 0))


def start_key(A, name, held, shadow=False):
    """The START key's frame as art.py's start_key(), its corners cut."""
    L = A.L
    if shadow:
        color, alpha = ((L.COL_LABEL_HELD_SHADOW, L.LABEL_HELD_SHADOW_ALPHA / 100.0) if held
                        else (L.COL_LABEL_SHADOW, L.LABEL_SHADOW_ALPHA / 100.0))
    else:
        color, alpha = L.COL_PRESS_INK if held else L.COL_WHITE, 1.0

    def draw(img, box):
        shape = Cut(box.x0, box.y0, box.x1, box.y1, (L.PRISM_START_CUT,) * 4)
        inner = shape.inset(1.5)
        img.paint(lambda x, y: (alpha * max(0.0, A.cover(shape.sd(x, y)) - A.cover(inner.sd(x, y))), A.rgb(color)))
    return A.across(name, L.START_H, L.PRISM_START_CUT, A.NO_PAD, draw)


# ---------------------------------------------------------------- pictures


def icon_mask():
    """A theme's 48x48 icon with its corners cut (art.py's icon_mask()), as alpha, row by row."""
    n = A.L.DETAIL_ICON
    shape = Cut(0, 0, n, n, (A.L.PRISM_ICON_CUT,) * 4)
    return bytes(round(A.cover(shape.sd(x + 0.5, y + 0.5)) * 255) for y in range(n) for x in range(n))


def beam(A, t):
    """In the place of the installing screen's drop: light a prism has split, four bands in the scheme's spectrum
    that lean as the ground's shards do and fade away from the screen's upper edge."""
    L = A.L
    n = L.DROP_SUN
    tones = t["spectrum"]
    img = A.Image(n, n)
    top, k = L.PRISM_BEAM_TOP, math.sqrt(1.0 + Facets.LEAN * Facets.LEAN)
    wide, first = L.PRISM_BEAM_BAND, L.PRISM_BEAM_AT

    def at(x, y):
        u = x + Facets.LEAN * y - first
        band = int(u // wide)
        if band < 0 or band > 3 or y < top:
            return None
        edge = min(u, 4 * wide - u) / k                 # how far inside the group of bands
        seam = abs(u - round(u / wide) * wide) / k      # how far from a line between two bands
        fade = max(0.0, 1.0 - (y - top) / L.PRISM_BEAM_REACH) ** 1.3
        a = L.PRISM_BEAM_ALPHA / 100.0 * fade * A.cover(0.5 - edge) * (0.82 + 0.18 * min(1.0, seam))
        return (a * (1.0 - 0.12 * band), tones[band])
    img.paint(at)
    return A.Sprite("DROP_SUN", img)


# ---------------------------------------------------------------- the look's own images


def gem(A, t):
    """The small lozenge that ends a hairline: an outline in the hairline's tone, light inside."""
    n = A.L.PRISM_GEM
    img = A.Image(n, n)
    c = n / 2.0
    k = 1.0 / S2
    img.paint(lambda x, y: (A.cover((abs(x - c) + abs(y - c) - c + 0.5) * k) * 0.85, t["rule"]))
    img.paint(lambda x, y: (A.cover((abs(x - c) + abs(y - c) - c + 2.0) * k) * 0.85, t["plate_light"]))
    return A.Sprite("PRISM_GEM", img)


def ring_corners(A, t, size, cut, bevel):
    """The selection's four corners for a stone with this cut and bevel (look_prism.c select): the outline's line,
    half outside it, and the bevel inside it, fainter; each corner in its tone, as far as the cut and
    PRISM_RING_STUB px along both edges. One pixel of each image lies outside the stone."""
    L = A.L
    n, far, half = cut + L.PRISM_RING_STUB, 400, L.PRISM_RING_LINE / 2.0
    tones = [t["spectrum"][k] for k in (0, 1, 3, 2)]
    out = []
    for k, pos in enumerate(("TL", "TR", "BL", "BR")):
        right, low = k & 1, k >> 1
        img = A.Image(n + 1, n + 1)
        x0, y0 = (n - far if right else 1), (n - far if low else 1)
        shape = Cut(x0, y0, x0 + far, y0 + far, (cut,) * 4)
        colour = tones[k]

        def body(x, y, shape=shape, colour=colour):
            d = shape.sd(x, y)
            line = A.cover(d - half) - A.cover(d + half)
            inside = A.cover(d + half) - A.cover(d + bevel)
            return (line + L.PRISM_RING_BEVEL / 100.0 * inside, colour)
        img.paint(body)
        out.append(A.Sprite("PRISM_RING_%s_%s" % (size, pos), img, (0 if right else 1, 0 if low else 1,
                                                                   1 if right else 0, 1 if low else 0)))
    return out


def sheet(names):
    """The sheet in the scheme drawn: the Shard sheet (the icons on buttons)."""
    out = A.glass_sheet("shard")
    assert [s.name for s in out] == names
    return out


def loader_sprites(A, t, index):
    """The loader (look_prism.c loader): a cut crystal PRISM_LOADER px across in the scheme's loader tone (the
    accent's bead tone, which follows "Farbe"), an octagon of PRISM_LOADER_CUT px cuts with a bevel of eight facets
    lit from the upper left as Prisma's stones are; each facet alone lit (PRISM_LOADER_LIT_0 to _7, from the top
    clockwise, as Cut numbers its edges), pale against the bevel's deeper tones."""
    L = A.L
    n, cut, bevel, pad = L.PRISM_LOADER, L.PRISM_LOADER_CUT, L.PRISM_LOADER_BEVEL, 2
    bead, white, black = t["bead"], (255.0, 255.0, 255.0), BLACK
    night = index == NIGHT
    dark, light = mix(bead, black, 0.40 if night else 0.30), mix(bead, white, 0.45 if night else 0.62)
    shade = (1.0, 0.72, 0.42, 0.22, 0.16, 0.30, 0.60, 0.88)        # facets from the top clockwise, lit from upper left
    shape = Cut(pad, pad, pad + n, pad + n, (cut,) * 4)
    img = A.Image(n + 2 * pad, n + 2 * pad)
    table = mix(bead, black, 0.35) if night else mix(bead, white, 0.84)
    stone(A, img, shape, bevel, [mix(dark, light, k) for k in shade], lambda x, y: table,
          mix(bead, white, 0.60 if night else 0.80), mix(bead, black, 0.60 if night else 0.45),
          (1.0, 2.0, t["shadow"], t["shadow_alpha"]), clear=0.80)
    out = [A.Sprite("PRISM_LOADER_STONE", img, (pad,) * 4)]
    lit = mix(bead, white, 0.60 if night else 0.78)
    for k in range(8):
        img = A.Image(n + 2 * pad, n + 2 * pad)

        def at(x, y, k=k):
            share = 0.0
            for sy in SUB:
                for sx in SUB:
                    d, f = shape.at(x + sx, y + sy)
                    share += (f == k) * A.cover(d) * (1.0 - A.cover(d + bevel))
            return (share / 9.0, lit) if share else None
        img.paint(at)
        out.append(A.Sprite("PRISM_LOADER_LIT_%d" % k, img, (pad,) * 4))
    return out


def named(sprite, name):
    sprite.name = name
    return sprite


# The download's bar in Prisma's own cut, in place of art.py's Shard rod (liq_bars), in the
# delivered bar's box and pads; the crystal in the scheme's bar tone as art.py turns the teal (bar_turned).
def stone_tones(tone):
    """A stone cut from tone: its eight bevel facets (from the top clockwise, lit from the upper left), its table's
    upper and lower tone, its light and its dark edge."""
    white = (255, 255, 255)
    facet = [mix(tone, white, 0.50), mix(tone, white, 0.28), mix(tone, BLACK, 0.08), mix(tone, BLACK, 0.30),
             mix(tone, BLACK, 0.38), mix(tone, BLACK, 0.18), mix(tone, white, 0.22), mix(tone, white, 0.60)]
    return facet, (mix(tone, white, 0.14), mix(tone, BLACK, 0.06)), mix(tone, white, 0.75), mix(tone, BLACK, 0.55)


def progress(A, t):
    """The bar: a channel cut in the plate with a point at each end (its bevel turned over: in shade above, catching
    the light below), and lying in it a crystal cut from the bar's tone with a point at each end (as the scroll bar's
    thumb: a table inside a bevel of facets lit from the upper left), one pixel in."""
    L = A.L
    tone = A.bar_turned(A.rgb(L.COL_GREEN))
    h = L.PROGRESS_H
    white = (255, 255, 255)

    def track(img, b):
        shape = Cut(b.x0, b.y0, b.x1, b.y1, (h / 2.0,) * 4)
        shade, lit = mix(t["off"], t["plate_dark"], 0.40), mix(t["off"], white, 0.55)
        stone(A, img, shape, 1.5, [shade, shade, t["off"], lit, lit, lit, t["off"], shade], lambda x, y: t["off"],
              t["edge_dark"], t["edge_dark"])

    def fill(img, b):
        facet, table, light, dark = stone_tones(tone)
        shape = Cut(b.x0 + 1.0, b.y0 + 1.0, b.x1 - 1.0, b.y1 - 1.0, (h / 2.0 - 1.0,) * 4)
        stone(A, img, shape, 2.5, facet, table_of(table[0], table[1], b.y0 + 3.5, b.y1 - 3.5), light, dark)
    return [A.across("PROGRESS_SHARD", h, 6, (3, 2, 3, 5), track),
            A.across("PROGRESS_FILL_SHARD", h, 8, A.NO_PAD, fill)]


def sprites(items, index):
    """art.py's images for scheme index (which A.L is drawn in) with this look's in the place of those that differ,
    then OWN_SPRITES."""
    L, t = A.L, tones(index)
    big, small = (L.PRISM_CUT_BIG, L.PRISM_BEVEL_BIG), (L.PRISM_CUT_SMALL, L.PRISM_BEVEL_SMALL)
    own = [
        face(A, "TILE", L.TILE_H, *big, t), face(A, "TILE_DOWN", L.TILE_H, *big, t, "down"),
        face(A, "DTILE", L.DTILE_H, *big, t), face(A, "DTILE_DOWN", L.DTILE_H, *big, t, "down"),
        face(A, "WIDE", L.WIDE_H, *big, t), face(A, "WIDE_DOWN", L.WIDE_H, *big, t, "down"),
        face(A, "TILE_OFF", L.TILE_H, *big, t, "off"),
        face(A, "CHOICE", L.CHOICE_H, *big, t), face(A, "CHOICE_DOWN", L.CHOICE_H, *big, t, "down"),
        face(A, "ROW", L.ROW_H, *small, t), face(A, "ROW_DOWN", L.ROW_H, *small, t, "down"),
        face(A, "ROW_OFF", L.ROW_H, *small, t, "off"),
        face(A, "FOLDER_ROW", L.FOLDER_H, *small, t), face(A, "FOLDER_ROW_DOWN", L.FOLDER_H, *small, t, "down"),
        face(A, "SET_ROW", L.SET_H, *small, t), face(A, "SET_ROW_DOWN", L.SET_H, *small, t, "down"),
        face(A, "GO", L.GO_H, *big, t, kind="go"), face(A, "GO_DOWN", L.GO_H, *big, t, "down", kind="go"),
        face(A, "ALT", L.GO_H, *big, t), face(A, "ALT_DOWN", L.GO_H, *big, t, "down"),
        bar(A, "BAR", t, False), bar(A, "BAR_DOWN", t, True), *tabs(A, t),
        plate(A, "CARD", t),
        corner(A, "CORNER", t, "up"), corner(A, "CORNER_DOWN", t, "down"), corner(A, "CORNER_OFF", t, "off"),
        help_button(A, "HELP", t, False), help_button(A, "HELP_DOWN", t, True),
        track(A, t), thumb(A, "THUMB", t, False), thumb(A, "THUMB_DOWN", t, True),
        dialog_panel(A, t), dialog_button(A, "DLG_BUTTON", t, False), dialog_button(A, "DLG_BUTTON_DOWN", t, True),
        dialog_divider(A, "DLG_DIVIDER", t, -1), dialog_divider(A, "DLG_DIVIDER_LEFT_DOWN", t, 0),
        dialog_divider(A, "DLG_DIVIDER_RIGHT_DOWN", t, 1),
        keycap(A, "KEY_A", "A"), keycap(A, "KEY_B", "B"), keycap(A, "KEY_Y", "Y"),
        keycap(A, "KEY_A_DOWN", "A", True), keycap(A, "KEY_B_DOWN", "B", True), keycap(A, "KEY_Y_DOWN", "Y", True),
        start_key(A, "START", False), start_key(A, "START_DOWN", True),
        start_key(A, "START_SHADOW", False, True), start_key(A, "START_DOWN_SHADOW", True, True),
        *progress(A, t),
        beam(A, t),
        gem(A, t), *loader_sprites(A, t, index), *ring_corners(A, t, "BIG", *big), *ring_corners(A, t, "SMALL", *small),
        ground_end(A, "PRISM_GROUND_END_TOP", 0, L.TOP_W),
        ground_end(A, "PRISM_GROUND_END_BOTTOM", L.PRISM_GROUND_ROW, L.BOTTOM_W),
    ]
    by_name = {s.name: s for s in own}
    out = [by_name.pop(s.name, s) for s in items]
    out += [by_name.pop(n) for n in OWN_SPRITES]
    assert not by_name, sorted(by_name)
    return out


# ---------------------------------------------------------------- the installing screen (its tile)
# The tile's liquid and pieces are in the look's pour tone (pour_tone: the loader's crystal tone, so it follows Farbe;
# schemes.py's COL_POUR is the neutral scheme's here and is not drawn).

def pour_tone(t):
    """The liquid's and the pieces' tone: the loader's crystal tone (accent's bead), its lightness moved
    away from the plate until it keeps schemes.POUR_MIN against the plate, the plate over the ground's deeper end
    and the tile's glass over the plate (schemes.py's rule for COL_POUR, on this look's plate)."""
    card = t["plate"]
    backs = (card, mix(card, t["ground_b"], 1.0 - t["plate_alpha"]), schemes.glass(card))
    l, c, h = schemes.oklch(t["bead"])
    step = 0.005 if schemes.luminance(card) < 0.2 else -0.005
    tone = t["bead"]
    while min(schemes.contrast(tone, b) for b in backs) < schemes.POUR_MIN:
        l += step
        tone = vivid(l, c, h)
    return tone
