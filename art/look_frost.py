"""Frost's images (art.py --look frost; look_frost.c draws with them): frosted glass over the eight colour schemes,
made for the Shard icons. The ground stays the scheme's colour with its pattern; what lies on it is glass.

Two kinds of glass, both drawn by slab():
  a pane    a thin frosted sheet for what only shows something (cards, the strips along a screen's edge, the
            popup's panel): the scheme's ground seen through milk (smoked in the dark scheme), a line of light
            along its upper edge, a faint dark one along its lower edge, a hairline round it and a soft shadow
  a button  a thick lozenge for what is pressed (tiles, rows, the buttons): the same glass a little whiter, with a
            bevel two pixels wide that catches the light above and lies in shade below, and a deeper shadow. Held,
            it sinks: no shadow, the bevel's light and shade change places, and the glass takes the scheme's held
            colour from inside.
The console draws no blur, so what frosted glass does to the pattern under it is drawn here too: per scheme and
pattern a second tile, the pattern's shapes blurred and thinned (ghost_tile, the blob "ghost"), which look_frost.c
lays over a pane's face at the pattern's own place, so a shape that drifts under a pane goes soft there. That tile
is a texture of its own, so the glass the letters stand on is drawn in three layers (the drawing rule, kit.h): its
body, the pattern under it, its rim (rim_of: what lies FROST_RIM px or less inside its edge, which the pattern never
reaches, so the three are pixel for pixel the glass drawn whole with the pattern over it).
The selection is the delivered look's four corners with their pulse (art.py select_corner), drawn here in the
scheme's own tone (T.accent): Frost has no colour that stays the same in every scheme except those that mean
something (the install green, the teal that brings in, the orange and red warnings) and the pictures' own.

Drawn through art.py's look hooks (art.py, LOOK); the pressed
tones are the scheme's own (schemes.py) and the carousel panes' rims are the sheet's (sheet). Python 3 standard
library only; the same input gives the same bytes."""
import math
import types
import zlib

import schemes

NAME = "frost"
A = None            # art.py's module (set by art.py before anything here is drawn)

WHITE = (255.0, 255.0, 255.0)
BLACK = (0.0, 0.0, 0.0)
TO_LIGHT = (-0.26, -0.966)      # towards the light: above, a little to the left (as the Shard icons')
ICE = (150.0, 186.0, 232.0)     # Weiss's glass: milk does not show on white
ACCENT_C = 0.13                 # the chroma of the design's coloured tones (OKLCH; sRGB's most where it has less)

# the selection's corners as art.py draws them (SELECT_SHAPES: item corner radius, arm tail), at rest and out, top
# left, top right, bottom left, bottom right: in the order ui.c counts art.py's own (corner_id)
CORNER_SHAPES = ((0, 3), (10, 4), (10, 8), (16, 3), (16, 8))
CORNER_NAMES = tuple("FROST_CORNER_R%d_T%d_%s_%s" % (r, t, pos, tone) for r, t in CORNER_SHAPES
                     for tone in ("IN", "OUT") for pos in ("TL", "TR", "BL", "BR"))
# the glass the pattern shows through (look_frost.c, FROST_GLASS), each with its rim drawn after the pattern: the
# delivered names (their bodies) and how much of the face's side shows below it (0: a thin pane)
GLASS = (("TILE", "TILE"), ("DTILE", "TILE"), ("WIDE", "TILE"), ("CHOICE", "TILE"), ("ALT", "TILE"), ("GO", "TILE"),
         ("ROW", "ROW"), ("FOLDER_ROW", "ROW"), ("SET_ROW", "ROW"))
GLASS_NAMES = tuple(n + held for n, _ in GLASS for held in ("", "_DOWN")) + ("TILE_OFF", "ROW_OFF", "CARD",
                                                                            "CARD_GLASS_SHARD")
BAR_NAMES = ("BAR", "BAR_DOWN", "TAB_L_DOWN", "TAB_R_DOWN")
STRIP_NAMES = ("FROST_STRIP_TOP", "FROST_STRIP_LOW")
RIM_NAMES = tuple("FROST_RIM_" + n for n in GLASS_NAMES + BAR_NAMES + STRIP_NAMES)
# the sprites only this look has, in the atlas after the common ones
OWN_SPRITES = STRIP_NAMES + ("FROST_DLG_BTN", "FROST_DLG_BTN_DOWN") + RIM_NAMES + CORNER_NAMES + (
    "FROST_LOADER_DIAL",) + tuple("FROST_LOADER_LIT_%d" % i for i in range(8))
# the colours only this look has, in the colour table after ui_layout.h's
OWN_COLOURS = ("COL_FROST_LABEL", "COL_FROST_LABEL_HELD")

GHOST_SIGMA = 1.7       # the blur of the pattern under a pane, px
BODY_MIN = 4.6          # the contrast text keeps on glass (4.5 and a little for what lies under the glass)


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def hex_of(c):
    return (int(round(c[0])) << 16) | (int(round(c[1])) << 8) | int(round(c[2]))


def lum(c):
    return schemes.luminance(tuple(min(255.0, max(0.0, v)) for v in c))


def ratio(a, b):
    ya, yb = lum(a), lum(b)
    return (max(ya, yb) + 0.05) / (min(ya, yb) + 0.05)


def ink_on(ink, grounds, least=BODY_MIN):
    """ink, moved away from the grounds (darker on light ones, lighter on dark ones) until it has the contrast
    `least` on each; unchanged when it has it."""
    dark = lum(grounds[0]) < 0.2
    for k in range(101):
        c = mix(ink, WHITE if dark else BLACK, k / 100.0)
        if all(ratio(c, g) >= least for g in grounds):
            return c
    return WHITE if dark else BLACK


def tokens():
    """The drawn scheme's glass: every colour and strength the sprites are drawn with."""
    L = A.L
    c = lambda name: tuple(float(v) for v in A.rgb(getattr(L, name)))
    T = types.SimpleNamespace()
    # the held glass in the scheme's own pressed tone (schemes.py: Weiss a cool light grey, Nacht a lighter night
    # blue), never the delivered yellow
    T.ground, T.shadow, T.held = c("COL_BG"), c("COL_SHADOW"), c("COL_PRESS_TOP")
    T.dark, white = lum(T.ground) < 0.2, lum(T.ground) > 0.93
    # the scheme's own hue (a white ground has none: its glass is ice) and from it the design's coloured tones: the
    # selection's corners at rest and out, the carousel panes' rims (dark to light)
    hue = schemes.oklch(ICE if white else T.ground)[2]
    tone = lambda y: tuple(float(v) for v in schemes.colour_at(hue, ACCENT_C, y))
    if T.dark:
        T.accent, T.accent_out, T.frame = tone(0.40), tone(0.55), (tone(0.22), tone(0.50))
    else:
        T.accent, T.accent_out, T.frame = tone(0.19), tone(0.27), (tone(0.17), tone(0.42))
    deep = lambda k: tuple(min(255.0, max(0.0, g + (d - g) * k)) for g, d in zip(T.ground, c("COL_PATTERN_DARK")))
    if T.dark:                              # smoked glass: a little light in the dark
        T.pane = (mix(T.ground, WHITE, 0.105), mix(T.ground, WHITE, 0.07))
        T.face = (mix(T.ground, WHITE, 0.16), mix(T.ground, WHITE, 0.10))
        T.off = (mix(T.ground, WHITE, 0.05), mix(T.ground, WHITE, 0.04))
        T.rim, T.glow, T.hair, T.shade, T.edge = 0.34, 0.07, (BLACK, 0.54), (BLACK, 0.30), 0.16
        T.pane_shadow, T.face_shadow = (1.0, 5.0, 0.42), (2.5, 6.0, 0.62)
        T.held_face = (mix(T.face[0], T.held, 0.62), mix(T.face[1], T.held, 0.52))
        T.ghost, T.ghost_dark = (0.15, 0.30), BLACK
        T.panel = (mix(T.ground, WHITE, 0.25), mix(T.ground, WHITE, 0.16))
        T.lip, T.lip_hair, T.caustic, T.pool = (mix(T.ground, BLACK, 0.24), mix(T.ground, BLACK, 0.44)), 0.55, 0.30, 0.10
    else:
        if white:                           # a white ground: milk does not show on it, the glass is ice
            milk = mix(T.ground, ICE, 0.22)
            T.pane = (mix(milk, WHITE, 0.50), mix(milk, WHITE, 0.30))
            T.face = (mix(milk, WHITE, 0.45), milk)
            T.lip = (mix(T.ground, ICE, 0.50), mix(T.ground, ICE, 0.72))
            T.panel = (mix(milk, WHITE, 0.75), mix(milk, WHITE, 0.35))
        else:
            T.pane = (mix(T.ground, WHITE, 0.56), mix(T.ground, WHITE, 0.42))
            T.face = (mix(T.ground, WHITE, 0.66), mix(T.ground, WHITE, 0.40))
            T.lip = (deep(1.8), deep(2.4))
            T.panel = (mix(T.ground, WHITE, 0.84), mix(T.ground, WHITE, 0.62))
        T.lip_hair, T.caustic, T.pool = 0.18, 0.85, 0.40
        T.off = (mix(T.ground, T.pane[1], 0.45), mix(T.ground, T.pane[1], 0.35))
        T.rim, T.glow, T.hair, T.shade, T.edge = 0.92, 0.30, (T.shadow, 0.25), (T.shadow, 0.13), 0.70
        T.pane_shadow, T.face_shadow = (1.0, 5.0, 0.10), (2.5, 6.0, 0.20)
        T.held_face = (mix(T.face[0], T.held, 0.72), mix(T.face[1], T.held, 0.82))
        T.ghost, T.ghost_dark = (0.80, 0.24), mix(T.ground, T.shadow, 0.55)
    T.go = (mix(c("COL_GO_TOP"), WHITE, 0.06), c("COL_GO_LOW"))
    T.go_held = (c("COL_GO_LOW"), mix(c("COL_GO_TOP"), c("COL_GO_LOW"), 0.4))
    T.go_lip = (mix(c("COL_GO_LOW"), BLACK, 0.25), mix(c("COL_GO_LOW"), BLACK, 0.45))
    return T


def seen(T, tones):
    """The tones of a glass as text finds them: plain, and with the pattern's shapes under the glass at their
    strongest (the dark ones under dark ink, the light ones under light ink)."""
    return list(tones) + [mix(t, WHITE, T.ghost[0]) if T.dark else mix(t, T.ghost_dark, T.ghost[1]) for t in tones]


def colours():
    """The scheme's colours as this design has them, {name: 0xRRGGBB}: the inks with their contrast on the glass
    they are drawn on now (seen), and the look's own (OWN_COLOURS)."""
    L, T = A.L, tokens()
    c = lambda name: tuple(float(v) for v in A.rgb(getattr(L, name)))
    panes, faces, off, held = seen(T, T.pane), seen(T, T.face), seen(T, T.off), seen(T, T.held_face)
    held_ink = mix(c("COL_PRESS_INK"), c("COL_INK"), 0.35)
    out = {
        "COL_INK": ink_on(c("COL_INK"), panes + faces),
        "COL_MUTED": ink_on(c("COL_MUTED"), panes + faces + off),
        "COL_TILE_INK": ink_on(c("COL_TILE_INK"), faces + panes),
        "COL_OFF_INK": ink_on(c("COL_OFF_INK"), off),
        "COL_PRESS_INK": ink_on(held_ink, held),
        "COL_ORANGE": ink_on(c("COL_ORANGE"), panes + faces),
        "COL_ALERT": ink_on(c("COL_ALERT"), panes + faces),
        "COL_TEAL": ink_on(c("COL_TEAL"), panes),
        "COL_DLG_INK": ink_on(mix(c("COL_INK"), WHITE, 0.75) if T.dark else mix(c("COL_INK"), BLACK, 0.30),
                              [T.panel[0], T.panel[1]]),
        "COL_FROST_LABEL": ink_on(c("COL_INK"), panes),
        "COL_FROST_LABEL_HELD": ink_on(held_ink, seen(T, [mix(T.pane[0], T.held, 0.78), mix(T.pane[1], T.held, 0.66)])),
    }
    return {n: hex_of(v) for n, v in out.items()}


# ---------------------------------------------------------------- glass


def slab(img, b, tones, T, bevel=2.0, rim=1.0, shadow=None, held=False, hair=1.0, glow=1.0, shade=1.0, edge=1.0,
         under=None, lip=None):
    """A piece of frosted glass in the box b: from tones[0] at its top to tones[1] at its bottom, milky light
    falling in from its upper edge (glow), the outermost pixel a hairline, inside it the polished edge: one pixel
    of light all the way round (edge) and a band bevel px wide that is light where the edge faces the light and in
    shade where it faces away (held: the other way round, the glass lies below its surroundings), and its shadow
    (drop, blur, alpha) outside the box. under: the glass is thick, b is its upper face and under the box it stands
    on: what of under shows below the face is the glass's polished side, in the deeper tone glass has seen edge
    on (lip, top and bottom), a line of light where face and side meet, and light pooled inside the face's
    lower edge in place of the shade."""
    top, low = tones
    foot = under or b
    if shadow:
        drop, blur, alpha = shadow
        sh = foot.moved(0, drop)
        img.paint(lambda x, y: (alpha * A.soft(sh.sd(x, y), blur) * (1.0 - A.cover(foot.sd(x, y))), T.shadow))
    lx, ly = (-TO_LIGHT[0], -TO_LIGHT[1]) if held else TO_LIGHT
    hair_c, hair_a = T.hair
    shade_c, shade_a = T.shade
    h = b.y1 - b.y0
    if under:
        lip = lip or T.lip
        side_h = max(1.0, under.y1 - b.y1)

        def side(x, y):
            d = under.sd(x, y)
            c = A.cover(d)
            if c <= 0.0:
                return None
            col = mix(lip[0], lip[1], min(1.0, max(0.0, (y - b.y1 + 1.0) / side_h)))
            ring = max(0.0, c - A.cover(d + 1.0)) / c
            return (c, mix(col, mix(lip[1], hair_c, T.lip_hair), 0.9 * ring))
        img.paint(side)

    def body(x, y):
        d = b.sd(x, y)
        c = A.cover(d)
        if c <= 0.0:
            return None
        col = mix(top, low, min(1.0, max(0.0, (y - b.y0) / h)))
        e = -d
        if not held:
            col = mix(col, WHITE, T.glow * glow * math.exp(-max(0.0, y - b.y0 - 1.0) / 5.0))
        if e < bevel + 4.0:
            nx, ny = b.sd(x + 0.5, y) - b.sd(x - 0.5, y), b.sd(x, y + 0.5) - b.sd(x, y - 0.5)
            n = math.hypot(nx, ny) or 1.0
            lit = (nx * lx + ny * ly) / n
            fall = 1.0 - A.smooth(0.0, bevel, e - 1.0)
            down = A.smooth(0.15, 0.75, ny / n) if under else 0.0
            if lit > 0:
                col = mix(col, WHITE, min(1.0, T.rim * rim * lit) * fall)
            else:
                col = mix(col, shade_c, min(1.0, shade_a * shade * -lit * (2.2 if held else 1.0)) * fall * (1.0 - down))
                col = mix(col, WHITE, T.pool * down * (1.0 - A.smooth(0.0, 4.0, e - 1.0)))
            if not held:        # the polished edge's own line of light, on the pixel inside the hairline
                col = mix(col, WHITE, T.edge * edge * max(0.0, A.cover(d + 1.0) - A.cover(d + 2.0)) * (1.0 - down))
            ring = max(0.0, c - A.cover(d + 1.0)) / c
            col = mix(mix(col, hair_c, hair_a * hair * ring * (1.0 - 0.55 * max(0.0, lit))),
                      mix(col, WHITE, T.caustic * ring), down)
        return (c, col)
    img.paint(body)


def lip_of(r):
    """How much of a thick glass's side shows below its face, px: more on the large round ones (ui.c lays the
    pattern under the face alone, by the same numbers)."""
    return float(A.L.FROST_LIP_TILE if r >= 14 else A.L.FROST_LIP_ROW)


def thick(box, r):
    """The upper face of a thick glass standing in box, and the box itself, as rounded boxes."""
    return A.Box(box.x0, box.y0, box.x1, box.y1 - lip_of(r), r), A.rounded(box, r)


def sunk(box, r):
    """The face of a thick glass held down in box: sunk by the height of its side, one pixel in at the sides."""
    return A.Box(box.x0 + 1, box.y0 + lip_of(r) - 1, box.x1 - 1, box.y1, r - 1)


def button(T, name, h, r, kind="face", held=False):
    """A tile, row or button of any width (the delivered face's size and reach). kind: "face", "go" (the green
    button) or "off" (greyed)."""
    def draw(img, box):
        b = A.rounded(box, r)
        face, foot = thick(box, r)
        if kind == "off":
            slab(img, b, T.off, T, bevel=1.0, rim=0.35, hair=0.7, glow=0.0, shade=0.5)
        elif kind == "go":
            if held:
                slab(img, sunk(box, r), T.go_held, T, held=True, rim=0.30, glow=0.0, shade=1.6)
            else:
                slab(img, face, T.go, T, shadow=T.face_shadow, rim=0.50 / T.rim, glow=0.45, hair=1.4, shade=1.8,
                     under=foot, lip=T.go_lip)
        elif held:
            slab(img, sunk(box, r), T.held_face, T, held=True)
        else:
            slab(img, face, T.face, T, shadow=T.face_shadow, under=foot)
    return A.across(name, h, r, A.FACE_PAD, draw, 2)


def corner(T, name, state):
    """The corner button: a lozenge that hangs out of the screen's upper left corner, only its lower right corner
    round (the delivered one's size and reach)."""
    L, r = A.L, A.L.CORNER_R
    pad = A.NO_PAD if state == "off" else (0, 0, 4, 7)

    def draw(img, box):
        b = A.Box(box.x0 - 40, box.y0 - 40, box.x1, box.y1, (0, 0, r, 0))
        if state == "off":
            slab(img, b, T.off, T, bevel=1.0, rim=0.35, hair=0.7, glow=0.0, shade=0.5)
        elif state == "down":
            slab(img, A.Box(b.x0, b.y0, b.x1 - 1, b.y1, (0, 0, r - 1, 0)), T.held_face, T, held=True)
        else:
            slab(img, A.Box(b.x0, b.y0, b.x1, b.y1 - 3, (0, 0, r, 0)), T.face, T, shadow=T.face_shadow, glow=0.0,
                 under=b)
    return A.across(name, L.CORNER_H + L.FROST_TAB_GROW, r, pad, draw, 2)


def help_button(T, name, held):
    n, lip, pad = A.L.HELP_FACE, A.L.FROST_HELP_LIP, A.FACE_PAD
    img = A.Image(n + pad[0] + pad[2], n + pad[1] + pad[3])
    b = A.Box(pad[0], pad[1], pad[0] + n, pad[1] + n, n / 2.0)
    if held:
        slab(img, A.Box(b.x0 + 1, b.y0 + 2, b.x1 - 1, b.y1, n / 2.0 - 1), T.held_face, T, held=True)
    else:
        slab(img, A.Box(b.x0, b.y0, b.x1, b.y1 - lip, (n - lip) / 2.0), T.face, T, shadow=T.face_shadow, under=b)
    return A.Sprite(name, img, pad)


def card(T):
    """The card: a pane, any size."""
    L = A.L
    pad = (3, 2, 3, 6)

    def draw(img, box):
        slab(img, A.rounded(box, L.CARD_R), T.pane, T, bevel=1.0, shadow=T.pane_shadow, hair=0.8, shade=0.8)
    side = 2 * (L.CARD_R + 2) + A.MID
    return A.sliced("CARD", side, side, pad, (pad[0] + L.CARD_R + 2, pad[1] + L.CARD_R + 2, L.CARD_R + 2 + pad[2],
                                              L.CARD_R + 2 + pad[3]), draw)


def glass_card(T, like):
    """The card of the working screens (the delivered one's size, reach and fixed edges): a pane."""
    L, pad = A.L, like.pad
    img = A.Image(like.img.w, like.img.h)
    b = A.Box(pad[0], pad[1], img.w - pad[2], img.h - pad[3], L.CARD_R)
    slab(img, b, T.pane, T, bevel=1.0, shadow=T.pane_shadow, hair=0.8, shade=0.8)
    return A.Sprite(like.name, img, pad, like.cap)


def dialog(T, like):
    """The popup's panel (the delivered one's size, reach and fixed edges): a thick sheet of opal glass, whiter than
    a pane, so that its text reads whatever lies dimmed under it; its buttons stand on it (FROST_DLG_BTN)."""
    L, pad = A.L, like.pad
    img = A.Image(like.img.w, like.img.h)
    foot = A.Box(pad[0], pad[1], img.w - pad[2], img.h - pad[3], L.DLG_R)
    face = A.Box(foot.x0, foot.y0, foot.x1, foot.y1 - L.FROST_LIP_TILE, L.DLG_R)
    slab(img, face, T.panel, T, shadow=(2.0, float(L.DLG_SHADOW_BLUR), min(0.75, 2.2 * T.face_shadow[2])),
         under=foot, hair=1.2)
    return A.Sprite(like.name, img, pad, like.cap)


def strip(T, name, low):
    """A strip of frosted glass across a screen, any width and height: along the upper edge (it hangs from above,
    its lower edge with the dark line and the shadow) or along the lower one (it stands on the edge, the line of
    light along its top). The sprite reaches 6 px past the strip on its open side."""
    pad = (0, 6, 0, 0) if low else (0, 0, 0, 6)

    def draw(img, box):
        b = A.Box(box.x0 - 40, box.y0, box.x1 + 40, box.y1 + 40, 0) if low else \
            A.Box(box.x0 - 40, box.y0 - 40, box.x1 + 40, box.y1, 0)
        if low:      # the shadow of what stands on the edge falls on the ground above it only faintly
            sh = b.moved(0, -1.0)
            img.paint(lambda x, y: (T.pane_shadow[2] * 0.8 * A.soft(sh.sd(x, y), 4.0) * (1.0 - A.cover(b.sd(x, y))),
                                    T.shadow))
            slab(img, b, T.pane, T, bevel=1.0, hair=0.8)
        else:
            slab(img, b, (T.pane[1], T.pane[0]), T, bevel=1.0, shadow=T.pane_shadow, hair=0.8, glow=0.0)
    cap = (0, 10, 0, 3) if low else (0, 3, 0, 10)
    return A.sliced(name, A.MID + 2, 16, pad, (1, pad[1] + cap[1], 1, cap[3] + pad[3]), draw)


def bar(T, name, held, x0=None, x1=None):
    """The bottom screen's bar, the whole strip as one image (the delivered bar's size and reach), or with x0 and
    x1 a tab's share of it held down: the glass there lit in the held colour from inside, fading out towards the
    strip's middle."""
    L = A.L
    pad = L.BAR_SHADOW
    w = L.BOTTOM_W if x0 is None else L.TAB_W + L.BAR_SHADOW
    img = A.Image(w, L.BAR_H + pad)
    b = A.Box(-40, pad, w + 40, pad + L.BAR_H + 40, 0)
    if not held:
        sh = b.moved(0, -1.0)
        img.paint(lambda x, y: (T.pane_shadow[2] * 0.8 * A.soft(sh.sd(x, y), 4.0) * (1.0 - A.cover(b.sd(x, y))),
                                T.shadow))
        slab(img, b, T.pane, T, bevel=1.0, hair=0.8)
        return A.Sprite(name, img, (0, pad, 0, 0))
    lit = (mix(T.pane[0], T.held, 0.78), mix(T.pane[1], T.held, 0.66))
    slab(img, b, lit, T, bevel=1.0, hair=0.8, held=True)
    if x0 is not None:          # a tab: the light ends softly towards the middle of the strip
        inner = x1 if x0 <= 0 else x0
        for y in range(img.h):
            for x in range(w):
                k = A.smooth(0.0, 26.0, (inner - (x + 0.5)) if x0 <= 0 else ((x + 0.5) - inner))
                j = 4 * (y * w + x)
                for i in range(4):
                    img.p[j + i] *= k
    return A.Sprite(name, img, (0, pad, 0, 0))


def empty(name, like):
    """A sprite this design does not draw, kept by name: nothing, in a corner of the delivered one's box."""
    return A.Sprite(name, A.Image(2, 2), like.pad, (0, 0, 0, 0))


def corners(T):
    """The selection's corners (CORNER_NAMES): the delivered look's shapes and pulse, at rest in the scheme's tone
    and out in its lighter one."""
    if tuple(A.SELECT_SHAPES) != CORNER_SHAPES:
        raise SystemExit("look_frost.py: CORNER_SHAPES and art.py's SELECT_SHAPES differ")
    names, out = iter(CORNER_NAMES), []
    for radius, tail in CORNER_SHAPES:
        for tone in (T.accent, T.accent_out):
            for fx, fy in ((False, False), (True, False), (False, True), (True, True)):
                out.append(A.select_corner(next(names), radius, tail, fx, fy, hex_of(tone)))
    return out


def keycap(T, name, letter, held=False):
    """A key of the bar: a bead of glass with the letter in the label's ink (the delivered key's size and reach)."""
    L = A.L
    n, up = L.KEYCAP, L.LABEL_SHADOW_DY
    img = A.Image(n, n + up)
    s = n / 24.0
    b = A.Box(0.5, up + 0.5, n - 0.5, up + n - 0.5, (n - 1) / 2.0)
    slab(img, b, T.held_face if held else T.face, T, bevel=1.0, hair=1.5, held=held, glow=0.0)
    mark = A.coverage(img, A.lines_of(A.KEY_LETTERS[letter]), s, 2.5 * s, 0, up)
    cols = colours()
    ink = A.rgb(cols["COL_PRESS_INK"] if held else cols["COL_TILE_INK"])
    for k, v in enumerate(mark):
        if v > 0:
            img.over(k, ink, v)
    return A.Sprite(name, img, (0, up, 0, 0))


def start_key(T, name, held, shadow):
    """The START key's pill, any width: glass as the keys; its engraved copy (shadow) is nothing here."""
    L = A.L

    def draw(img, box):
        if not shadow:
            slab(img, A.rounded(box, L.START_R), T.held_face if held else T.face, T, bevel=1.0, hair=1.5, held=held,
                 glow=0.0)
    return A.across(name, L.START_H, L.START_R, A.NO_PAD, draw)


def track(T):
    """The scroll bar's track: a groove in the ground, any height."""
    L = A.L

    def draw(img, box):
        b = A.rounded(box, L.SCROLL_R)
        tone = mix(T.ground, T.shadow, 0.16 if not T.dark else 0.45)
        slab(img, b, (tone, mix(tone, WHITE, 0.10)), T, bevel=1.0, held=True, hair=0.7, glow=0.0)
    return A.upright("TRACK", L.SCROLL_W, L.SCROLL_R, A.NO_PAD, draw)


def thumb(T, name, held):
    L = A.L

    def draw(img, box):
        b = A.rounded(box, L.SCROLL_R)
        if held:
            slab(img, b, T.held_face, T, bevel=1.0, hair=1.3, glow=0.0, shadow=(1.0, 3.0, T.face_shadow[2]))
        else:
            slab(img, b, T.face, T, bevel=1.0, hair=1.3, glow=0.0, shadow=(1.0, 3.0, T.face_shadow[2]))
    return A.upright(name, L.SCROLL_W, L.SCROLL_R, (3, 1, 3, 4), draw)


# The download's bar in Frost's own glass, in place of art.py's Shard rod (liq_bars), in the
# delivered bar's box and pads; the glass in the scheme's bar tone as art.py turns the teal (bar_turned).
def progress(T):
    """The bar: a groove in the ground (as the scroll bar's track) and in it a lozenge of thick glass (as a button's:
    its face, its polished side below it, the bevel catching the light above), one pixel in all round."""
    L = A.L
    tone = tuple(float(v) for v in A.bar_turned(A.rgb(L.COL_GREEN)))
    h, r = L.PROGRESS_H, L.PROGRESS_R
    groove = mix(T.ground, T.shadow, 0.16 if not T.dark else 0.45)

    def track(img, b):
        slab(img, A.rounded(b, r), (groove, mix(groove, WHITE, 0.10)), T, bevel=1.0, held=True, hair=0.7, glow=0.0)

    def fill(img, b):
        foot = A.Box(b.x0 + 1.0, b.y0 + 1.0, b.x1 - 1.0, b.y1 - 1.0, r - 1.0)
        face = A.Box(foot.x0, foot.y0, foot.x1, foot.y1 - 2.0, r - 1.0)
        slab(img, face, (mix(tone, WHITE, 0.16), tone), T, bevel=1.5, rim=0.8, glow=0.5,
             under=foot, lip=(mix(tone, BLACK, 0.22), mix(tone, BLACK, 0.42)))
    return [A.across("PROGRESS_SHARD", h, r, (3, 2, 3, 5), track),
            A.across("PROGRESS_FILL_SHARD", h, r - 1, A.NO_PAD, fill, 1)]


def progress_stripes():
    """Frost a's stripes: the light at 45 degrees as frost passes it on, a soft wave (one period, STRIPE_W wide)."""
    L = A.L
    w, h, a = L.STRIPE_W, L.PROGRESS_H, L.STRIPE_ALPHA / 100.0
    img = A.Image(w, h)
    img.paint(lambda x, y: (a * (0.5 + 0.5 * math.cos(2 * math.pi * (x + y - 1.0) / w)) ** 1.6, WHITE))
    return A.Sprite("STRIPES", img)


def sprites(delivered, index):
    """The scheme's atlas in Frost (index: the scheme, which A.L is drawn in): the delivered sprites, those of the
    same name drawn here in their place (the glass the pattern shows through as its body), and its own after them (OWN_SPRITES: the strips' bodies, the popup's buttons, every glass's rim, the corners)."""
    L, T = A.L, tokens()
    faces = (("TILE", L.TILE_H, L.TILE_R), ("DTILE", L.DTILE_H, L.TILE_R), ("WIDE", L.WIDE_H, L.TILE_R),
             ("CHOICE", L.CHOICE_H, L.TILE_R), ("ROW", L.ROW_H, L.ROW_R), ("FOLDER_ROW", L.FOLDER_H, L.ROW_R),
             ("SET_ROW", L.SET_H, L.ROW_R), ("ALT", L.GO_H, L.GO_R))
    mine = []
    for name, h, r in faces:
        mine += [button(T, name, h, r), button(T, name + "_DOWN", h, r, held=True)]
    mine += [
        button(T, "TILE_OFF", L.TILE_H, L.TILE_R, "off"), button(T, "ROW_OFF", L.ROW_H, L.ROW_R, "off"),
        button(T, "GO", L.GO_H, L.GO_R, "go"), button(T, "GO_DOWN", L.GO_H, L.GO_R, "go", held=True),
        corner(T, "CORNER", "up"), corner(T, "CORNER_DOWN", "down"), corner(T, "CORNER_OFF", "off"),
        help_button(T, "HELP", False), help_button(T, "HELP_DOWN", True),
        card(T), track(T), thumb(T, "THUMB", False), thumb(T, "THUMB_DOWN", True),
        bar(T, "BAR", False), bar(T, "BAR_DOWN", True),
        bar(T, "TAB_L_DOWN", True, 0, L.TAB_EDGE_10 / 10.0),
        bar(T, "TAB_R_DOWN", True, L.TAB_W + L.BAR_SHADOW - L.TAB_EDGE_10 / 10.0, L.TAB_W + L.BAR_SHADOW),
        start_key(T, "START", False, False), start_key(T, "START_DOWN", True, False),
        start_key(T, "START_SHADOW", False, True), start_key(T, "START_DOWN_SHADOW", True, True),
        *progress(T), progress_stripes(),
    ]
    for letter in "ABY":
        mine += [keycap(T, "KEY_" + letter, letter), keycap(T, "KEY_%s_DOWN" % letter, letter, True)]
    drawn = {s.name: s for s in mine}
    out = []
    for s in delivered:
        if s.name in ("TAB_L", "TAB_R"):        # the bar is one strip: the tabs at rest are not drawn
            out.append(empty(s.name, s))
        elif s.name.startswith("DLG_"):         # the popup's buttons are lozenges on its panel, not a band
            out.append(empty(s.name, s))
        elif s.name == "DLG":
            out.append(dialog(T, s))
        elif s.name == "CARD_GLASS_SHARD":
            out.append(glass_card(T, s))
        elif s.name in ("LOADER_BEAD", "LOADER_BEAD_ON"):     # its loader is a dial of panes (loader_dial)
            out.append(empty(s.name, s))
        else:
            out.append(drawn.pop(s.name, s))
    if drawn:
        raise SystemExit("look_frost.py draws what the atlas does not name: %s" % sorted(drawn))
    own = [
        strip(T, "FROST_STRIP_TOP", False), strip(T, "FROST_STRIP_LOW", True),
        button(T, "FROST_DLG_BTN", L.FROST_DLG_BTN_H, L.ROW_R),
        button(T, "FROST_DLG_BTN_DOWN", L.FROST_DLG_BTN_H, L.ROW_R, held=True),
    ]
    bodies, rims = {}, {}
    for s in out + own:
        if s.name in GLASS_NAMES + BAR_NAMES + STRIP_NAMES:
            bodies[s.name], rims[s.name] = rim_of(s)
    out = [bodies.get(s.name, s) for s in out]
    own = [bodies.get(s.name, s) for s in own] + [rims[n[len("FROST_RIM_"):]] for n in RIM_NAMES] + corners(T)
    own += [loader_dial(T)] + [loader_lit(T, i) for i in range(L.LOADER_DOTS)]
    if tuple(s.name for s in own) != OWN_SPRITES:
        raise SystemExit("look_frost.py: OWN_SPRITES and the sprites drawn differ")
    return out + own


def rim_band(name):
    """How many rows of the element the glass name keeps for its rim, drawn after the pattern under it: the rows at
    its top the pattern never reaches (look_frost.c, FROST_GLASS and frost_ghost: FROST_RIM, held the face sunk by
    its side below it, one less), or a strip's and the bar's FROST_STRIP_RIM on the screen's side; the rim is at the
    element's lower edge for the strip hanging from above (FROST_STRIP_TOP), else at its upper one."""
    L = A.L
    if name in BAR_NAMES + STRIP_NAMES:
        return L.FROST_STRIP_RIM
    lips = dict((n, L.FROST_LIP_TILE if k == "TILE" else L.FROST_LIP_ROW) for n, k in GLASS)
    down = name.endswith("_DOWN")
    return L.FROST_RIM + (lips[name[:-5]] - 1 if down else 0)


def rim_of(sprite):
    """The glass sprite as its body (without the rim's rows) and its rim (FROST_RIM_<name>: those rows, with the
    sprite's reach beyond them on that side, as wide as the sprite and stretched across as it is): drawn one over
    the other in their places they are the sprite, every pixel in one of them, and the pattern laid over the body
    between them reaches nothing of the rim. The rim is an image of the atlas with real pixels (the glass's edge),
    so the letters on the glass follow the atlas (the drawing rule, kit.h)."""
    img, pad, cap = sprite.img, sprite.pad, sprite.cap
    low = sprite.name == "FROST_STRIP_TOP"
    rows = rim_band(sprite.name) + (pad[3] if low else pad[1])
    if rows > (cap[3] if low else cap[1]) and (cap[1] or cap[3]):
        raise SystemExit("look_frost.py: %s's rim reaches past its fixed edge" % sprite.name)
    y0 = img.h - rows if low else 0
    body, rim = A.Image(img.w, img.h), A.Image(img.w, rows)
    body.p[:] = img.p
    body.p[4 * y0 * img.w:4 * (y0 + rows) * img.w] = [0.0] * (4 * rows * img.w)
    rim.p[:] = img.p[4 * y0 * img.w:4 * (y0 + rows) * img.w]
    edge = (pad[0], 0, pad[2], pad[3]) if low else (pad[0], pad[1], pad[2], 0)
    return A.Sprite(sprite.name, body, pad, cap), A.Sprite("FROST_RIM_" + sprite.name, rim, edge,
                                                           (cap[0], 0, cap[2], 0))


def layout(index):
    """The scheme index as Frost draws it: the yellow original in the scheme's colours, with Frost's own numbers
    (look_frost.h)."""
    L = A.scheme_layout(A.D, index)
    for n, v in vars(A.read_layout(A.HEADERS / "look_frost.h")).items():
        if not n.startswith("__"):
            setattr(L, n, v)
    return L


def sheet(names):
    """The scheme's sheet: the Shard one as every look without its own has it."""
    made = {s.name: s for s in A.glass_sheet("shard")}
    return [made[name] for name in names]


def blobs(index):
    """The scheme's tiles of the pattern under glass (ghost_tile), one per pattern, zlib-compressed in the GPU's
    order: ART_FROST_GHOST_Z[ART_SCHEMES][ART_PATTERNS]."""
    return {"ghost": [zlib.compress(A.tile_order(ghost_tile(shapes), A.L.PATTERN_TILE), 9) for shapes in A.PATTERNS]}


# ---------------------------------------------------------------- the loader
# Frost's own, frosted glass tiles: eight small panes of frosted glass set like a dial, DIAL_W x DIAL_H with
# corners DIAL_R, their middles FROST_LOADER_DIAL_R2 / 2 px from the loader's middle, each pointing at it; one lit
# from inside in the scheme's loader tone, going round as Stock's beads (look_frost.c, loader). One picture holds the
# eight at rest (FROST_LOADER_DIAL: LOADER_BOX and DIAL_PAD round it for the shadows and the light); each lit one is a
# picture of its own, FROST_LOADER_LIT px square round its pane's middle (FROST_LOADER_LIT_0 to _7), drawn over the
# rest as solid as it glows. The glass is drawn as the other glass of the look is, from the scheme's tokens (tokens());
# the light comes from the upper left of the screen for every pane, whichever way it points. A pane is a few px
# wide, so its layers are laid over each other at DIAL_SAMPLES x DIAL_SAMPLES points of each pixel with hard edges
# and then averaged (stacked): on its edges the light edge covers the hairline where it lies, as on the larger glass.
# The masks are signed distances; a soft edge (a blur) is A.soft of the distance, and a blurred band along an edge
# (the part of the pane its copy moved by v does not cover) the pane's soft edge less the soft edge of the two's
# overlap.

DIAL_W, DIAL_H, DIAL_R = 5.5, 11.0, 2.4
DIAL_PAD = 6
DIAL_LIP, DIAL_BEVEL = 0.9, 0.9
DIAL_SAMPLES = 4
DIAL_REACH = 7.0        # a pane's shadow and light end this far outside it
# strengths on the tokens, so the panes read alike in every scheme (light schemes, Nacht where two): the
# shadow (of T.face_shadow's alpha), the hairline (of T.hair's; its tone this far towards the scheme's T.frame[0]),
# the light edge (of T.rim; white this far towards the ground), the milky sheen high on the left (of T.glow)
DIAL_SHADOW, DIAL_SHEEN = 1.25, 1.8
DIAL_HAIR, DIAL_HAIR_TINT = 1.35, (0.5, 0.1)
DIAL_RIM, DIAL_RIM_TINT = (1.2, 2.35), 0.35
DIAL_HALO = (0.36, 0.50)        # the lit pane's halo in the tone: light schemes, Nacht


def dial_place(i):
    """Pane i's middle (from the top, clockwise) in LOADER_BOX's px, and how far it is turned (radians)."""
    L = A.L
    a = 2 * math.pi * i / L.LOADER_DOTS - math.pi / 2
    r, c = L.FROST_LOADER_DIAL_R2 / 2.0, (2 * L.LOADER_BEAD_R + L.LOADER_BEAD) / 2.0
    return c + r * math.cos(a), c + r * math.sin(a), 2 * math.pi * i / L.LOADER_DOTS


def dial_at(i):
    """The corner of pane i's lit picture in LOADER_BOX's px, as look_frost.c places it (whole px; a quarter px added
    so that no middle lies on a pixel's edge, whatever the rounding of its sine)."""
    x, y, _ = dial_place(i)
    half = A.L.FROST_LOADER_LIT // 2
    return math.floor(x + 0.25) - half, math.floor(y + 0.25) - half


def dial_pane(i, ox, oy):
    """Pane i's signed distance at a point of a picture whose corner lies at ox, oy of LOADER_BOX."""
    px, py, turn = dial_place(i)
    c, s = math.cos(turn), math.sin(turn)
    box = A.Box(-DIAL_W / 2, -DIAL_H / 2, DIAL_W / 2, DIAL_H / 2, DIAL_R)

    def sd(x, y):
        dx, dy = x + ox - px, y + oy - py
        return box.sd(dx * c + dy * s, -dx * s + dy * c)
    return sd


def inside(d):
    return 1.0 if d < 0.0 else 0.0


def band(sd, vx, vy, blur):
    """The edge of a pane its copy moved by vx, vy leaves uncovered, blurred (blur: A.soft's, twice the sigma)."""
    return lambda x, y: max(0.0, A.soft(sd(x, y), blur) - A.soft(max(sd(x, y), sd(x - vx, y - vy)), blur))


def stacked(img, sd, layers):
    """The layers ((x, y) -> (alpha, colour)) laid over each other at DIAL_SAMPLES x DIAL_SAMPLES points of each
    pixel no further than DIAL_REACH from the pane (sd), averaged, and painted over img."""
    n = DIAL_SAMPLES
    steps = [-0.5 + (k + 0.5) / n for k in range(n)]

    def pixel(u, v):
        if sd(u, v) > DIAL_REACH:
            return None
        pa = pr = pg = pb = 0.0
        for dy in steps:
            for dx in steps:
                a = r = g = b = 0.0
                for layer in layers:
                    k, c = layer(u + dx, v + dy)
                    if k > 0.0:
                        k = min(k, 1.0)
                        j = 1.0 - k
                        a, r, g, b = k + a * j, c[0] * k + r * j, c[1] * k + g * j, c[2] * k + b * j
                pa, pr, pg, pb = pa + a, pr + r, pg + g, pb + b
        return (pa / (n * n), (pr / pa, pg / pa, pb / pa)) if pa > 0.0 else None
    img.paint(pixel)


def dial_glass(img, T, sd, y0, y1):
    """One pane at rest: its shadow, the lip under it, the face from T.face[0] at y0 to T.face[1] at y1, the sheen high
    on the left, the shade low on the right, the hairline, the light edge high on the left (as slab's glass)."""
    lip = DIAL_LIP
    sheen, shade = band(sd, 2.2, 2.6, 2.6), band(sd, -DIAL_BEVEL, -DIAL_BEVEL, 1.0)
    rim = band(sd, DIAL_BEVEL, DIAL_BEVEL, 0.6)
    k_shadow, k_sheen = DIAL_SHADOW * T.face_shadow[2], min(1.0, DIAL_SHEEN * T.glow)
    k_hair, k_rim = min(1.0, DIAL_HAIR * T.hair[1]), min(1.0, DIAL_RIM[T.dark] * T.rim)
    hair, light = mix(T.hair[0], T.frame[0], DIAL_HAIR_TINT[T.dark]), mix(WHITE, T.ground, DIAL_RIM_TINT)
    face = lambda x, y: mix(T.face[0], T.face[1], min(1.0, max(0.0, (y - y0) / (y1 - y0))))
    stacked(img, sd, (
        lambda x, y: (k_shadow * A.soft(sd(x, y - lip - 0.6), 2.6), T.shadow),
        lambda x, y: (inside(sd(x, y - lip)), T.lip[1]),
        lambda x, y: (inside(sd(x, y)), face(x, y)),
        lambda x, y: (k_sheen * inside(sd(x, y)) * sheen(x, y), WHITE),
        lambda x, y: (T.shade[1] * inside(sd(x, y)) * shade(x, y), T.shade[0]),
        lambda x, y: (k_hair * (inside(sd(x, y)) - inside(sd(x, y) + 0.7)), hair),
        lambda x, y: (k_rim * inside(sd(x, y)) * rim(x, y), light)))


def dial_lit(img, T, sd, cx, cy):
    """One pane lit from inside in the scheme's loader tone: a halo round it, the body in the tone, a lighter core
    high in it (round cx, cy), the light edge kept; on the light schemes a deeper hairline."""
    tone = tuple(float(v) for v in A.rgb(A.L.COL_LOADER))
    core = lambda x, y: A.soft(math.hypot(x - cx, y - cy) - 2.4, 2.0)
    edge = band(sd, DIAL_BEVEL, DIAL_BEVEL, 0.9)
    halo = lambda x, y: (DIAL_HALO[T.dark] * A.soft(sd(x, y), 3.8), tone)
    if T.dark:
        layers = (halo, lambda x, y: (inside(sd(x, y)), tone),
                  lambda x, y: (0.95 * inside(sd(x, y)) * core(x, y), mix(tone, WHITE, 0.62)),
                  lambda x, y: (0.55 * inside(sd(x, y)) * edge(x, y), WHITE))
    else:
        layers = (halo, lambda x, y: (inside(sd(x, y)), mix(tone, BLACK, 0.10)),
                  lambda x, y: (0.90 * inside(sd(x, y)) * core(x, y), mix(tone, WHITE, 0.30)),
                  lambda x, y: (0.75 * (inside(sd(x, y)) - inside(sd(x, y) + 0.7)), mix(tone, BLACK, 0.34)),
                  lambda x, y: (0.70 * inside(sd(x, y)) * edge(x, y), WHITE))
    stacked(img, sd, layers)


def loader_dial(T):
    """The eight panes at rest round LOADER_BOX (FROST_LOADER_DIAL)."""
    L = A.L
    box = 2 * L.LOADER_BEAD_R + L.LOADER_BEAD
    img = A.Image(box + 2 * DIAL_PAD, box + 2 * DIAL_PAD)
    for i in range(L.LOADER_DOTS):
        _, y, _ = dial_place(i)
        dial_glass(img, T, dial_pane(i, -DIAL_PAD, -DIAL_PAD), y + DIAL_PAD - 5.0, y + DIAL_PAD + 5.0)
    return A.Sprite("FROST_LOADER_DIAL", img, (DIAL_PAD,) * 4)


def loader_lit(T, i):
    """Pane i lit, FROST_LOADER_LIT px square at dial_at(i) (FROST_LOADER_LIT_<i>)."""
    n = A.L.FROST_LOADER_LIT
    ox, oy = dial_at(i)
    x, y, _ = dial_place(i)
    img = A.Image(n, n)
    dial_lit(img, T, dial_pane(i, ox, oy), x - ox - 0.4, y - oy - 0.8)
    return A.Sprite("FROST_LOADER_LIT_%d" % i, img)


# ---------------------------------------------------------------- the pattern under glass


def ghost_tile(shapes):
    """A pattern's tile as it shows through a pane, straight RGBA bytes row by row: the shapes blurred (a Gaussian,
    GHOST_SIGMA, round the tile's edges as the tile repeats), the light ones white and the dark ones the scheme's
    shadow, each thinned to the share T.ghost has for it; laid over a pane's face at the pattern's own place."""
    L, T = A.L, tokens()
    n = L.PATTERN_TILE
    wrapped = lambda v: (v + n / 2.0) % n - n / 2.0
    maps = {1: [0.0] * (n * n), -1: [0.0] * (n * n)}
    for py in range(n):
        for px in range(n):
            d, tone = min((sd(wrapped(px + 0.5 - cx), wrapped(py + 0.5 - cy)), t) for cx, cy, sd, t in shapes)
            maps[1 if tone > 0 else -1][py * n + px] = A.cover(d)
    reach = int(math.ceil(3 * GHOST_SIGMA))
    k = [math.exp(-0.5 * (i / GHOST_SIGMA) ** 2) for i in range(-reach, reach + 1)]
    k = [v / sum(k) for v in k]

    def blur(m):
        tmp = [sum(m[y * n + (x + i - reach) % n] * w for i, w in enumerate(k)) for y in range(n) for x in range(n)]
        return [sum(tmp[((y + i - reach) % n) * n + x] * w for i, w in enumerate(k)) for y in range(n)
                for x in range(n)]
    light, dark = blur(maps[1]), blur(maps[-1])
    dark_c = T.ghost_dark
    out = bytearray()
    for i in range(n * n):
        al, ad = T.ghost[0] * light[i], T.ghost[1] * dark[i]
        a = al + ad
        col = mix(dark_c, WHITE, al / a) if a > 1e-6 else WHITE
        out += bytes(int(round(min(255.0, max(0.0, v)))) for v in col) + bytes([int(round(min(1.0, a) * 255))])
    return bytes(out)
