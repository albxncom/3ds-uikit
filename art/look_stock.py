"""Stock's images (art.py --look stock; the look every build holds, and its names the ones every look draws): the
look of the console's System Settings in the colour schemes as they are, without any glass. Every sprite keeps its
name, size (the settings rows' faces aside: set_row_face_h) and place in the code; what is glass in the other looks is
drawn here instead: Tabler line icons on buttons and tiles, cut into their face (engraved), flat
badges, marks, bars and frames, and per scheme the start screen's word and turning house, raised. art.py calls in
here through its look hooks (art.py, LOOK); its own images stay as they are. The application's images in Stock (the
badges, the step marks, the word, the turning house, the panes, the installing screen's) are drawn by the
application's half (art.py, APP) with the helpers here.

Engraved: a symbol is a groove in the surface it stands on, lit from above: its stroke in its colour, the groove's
upper wall in shade (the stroke's upper edge darker), its lower wall lit (the stroke's lower edge lighter, and a light
line under the stroke where the ground lets one show), at the lightest of the three strengths tried
(the one that reads best on the console)."""
NAME = "stock"
OWN_SPRITES = ()    # every name Stock draws is a common one, which the other looks draw too
OWN_COLOURS = ()
A = None            # art.py's module (set by art.py before anything here is drawn)
# the engraving: the light under the stroke (alpha of COL_SYMBOL_LIGHT), the shade on its upper edge (alpha of
# black), the light on its lower edge (alpha of white), and how far into the stroke each edge reaches, in px
LIT, SHADE, LOW, DEEP = 0.60, 0.22, 0.18, 1.0
BLACK = (0.0, 0.0, 0.0)
# The start screen's word and house, raised: soft as the buttons, lit from above, a soft shadow under them; a raised
# plateau: a narrow bevel, a fine light along the upper edges, a shade along the lower
# ones. How far its bevel reaches (the blur radius its slope is taken from, px), the light on what faces up (alpha of
# white), the shade on what faces down and on the sides (alpha of black), the face lighter at its top by lift
# (towards white).
SURFACE = dict(reach=1, light=0.55, shade=0.30, side=0.10, lift=0.12)
# the shadow under them, as under a tile: SHADOW_DOWN px down, blurred by SHADOW_BLUR px, SHADOW_ALPHA of COL_SHADOW
SHADOW_DOWN, SHADOW_BLUR, SHADOW_ALPHA = 2, 1, 0.40
# The settings rows' faces: ui.c face_tall draws a row taller than its image with the image's one middle
# pixel row stretched, which at SET_H (34) was a flat band of 9 to 10 px in the face's gradient (the rows are
# layout.h SET_ROW_H: 43 with four rows or more, SET_H_MAX with fewer). Stock's image is 1 px lower than the lowest
# row instead, so that row repeats at most twice and the face's run is the one material() grades at that height.
def set_row_face_h():
    L = A.L
    room = L.BAR_Y - L.BAR_SHADOW - L.SET_AIR - L.SET_Y
    return min((room - (L.SET_SHOWN - 1) * L.SET_GAP) // L.SET_SHOWN, L.SET_H_MAX) - 1


# sprites that only the other glass styles draw (the glass choice is Shard's slot in Stock, ui.c): kept at their
# size, empty, so the tables stay alike in every look and no glass is left in the build (the application's half
# blanks its own the same way)
UNUSED = {"PROGRESS_COLLAR"}


def engrave(img, sd, color, light=None):
    """The shape sd (px, negative inside) cut into whatever img is drawn on, in color."""
    L, cover = A.L, A.cover
    under = A.rgb(L.COL_SYMBOL_LIGHT) if light is None else light
    img.paint(lambda x, y: (LIT * cover(sd(x, y - 1.0)) * (1.0 - cover(sd(x, y))), under))
    img.paint(lambda x, y: (cover(sd(x, y)), color))
    img.paint(lambda x, y: (SHADE * cover(sd(x, y)) * (1.0 - cover(sd(x, y - DEEP))), BLACK))
    img.paint(lambda x, y: (LOW * cover(sd(x, y)) * (1.0 - cover(sd(x, y + DEEP))), A.WHITE))


def stroke_shape(name, n, width, slash=False):
    """A Tabler icon's strokes, n px square, width in its 24 units, as a shape; slash: crossed out as Tabler's "-off"
    icons are, the strokes cut away beside the slash."""
    return strokes(A.tabler(name), n / 24.0, width, slash)


def strokes(shapes, s, width, slash):
    shape = A.sd_strokes(A.lines_of(shapes), s, width, 0.0, 0.0)
    if slash:
        line = A.path_lines(A.SLASH)
        gap = A.sd_strokes(line, s, width + 2 * A.SLASH_GAP, 0.0, 0.0)
        shape = A.sd_union(A.sd_without(shape, gap), A.sd_strokes(line, s, width, 0.0, 0.0))
    return shape


def symbol(shapes, size, color, width, view=24.0, slash=False):
    """art.py symbol() in Stock: the same strokes, engraved; color as art.py gives it (0..255 floats)."""
    img = A.Image(size, size + 1)
    engrave(img, strokes(shapes, size / view, width, slash), color)
    return img


def house_shape(n):
    """The house (art.py glass_house, our own drawing) n px square."""
    k, shape = n / A.GLASS_U, A.glass_house()
    return lambda x, y: shape(x / k, y / k) * k


def icon(name, n, shape, color, light=None):
    """An engraved symbol n px square: the sprite 1 px higher for the light under it."""
    img = A.Image(n, n + 1)
    engrave(img, shape, A.rgb(color), light)
    return A.Sprite(name, img, (0, 0, 0, 1))


def groove(img, sd, ground):
    """A channel cut into a surface of colour ground: a little darker than it, shaded inside its upper edge, lit
    along its lower edge."""
    L, cover = A.L, A.cover
    img.paint(lambda x, y: (LIT * cover(sd(x, y - 1.0)) * (1.0 - cover(sd(x, y))), A.rgb(L.COL_SYMBOL_LIGHT)))
    img.paint(lambda x, y: (cover(sd(x, y)), A.lerp(ground, BLACK, 0.10)))
    img.paint(lambda x, y: (SHADE * cover(sd(x, y)) * (1.0 - cover(sd(x, y - 1.5))), BLACK))


def flat_fill(img, sd, top, low, y0, y1):
    """What fills a groove: flat, its colour from top to low, a light line along its top."""
    cover = A.cover
    img.paint(lambda x, y: (cover(sd(x, y)), A.lerp(top, low, A.clamp((y - y0) / max(1.0, y1 - y0)))))
    img.paint(lambda x, y: (0.30 * cover(sd(x, y)) * (1.0 - cover(sd(x, y - 1.0))), A.WHITE))


def bead(name, on):
    """A dot of the loader (art.py liq_loader_beads' size): a groove in the bottom screen, the lit one in the
    scheme's loader tone (schemes.py, COL_LOADER)."""
    L = A.L
    pad, n = 3, L.LOADER_BEAD
    c = pad + n / 2.0
    img = A.Image(n + 2 * pad, n + 2 * pad)
    disc = A.sd_circle(c, c, n / 2.0)
    if on:
        img.paint(lambda x, y: (A.cover(disc(x, y)), A.rgb(L.COL_LOADER)))
    else:
        groove(img, disc, A.rgb(L.COL_BOT_BG_A))
    return A.Sprite(name, img, (pad,) * 4)


def bars(style):
    """The download's bar (art.py liq_bars' size): a groove in the card, the fill flat in the install green's tubes'
    tones turned to the scheme's bar tone (art.py bar_turned)."""
    L = A.L
    h, r, pad = L.PROGRESS_H, L.PROGRESS_R, (3, 2, 3, 5)
    top = A.rgb(L.COL_TUBE_FILL_TOP)
    top, low = A.bar_turned(top), A.bar_turned(A.lerp(top, A.rgb(L.COL_TUBE_FILL_LOW), 0.45))

    def track(img, b):
        groove(img, A.Box(b.x0, b.y0, b.x1, b.y1, r).sd, A.rgb(L.COL_CARD))

    def fill(img, b):
        flat_fill(img, A.Box(b.x0 + 1, b.y0 + 1, b.x1 - 1, b.y1 - 1, r - 1).sd, top, low, b.y0, b.y1)
    return [A.across(A.style_name("PROGRESS", style), h, r, pad, track, 6),
            A.across(A.style_name("PROGRESS_FILL", style), h, r, A.NO_PAD, fill, 6)]


def card(name):
    """The installing and download screens' card: the plain card (art.py card), at liq_card's pad."""
    L = A.L

    def draw(img, box):
        A.element(img, A.rounded(box, L.CARD_R), A.rgb(L.COL_CARD))
    return A.both_ways(name, L.CARD_R, (4, 2, 4, 6), draw)


def empty_like(sprite):
    return A.Sprite(sprite.name, A.Image(sprite.img.w, sprite.img.h), sprite.pad, sprite.cap)


def sprites(items, index):
    """The scheme's atlas for Stock (index: the scheme, which A.L is drawn in): art.py's sprites with every glass one
    redrawn."""
    L = A.L
    styled = {"PROGRESS": None, "PROGRESS_FILL": None, "CARD_GLASS": card}
    by_style = {A.style_name(base, style): (base, style) for base in styled for style in A.GLASS_STYLES}
    bar = {s.name: s for style in A.GLASS_STYLES for s in bars(style)}
    own = {"LOADER_BEAD": lambda name: bead(name, False), "LOADER_BEAD_ON": lambda name: bead(name, True),
           "SET_ROW": lambda name: A.face(name, set_row_face_h(), L.ROW_R),
           "SET_ROW_DOWN": lambda name: A.face(name, set_row_face_h(), L.ROW_R, "press")}
    out = []
    for s in items:
        if s.name in bar:
            new = bar[s.name]
        elif s.name in own:
            new = own[s.name](s.name)
        elif s.name in by_style:
            new = styled[by_style[s.name][0]](s.name)
        elif s.name in UNUSED:
            new = empty_like(s)
        else:
            new = s
        out.append(new)
    return out


# ---------------------------------------------------------------- raised (the start screen's word and house)

def box_blur(grid, w, h, r, across):
    """grid (w * h values) averaged over 2r + 1 px along its rows (across) or its columns, 0 outside."""
    out, k = [0.0] * (w * h), 1.0 / (2 * r + 1)
    n, lines = (w, h) if across else (h, w)
    for j in range(lines):
        at = [j * w + i for i in range(n)] if across else [i * w + j for i in range(n)]
        line = [grid[i] for i in at]
        s = sum(line[:r + 1])
        for i in range(n):
            out[at[i]] = s * k
            if i + r + 1 < n:
                s += line[i + r + 1]
            if i - r >= 0:
                s -= line[i - r]
    return out


def blurred(grid, w, h, r):
    """grid blurred twice by a box of 2r + 1 px each way (close to a Gaussian)."""
    for _ in range(2):
        grid = box_blur(box_blur(grid, w, h, r, True), w, h, r, False)
    return grid


def raised(img, cov, color, under=None):
    """The shape of coverage cov (img.w * img.h, 0 to 1) raised from what img is drawn on, in color (SURFACE): its
    shadow, then under (coverage, colour: the house's cut edge, which casts the shadow with it), then its face,
    lighter at the top, with light where it faces up and shade where it faces down or sideways."""
    L, w, h, s = A.L, img.w, img.h, SURFACE
    caster = cov if under is None else [max(a, b) for a, b in zip(cov, under[0])]
    shadow = blurred([0.0] * (w * SHADOW_DOWN) + caster[:w * (h - SHADOW_DOWN)], w, h, SHADOW_BLUR)
    dark = A.rgb(L.COL_SHADOW)
    for k in range(w * h):
        if shadow[k] > 0.0 and caster[k] < 1.0:
            img.over(k, dark, SHADOW_ALPHA * shadow[k] * (1.0 - caster[k]))
    if under is not None:
        for k in range(w * h):
            if under[0][k] > 0.0:
                img.over(k, under[1], under[0][k])
    rows = [y for y in range(h) if max(cov[y * w:(y + 1) * w]) > 0.0]
    if not rows:
        return
    y0, y1, n = rows[0], rows[-1], 2 * s["reach"] + 1
    top = A.lerp(color, A.WHITE, s["lift"])
    bevel = blurred(cov, w, h, s["reach"])

    def at(x, y):
        return bevel[y * w + x] if 0 <= x < w and 0 <= y < h else 0.0
    for k in range(w * h):
        c = cov[k]
        if c <= 0.0:
            continue
        x, y = k % w, k // w
        img.over(k, A.lerp(top, color, (y - y0) / max(1.0, y1 - y0)), c)
        up = (at(x, y + 1) - at(x, y - 1)) / 2.0 * n        # 1 where the face turns fully up, -1 down
        side = abs(at(x + 1, y) - at(x - 1, y)) / 2.0 * n
        if up > 0.0:
            img.over(k, A.WHITE, s["light"] * min(1.0, up) * c)
        elif up < 0.0:
            img.over(k, BLACK, s["shade"] * min(1.0, -up) * c)
        if side > 0.0:
            img.over(k, BLACK, s["side"] * min(1.0, side) * c)


# ---------------------------------------------------------------- the sheet (per scheme in Stock)

def sheet_icons():
    """The glass sheet's icons (art.py GLASS_ICONS, the same names and sizes) as engraved line icons."""
    L = A.L
    teal, orange, folder = L.COL_TEAL, L.COL_ORANGE, L.COL_FOLDER
    return {
        "SD": lambda n: stroke_shape("device-sd-card", n, 1.8), "QR": lambda n: stroke_shape("qrcode", n, 1.8),
        "INSTALLED": house_shape, "DOWNLOAD": lambda n: stroke_shape("download", n, 2.2),
        "NO_MUSIC": lambda n: stroke_shape("music", n, 2.2, slash=True), "DELETE_THEME": house_shape,
        "DELETE_FILE": lambda n: stroke_shape("device-sd-card", n, 1.8),
        "KEEP": lambda n: stroke_shape("file-check", n, 1.8), "TRASH": lambda n: stroke_shape("trash", n, 1.8),
        "HELP_MARK": lambda n: stroke_shape("help", n, 2.0), "FOLDER_UP": lambda n: stroke_shape("folder-up", n, 2.0),
        "SETTINGS": lambda n: stroke_shape("tool", n, 2.0), "FOLDER": lambda n: stroke_shape("folder", n, 2.0),
    }, {"SD": teal, "QR": teal, "INSTALLED": teal, "DOWNLOAD": L.COL_WHITE, "NO_MUSIC": teal, "DELETE_THEME": orange,
        "DELETE_FILE": orange, "KEEP": teal, "TRASH": orange, "HELP_MARK": teal, "FOLDER_UP": folder,
        "SETTINGS": teal, "FOLDER": folder}


def sheet(names):
    """The scheme's sheet: the sprites named as art.py's glass sheet, in its order."""
    shapes, colours = sheet_icons()
    sizes = {name: n for name, n, _, _ in A.GLASS_ICONS}
    made = {}
    for name, n in sizes.items():
        made[name] = icon(name, n, shapes[name](n), colours[name])
    return [made[name] for name in names]

