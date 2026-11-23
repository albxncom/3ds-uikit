"""Draws every image of the kit at build time, at the sizes and in the colours of ui_layout.h: the sprites the
screens are drawn with (tiles, rows, buttons, the bar, the corner button, the scroll bar, tags, symbols) and a
CIA banner's chime (banner.wav); an application's own images through its hook (--app, below at APP).

Each pixel gets the exact share of a shape that covers it (signed distances for boxes, distances to the outline for
strokes and paths), so every edge is anti-aliased. Gradients and shadows follow the prototype's CSS: linear
gradients in sRGB, box shadows as a Gaussian blur with sigma = blur radius / 2, the focus ring as a 3 px spread.
Sprites that stretch keep their corners and repeat a 2 px middle (9-slice), so they are drawn once for any width.
The symbols are Tabler Icons' 24-unit outline icons (icons/, read as they are) and a few own drawings, stroked
with round caps and joins. Python 3 standard library only; the same input gives the same bytes.

Colour schemes (schemes.py): ui_layout.h and popup_band.h hold the yellow original; the atlas and the pattern tiles are
drawn once per scheme from that scheme's COL_* values, each scheme in a process of its own (the colours are module
constants), and the application unpacks the chosen one. The same in every scheme: where each sprite is, the
selection's corners, the icon mask; the chime is the original's.

Looks: a look is a module look_NAME.py beside this file that draws the images in its own material through the
hooks below (LOOK). Each look's part is drawn on its own (one make target per look), and the join writes the C of
all looks a build holds: one sprite enum (the names every look draws, Stock's, in Stock's order, then each look's
own), one colour enum, and per look its blobs and tables as a const look_art (ui_art.h), a blob equal to an earlier
one named once. Per look and scheme: the atlas, the sheet (the glass drawn per scheme: the start screen's word, the
icons on buttons, the carousel's panes), the pattern tiles. The application's part (with --app) has its own: an
atlas, a sheet and its pictures.

Usage (APP.py: the application's half, below at APP):
  python3 -I art.py --app APP.py --look NAME --part OUT.part
                                     the look's share: its images in every scheme (look_NAME.py), the colours the
                                     application draws by code, for the join
  python3 -I art.py --app APP.py --join OUT_DIR NAME...
                                     the looks' parts OUT_DIR/NAME.part, Stock first, joined into OUT_DIR/ui_art.h,
                                     ui_art.c (what no look owns: the selection's corners), art_NAME.c per look
                                     (zlib-compressed RGBA8 textures in the GPU's tile order, where each sprite is,
                                     the colours, const look_art ART_NAME) and banner.wav
  python3 -I art.py --app APP.py [--look NAME] ARGS...
                                     the application's own commands (its main)
"""
import colorsys
import functools
import importlib
import math
import os
import pickle
import re
import struct
import subprocess
import sys
import tempfile
import wave
import zlib
from pathlib import Path

KIT = Path(__file__).resolve().parent.parent  # the kit's folder, the one above this file's: art/ its modules, icons/,
                                              # include/ ui_layout.h, popup_band.h, look_<name>.h (as on the
                                              # Makefiles' -I path); installed, share/3ds-uikit/ in the same layout
HEADERS = KIT / "include"
MID = 2                 # the middle of a stretched sprite, in px
GAP = 2                 # between sprites in the atlas
BORDER = 1              # transparent texels between every packed sprite and each edge of its texture: a linear
                        # filter reaches one texel past a sprite's edge, and at the texture's edge the GPU clamps
                        # (reads the sprite's own edge again) where inside it reads the gap, so without the border
                        # what a call samples would depend on where the packer put the sprite
ATLAS_W = 512
PART_ATLAS_W = 512      # the application's part's atlas and sheet (art_part's sizes): 512 wide unless a narrower
PART_SHEET_W = 256      # one is smaller (its sheet: 256 x 256, at 512 it would be 512 x 256)
SIDES = (8, 16, 32, 64, 128, 256, 512, 1024)    # a texture's sides the GPU takes (the part's pictures)
SQRT2 = math.sqrt(2.0)
INF = float("inf")


def read_layout(path, values=None):
    """The numbers #defined in path and in the files it #includes from its own folder or the kit's include/."""
    values = {} if values is None else values
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"#define\s+([A-Z][A-Z0-9_]*)\s+(0x[0-9A-Fa-f]+|\d+)\b", line)
        if m:
            values[m.group(1)] = int(m.group(2), 0)
        inc = re.match(r'#include\s+"([^"]+)"', line)
        found = [d / inc.group(1) for d in (path.parent, HEADERS) if (d / inc.group(1)).is_file()] if inc else []
        if found:
            read_layout(found[0], values)
    return type("Layout", (), values)


sys.dont_write_bytecode = True      # no __pycache__ beside the sources
sys.path.insert(0, str(KIT / "art"))    # python3 -I leaves the script's folder out
import schemes  # noqa: E402
import liquid  # noqa: E402

# The look drawn (--look NAME: look_NAME.py; make LOOKS), None: art.py's own images as they are (the application's
# commands, the host tests). Its module is told this one as A before anything is drawn and has NAME, OWN_SPRITES (its sprite names
# after the common ones), OWN_COLOURS (its colour names after ui_layout.h's) and sprites(items, index) (art.py's sprites
# in, the look's out: every common name, redrawn or kept, and its own); and if it wants them the hooks
#   layout(index)      the Layout scheme index is drawn from (else scheme_layout(D, index))
#   colours()          per scheme {name: 0xRRGGBB}: ui_layout.h's colours it draws in other values, and its own
#   sheet(names)       per scheme the sheet's sprites by name (else art.py's in Shard, the same in every scheme)
#   icon_mask()        a theme icon's corners
#   symbol(shapes, size, color, width, view, slash)  a symbol on a face (Stock: engraved)
#   blobs(index)       per scheme {name: [bytes, ...]}, zlib-compressed: ART_<LOOK>_<NAME>_Z[ART_SCHEMES][n]
#   header()           lines for ui_art.h
FALLBACK = "stock"      # the look every build holds and an unknown setting draws; its names are the common ones
LOOK, APP, LOOK_ARGS = None, None, []
while sys.argv[1:2] in (["--look"], ["--app"]):
    LOOK_ARGS += sys.argv[1:3]
    del sys.argv[1:3]
# The application's half (--app FILE.py; the hook, its one place): the application's images are drawn by a module of
# the application's, which this file names nowhere else. It is imported before anything is drawn and must have
# (APP_NEEDS; a module that lacks one ends the run at once with one line, "--app: ... lacks ...")
#   LAYOUT          the header the numbers are read from (D, L): the application's, which includes the kit's
#                   ui_layout.h
#   STOCK_ONLY      its names only Stock fills (every other look's atlas leaves them out; a look left with none of
#                   its sprites has no atlas of the part's), a set, empty for none
#   sprites(index)  its sprites of the atlas in scheme index, drawn in the look (LOOK, L) with the kit's helpers: they
#                   lie in an atlas of the part's own; their ids follow the kit's end (part_art.h, from ART_ALL on);
#                   a list, empty for none
#   sheet()         its sprites of the sheet in the scheme drawn (SCHEME), in a sheet of the part's own; their ids
#                   after the atlas's (part_art.h); a list, empty for none
#   main(args)      its own commands (the contact sheet, the comparison), True if args are one
# and may have
#   pictures(index) the part's pictures in scheme index (the one drawn, SCHEME): one Image whose sides are powers of
#                   two up to 1024, of one size in every look and scheme, or None; a texture of the part's own
# Its sprites' names are its own: a name the kit's half draws too in the look (atlas or sheet) ends the run with one
# line, "--app: ... names sprites the kit draws too", before any file is written (look_part).
# Its modules import the kit's helpers from this module as art (registered under that name here).
APP_NEEDS = ("LAYOUT", "STOCK_ONLY", "sprites", "sheet", "main")
sys.modules.setdefault("art", sys.modules[__name__])
if "--app" in LOOK_ARGS:
    APP_FILE = Path(LOOK_ARGS[LOOK_ARGS.index("--app") + 1]).resolve()
    if APP_FILE.suffix != ".py" or not APP_FILE.is_file():
        sys.exit("--app: no %s" % APP_FILE)
    sys.path.insert(1, str(APP_FILE.parent))
    APP = importlib.import_module(APP_FILE.stem)
    APP_LACKS = [n for n in APP_NEEDS if not hasattr(APP, n)]
    if APP_LACKS:
        sys.exit("--app: %s lacks %s (the hook needs %s; pictures may be absent)" % (
            APP_FILE.name, ", ".join(APP_LACKS), ", ".join(APP_NEEDS)))
if "--look" in LOOK_ARGS:
    LOOK_NAME = LOOK_ARGS[LOOK_ARGS.index("--look") + 1]
    if not re.fullmatch(r"[a-z]+", LOOK_NAME) or not (KIT / "art" / ("look_%s.py" % LOOK_NAME)).is_file():
        sys.exit("--look: no look_%s.py" % LOOK_NAME)
    LOOK = importlib.import_module("look_" + LOOK_NAME)
    LOOK.A = sys.modules[__name__]
SCHEME = 0
if sys.argv[1:2] == ["--scheme"]:
    SCHEME = int(sys.argv[2])
    del sys.argv[1:3]


liquid.A = sys.modules[__name__]


def scheme_layout(layout, index):
    """layout with the COL_* values of scheme index (schemes.py)."""
    values = {n: v for n, v in vars(layout).items() if not n.startswith("__")}
    values.update(schemes.palette(index, {n: v for n, v in values.items() if n.startswith("COL_")}, layout.TRAY_FLOOR))
    return type("Layout", (), values)


def look_layout(index):
    """What scheme index is drawn from in the look: its layout hook's, else the yellow original in the scheme's
    colours."""
    return LOOK.layout(index) if hasattr(LOOK, "layout") else scheme_layout(D, index)


D = read_layout(APP.LAYOUT if APP else HEADERS / "ui_layout.h")  # the yellow original, as measured
L = look_layout(SCHEME)             # what is drawn from: the scheme's colours, in the look


def rgb(v):
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


# ---------------------------------------------------------------- pixels


class Image:
    """Premultiplied RGBA, 0..255 per colour channel and 0..1 alpha."""

    def __init__(self, w, h, background=None):
        self.w, self.h = w, h
        self.p = [0.0] * (w * h * 4)
        if background:
            for i in range(w * h):
                self.p[4 * i:4 * i + 4] = [float(background[0]), float(background[1]), float(background[2]), 1.0]

    def over(self, i, color, a):
        p, k, j = self.p, 1.0 - a, 4 * i
        p[j] = color[0] * a + p[j] * k
        p[j + 1] = color[1] * a + p[j + 1] * k
        p[j + 2] = color[2] * a + p[j + 2] * k
        p[j + 3] = a + p[j + 3] * k

    def paint(self, shade):
        """shade(x, y) at each pixel's centre returns (alpha, colour) or None."""
        for y in range(self.h):
            cy = y + 0.5
            for x in range(self.w):
                got = shade(x + 0.5, cy)
                if got and got[0] > 0.0:
                    self.over(y * self.w + x, got[1], min(got[0], 1.0))

    def rgba(self):
        """Straight (not premultiplied) RGBA bytes, row by row; fully transparent pixels are 0, 0, 0, 0."""
        out = bytearray(self.w * self.h * 4)
        for i in range(self.w * self.h):
            a = self.p[4 * i + 3]
            alpha = round(min(a, 1.0) * 255)
            if alpha:
                for c in range(3):
                    out[4 * i + c] = max(0, min(255, round(self.p[4 * i + c] / a)))
                out[4 * i + 3] = alpha
        return out

    def blit(self, src, dx, dy, scale=1):
        for y in range(src.h * scale):
            ty = dy + y
            if not 0 <= ty < self.h:
                continue
            for x in range(src.w * scale):
                tx = dx + x
                if not 0 <= tx < self.w:
                    continue
                j = 4 * ((y // scale) * src.w + x // scale)
                a = src.p[j + 3]
                if a > 0.0:
                    self.over(ty * self.w + tx, (src.p[j] / a, src.p[j + 1] / a, src.p[j + 2] / a), a)

    def png(self, opaque=False):
        data = self.rgba()
        stride = self.w * 4
        rows = []
        for y in range(self.h):
            row = data[y * stride:(y + 1) * stride]
            if opaque:
                row = bytes(b for i, b in enumerate(row) if i % 4 != 3)
            rows.append(b"\0" + bytes(row))

        def chunk(kind, payload):
            return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))
        header = struct.pack(">IIBBBBB", self.w, self.h, 8, 2 if opaque else 6, 0, 0, 0)
        return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
                + chunk(b"IEND", b""))


def cover(d):
    """The share of a pixel inside a shape whose edge is d px away from the pixel's centre (negative: inside)."""
    return 1.0 if d <= -0.5 else 0.0 if d >= 0.5 else 0.5 - d


def soft(d, blur):
    """A CSS box shadow's alpha d px outside its shape: a Gaussian with sigma = blur / 2."""
    return 0.5 * math.erfc(d / (blur / 2.0 * SQRT2)) if blur > 0 else cover(d)


class Box:
    """A rectangle with rounded corners (top left, top right, bottom right, bottom left), as a signed distance."""

    def __init__(self, x0, y0, x1, y1, radii=0):
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.r = tuple(radii) if isinstance(radii, (tuple, list)) else (radii,) * 4
        self.cx, self.cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        self.hw, self.hh = (x1 - x0) / 2.0, (y1 - y0) / 2.0

    def sd(self, px, py):
        qx, qy = px - self.cx, py - self.cy
        if qx < 0:
            r = self.r[0] if qy < 0 else self.r[3]
        else:
            r = self.r[1] if qy < 0 else self.r[2]
        dx, dy = abs(qx) - self.hw + r, abs(qy) - self.hh + r
        if dx > 0 and dy > 0:
            return math.hypot(dx, dy) - r
        return max(dx, dy) - r

    def moved(self, dx, dy):
        return Box(self.x0 + dx, self.y0 + dy, self.x1 + dx, self.y1 + dy, self.r)

    def grown(self, d):
        return Box(self.x0 - d, self.y0 - d, self.x1 + d, self.y1 + d, tuple(max(0.0, r + d) if r else 0.0 for r in self.r))

    def inset(self, left, top, right, bottom):
        r = self.r
        return Box(self.x0 + left, self.y0 + top, self.x1 - right, self.y1 - bottom,
                   (max(0.0, r[0] - max(left, top)), max(0.0, r[1] - max(right, top)),
                    max(0.0, r[2] - max(right, bottom)), max(0.0, r[3] - max(left, bottom))))


def vertical(top, low, y0, y1):
    def at(x, y):
        t = min(1.0, max(0.0, (y - y0) / (y1 - y0)))
        return tuple(top[i] + (low[i] - top[i]) * t for i in range(3))
    return at


def horizontal(left, right, x0, x1):
    def at(x, y):
        t = min(1.0, max(0.0, (x - x0) / (x1 - x0)))
        return tuple(left[i] + (right[i] - left[i]) * t for i in range(3))
    return at


def element(img, box, fill, edge=None, sides=(1, 1, 1, 1), shadows=(), insets=()):
    """A CSS box: its outer shadows (outside the box only), the fill and border as one layer, its inset shadows.
    fill and the border colour are (r, g, b) or a function of the pixel's centre; fill None leaves only the border."""
    for dx, dy, blur, color, alpha in shadows:
        shape = box.moved(dx, dy)
        img.paint(lambda x, y: (alpha * soft(shape.sd(x, y), blur) * (1.0 - cover(box.sd(x, y))), color))
    inner = box.inset(*(edge[0] * s for s in sides)) if edge else None

    def body(x, y):
        c = cover(box.sd(x, y))
        if c <= 0.0:
            return None
        e = max(0.0, c - cover(inner.sd(x, y))) / c if edge else 0.0
        base = fill(x, y) if callable(fill) else fill
        if base is None:
            return (c * e, edge[1])
        if not edge:
            return (c, base)
        return (c, tuple(base[i] + (edge[1][i] - base[i]) * e for i in range(3)))
    img.paint(body)
    for dx, dy, blur, color, alpha in insets:
        hole = box.moved(dx, dy)
        img.paint(lambda x, y: (alpha * (1.0 - soft(hole.sd(x, y), blur)) * cover(box.sd(x, y)), color))


# ---------------------------------------------------------------- strokes and paths

NUMBER = r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?"
# a number, a letter, or any other character (which path_lines refuses); spaces and commas only separate
TOKEN = re.compile(NUMBER + r"|[A-Za-z]|[^\s,]")
COMMANDS = "MmLlHhVvCcSsQqTtAaZz"


def arc_points(x1, y1, rx, ry, phi, large, sweep, x2, y2):
    """An SVG elliptical arc as points after (x1, y1), centre parametrisation as in SVG 1.1, F.6.5."""
    if rx == 0 or ry == 0 or (x1 == x2 and y1 == y2):
        return [(x2, y2)]
    rx, ry = abs(rx), abs(ry)
    c, s = math.cos(math.radians(phi)), math.sin(math.radians(phi))
    hx, hy = (x1 - x2) / 2.0, (y1 - y2) / 2.0
    xp, yp = c * hx + s * hy, -s * hx + c * hy
    lam = xp * xp / (rx * rx) + yp * yp / (ry * ry)
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * yp * yp - ry * ry * xp * xp
    den = rx * rx * yp * yp + ry * ry * xp * xp
    k = math.sqrt(max(0.0, num / den)) * (-1 if large == sweep else 1)
    cxp, cyp = k * rx * yp / ry, -k * ry * xp / rx
    cx, cy = c * cxp - s * cyp + (x1 + x2) / 2.0, s * cxp + c * cyp + (y1 + y2) / 2.0

    def angle(ux, uy, vx, vy):
        a = math.atan2(ux * vy - uy * vx, ux * vx + uy * vy)
        return a
    t1 = angle(1, 0, (xp - cxp) / rx, (yp - cyp) / ry)
    dt = angle((xp - cxp) / rx, (yp - cyp) / ry, (-xp - cxp) / rx, (-yp - cyp) / ry)
    if not sweep and dt > 0:
        dt -= 2 * math.pi
    elif sweep and dt < 0:
        dt += 2 * math.pi
    n = max(4, int(abs(dt) / (math.pi / 48)) + 1)
    out = []
    for i in range(1, n + 1):
        t = t1 + dt * i / n
        ex, ey = rx * math.cos(t), ry * math.sin(t)
        out.append((c * ex - s * ey + cx, s * ex + c * ey + cy))
    out[-1] = (x2, y2)
    return out


def cubic_points(p0, p1, p2, p3, n=24):
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
                    u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]))
    return out


def quad_points(p0, p1, p2, n=16):
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0], u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]))
    return out


def path_lines(d):
    """The subpaths of an SVG path (M L H V C S Q T A Z, absolute and relative, as in SVG 1.1, 8.3) as
    [(points, closed)]. Anything else in d is refused with the path in the message."""
    toks = TOKEN.findall(d)
    lines, pts = [], None
    x = y = sx = sy = 0.0
    i, cmd = 0, None
    last_c = last_q = None      # the previous curve's last control point, for S and T

    def num():
        nonlocal i
        if i >= len(toks) or toks[i] in COMMANDS:
            raise ValueError("path %r: command %r lacks a number" % (d, cmd))
        try:
            v = float(toks[i])
        except ValueError:
            raise ValueError("path %r: %r is no number and no path command" % (d, toks[i])) from None
        i += 1
        return v

    def finish(closed):
        nonlocal pts
        if pts:
            lines.append((pts, closed))
        pts = None

    while i < len(toks):
        if toks[i].isalpha():
            if toks[i] not in COMMANDS:
                raise ValueError("path %r: command %r is not supported" % (d, toks[i]))
            cmd = toks[i]
            i += 1
            if cmd in "Zz":
                if pts:
                    finish(True)
                x, y = sx, sy
                last_c = last_q = None
                continue
        elif cmd is None:
            raise ValueError("path %r does not start with a command" % d)
        elif cmd in "Zz":
            raise ValueError("path %r: a number after Z" % d)
        rel = cmd.islower()
        ox, oy = (x, y) if rel else (0.0, 0.0)
        c, q = None, None
        if cmd in "Mm":
            finish(False)
            x, y = num() + ox, num() + oy
            sx, sy = x, y
            pts = [(x, y)]
            cmd = "l" if rel else "L"
            last_c = last_q = None
            continue
        if pts is None:
            pts = [(x, y)]
        if cmd in "Ll":
            x, y = num() + ox, num() + oy
            pts.append((x, y))
        elif cmd in "Hh":
            x = num() + ox
            pts.append((x, y))
        elif cmd in "Vv":
            y = num() + oy
            pts.append((x, y))
        elif cmd in "CcSs":
            if cmd in "Cc":
                p1 = (num() + ox, num() + oy)
            else:   # the first control point mirrors the previous cubic's second one, or is the current point
                p1 = (2 * x - last_c[0], 2 * y - last_c[1]) if last_c else (x, y)
            p2 = (num() + ox, num() + oy)
            p3 = (num() + ox, num() + oy)
            pts += cubic_points((x, y), p1, p2, p3)
            x, y = p3
            c = p2
        elif cmd in "QqTt":
            if cmd in "Qq":
                p1 = (num() + ox, num() + oy)
            else:   # the control point mirrors the previous quadratic's, or is the current point
                p1 = (2 * x - last_q[0], 2 * y - last_q[1]) if last_q else (x, y)
            p2 = (num() + ox, num() + oy)
            pts += quad_points((x, y), p1, p2)
            x, y = p2
            q = p1
        elif cmd in "Aa":
            rx, ry, phi, large, sweep = num(), num(), num(), num(), num()
            ex, ey = num() + ox, num() + oy
            pts += arc_points(x, y, rx, ry, phi, int(large), int(sweep), ex, ey)
            x, y = ex, ey
        last_c, last_q = c, q
    finish(False)
    return lines


def shape_lines(shape):
    kind = shape[0]
    if kind == "path":
        return path_lines(shape[1])
    if kind == "rect":
        x, y, w, h, r = shape[1:]
        return path_lines("M{0} {1}H{2}A{4} {4} 0 0 1 {3} {5}V{6}A{4} {4} 0 0 1 {2} {7}H{0}A{4} {4} 0 0 1 {8} {6}"
                          "V{5}A{4} {4} 0 0 1 {0} {1}z".format(x + r, y, x + w - r, x + w, r, y + r, y + h - r,
                                                               y + h, x))
    if kind == "circle":
        cx, cy, r = shape[1:]
        n = 96
        return [([(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n))
                  for k in range(n)], True)]
    raise ValueError(kind)


def segments(lines, scale, dx, dy):
    for pts, closed in lines:
        q = [(px * scale + dx, py * scale + dy) for px, py in pts]
        if len(q) == 1:
            yield q[0], q[0]
        for a, b in zip(q, q[1:]):
            yield a, b
        if closed and q[0] != q[-1]:
            yield q[-1], q[0]


def nearest(img, segs, reach):
    """Squared distance from each pixel's centre to the nearest segment, for pixels within reach of one."""
    best = [INF] * (img.w * img.h)
    for (ax, ay), (bx, by) in segs:
        x0, x1 = max(0, int(min(ax, bx) - reach)), min(img.w, int(max(ax, bx) + reach) + 1)
        y0, y1 = max(0, int(min(ay, by) - reach)), min(img.h, int(max(ay, by) + reach) + 1)
        vx, vy = bx - ax, by - ay
        ll = vx * vx + vy * vy
        for py in range(y0, y1):
            cy = py + 0.5
            for px in range(x0, x1):
                cx = px + 0.5
                t = ((cx - ax) * vx + (cy - ay) * vy) / ll if ll else 0.0
                t = 0.0 if t < 0 else 1.0 if t > 1 else t
                ex, ey = cx - ax - t * vx, cy - ay - t * vy
                d2 = ex * ex + ey * ey
                k = py * img.w + px
                if d2 < best[k]:
                    best[k] = d2
    return best


def stroke(img, lines, scale, width, color, dx=0.0, dy=0.0, alpha=1.0):
    """Strokes lines (in the shape's units, times scale, moved by dx, dy) width px wide, round caps and joins."""
    half = width / 2.0
    best = nearest(img, list(segments(lines, scale, dx, dy)), half + 1)
    for k, d2 in enumerate(best):
        if d2 < INF:
            a = half + 0.5 - math.sqrt(d2)
            if a > 0:
                img.over(k, color, min(a, 1.0) * alpha)


def fill(img, lines, scale, color, dx=0.0, dy=0.0, evenodd=False):
    """Fills closed lines (non-zero winding, or even-odd), the edge anti-aliased by its distance."""
    segs = list(segments([(p, True) for p, _ in lines], scale, dx, dy))
    best = nearest(img, segs, 1.0)
    for py in range(img.h):
        cy = py + 0.5
        crossings = sorted((ax + (cy - ay) * (bx - ax) / (by - ay), 1 if by > ay else -1)
                           for (ax, ay), (bx, by) in segs if (ay <= cy < by) or (by <= cy < ay))
        winding, j = 0, 0
        for px in range(img.w):
            cx = px + 0.5
            while j < len(crossings) and crossings[j][0] < cx:
                winding += 1 if evenodd else crossings[j][1]
                j += 1
            k = py * img.w + px
            d = math.sqrt(best[k]) if best[k] < INF else INF
            inside = winding % 2 if evenodd else winding
            a = (0.5 + d if inside else 0.5 - d)
            if a > 0:
                img.over(k, color, min(a, 1.0))


# ---------------------------------------------------------------- symbols (24 units)

# Tabler Icons 3.51.0 (MIT, icons/LICENSE; from https://github.com/tabler/tabler-icons, release v3.51.0): each symbol
# in use is the pack's outline icon of that name, copied unmodified to icons/<name>.svg: paths only, round caps and
# joins; every <path> is drawn but the one with stroke="none" (the 24x24 frame). Another symbol: copy its file there.
ICONS = KIT / "icons"
PATH_TAG = re.compile(r"<path\b([^>]*?)/?>", re.S)
OTHER_TAG = re.compile(r"<(circle|rect|line|polyline|polygon|ellipse)\b")


def tabler(name):
    path = ICONS / (name + ".svg")
    text = path.read_text(encoding="utf-8")
    other = OTHER_TAG.search(text)
    if other:
        raise SystemExit("%s: <%s> is not drawn, only <path>" % (path, other.group(1)))
    shapes = []
    for m in PATH_TAG.finditer(text):
        if 'stroke="none"' in m.group(1):
            continue
        d = re.search(r'\bd="([^"]*)"', m.group(1))
        if not d:
            raise SystemExit("%s: a <path> without d" % path)
        shapes.append(("path", d.group(1)))
    if not shapes:
        raise SystemExit("%s: no path to draw" % path)
    return shapes


# the slash of a symbol that says "not", as Tabler's own "-off" icons draw it; the symbol's strokes are cut away
# SLASH_GAP units beside it so the slash stays readable
# The warning beside a line of text: a filled triangle with the exclamation mark cut out of it (even-odd), so it
# reads at the size of a capital, where the outlined icon's strokes run together.
WARNING = "M12 2L23 21H1ZM10.1 8.2V14.4H13.9V8.2ZM10.1 16.2V19.2H13.9V16.2Z"
WARNING_SOFT = 0.4
SLASH = "M3 3l18 18"
SLASH_GAP = 1.5


def lines_of(shapes):
    out = []
    for s in shapes:
        out += shape_lines(s)
    return out


def segment_distance(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy)))
    return math.hypot(px - ax - t * vx, py - ay - t * vy)


def cut_band(img, line, scale, half, dys):
    """Clears what is drawn within half px of a straight line (in the shape's units, times scale), and of its copies
    moved down by each of dys px, anti-aliased."""
    (a, b), = [(pts[0], pts[-1]) for pts, _ in path_lines(line)]
    ax, ay, bx, by = a[0] * scale, a[1] * scale, b[0] * scale, b[1] * scale
    for k in range(img.w * img.h):
        x, y = k % img.w + 0.5, k // img.w + 0.5
        d = min(segment_distance(x, y, ax, ay + dy, bx, by + dy) for dy in dys)
        keep = 1.0 - cover(d - half) if d < half + 0.5 else 1.0
        if keep < 1.0:
            for c in range(4):
                img.p[4 * k + c] *= keep


def symbol(shapes, size, color, width, view=24.0, light=True, slash=False):
    """A stroke symbol size x size; with light, 1 px more below for the light under it (as in the System Settings,
    where it makes a symbol look set into its face); with slash, crossed out. A look may draw it its own way (its
    symbol hook; Stock engraves it)."""
    if light and hasattr(LOOK, "symbol"):
        return LOOK.symbol(shapes, size, color, width, view, slash)
    img = Image(size, size + 1 if light else size)
    scale = size / view
    lines = lines_of(shapes)
    if light:
        stroke(img, lines, scale, width * scale, rgb(L.COL_SYMBOL_LIGHT), 0, 1, L.SYMBOL_LIGHT_ALPHA / 100.0)
    stroke(img, lines, scale, width * scale, color)
    if slash:
        cut_band(img, SLASH, scale, (width / 2.0 + SLASH_GAP) * scale, (0, 1) if light else (0,))
        if light:
            stroke(img, path_lines(SLASH), scale, width * scale, rgb(L.COL_SYMBOL_LIGHT), 0, 1,
                   L.SYMBOL_LIGHT_ALPHA / 100.0)
        stroke(img, path_lines(SLASH), scale, width * scale, color)
    return img


def ink_box(lines, half):
    """The ink's extent of lines (in their units) stroked half wide: x0, y0, x1, y1."""
    xs = [p[0] for pts, _ in lines for p in pts]
    ys = [p[1] for pts, _ in lines for p in pts]
    return min(xs) - half, min(ys) - half, max(xs) + half, max(ys) + half


def glyph(shapes, cap, color, width, light=False, filled=False):
    """A symbol that stands in a line of text: its ink exactly cap px high (the text's cap height), the image
    cropped to the ink, so it is drawn with its bottom on the baseline; light: the light line under it (on a face).
    width in the symbol's 24 units; filled: filled even-odd with that stroke for soft corners (the house)."""
    lines = lines_of(shapes)
    x0, y0, x1, y1 = ink_box(lines, width / 2.0)
    scale = cap / (y1 - y0)
    w = int(math.ceil((x1 - x0) * scale))
    img = Image(w, cap + 1 if light else cap)
    dx, dy = -x0 * scale + (w - (x1 - x0) * scale) / 2.0, -y0 * scale

    def draw(color_, oy, alpha):
        if filled:
            fill(img, lines, scale, color_, dx, dy + oy, evenodd=True)
            stroke(img, lines, scale, width * scale, color_, dx, dy + oy, alpha)
        else:
            stroke(img, lines, scale, width * scale, color_, dx, dy + oy, alpha)
    if light:
        draw(rgb(L.COL_SYMBOL_LIGHT), 1, L.SYMBOL_LIGHT_ALPHA / 100.0)
    draw(color, 0, 1.0)
    return Sprite(None, img, (0, 0, 0, 1) if light else NO_PAD)


# ---------------------------------------------------------------- glass slabs and the patterns
# A glass slab is see-through glass in a shape (glass_house: a house), lit from the upper left: white rims,
# two highlight bands, a soft inner shade, one hole (the window). Its pixels carry alpha, so whatever is behind it
# shows through. Shapes are signed distances in their own drawing units, which glass_slab draws s px to a unit.

GLASS_U = 64.0
WHITE = (255.0, 255.0, 255.0)
TO_LIGHT = (-0.5, -0.866)           # the light comes from the upper left
GLASS_COOL = rgb(L.COL_GLASS_SHADE)  # the shade inside clear glass


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def clamp(t):
    return 0.0 if t < 0.0 else 1.0 if t > 1.0 else t


def smooth(a, b, t):
    t = clamp((t - a) / (b - a))
    return t * t * (3.0 - 2.0 * t)


def sd_poly(pts, r=0.0):
    """A closed polygon as a signed distance, grown by r (which rounds its corners)."""
    segs = [(a[0], a[1], b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:] + pts[:1])]

    def sd(u, v):
        best, inside = INF, False
        for ax, ay, vx, vy in segs:
            wx, wy = u - ax, v - ay
            t = (wx * vx + wy * vy) / (vx * vx + vy * vy)
            t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
            ex, ey = wx - t * vx, wy - t * vy
            d2 = ex * ex + ey * ey
            if d2 < best:
                best = d2
            if (ay > v) != (ay + vy > v) and wx < vx * wy / vy:
                inside = not inside
        return (-math.sqrt(best) if inside else math.sqrt(best)) - r
    return sd


def sd_without(f, g):
    return lambda u, v: max(f(u, v), -g(u, v))


def fillet(pts, radii, steps=8):
    """A polygon with each corner cut round to its radius (0: left sharp), as points."""
    out, n = [], len(pts)
    for i, (p, r) in enumerate(zip(pts, radii)):
        if not r:
            out.append(p)
            continue
        a, b = pts[i - 1], pts[(i + 1) % n]
        d1 = ((a[0] - p[0]) / math.dist(a, p), (a[1] - p[1]) / math.dist(a, p))
        d2 = ((b[0] - p[0]) / math.dist(b, p), (b[1] - p[1]) / math.dist(b, p))
        half_angle = math.acos(max(-1.0, min(1.0, d1[0] * d2[0] + d1[1] * d2[1]))) / 2.0
        along, to_centre = r / math.tan(half_angle), r / math.sin(half_angle)
        mid = math.hypot(d1[0] + d2[0], d1[1] + d2[1])
        c = (p[0] + (d1[0] + d2[0]) / mid * to_centre, p[1] + (d1[1] + d2[1]) / mid * to_centre)
        a0 = math.atan2(p[1] + d1[1] * along - c[1], p[0] + d1[0] * along - c[0])
        a1 = math.atan2(p[1] + d2[1] * along - c[1], p[0] + d2[0] * along - c[0])
        turn = (a1 - a0 + math.pi) % (2.0 * math.pi) - math.pi
        out += [(c[0] + r * math.cos(a0 + turn * k / steps), c[1] + r * math.sin(a0 + turn * k / steps))
                for k in range(steps + 1)]
    return out


def glass_house(apex=5.0, eave=30.0, roof=27.5, wall=19.0, bottom=56.0, top_r=2.6, tip_r=1.6, corner=2.8):
    """Roof and walls as one outline with round corners (where the roof overhangs the walls it stays sharp), the
    window cut out of it."""
    pts = [(32.0, apex), (32.0 + roof, eave), (32.0 + wall, eave), (32.0 + wall, bottom), (32.0 - wall, bottom),
           (32.0 - wall, eave), (32.0 - roof, eave)]
    return sd_without(sd_poly(fillet(pts, (top_r, tip_r, 0, corner, corner, 0, tip_r))),
                      Box(25.0, 35.0, 39.0, 48.0, 1.6).sd)


def glass_surface(u, v, e, facing, s, body, bands=(), rim=2.3, rim_alpha=0.96, shade=GLASS_COOL, inner=0.22,
                  glow=0.32, sweep=0.0, veil=0.0):
    """The glass at one point of a slab's face: (alpha, colour). e: px in from the edge; facing: how far the nearest
    edge faces the light (-1 to 1); body(u, v): (alpha, colour) of the glass itself. Over it a soft inner shade, a
    glow where the light leaves on the far side, the highlight bands (where u + v is, width, alpha; clean-edged,
    kept off the rim; sweep moves them, for a slab that turns), a veil of light over the whole face, the white rim
    (rim units wide, never under 1 px)."""
    rim_px, w = max(1.0, rim * s), max(1.2, 4.0 * s)
    a, col = body(u, v)

    def over(a2, c2):
        nonlocal a, col
        if a2 <= 0.0:
            return
        total = a2 + a * (1.0 - a2)
        col = tuple((c2[i] * a2 + col[i] * a * (1.0 - a2)) / total for i in range(3))
        a = total
    deep = max(0.0, e - rim_px)
    over(inner * (1.35 if facing < 0.0 else 0.75) * math.exp(-deep / w), shade)
    if facing < 0.0:
        over(glow * -facing * math.exp(-((deep - 0.9 * w) / (0.6 * w)) ** 2), WHITE)
    for where, width, alpha in bands:
        over(alpha * cover((abs(u + v - where - sweep) / SQRT2 - width / 2.0) * s) * clamp(deep - 0.4), WHITE)
    over(veil * clamp(deep - 0.4), WHITE)
    over(clamp(rim_px + 0.5 - e) * (rim_alpha if facing > -0.3 else 0.82 * rim_alpha),
         WHITE if facing > -0.3 else lerp(WHITE, shade, 0.10))
    return a, col


# The one glass setting: every glass element (the house still and turning, the tiles' two symbols, the
# picture's two badges) is drawn in each of three styles, in the order of ui.h's GLASS_*. Clear: white glass that
# lets nearly everything through. Shard: coloured glass. Platin: bright glass in two tones that meet at a horizon, as
# polished metal mirrors sky and ground; it has no colour of its own.
GLASS_STYLES = ("clear", "shard", "platin")
GLASS_BANDS = ((43.0, 7.0, 0.50), (56.5, 2.4, 0.40))        # the house's two highlight bands, in its 64 units
SYMBOL_BANDS = ((40.0, 6.0, 0.50), (51.5, 2.0, 0.40))       # a tile symbol's, in px


def deeper(c, k=1.0):
    """The colour of thicker glass of the same tint."""
    return lerp(c, (c[0] * 0.50, c[1] * 0.46, c[2] * 0.60), k)


def glass_preset(kind, n=0.0, centre=0.0):
    """What depends on the element, not on the style: the rim's width, the two highlight bands (where u + v is,
    width, alpha), hairline, shadow, and from where to where the glass runs from its top to its low tone. logo: in
    the house's 64 units; tile: a tile's symbol, in px; badge: a disc of n px about centre; icon: anything else, n
    px (an icon on a button, a line of the word)."""
    if kind == "logo":
        return dict(rim=2.3, bands=GLASS_BANDS, hair=0.36, shadow=0.40, y0=6.0, y1=56.0)
    if kind == "tile":
        return dict(rim=1.5, bands=SYMBOL_BANDS, hair=0.36, shadow=0.34, y0=0.0, y1=float(L.MAIN_ICON))
    if kind == "badge":
        return dict(rim=1.3, bands=((2 * centre - 0.36 * n, 0.20 * n, 0.50), (2 * centre - 0.14 * n, 0.07 * n, 0.40)),
                    hair=0.36, shadow=0.40, y0=centre - n / 2.0, y1=centre + n / 2.0)
    if kind == "icon":
        return dict(rim=max(1.0, 0.027 * n), bands=((0.714 * n, 0.107 * n, 0.50), (0.92 * n, 0.036 * n, 0.40)),
                    hair=0.36, shadow=0.34, y0=0.07 * n, y1=0.93 * n)
    raise ValueError(kind)


def glass_style(style, tint, base, alpha=None, deep=False, diag=None, faint=0.0, boost=None, **over):
    """A style's glass for an element (base: glass_preset): (the glass itself, the look for glass_surface, hairline
    and shadow for glass_slab). tint: a colour, or two for the top and the low tone; Shard's glass is made of it,
    Clear takes faint of it into its white (the fan's samples), Platin none. alpha: the glass's own, where an
    element needs another than the style's. deep: Shard under a white symbol (the picture's badges): nearly opaque,
    darker, with a wider rim, a stronger hairline and shadow, and weaker bands. diag: Shard's two tones run along
    u + v from diag[0] to diag[1] instead of from top to bottom. boost: how much more white Clear's glass has, where
    not GLASS_CLEAR_BOOST (the glass in the cut of Platin's badges). over: any single value of the look, replaced."""
    y0, y1 = base["y0"], base["y1"]
    run = lambda u, v: clamp((v - y0) / (y1 - y0))
    look = dict(rim=base["rim"], hair=base["hair"], shadow=base["shadow"])
    k = 1.0                             # the style's strength of the bands
    if style == "clear":
        more = L.GLASS_CLEAR_BOOST / 100.0 if boost is None else boost
        white = lerp(WHITE, tint, faint) if faint else WHITE
        body = lambda u, v: (0.10 + more + 0.12 * run(u, v), white)
        look.update(shade=GLASS_COOL, inner=0.24)
    elif style == "shard":
        top, low = tint if isinstance(tint[0], tuple) else (lerp(tint, WHITE, 0.18), tint)
        look.update(shade=deeper(low), inner=0.30)
        if deep:
            top, low, alpha = lerp(top, low, 0.35), deeper(low, 0.45), alpha or 0.92
            look.update(rim=base["rim"] * 2.0 / 1.3, inner=0.26, hair=0.62, shadow=0.60)
            k = 0.45
        a = alpha or 0.74
        if diag:
            body = lambda u, v: (a, lerp(top, low, smooth(diag[0], diag[1], u + v)))
        else:
            body = lambda u, v: (a, lerp(top, low, run(u, v)))
    elif style == "platin":             # two tones that meet at a horizon, as polished metal mirrors sky and ground
        a = alpha or 0.95
        sky, ground = (rgb(L.COL_PLATIN_SKY_A), rgb(L.COL_PLATIN_SKY_B)), (rgb(L.COL_PLATIN_GROUND_A),
                                                                         rgb(L.COL_PLATIN_GROUND_B))
        horizon = L.PLATIN_HORIZON_PCT / 100.0

        def body(u, v):
            t = run(u, v) + 0.10 * ((u - y0) / (y1 - y0) - 0.5)        # the horizon leans a little
            if t < horizon:
                return (a, lerp(sky[0], sky[1], t / horizon))
            return (a, lerp(ground[0], ground[1], (t - horizon) / (1 - horizon)))
        look.update(shade=rgb(L.COL_PLATIN_SHADE), inner=0.38, hair=0.48)
        k = 1.2
    else:
        raise ValueError(style)
    look["bands"] = tuple((where, width, min(0.95, a_band * k)) for where, width, a_band in base["bands"])
    look.update(over)
    slab_only = {key: look.pop(key) for key in ("hair", "shadow")}
    return body, look, slab_only


def glass_slab(img, shape, s, body, hair=0.36, shadow=0.40, **look):
    """One slab of see-through glass face-on, s px to a unit of its shape: a soft shadow under it and a hairline
    around it, so it holds against a light ground, then the glass (glass_surface)."""
    at = lambda x, y: shape(x / s, y / s) * s
    drop, blur = max(1.0, 2.0 * s), max(1.5, 3.0 * s)
    dark = rgb(L.COL_GLASS_SHADOW)
    img.paint(lambda x, y: (shadow * soft(at(x, y - drop), blur) * (1.0 - cover(at(x, y))), dark))
    img.paint(lambda x, y: (hair * cover(at(x, y) - 0.7) * (1.0 - cover(at(x, y))), dark))

    def glass_at(x, y):
        d = at(x, y)
        c = cover(d)
        if c <= 0.0:
            return None
        nx, ny = at(x + 0.3, y) - at(x - 0.3, y), at(x, y + 0.3) - at(x, y - 0.3)
        n = math.hypot(nx, ny) or 1.0
        a, col = glass_surface(x / s, y / s, max(0.0, -d), (nx * TO_LIGHT[0] + ny * TO_LIGHT[1]) / n, s, body, **look)
        return (c * a, col)
    img.paint(glass_at)


def glass(img, shape, style, kind, tint=None, s=1.0, n=0.0, centre=0.0, plain=False, **over):
    """One slab of glass in a style, drawn on img (a symbol of several slabs: the first at the bottom, so each casts
    its shadow on the ones before). shape: its signed distance, s px to a unit; plain: only the glass itself,
    without rim, shade, bands, hairline and shadow (a layer under another)."""
    body, look, slab_only = glass_style(style, tint, glass_preset(kind, n, centre), **over)
    if plain:
        look.update(rim_alpha=0.0, inner=0.0, glow=0.0, bands=())
        slab_only = dict(hair=0.0, shadow=0.0)
    glass_slab(img, shape, s, body, **slab_only, **look)


# The tiles' two symbols, MAIN_ICON px square with room below for the
# shadow; their shapes' units are px on the tile. "Designs": a fan of three colour samples on one pin, as paint
# samples are shown. "Badges": a scalloped seal, a star cut through. Each in the three styles, side by side in the
# sprite list, so the application draws ART_DESIGNS + style.
def sd_circle(cx, cy, r):
    return lambda u, v: math.hypot(u - cx, v - cy) - r


def style_name(name, style):
    """A glass sprite's name: the first style's is the plain one, the others follow it in the list."""
    return name if style == GLASS_STYLES[0] else name + "_" + style.upper()


def sd_strokes(lines, scale, width, x, y, grow=0.0):
    """The strokes of an outline symbol as a shape: width in the symbol's units, grow px thicker all round."""
    segs = list(segments(lines, scale, x, y))
    half = width * scale / 2.0 + grow

    def sd(px, py):
        best = INF
        for (ax, ay), (bx, by) in segs:
            vx, vy, wx, wy = bx - ax, by - ay, px - ax, py - ay
            ll = vx * vx + vy * vy
            t = clamp((wx * vx + wy * vy) / ll) if ll else 0.0
            best = min(best, math.hypot(wx - t * vx, wy - t * vy))
        return best - half
    return sd


# ---------------------------------------------------------------- the glass sheet
# Sprites drawn in each glass style and the same in every scheme (fixed colours and COL_GLASS_SHADOW only): per style
# one texture of ATLAS_W px wide, only the chosen style's in memory, the sprites at the same places in all three.
# Their ids follow the atlas's (ui_art.h), so the application draws ART_SD as any sprite and gets it in the chosen
# glass. glass_sheet() lists them: the start screen's word, then GLASS_ICONS.


def sd_union(*f):
    return lambda u, v: min(g(u, v) for g in f)


def sd_box(x0, y0, x1, y1, r=0.0):
    return Box(x0, y0, x1, y1, r).sd


# The icons' forms: signed distances in px of a 40 px box, origin at its top left; own drawings
# after the Tabler outline in shape only. All parts of a form are one slab.
def form_qr():
    """Four scanner corner marks round three solid squares and one block."""
    def mark(fx, fy):
        x = lambda a: a if fx > 0 else 40 - a
        y = lambda a: a if fy > 0 else 40 - a
        arm = lambda a0, b0, a1, b1: sd_box(min(x(a0), x(a1)), min(y(b0), y(b1)), max(x(a0), x(a1)),
                                            max(y(b0), y(b1)), 2.2)
        return sd_union(arm(1, 1, 13, 5.4), arm(1, 1, 5.4, 13))
    finders = [sd_box(x, y, x + 8.5, y + 8.5, 2.0) for x, y in ((10, 10), (21.5, 10), (10, 21.5))]
    return sd_union(*(mark(fx, fy) for fx in (1, -1) for fy in (1, -1)), *finders, sd_box(22.5, 22.5, 28.5, 28.5, 1.5))


def form_sd():
    """An SD card, 28 x 36 px, its upper right corner cut off, three contacts cut through near the top."""
    r = 2.2
    body = sd_poly([(6 + r, 2 + r), (25.0, 2 + r), (34 - r, 11.0), (34 - r, 38 - r), (6 + r, 38 - r)], r)
    return sd_without(body, sd_union(*(sd_box(c - 1.25, 6.0, c + 1.25, 13.5, 1.25) for c in (12.0, 18.0, 24.0))))


def sd_capsule(ax, ay, bx, by, half):
    def sd(u, v):
        vx, vy, wx, wy = bx - ax, by - ay, u - ax, v - ay
        t = clamp((wx * vx + wy * vy) / (vx * vx + vy * vy))
        return math.hypot(wx - t * vx, wy - t * vy) - half
    return sd


def sd_polyline(pts, half):
    return sd_union(*(sd_capsule(a[0], a[1], b[0], b[1], half) for a, b in zip(pts, pts[1:])))


def sd_scaled(form, n):
    """A form drawn for a 40 px box, at n px."""
    k = n / 40.0
    return lambda u, v: form(u / k, v / k) * k


# Round 15's forms, each in its own n px box.
def form_download():
    """28: shaft, head, tray."""
    tray = sd_union(sd_box(2.5, 16.0, 7.1, 25.8, 2.3), sd_box(20.9, 16.0, 25.5, 25.8, 2.3),
                    sd_box(2.5, 21.2, 25.5, 25.8, 2.3))
    return sd_union(sd_box(11.7, 2.0, 16.3, 12.0, 2.3), sd_poly([(7.9, 10.2), (20.1, 10.2), (14.0, 16.0)], 1.3), tray)


def form_no_music():
    """28: two filled heads, stems, one beam, struck out."""
    note = sd_union(sd_circle(7.0, 21.0, 4.0), sd_circle(19.4, 21.0, 4.0), sd_box(7.0, 3.0, 11.0, 21.0, 0.8),
                    sd_box(19.4, 3.0, 23.4, 21.0, 0.8), sd_box(7.0, 3.0, 23.4, 7.6, 1.6))
    return sd_union(sd_without(note, sd_capsule(3.5, 3.5, 24.5, 24.5, 3.1)), sd_capsule(4.0, 4.0, 24.0, 24.0, 2.1))


def form_keep():
    """36: a sheet with its corner cut off, a check cut through."""
    body = sd_poly([(9.6, 5.2), (20.4, 5.2), (27.0, 11.8), (27.0, 30.8), (9.6, 30.8)], 2.2)
    return sd_without(body, sd_polyline([(12.6, 20.6), (16.4, 24.4), (23.6, 17.2)], 1.6))


def form_trash():
    """36: lid, handle, a tapered body with two ribs cut through."""
    handle = sd_without(sd_box(12.6, 3.0, 23.4, 10.0, 2.4), sd_box(16.4, 6.6, 19.6, 10.5, 0.8))
    body = sd_poly([(9.4, 13.0), (26.6, 13.0), (25.0, 30.6), (11.0, 30.6)], 2.2)
    ribs = sd_union(sd_box(13.4, 17.0, 15.8, 28.0, 1.2), sd_box(20.2, 17.0, 22.6, 28.0, 1.2))
    return sd_union(sd_box(4.4, 8.4, 31.6, 13.0, 2.3), handle, sd_without(body, ribs))


def form_help():
    """22: the question mark alone; the round help button is its circle."""
    cx, cy, r = 11.0, 7.0, 3.8
    bow = [(cx + r * math.cos(a), cy + r * math.sin(a)) for a in (math.pi + k * 1.3 * math.pi / 12 for k in range(13))]
    return sd_union(sd_polyline(bow + [(11.0, 11.4), (11.0, 12.2)], 2.2), sd_circle(11.0, 18.6, 2.3))


def form_folder_up():
    """22: a smaller folder and a solid arrow up beside its corner."""
    folder = sd_poly([(3.0, 4.6), (6.6, 4.6), (9.0, 7.0), (15.4, 7.0), (15.4, 14.6), (3.0, 14.6)], 1.5)
    arrow = sd_union(sd_poly([(13.6, 15.4), (20.4, 15.4), (17.0, 12.0)], 1.0), sd_box(15.3, 14.0, 18.7, 21.2, 1.2))
    return sd_union(sd_without(folder, lambda u, v: arrow(u, v) - 1.4), arrow)


def form_wrench():
    """22: a round head with its jaw open to the upper left, the handle down to the right."""
    head = sd_without(sd_circle(7.6, 7.6, 6.0), sd_capsule(7.6, 7.6, 1.0, 1.0, 2.2))
    return sd_union(head, sd_capsule(11.0, 11.0, 18.6, 18.6, 2.6))


def form_folder():
    """22: the folder, filled."""
    return sd_poly([(4.0, 5.0), (8.0, 5.0), (10.6, 7.6), (18.0, 7.6), (18.0, 16.5), (4.0, 16.5)], 1.5)


# An icon on a button: (sprite name, n px, form or "house", Shard's two tones by what the button does). The house is
# the start screen's (glass_house, its 64 units at n / 64 px) with its own tones. The ring of tones: bring in or keep
# teal into blue, help blue into violet, storage and settings violet into rose, remove coral into rose, folders amber
# into coral.
GLASS_ICONS = (
    ("SD", L.DESIGNS_ICON, form_sd(), (L.COL_SHARD_VIOLET, L.COL_SHARD_ROSE)),     # storage
    ("QR", L.DESIGNS_ICON, form_qr(), (L.COL_SHARD_TEAL, L.COL_SHARD_BLUE)),       # bring in
    ("INSTALLED", L.DESIGNS_ICON, "house", (L.COL_SHARD_A, L.COL_SHARD_B)),
    # the rest of the icons on buttons, in their table's order
    ("DOWNLOAD", L.GO_ICON, form_download(), (L.COL_SHARD_TEAL, L.COL_SHARD_BLUE)),         # Frost (GLASS_FROST)
    ("NO_MUSIC", L.GO_ICON, form_no_music(), (L.COL_SHARD_TEAL, L.COL_SHARD_BLUE)),
    ("DELETE_THEME", L.CHOICE_ICON, "house", (L.COL_SHARD_CORAL, L.COL_SHARD_ROSE)),
    ("DELETE_FILE", L.CHOICE_ICON, sd_scaled(form_sd(), L.CHOICE_ICON), (L.COL_SHARD_CORAL, L.COL_SHARD_ROSE)),
    ("KEEP", L.CHOICE_ICON, form_keep(), (L.COL_SHARD_TEAL, L.COL_SHARD_BLUE)),
    ("TRASH", L.CHOICE_ICON, form_trash(), (L.COL_SHARD_CORAL, L.COL_SHARD_ROSE)),
    ("HELP_MARK", L.HELP_ICON, form_help(), (L.COL_SHARD_BLUE, L.COL_SHARD_VIOLET)),
    ("FOLDER_UP", L.CORNER_ICON, form_folder_up(), (L.COL_SHARD_AMBER, L.COL_SHARD_CORAL)),
    ("SETTINGS", L.CORNER_ICON, form_wrench(), (L.COL_SHARD_VIOLET, L.COL_SHARD_ROSE)),
    ("FOLDER", L.FOLDER_ICON, form_folder(), (L.COL_SHARD_AMBER, L.COL_SHARD_CORAL)),
)
ICON_PAD = 3            # px round an icon's box for its hairline and shadow, and 2 more below
GLASS_FROST = ("DOWNLOAD",)     # white glass in every style (on the green button): Frost
FROST = dict(boost=0.48, hair=0.50, shadow=0.45)
EDGE = dict(hair=0.62, shadow=0.50)     # the stronger edge of icons below EDGE_BELOW px in Clear and Platin
EDGE_BELOW = 28


def glass_icon(name, style, n, form, tones):
    """An icon in a style, n px square where it stands; the sprite reaches ICON_PAD px past it (2 more below)."""
    img = Image(n + 2 * ICON_PAD, n + 2 * ICON_PAD + 2)
    tint = (rgb(tones[0]), rgb(tones[1]))
    if name in GLASS_FROST:
        style = "clear"
    if form == "house":
        s, base, shape = n / GLASS_U, glass_preset("logo"), glass_house()
        more = dict(diag=(34.0, 92.0), hair=0.55, shadow=0.50) if style == "shard" else {}
    else:
        s, base, shape = 1.0, glass_preset("icon", n), form
        more = dict(diag=(14.0 * n / 40.0, 66.0 * n / 40.0)) if style == "shard" else {}
    if name in GLASS_FROST:
        more.update(FROST)
    elif n < EDGE_BELOW and style != "shard":
        more.update(EDGE)
    off = ICON_PAD / s                  # the box's place in the shape's units: every run moves with it
    base = dict(base, y0=base["y0"] + off, y1=base["y1"] + off,
                bands=tuple((where + 2 * off, wide, a) for where, wide, a in base["bands"]))
    if "diag" in more:
        more["diag"] = (more["diag"][0] + 2 * off, more["diag"][1] + 2 * off)
    body, look, slab_only = glass_style(style, tint, base, **more)
    glass_slab(img, lambda u, v: shape(u - off, v - off), s, body, **slab_only, **look)
    return Sprite(name, img, (ICON_PAD, ICON_PAD, ICON_PAD, ICON_PAD + 2))


def glass_sheet(style):
    """The kit's sprites of one style's glass sheet, in the order of their ids."""
    return [glass_icon(name, style, n, form, tones) for name, n, form, tones in GLASS_ICONS]


# The patterns, in the order of ui.h's PATTERN_*: soft shapes (x, y, shape, tone) in a tile that repeats; tone 1
# lightens the background, -1 darkens it. Shapes are signed distances from their own middle.
def tile_dot(r):
    return lambda dx, dy: math.hypot(dx, dy) - r


def tile_ring(r, w):
    return lambda dx, dy: abs(math.hypot(dx, dy) - r) - w / 2.0


def tile_plus(arm, w):
    return lambda dx, dy: min(max(abs(dx) - arm, abs(dy) - w / 2.0), max(abs(dy) - arm, abs(dx) - w / 2.0)) - 0.4


def tile_sparkle(r, waist=0.32, soft_r=0.4):
    """Four points, drawn in to waist * r between them."""
    pts = []
    for k in range(8):
        a, d = math.radians(45 * k - 90), (r - soft_r) * (1.0 if k % 2 == 0 else waist)
        pts.append((d * math.cos(a), d * math.sin(a)))
    return sd_poly(pts, soft_r)


def tile_house(size):
    shape, k = glass_house(), size / 56.0
    return lambda dx, dy: shape(dx / k + 32.0, dy / k + 31.0) * k


PATTERNS = (
    # plus signs and rings (drifting down to the right)
    ((42, 9, tile_ring(5.0, 1.8), -1), (26, 36, tile_plus(4.0, 2.0), 1), (11, 53, tile_ring(3.6, 1.6), 1),
     (45, 57, tile_plus(3.6, 1.8), -1)),
    # small houses and sparkles (drifting to the left)
    ((16, 17, tile_house(15.0), 1), (48, 15, tile_sparkle(6.0), -1), (48, 49, tile_house(15.0), -1),
     (16, 48, tile_sparkle(6.0), 1), (32, 33, tile_dot(1.6), 1), (0, 0, tile_dot(1.6), -1)),
    # rings and dots (rising like bubbles)
    ((14, 15, tile_ring(7.0, 1.8), 1), (45, 9, tile_ring(4.0, 1.6), -1), (36, 33, tile_dot(2.4), 1),
     (52, 46, tile_ring(5.6, 1.8), -1), (18, 49, tile_ring(3.2, 1.5), 1), (5, 34, tile_dot(1.7), -1),
     (30, 58, tile_dot(1.4), -1)),
)


def pattern_tile(shapes):
    """A pattern's tile as straight RGBA bytes, row by row: every pixel carries the tone of the shape nearest to it,
    its alpha that shape's cover times PATTERN_ALPHA. The see-through pixels around a shape so have the shape's
    own tone: the GPU's linear filter mixes the colours of neighbouring texels whatever their alpha, and another
    tone there would tint the shape's edge each time the drift passes half a pixel."""
    n = L.PATTERN_TILE
    wrapped = lambda v: (v + n / 2.0) % n - n / 2.0
    light, dark, out = bytes(rgb(L.COL_PATTERN_LIGHT)), bytes(rgb(L.COL_PATTERN_DARK)), bytearray()
    for py in range(n):
        for px in range(n):
            d, tone = min((sd(wrapped(px + 0.5 - cx), wrapped(py + 0.5 - cy)), t) for cx, cy, sd, t in shapes)
            out += (light if tone > 0 else dark) + bytes([int(round(L.PATTERN_ALPHA / 100.0 * cover(d) * 255))])
    return bytes(out)


# ---------------------------------------------------------------- sprites


class Sprite:
    """An image in the atlas. pad: how far it reaches past the element it draws (a shadow, a ring, a symbol's light),
    left, top, right, bottom. cap: on an axis where it stretches, the fixed edges; 0 and 0 on an axis: drawn at its
    size."""

    def __init__(self, name, img, pad=(0, 0, 0, 0), cap=(0, 0, 0, 0)):
        self.name, self.img, self.pad, self.cap = name, img, tuple(pad), tuple(cap)
        self.at = (0, 0)


def sliced(name, w, h, pad, cap, draw):
    img = Image(w + pad[0] + pad[2], h + pad[1] + pad[3])
    draw(img, Box(pad[0], pad[1], pad[0] + w, pad[1] + h))
    return Sprite(name, img, pad, cap)


# A face's side shading reaches this far in from a corner's arc before it is under 1 %: the fixed ends of a face
# sprite are this much wider, so the repeated middle is drawn as the middle of a wide face is.
SHADE_REACH = 18


def across(name, h, r, pad, draw, extra=0):
    """An element of height h and any width: corners of radius r (and extra px beside them) fixed, the middle
    stretched."""
    end = r + 2 + extra
    return sliced(name, 2 * end + MID, h, pad, (pad[0] + end, 0, end + pad[2], 0), draw)


def upright(name, w, r, pad, draw):
    """An element of width w and any height."""
    return sliced(name, w, 2 * (r + 2) + MID, pad, (0, pad[1] + r + 2, 0, r + 2 + pad[3]), draw)


def both_ways(name, r, pad, draw):
    side = 2 * (r + 2) + MID
    return sliced(name, side, side, pad, (pad[0] + r + 2, pad[1] + r + 2, r + 2 + pad[2], r + 2 + pad[3]), draw)


def rounded(box, radii):
    return Box(box.x0, box.y0, box.x1, box.y1, radii)


def drop(dx=0, dy=2, blur=3):
    return (dx, dy, blur, rgb(L.COL_SHADOW), L.SHADOW_ALPHA / 100.0)


def arc_in(r, d):
    """How far a corner of radius r stands in from its straight edge, d px along that edge from the corner's end."""
    if not r or d >= r:
        return 0.0
    return r - math.sqrt(max(0.0, r * r - (r - d) ** 2))


class Edges:
    """Where the outline of a rounded box is, row by row and column by column (for the inner shading)."""

    def __init__(self, b):
        self.b = b

    def left(self, y):
        b = self.b
        return b.x0 + max(arc_in(b.r[0], y - b.y0), arc_in(b.r[3], b.y1 - y))

    def right(self, y):
        b = self.b
        return b.x1 - max(arc_in(b.r[1], y - b.y0), arc_in(b.r[2], b.y1 - y))

    def top(self, x):
        b = self.b
        return b.y0 + max(arc_in(b.r[0], x - b.x0), arc_in(b.r[1], b.x1 - x))

    def bottom(self, x):
        b = self.b
        return b.y1 - max(arc_in(b.r[3], x - b.x0), arc_in(b.r[2], b.x1 - x))


def table(points, d):
    """Linear between (d, value) points; 0 outside them."""
    if d <= points[0][0]:
        return points[0][1]
    for (d0, v0), (d1, v1) in zip(points, points[1:]):
        if d <= d1:
            return v0 + (v1 - v0) * (d - d0) / (d1 - d0)
    return 0.0


@functools.lru_cache(maxsize=None)
def stops(prefix, layout=None):
    """The colours in ui_layout.h named prefix + a row number, as (row, colour), by row."""
    layout = layout or L
    return sorted((int(n[len(prefix):]), rgb(getattr(layout, n))) for n in vars(layout)
                  if n.startswith(prefix) and n[len(prefix):].isdigit())


def at_row(rows, d):
    """The colour at row d of measured stops, linear between them, the last one's beyond."""
    if d <= rows[0][0]:
        return rows[0][1]
    for (d0, c0), (d1, c1) in zip(rows, rows[1:]):
        if d <= d1:
            return lerp(c0, c1, (d - d0) / (d1 - d0))
    return rows[-1][1]


# The System Settings' held tile, re-measured: two rows of rim under its top edge, its last eight rows from the bottom
# up, as measured in the middle of the tile (the sides' shading is put on them).
PRESS_TOP_ROWS = [c for _, c in stops("COL_PRESS_RIM_")]
PRESS_LOW_ROWS = [c for _, c in stops("COL_PRESS_LOW_")]


def luminance(c):
    """The relative luminance of an sRGB colour (channels 0..255; Rec. 709 primaries, as WCAG computes it)."""
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(x) for x in c)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def held_like(rest, ref, held):
    """rest, held the way the console's tiles are: their resting colour ref becomes held. The luminance changes by
    the same ratio, the saturation (HSV) closes the same share of its gap to full (or falls by the same ratio when it
    falls), the hue stays rest's. So a coloured button's held look follows the measured one without taking its
    yellow."""
    h, s, _ = colorsys.rgb_to_hsv(*(x / 255.0 for x in rest))
    s0, s1 = (colorsys.rgb_to_hsv(*(x / 255.0 for x in c))[1] for c in (ref, held))
    s = s + (s1 - s0) / (1 - s0) * (1 - s) if s1 >= s0 else s * s1 / s0
    want = luminance(rest) * luminance(held) / luminance(ref)
    lo, hi = 0.0, 1.0
    for _ in range(40):         # the value (HSV) that gives that luminance at that hue and saturation
        v = (lo + hi) / 2
        lo, hi = (v, hi) if luminance([x * 255 for x in colorsys.hsv_to_rgb(h, s, v)]) < want else (lo, v)
    return tuple(round(x * 255) for x in colorsys.hsv_to_rgb(h, s, (lo + hi) / 2))


def held_palette(top, low, shade):
    """A coloured button's held material from its resting colours: face, shade, and the rim and lower rows each
    related to the held face as the measured rows are to the held tile's. The measured tile is the yellow original's
    (D) in every scheme: the button is the same everywhere, and a dark scheme's faces give the relation no answer."""
    t, lo_, sh = (held_like(rgb(c), rgb(ref), rgb(h)) for c, ref, h in
                  ((top, D.COL_FACE_TOP, D.COL_PRESS_TOP), (low, D.COL_FACE_LOW, D.COL_PRESS_LOW),
                   (shade, D.COL_SHADE, D.COL_PRESS_SHADE)))
    rims = [held_like(t, rgb(D.COL_PRESS_TOP), c) for _, c in stops("COL_PRESS_RIM_", D)]
    lows = [held_like(lo_, rgb(D.COL_PRESS_LOW), c) for _, c in stops("COL_PRESS_LOW_", D)]
    return (t, lo_, sh, None, None), (rims, lows)


GO_HELD, GO_HELD_ROWS = held_palette(L.COL_GO_TOP, L.COL_GO_LOW, L.COL_GO_SHADE)
PALETTES = {
    "face": (L.COL_FACE_TOP, L.COL_FACE_LOW, L.COL_SHADE, L.COL_EDGE_TOP, L.COL_EDGE_LOW),
    "press": (L.COL_PRESS_TOP, L.COL_PRESS_LOW, L.COL_PRESS_SHADE, None, None),
    "go": (L.COL_GO_TOP, L.COL_GO_LOW, L.COL_GO_SHADE, L.COL_GO_EDGE_TOP, L.COL_GO_EDGE_LOW),
    "go_press": GO_HELD,
}
HELD_ROWS = {"press": (PRESS_TOP_ROWS, PRESS_LOW_ROWS), "go_press": GO_HELD_ROWS}


EDGE_LOW = L.EDGE_LOW_100 / 100.0


def held_row(rows, at, share):
    """A held face's measured row at `at` rows from the lower edge: the row itself on an element as high as the
    measured tile (share 1); on a lower one, where the rows are squeezed, the colour between the two rows' middles."""
    if share >= 1.0:
        return rows[int(at)]
    k = min(max(at - 0.5, 0.0), len(rows) - 1.0)
    return lerp(rows[int(k)], rows[min(int(k) + 1, len(rows) - 1)], k - int(k))


def material(img, b, palette, shadow=True, shaded=(1, 1, 1), edge_inset=(1, 1, 1, EDGE_LOW)):
    """The System Settings' tile, measured: a face that darkens from top to bottom (the gradient runs from 2 px
    below the top to 16 px above the lower edge and goes on beyond), shaded towards the sides (25 % at the edge,
    falling by e every 4.5 px) and strongly towards the lower edge (85 %, every 5 px), a 1 px edge (1.5 px along
    the bottom) that darkens from top to bottom, and a shadow 1 px narrower, its lower edge 1.5 px below the
    element's, blurred with sigma 1.2. A held face (no edge colours) gets the measured rows along its top and its
    bottom instead of the edge, and no shadow; lower than the tile they were measured on (PRESS_REF_H), its shade
    towards the lower edge (those rows and the shading above them) is squeezed and weakened by the same share, and
    its gradient stops where that tile's does."""
    top, low, shade, edge_top, edge_low = (rgb(c) if isinstance(c, int) else c for c in PALETTES[palette])
    top_rows, low_rows = HELD_ROWS.get(palette, (PRESS_TOP_ROWS, PRESS_LOW_ROWS))
    held = not edge_top
    share = min(1.0, (b.y1 - b.y0) / (L.PRESS_REF_H - 2.0)) if held else 1.0
    t_most = 1.0 + 16.0 / (L.PRESS_REF_H - 2.0 - 18.0) if held else INF
    e = Edges(b)
    if shadow:
        sh = b.moved(0, L.SHADOW_DROP_10 / 10.0 + 1).grown(-1)
        k, a = rgb(L.COL_SHADOW), L.SHADOW_ALPHA / 100.0
        img.paint(lambda x, y: (a * soft(sh.sd(x, y), 2.4) * (1.0 - cover(b.sd(x, y))), k))
    inner = b.inset(*edge_inset) if edge_top else None
    y_top, y_low = b.y0 + 2, b.y1 - 16
    side, low_k = L.SHADE_SIDE / 100.0, L.SHADE_LOW / 100.0

    def body(x, y):
        c = cover(b.sd(x, y))
        if c <= 0.0:
            return None
        base = lerp(top, low, min(t_most, (y - y_top) / (y_low - y_top)))
        sides = 1.0
        if shaded[0]:
            sides *= 1.0 - side * math.exp(-max(0.0, x - e.left(y) - 0.5) / 4.5)
        if shaded[1]:
            sides *= 1.0 - side * math.exp(-max(0.0, e.right(y) - x - 0.5) / 4.5)
        keep = sides
        from_low = (e.bottom(x) - y) / share      # as far from the lower edge as on the measured tile
        if shaded[2]:
            keep *= 1.0 - low_k * math.exp(-max(0.0, from_low - 2.0) / 5.0)
        col = lerp(base, shade, 1.0 - keep)
        if held:
            from_top = int(y - e.top(x))
            if from_top < len(top_rows):
                col = lerp(top_rows[from_top], shade, 1.0 - sides)
            else:
                if shaded[2] and from_low < len(low_rows):
                    col = lerp(held_row(low_rows, from_low, share), shade, 1.0 - sides)
                if share < 1.0:
                    col = lerp(lerp(base, shade, 1.0 - sides), col, share)
        else:
            k = max(0.0, c - cover(inner.sd(x, y))) / c
            col = lerp(col, lerp(edge_top, edge_low, (y - b.y0) / (b.y1 - b.y0)), k)
        return (c, col)
    img.paint(body)


FACE_PAD = (L.FACE_PAD_X, 0, L.FACE_PAD_X, L.FACE_PAD_LOW)
NO_PAD = (0, 0, 0, 0)


def face(name, h, r, palette="face"):
    """A tile, row or button; held ("press"): 1 px smaller on every side, no shadow."""
    held = palette.endswith("press")

    def draw(img, box):
        b = rounded(box, r)
        if held:
            material(img, b.inset(1, 1, 1, 1), palette, shadow=False)
        else:
            material(img, b, palette)
    return across(name, h, r, FACE_PAD, draw, SHADE_REACH)


def flat(name, h, r, color, edge=None):
    def draw(img, box):
        element(img, rounded(box, r), rgb(color), (1, rgb(edge)) if edge else None)
    return across(name, h, r, NO_PAD, draw)


def select_corner(name, radius, tail, flip_x, flip_y, color):
    """One of the selection's corners for an item corner of radius radius (ui_layout.h SELECT_*): a quarter of a
    rounded outline concentric with it, its arms straight for SELECT_TAIL past the curve with round ends, soft
    edges, a faint haze outside; drawn top left and mirrored for the others. The item's corner is at SELECT_PAD,
    SELECT_PAD: SELECT_FADE px of room outside the band and past the arms' round ends, where the haze has fallen to
    nothing, so it never ends at the image's edge."""
    w, aa, pad = L.SELECT_W, L.SELECT_AA_10 / 10.0, L.SELECT_PAD
    cx = cy = pad + radius                          # the item's corner circle
    rc = radius + L.SELECT_OUT - w / 2.0            # the band's middle line
    n = int(math.ceil(pad + radius + tail + w / 2.0 + L.SELECT_FADE))      # the same size at every shift
    img = Image(n, n)
    haze, fall = L.SELECT_HAZE / 100.0, L.SELECT_HAZE_FALL / 100.0

    def dist(x, y):
        """From the band's middle line (the arc, then the two straight arms), and whether outside the item."""
        dx, dy = x - cx, y - cy
        best = []
        if dx <= 0 and dy <= 0:                     # the arc
            r = math.hypot(dx, dy)
            best.append((abs(r - rc), r > rc))
        top_y, left_x = cy - rc, cx - rc
        ex = min(max(x, cx), cx + tail)             # the top arm, to the middle of its round end
        best.append((math.hypot(x - ex, y - top_y), y < top_y))
        ey = min(max(y, cy), cy + tail)             # the left arm
        best.append((math.hypot(x - left_x, y - ey), x < left_x))
        return min(best)

    def at(px, py):
        x, y = (n - px if flip_x else px), (n - py if flip_y else py)
        total = 0.0
        for i in range(4):
            for j in range(4):
                d, outside = dist(x - 0.5 + (i + 0.5) / 4, y - 0.5 + (j + 0.5) / 4)
                band = max(0.0, min(1.0, (w / 2.0 - d) / aa + 0.5))
                glow = haze * fall ** max(0.0, d - w / 2.0 - 0.5) if outside and d > w / 2.0 else 0.0
                total += max(band, glow)
        a = total / 16.0
        return (a, rgb(color)) if a > 0.003 else None
    img.paint(at)
    return Sprite(name, img)


# The corners the items need: (item corner radius, arm tail) by the rule in ui_layout.h (SELECT_TAIL_*): rows (radius
# ROW_R, 50 px high: 8), the folder rows (38 px: 4), the tiles and the green buttons (TILE_R: 8), the popup's buttons
# (41 px, square and DLG_BTN_R corners: 3).
SELECT_SHAPES = ((0, 3), (L.ROW_R, 4), (L.ROW_R, 8), (L.TILE_R, 3), (L.TILE_R, 8))


def select_corners():
    """Every corner image the selection needs, in the order ui.c counts them from ART_CORNER_R0_T3_TL_IN: per shape
    (SELECT_SHAPES), at rest and out, per position (top left, top right, bottom left, bottom right). They go into a
    texture of their own that the GPU reads with linear filtering, so a corner drawn at a fractional place moves
    smoothly."""
    out = []
    for radius, tail in SELECT_SHAPES:
        for tone, color in (("IN", L.COL_SELECT_IN), ("OUT", L.COL_SELECT_OUT)):
            for pos, fx, fy in (("TL", False, False), ("TR", True, False), ("BL", False, True), ("BR", True, True)):
                out.append(select_corner("CORNER_R%d_T%d_%s_%s" % (radius, tail, pos, tone), radius, tail, fx, fy,
                                         color))
    return out


def ellipse_sd(dx, dy, rx, ry):
    """Signed distance to an ellipse around the origin, close to its outline (where anti-aliasing needs it)."""
    k0 = math.hypot(dx / rx, dy / ry)
    k1 = math.hypot(dx / (rx * rx), dy / (ry * ry))
    if k1 == 0.0:
        return -min(rx, ry)
    return k0 * (k0 - 1.0) / k1


class BarShape:
    """The System Settings' bar: straight along the top, each upper corner a quarter ellipse BAR_RX across that
    reaches down to the screen's lower edge."""

    def __init__(self, x0, y0, x1, y1, rx, ry=None):
        self.x0, self.y0, self.x1, self.y1, self.rx = x0, y0, x1, y1, rx
        self.ry = y1 - y0 if ry is None else ry     # a corner may end above the lower edge (the tabs')

    def sd(self, px, py):
        cy = self.y0 + self.ry
        if px < self.x0 + self.rx:
            return ellipse_sd(px - (self.x0 + self.rx), min(0.0, py - cy), self.rx, self.ry)
        if px > self.x1 - self.rx:
            return ellipse_sd(px - (self.x1 - self.rx), min(0.0, py - cy), self.rx, self.ry)
        return self.y0 - py

    def top(self, x):
        """Where the outline is in column x, and how steep it is there."""
        ry = self.ry
        for cx, sign in ((self.x0 + self.rx, -1), (self.x1 - self.rx, 1)):
            dx = x - cx
            if dx * sign > 0:
                u = min(abs(dx) / self.rx, 0.999)
                root = math.sqrt(1.0 - u * u)
                return self.y0 + ry - ry * root, ry * u / (self.rx * root)
        return self.y0, 0.0


# The bar's light rim, measured in its first three rows: 56, 43 and 15 % of BAR_LIGHT_ALPHA's colour.
BAR_RIM = ((0.5, 1.0), (1.5, 0.77), (2.5, 0.27), (3.5, 0.0))


def bar(name, stops, light, x0=L.BAR_X, x1=L.BOTTOM_W - L.BAR_X, w=L.BOTTOM_W, rx=L.BAR_RX, ry=None):
    """The whole bar as one image, 320 wide: its shadow (a blur with sigma 3 of a shape 2 px wider, fitted to the
    measured shadow along the top and around the corners), the three-stop grey, the light rim along the top that
    thins out where the corners get steep, as in the screenshots. A tab: the same from x0 to x1 in an image w wide
    (an end beyond the image is the screen's edge)."""
    pad = L.BAR_SHADOW
    img = Image(w, L.BAR_H + pad)
    shape = BarShape(x0, pad, x1, pad + L.BAR_H, rx, ry)
    wide = BarShape(x0 - 2, pad, x1 + 2, pad + L.BAR_H, rx + 2, None if ry is None else ry + 2)
    k, a = rgb(L.COL_BAR_SHADOW), L.BAR_SHADOW_ALPHA / 100.0
    img.paint(lambda x, y: (a * soft(wide.sd(x, y), 6.0) * (1.0 - cover(shape.sd(x, y))), k))
    top, mid, low = (rgb(c) for c in stops)
    knee = 10.0 / L.BAR_H

    def body(x, y):
        c = cover(shape.sd(x, y))
        if c <= 0.0:
            return None
        t = (y - pad) / L.BAR_H
        return (c, lerp(top, mid, t / knee) if t < knee else lerp(mid, low, (t - knee) / (1.0 - knee)))
    img.paint(body)

    def rim(x, y):
        top, slope = shape.top(x)
        return (light * table(BAR_RIM, y - top) / (1.0 + slope * slope) * cover(shape.sd(x, y)), rgb(L.COL_BAR_LIGHT))
    img.paint(rim)
    return Sprite(name, img, (0, pad, 0, 0))


def tabs():
    """The two tabs: images TAB_W + the shadow's reach wide, the left one from the screen's left edge, the right one
    placed at BOTTOM_W - that width; the inner upper corner an ellipse TAB_RX across and TAB_RY high (fitted to the
    console's edge row by row), its outer end the screen's edge."""
    grey = (L.COL_BAR_TOP, L.COL_BAR_MID, L.COL_BAR_LOW)
    w, edge, rx, ry = L.TAB_W + L.BAR_SHADOW, L.TAB_EDGE_10 / 10.0, L.TAB_RX, L.TAB_RY
    left, right = (-2 * rx, edge), (w - edge, w + 2 * rx)
    return [bar("TAB_L", grey, L.BAR_LIGHT_ALPHA / 100.0, *left, w, rx, ry),
            bar("TAB_R", grey, L.BAR_LIGHT_ALPHA / 100.0, *right, w, rx, ry),
            bar_held("TAB_L_DOWN", *left, w, rx, ry), bar_held("TAB_R_DOWN", *right, w, rx, ry)]


def start_key(name, held, shadow=False):
    """The START key's pill; shadow: its engraving, the copy the label has (drawn LABEL_SHADOW_DY px above): dark at
    rest, white when held."""
    if shadow:
        color, alpha = ((L.COL_LABEL_HELD_SHADOW, L.LABEL_HELD_SHADOW_ALPHA / 100.0) if held
                        else (L.COL_LABEL_SHADOW, L.LABEL_SHADOW_ALPHA / 100.0))
    else:
        color, alpha = L.COL_PRESS_INK if held else L.COL_WHITE, 1.0

    def draw(img, box):
        b = rounded(box, L.START_R)
        inner = b.inset(1.5, 1.5, 1.5, 1.5)
        img.paint(lambda x, y: (alpha * max(0.0, cover(b.sd(x, y)) - cover(inner.sd(x, y))), rgb(color)))
    return across(name, L.START_H, L.START_R, NO_PAD, draw)


def bar_held(name, x0=L.BAR_X, x1=L.BOTTOM_W - L.BAR_X, w=L.BOTTOM_W, rx=L.BAR_RX, ry=None):
    """The bar (or a tab, as in bar) held down, as measured: yellow rows from its top, no shadow, no rim of its own."""
    pad = L.BAR_SHADOW
    img = Image(w, L.BAR_H + pad)
    shape = BarShape(x0, pad, x1, pad + L.BAR_H, rx, ry)
    rows = stops("COL_BAR_PRESS_")
    img.paint(lambda x, y: (cover(shape.sd(x, y)), at_row(rows, y - pad - 0.5)))
    return Sprite(name, img, (0, pad, 0, 0))


def corner(name, state):
    """The corner button (one folder up, with the path; the settings button), the tile's make, any width from
    CORNER_W: only its lower right corner is round, edges right and below, shaded towards them. It stands on the
    screen's left edge, so only its right end is fixed and the stretched middle is the face far from that end, its
    shadow even to the screen's edge (two fixed ends, each wider than half of CORNER_W, overlapped there and drew the
    edge and the shadow twice). Its symbol and path are drawn over it."""
    r = L.CORNER_R
    pad = NO_PAD if state == "off" else (0, 0, 4, 7)
    end = r + 2 + SHADE_REACH

    def draw(img, box):
        b = Box(box.x0 - end, box.y0, box.x1, box.y1, (0, 0, r, 0))
        if state == "off":
            element(img, b, rgb(L.COL_OFF), (1, rgb(L.COL_OFF_EDGE)), sides=(0, 0, 1, 1))
        elif state == "down":
            material(img, b.inset(0, 0, 1, 1), "press", shadow=False, shaded=(0, 1, 1))
        else:
            material(img, b, "face", shaded=(0, 1, 1), edge_inset=(0, 0, 1, EDGE_LOW))
    return sliced(name, end + MID, L.CORNER_H, pad, (0, 0, end + pad[2], 0), draw)


def help_button(name, held):
    """The round help button, the tile's make; its symbol is the glass sheet's HELP_MARK, drawn over it."""
    n, pad = L.HELP_FACE, FACE_PAD
    img = Image(n + pad[0] + pad[2], n + pad[1] + pad[3])
    b = Box(pad[0], pad[1], pad[0] + n, pad[1] + n, n / 2.0)
    if held:
        material(img, b.inset(1, 1, 1, 1), "press", shadow=False)
    else:
        material(img, b, "face")
    return Sprite(name, img, pad)


DLG_BASE = None     # the panel's colour away from its edges: where both measured profiles end


def dlg_light(c, ref):
    """How far c is from ref towards white, per channel."""
    return tuple((c[i] - ref[i]) / (255.0 - ref[i]) if ref[i] < 255 else 0.0 for i in range(3))


def dlg_panel_at(dx, dy):
    """The panel dx px in from its nearer side and dy px down from its top (popup_band.h): the two measured profiles,
    each the panel lightened towards white near that edge, put together as two such lightenings are."""
    side, top = at_row(stops("COL_DLG_SIDE_"), dx), at_row(stops("COL_DLG_TOP_"), dy)
    base = stops("COL_DLG_SIDE_")[-1][1]
    a, b = dlg_light(side, base), dlg_light(top, base)
    return tuple(base[i] + (255 - base[i]) * (1 - (1 - a[i]) * (1 - b[i])) for i in range(3))


def dialog_panel():
    """The popup's panel: its soft shadow, and the panel lighter near its sides and top, as measured. The button band
    covers its lower edge."""
    r, pad = L.DLG_R, (L.DLG_SHADOW_BLUR,) * 4
    low = L.DLG_BTN_R_10 / 10.0     # the lower corners are the button band's
    reach_x = stops("COL_DLG_SIDE_")[-1][0] + 1
    reach_y = stops("COL_DLG_TOP_")[-1][0] + 1
    w, h = 2 * reach_x + MID, reach_y + MID + int(math.ceil(low)) + 2
    img = Image(w + 2 * pad[0], h + 2 * pad[1])
    b = Box(pad[0], pad[1], pad[0] + w, pad[1] + h, (r, r, low, low))
    e = Edges(b)
    k, a = rgb(L.COL_DLG_SHADOW), L.DLG_SHADOW_ALPHA / 100.0
    img.paint(lambda x, y: (a * soft(b.sd(x, y), L.DLG_SHADOW_BLUR) * (1.0 - cover(b.sd(x, y))), k))

    def body(x, y):
        c = cover(b.sd(x, y))
        if c <= 0.0:
            return None
        dx = max(0.0, min(x - e.left(y), e.right(y) - x) - 0.5)       # 0 in the first column (pixel centres)
        return (c, dlg_panel_at(int(dx), int(max(0.0, y - e.top(x)))))
    img.paint(body)
    caps = (pad[0] + reach_x, pad[1] + reach_y, reach_x + pad[2], int(math.ceil(low)) + 2 + pad[3])
    return Sprite("DLG", img, pad, caps)


BAND_STRAIGHT = 25      # the band's rows measured at its outer edges: 0 to 25 (rows 187 to 212); the corners below


def band_column(prefix, d, face_prefix, held):
    """Row d of a measured edge column; below the measured rows it keeps its last relation to the face (lighter
    towards white at rest, a factor when held)."""
    rows = stops(prefix)
    if d <= rows[-1][0]:
        return at_row(rows, d)
    last, face_then = at_row(rows, rows[-1][0]), at_row(stops(face_prefix), rows[-1][0])
    face = at_row(stops(face_prefix), d)
    if held:
        return tuple(min(255.0, face[i] * last[i] / max(1.0, face_then[i])) for i in range(3))
    a = dlg_light(last, face_then)
    return tuple(face[i] + (255 - face[i]) * a[i] for i in range(3))


def band_at(e, d, held):
    """The band e px in from an outer edge (fractional in the corners), row d (0: its dark line)."""
    tag = "HELD_E" if held else "E"
    edges = len([n for n in vars(L) if re.fullmatch(r"COL_DLG_%s\d\d_00" % tag, n)])
    cols = ["COL_DLG_%s%02d_" % (tag, k) for k in range(edges)]
    face_prefix = "COL_DLG_HELD_" if held else "COL_DLG_FACE_"
    face = at_row(stops(face_prefix), d)
    pick = lambda k: band_column(cols[k], d, face_prefix, held) if k < len(cols) else face
    k = int(e)
    return lerp(pick(k), pick(k + 1), e - k)


def dialog_button(name, held):
    """The popup's button band across the panel's lower edge, as measured (popup_band.h): one row of the panel's
    shadow above it, its dark line, the face row by row, the edges column by column, the lower corners of radius
    DLG_BTN_R_10 / 10 (fitted to the corners' anti-aliased edge)."""
    h, r = L.DLG_BTN_H + 1, L.DLG_BTN_R_10 / 10.0
    end = max(int(math.ceil(r)) + 6, stops("COL_DLG_SIDE_")[-1][0] + 1)   # the ends reach past the panel's glow
    w = 2 * end + MID
    base = stops("COL_DLG_SIDE_")[-1][1]
    face = stops("COL_DLG_FACE_")[0][1]
    line = tuple(face[i] / base[i] for i in range(3))       # the dark line: the panel above it times this
    img = Image(w, h)
    b = Box(0, 1, w, h, (0, 0, r, r))
    e = Edges(b)
    shadow = L.DLG_SHADOW_ROW_PER_MILLE / 1000.0

    def at(x, y):          # pixel centres
        ix, iy = int(x), int(y)
        if iy == 0:     # the panel's last row, in the band's shadow
            return (1.0, tuple(v * shadow for v in dlg_panel_at(min(ix, w - 1 - ix), 99)))
        c = cover(b.sd(x, y))
        if c <= 0.0:
            return None
        d = iy - 1
        if d == 0:
            return (1.0, tuple(v * line[i] for i, v in enumerate(dlg_panel_at(min(ix, w - 1 - ix), 99))))
        if d > BAND_STRAIGHT:   # in the corners: how far in from the curved outline, as from a straight one
            edge = max(0.0, min(x - e.left(y), e.right(y) - x) - 0.5)
        else:
            edge = min(ix, w - 1 - ix)
        return (c, band_at(edge, d, held))
    img.paint(at)
    return Sprite(name, img, NO_PAD, (end, 0, end, 0))


def dialog_divider(name, columns):
    """Between the two buttons, forty columns as measured (popup_band.h), from x 140 (the dark line at 159, the
    light one at 160): each column's prefix, or a (prefix, face prefix, held) to carry on below its measured rows."""
    img = Image(len(columns), L.DLG_BTN_H)

    def at(x, y):
        col, d = columns[int(x)], int(y)
        if isinstance(col, tuple):
            return (1.0, band_column(col[0], d, col[1], col[2]))
        return (1.0, at_row(stops(col), d))
    img.paint(at)
    return Sprite(name, img)


def dialog_dividers():
    rest = ["COL_DLG_DIV_%02d_" % k for k in range(40)]
    held_left = ["COL_DLG_DIV_HELD_%02d_" % k for k in range(20)] + rest[20:]
    # the right one held: no console shot of it; its edge at the divider as the held left one's outer edge
    held_right = rest[:20] + [("COL_DLG_HELD_E%02d_" % k, "COL_DLG_HELD_", True) for k in range(20)]
    return [dialog_divider("DLG_DIVIDER", rest), dialog_divider("DLG_DIVIDER_LEFT_DOWN", held_left),
            dialog_divider("DLG_DIVIDER_RIGHT_DOWN", held_right)]


def track():
    def draw(img, box):
        element(img, rounded(box, L.SCROLL_R), rgb(L.COL_TRACK), insets=(drop(0, 1, 2),))
    return upright("TRACK", L.SCROLL_W, L.SCROLL_R, NO_PAD, draw)


def thumb(name, held):
    a, b_, edge = ((L.COL_THUMB_PRESS_A, L.COL_THUMB_PRESS_B, L.COL_THUMB_PRESS_EDGE) if held
                   else (L.COL_FACE_TOP, L.COL_THUMB_LOW, L.COL_THUMB_EDGE))

    def draw(img, box):
        b = rounded(box, L.SCROLL_R)
        element(img, b, horizontal(rgb(a), rgb(b_), b.x0, b.x1), (1, rgb(edge)), shadows=(drop(0, 1, 2),))
    return upright(name, L.SCROLL_W, L.SCROLL_R, (3, 1, 3, 4), draw)


def card():
    def draw(img, box):
        element(img, rounded(box, L.CARD_R), rgb(L.COL_CARD))
    return both_ways("CARD", L.CARD_R, NO_PAD, draw)


def disc(name, size, color):
    img = Image(size, size)
    b = Box(0, 0, size, size, size / 2.0)
    img.paint(lambda x, y: (cover(b.sd(x, y)), rgb(color)))
    return Sprite(name, img)


def stripes():
    """One period of the progress bar's stripes: a light band at 45 degrees, half the period wide, repeated side by
    side, as strong over the tube's whole height (the fill's outline cuts them: stripe_ends)."""
    w, h, n = L.STRIPE_W, L.PROGRESS_H, 4
    img = Image(w, h)
    a = L.STRIPE_ALPHA / 100.0

    def shade(x, y):
        hits = 0
        for i in range(n):
            for j in range(n):
                if (x - 0.5 + (i + 0.5) / n + y - 0.5 + (j + 0.5) / n) % w < w / 2.0:
                    hits += 1
        return (a * hits / (n * n), rgb(L.COL_STRIPE))
    img.paint(shade)
    return Sprite("STRIPES", img)


# The stripes over the fill take its full height and end cleanly at its edges, at both ends: the stripes cut
# by the fill's own outline. For every glass style whose fill the look
# draws, one sprite STRIPES(_STYLE) of two rows PROGRESS_H high: the first holds the fill's left end under the stripes
# at each of the STRIPE_W phases (frame p: the period moved p px to the right), then one period over the fill's
# straight middle; the second row the right end's STRIPE_W frames. A pixel is the period's, times how much of it the
# fill covers (its alpha against the fill's most opaque one): the stripes reach every row the fill has and end where
# its outline ends, round or slanted. ui.c fill() picks the frames.
def stripes_over(name, period, fill):
    w, h, left, right = L.STRIPE_W, L.PROGRESS_H, fill.cap[0], fill.cap[2]
    assert fill.pad == NO_PAD and fill.img.h == h and (period.img.w, period.img.h) == (w, h), name
    full = max(fill.img.p[3::4]) or 1.0
    img = Image(w * left + w, 2 * h)

    def put(x, y, from_x, fill_x, row):
        k = min(1.0, fill.img.p[4 * (y * fill.img.w + fill_x) + 3] / full)
        i, j = 4 * ((y + row * h) * img.w + x), 4 * (y * w + from_x)
        img.p[i:i + 4] = [v * k for v in period.img.p[j:j + 4]]
    for y in range(h):
        for p in range(w):
            for x in range(left):
                put(p * left + x, y, (x - p) % w, x, 0)
            for x in range(right):
                put(p * right + x, y, (x - p) % w, fill.img.w - right + x, 1)
        for x in range(w):
            put(w * left + x, y, x, left, 0)
    return Sprite(name, img)


def stripe_ends(items):
    """items with the period STRIPES replaced by STRIPES_<style> (stripes_over) of every fill among them, in the
    styles' order."""
    by = {s.name: s for s in items}
    made = [stripes_over(style_name("STRIPES", style), by["STRIPES"], by[style_name("PROGRESS_FILL", style)])
            for style in GLASS_STYLES if style_name("PROGRESS_FILL", style) in by]
    at = [s.name for s in items].index("STRIPES")
    return items[:at] + made + items[at + 1:]


# The bar's A and B keys, our own drawing: a ring and the letter, 24 units.
KEY_RING = ("circle", 12, 12, 11)
KEY_LETTERS = {
    "A": [("path", "M7.6 17.6L12 6.6l4.4 11M9.2 13.7h5.6")],
    "B": [("path", "M8.6 6.6h4.1a2.6 2.6 0 0 1 0 5.2H8.6zM8.6 11.8h4.6a2.9 2.9 0 0 1 0 5.8H8.6z")],
}


# the Y key: the key that opens the preview
KEY_LETTERS["Y"] = [("path", "M7.8 6.6L12 12.4l4.2-5.8M12 12.4v5.2")]


def coverage(img, lines, scale, width, dx=0.0, dy=0.0):
    """How much of each pixel a stroke covers, as a list (the same anti-aliasing as stroke())."""
    half = width / 2.0
    best = nearest(img, list(segments(lines, scale, dx, dy)), half + 1)
    return [min(1.0, max(0.0, half + 0.5 - math.sqrt(d2))) if d2 < INF else 0.0 for d2 in best]


def keycap(name, letter, held=False):
    """A key of the bar, engraved like its label but without doubling the thin ring: the dark copy LABEL_SHADOW_DY
    px above darkens only outside the ring's disc (a crescent over it) and above the letter's strokes; the white
    on top.
    Held: in the held ink, the copy above white (as the console's held bar, layout.h)."""
    n, up = L.KEYCAP, L.LABEL_SHADOW_DY
    img = Image(n, n + up)
    s = n / 24.0
    ring, mark = shape_lines(KEY_RING), lines_of(KEY_LETTERS[letter])
    disc = Box(s * 1, up + s * 1, s * 23, up + s * 23, s * 11)
    ring_up = coverage(img, ring, s, 2.1 * s, 0, 0)
    mark_up = coverage(img, mark, s, 2.4 * s, 0, 0)
    ring_at = coverage(img, ring, s, 2.1 * s, 0, up)
    mark_at = coverage(img, mark, s, 2.4 * s, 0, up)
    dark, a = ((rgb(L.COL_LABEL_HELD_SHADOW), L.LABEL_HELD_SHADOW_ALPHA / 100.0) if held
               else (rgb(L.COL_LABEL_SHADOW), L.LABEL_SHADOW_ALPHA / 100.0))
    ink = rgb(L.COL_PRESS_INK if held else L.COL_LABEL)
    for k in range(n * (n + up)):
        x, y = k % n + 0.5, k // n + 0.5
        shade = max(ring_up[k] * (1.0 - cover(disc.sd(x, y))), mark_up[k])
        if shade > 0:
            img.over(k, dark, a * shade)
        white = max(ring_at[k], mark_at[k])
        if white > 0:
            img.over(k, ink, white)
    return Sprite(name, img, (0, up, 0, 0))


def sprites():
    """The kit's sprites of the atlas as art.py draws them, in the order of their ids (the application's follow them:
    APP.sprites)."""
    orange, muted, off = L.COL_ORANGE, L.COL_MUTED, L.COL_OFF_INK

    def sym(name, icon, size, color, width, light=True, slash=False):
        """A sprite of the Tabler icon named icon."""
        return Sprite(name, symbol(tabler(icon), size, rgb(color), width, light=light, slash=slash),
                      (0, 0, 0, 1) if light else NO_PAD)

    def named(name, sprite):
        sprite.name = name
        return sprite
    return [
        face("TILE", L.TILE_H, L.TILE_R),
        face("TILE_DOWN", L.TILE_H, L.TILE_R, "press"),
        face("DTILE", L.DTILE_H, L.TILE_R),
        face("DTILE_DOWN", L.DTILE_H, L.TILE_R, "press"),
        face("WIDE", L.WIDE_H, L.TILE_R),
        face("WIDE_DOWN", L.WIDE_H, L.TILE_R, "press"),
        flat("TILE_OFF", L.TILE_H, L.TILE_R, L.COL_OFF, L.COL_OFF_EDGE),
        face("CHOICE", L.CHOICE_H, L.TILE_R),
        face("CHOICE_DOWN", L.CHOICE_H, L.TILE_R, "press"),
        face("ROW", L.ROW_H, L.ROW_R),
        face("ROW_DOWN", L.ROW_H, L.ROW_R, "press"),
        flat("ROW_OFF", L.ROW_H, L.ROW_R, L.COL_OFF, L.COL_OFF_EDGE),
        face("FOLDER_ROW", L.FOLDER_H, L.ROW_R),
        face("FOLDER_ROW_DOWN", L.FOLDER_H, L.ROW_R, "press"),
        face("SET_ROW", L.SET_H, L.ROW_R),
        face("SET_ROW_DOWN", L.SET_H, L.ROW_R, "press"),
        face("GO", L.GO_H, L.GO_R, "go"),
        face("GO_DOWN", L.GO_H, L.GO_R, "go_press"),
        face("ALT", L.GO_H, L.GO_R),
        face("ALT_DOWN", L.GO_H, L.GO_R, "press"),
        bar("BAR", (L.COL_BAR_TOP, L.COL_BAR_MID, L.COL_BAR_LOW), L.BAR_LIGHT_ALPHA / 100.0),
        bar_held("BAR_DOWN"),
        # the two tabs: images TAB_W + the shadow's reach wide, the left one from the screen's left edge, the right
        # one placed at BOTTOM_W - that width
        *tabs(),
        start_key("START", False),
        start_key("START_DOWN", True),
        start_key("START_SHADOW", False, shadow=True),
        start_key("START_DOWN_SHADOW", True, shadow=True),
        keycap("KEY_A", "A"),
        keycap("KEY_B", "B"),
        keycap("KEY_A_DOWN", "A", held=True),
        keycap("KEY_B_DOWN", "B", held=True),
        corner("CORNER", "up"),
        corner("CORNER_DOWN", "down"),
        corner("CORNER_OFF", "off"),
        card(),
        stripes(),
        track(),
        thumb("THUMB", False),
        thumb("THUMB_DOWN", True),
        disc("DROP_SUN", L.DROP_SUN, L.COL_DROP_SUN),
        disc("DOT", L.DOT, L.COL_DOTS),
        named("WARN_FOOT", glyph([("path", WARNING)], L.CAP_LINE, rgb(L.COL_ALERT), WARNING_SOFT, filled=True)),
        sym("FOLDER_UP_OFF", "folder-up", L.CORNER_ICON, off, 2, light=False),
        named("CHECK_ROW", glyph(tabler("check"), L.CAP_ROW_NAME, rgb(muted), 2.6, light=True)),
        sym("WARN", "alert-triangle", L.WARN_ICON, orange, 1.8),
        # the notes' signs (ui_popup_sign; the box has none), one size; a refusal's in orange, the
        # SD card's calm
        sym("INFO_WARN", "alert-triangle", L.INFO_ICON, orange, 1.8),
        help_button("HELP", False),
        help_button("HELP_DOWN", True),
        dialog_panel(),
        dialog_button("DLG_BUTTON", False),
        dialog_button("DLG_BUTTON_DOWN", True),
        *dialog_dividers(),
        # the Y key that opens the preview
        keycap("KEY_Y", "Y"),
        keycap("KEY_Y_DOWN", "Y", held=True),
        # the installing screen's
        *installing(),
    ]


# ---------------------------------------------------------------- installing (liquid glass)
# The installing screen's step marks and line, the loader, the download's bar and the calm glass edge of the cards
# of the installing and download screens, per glass style, drawn
# with the app's layout values. The glass lies over the scheme's card (the card's colour is baked
# in where a mark is, so a mark covers the line under it), so these are each scheme's own sprites in the atlas, not
# glass-sheet sprites; the glass's shadow and inner shade are COL_GLASS_SHADOW, the liquid the fixed teal.

LIQ_TUBE_SHADE = dict(shadow=0.30, hair=0.30, drop=1.5, blur=2.5)


def liq_bell(t):
    return math.exp(-t * t)


def liq_normal(shape, x, y, h=0.4):
    """Which way a shape's nearest edge faces at a point (outwards), as a unit vector."""
    nx, ny = shape(x + h, y) - shape(x - h, y), shape(x, y + h) - shape(x, y - h)
    n = math.hypot(nx, ny) or 1.0
    return nx / n, ny / n


def liq_shade_of(shape, shadow=0.40, hair=0.36, drop=2.0, blur=3.0):
    """glass_slab's soft shadow and hairline round a shape, as how dark the ground gets at a point."""
    return lambda x, y: max(shadow * soft(shape(x, y - drop), blur), hair * cover(shape(x, y) - 0.7))


def liq_under(img, shape, shade, there=None):
    """Paints shade (liq_shade_of) round shape. there: the shade of something under this sprite that its own sprite
    draws (the tube under a mark): only what is missing is added, so the two never add up."""
    dark = rgb(L.COL_GLASS_SHADOW)

    def at(x, y):
        out = 1.0 - cover(shape(x, y))
        if out <= 0.0:
            return None
        a = shade(x, y)
        if there:
            t = there(x, y)
            a = (a - t) / (1.0 - t) if a > t else 0.0
        return (a * out, dark)
    img.paint(at)


def liq_looks():
    """What thick clear glass looks like on the scheme's card: for a bead and for a tube 8 px across."""
    shade = lerp(rgb(L.COL_CARD), rgb(L.COL_GLASS_SHADOW), 0.68)
    bead = dict(shade=shade, tint=0.09, dark=0.70, dark_w=4.5, bright=0.66, bright_at=2.2, bright_w=1.5, spec=0.95,
                spec_at=3.6, spec_w=1.1, spec_cos=0.78, rim_w=1.1, rim_lit=0.97, rim_side=0.55)
    tube = dict(shade=shade, tint=0.07, dark=0.36, dark_w=1.3, bright=0.50, bright_at=1.5, bright_w=0.9, spec=0.0,
                spec_at=2.0, spec_w=0.6, spec_cos=0.80, rim_w=1.0, rim_lit=0.90, rim_side=0.40)
    return bead, tube


def liq_teal(lit):
    """Teal liquid as a drop of coloured water is lit: deeper on the side towards the light (lit up to 1), lighter
    on the far side (down to -1)."""
    top = rgb(L.COL_TUBE_FILL_TOP)
    mid = lerp(top, rgb(L.COL_GREEN), 0.62)
    if lit > 0.0:
        return lerp(mid, rgb(L.COL_TUBE_FILL_LOW), 0.70 * min(1.0, lit) ** 1.3)
    return lerp(mid, lerp(top, WHITE, 0.30), 0.80 * min(1.0, -lit) ** 1.3)


def liq_lit_tube(cy, half):
    return lambda x, y: (y - cy) * TO_LIGHT[1] / half


def liq_glass(img, shape, look_at, liquid=None, lit=None, only_liquid=False, bead=None):
    """Thick clear glass over the card: a dark crescent inside the edge that faces the light, a bright one inside
    the far edge, a sharp light on the lit side, a bright thin rim; the glass takes a little of the shade. liquid: a
    shape inside it filled with teal liquid, lit as liq_teal wants it. only_liquid: draw only where the liquid is.
    bead: (cx, cy, r) of the round part of a shape that runs out into a tube: the sharp light follows that circle."""
    card = rgb(L.COL_CARD)

    def at(x, y):
        d = shape(x, y)
        c = cover(d)
        if c <= 0.0:
            return None
        col, wet = card, 1.0
        P = look_at(x, y)
        if liquid:
            wet = cover(liquid(x, y))
            if wet > 0.0:
                col = lerp(col, liq_teal(lit(x, y)), wet)
        if only_liquid and wet <= 0.0:
            return None
        e = max(0.0, -d)
        nx, ny = liq_normal(shape, x, y)
        f = nx * TO_LIGHT[0] + ny * TO_LIGHT[1]
        col = lerp(col, P["shade"], P["tint"] + P["dark"] * max(0.0, f) ** 1.3 * math.exp(-e / P["dark_w"]))
        col = lerp(col, WHITE, P["bright"] * max(0.0, -f) ** 1.3 * liq_bell((e - P["bright_at"]) / P["bright_w"]))
        if P["spec"]:
            fs, es = f, e
            if bead:
                q = math.hypot(x - bead[0], y - bead[1]) or 1.0
                fs, es = ((x - bead[0]) * TO_LIGHT[0] + (y - bead[1]) * TO_LIGHT[1]) / q, bead[2] - q
            col = lerp(col, WHITE, P["spec"] * smooth(P["spec_cos"], 1.0, fs) * liq_bell((es - P["spec_at"]) /
                                                                                          P["spec_w"]))
        col = lerp(col, WHITE, clamp(P["rim_w"] + 0.5 - e) * (P["rim_side"] + (P["rim_lit"] - P["rim_side"]) * f * f))
        return (c * (wet if only_liquid else 1.0), col)
    img.paint(at)


def liq_metal(img, shape, lean, bow=0.16, spot=None, warm=0.55):
    """Platin's two tones on a rounded body as polished metal: what leans up mirrors the sky, what leans down the
    ground, darker at the horizon and towards the bottom the card's colour; a lit rim, a dark far rim. lean(x, y, e,
    nx, ny): how far the surface leans sideways and down (-1 to 1) and the horizon below the middle. spot: (x, y,
    radius) of the sharp light."""
    sky_a = rgb(L.COL_PLATIN_SKY_A)
    sky_low = lerp(rgb(L.COL_PLATIN_SKY_B), rgb(L.COL_PLATIN_GROUND_A), 0.45)
    deep = lerp(rgb(L.COL_PLATIN_GROUND_A), rgb(L.COL_PLATIN_SHADE), 0.55)
    low = lerp(rgb(L.COL_PLATIN_GROUND_B), lerp(rgb(L.COL_CARD), WHITE, 0.25), warm)

    def at(x, y):
        d = shape(x, y)
        c = cover(d)
        if c <= 0.0:
            return None
        e = max(0.0, -d)
        nx, ny = liq_normal(shape, x, y)
        tx, ty, horizon = lean(x, y, e, nx, ny)
        v = ty - (horizon - bow * tx * tx) + 0.05 * tx
        sky = lerp(sky_low, sky_a, clamp(-v / (1.0 + horizon)) ** 0.75)
        ground = lerp(deep, low, 0.92 * smooth(0.0, 0.95, clamp(v / (1.0 - horizon))))
        col = lerp(sky, ground, clamp(0.5 + v / 0.05))
        if spot:
            col = lerp(col, WHITE, 0.95 * liq_bell(math.hypot(x - spot[0], y - spot[1]) / spot[2]))
        facing = nx * TO_LIGHT[0] + ny * TO_LIGHT[1]
        col = lerp(col, WHITE, 0.60 * clamp(1.3 - e) * max(0.0, facing))
        col = lerp(col, deep, 0.60 * clamp(1.2 - e) * max(0.0, -facing) ** 1.5)
        return (c, col)
    img.paint(at)


def liq_shard(tint, deep, alpha=None, rim=1.0, y0=2.0, y1=2.0 + 8):
    base = dict(rim=rim, bands=(), hair=0.36, shadow=0.30, y0=y0, y1=y1)
    return glass_style("shard", tint, base, deep=deep, alpha=alpha)


def liq_tints():
    """Shard's pale glass (the tone of a mark to come) and its teal glass (the done mark's)."""
    dots, green = rgb(L.COL_DOTS), rgb(L.COL_GREEN)
    return (lerp(dots, WHITE, 0.30), dots), (lerp(green, WHITE, 0.22), green)


# The bar in the scheme's colour (in every style the bar follows the chosen scheme, except in Gelb, where it stays
# teal): the download's bar in the scheme's colour in every look, Gelb's (BAR_KEEPS) in the teal it is drawn in.
# Take a: the scheme's liquid in the installing tile (liquid.liquid_tone of the look's COL_POUR), so the bar
# and the tile are one family; take b: the scheme's loader tone (schemes.py, COL_LOADER). Each look's bar keeps its
# drawing and its luminances (bar_turned), so every contrast of the bar stays what it was.
BAR_TAKE = "a"
BAR_KEEPS = ("GELB",)


def bar_keeps():
    return schemes.SCHEMES[SCHEME]["name"] in BAR_KEEPS


def bar_tone():
    """The download bar's tone in the drawn scheme, (r, g, b) of 0 to 255: BAR_TAKE's; in a scheme of BAR_KEEPS
    today's teal (COL_GREEN)."""
    if bar_keeps():
        return rgb(L.COL_GREEN)
    # Prisma draws its tile and its loader in tones of its own (look_prism.py tones, pour_tone), not COL_POUR's
    own = LOOK.tones(SCHEME) if hasattr(LOOK, "pour_tone") else None
    if BAR_TAKE == "b":
        return own["bead"] if own else rgb(L.COL_LOADER)
    return liquid.liquid_tone(LOOK.pour_tone(own) if own else rgb(L.COL_POUR))


def bar_turned(c, ref=None):
    """A colour c of a look's bar as drawn for the tone ref (COL_GREEN unless given), in bar_tone()'s family: its hue
    turned by the tone's hue less ref's, its chroma scaled by the tone's over ref's (as far as sRGB shows it there),
    its luminance kept; c itself in a scheme of BAR_KEEPS and where the tone is ref's within rounding (Prisma's Weiss,
    whose tile is teal)."""
    if bar_keeps():
        return c
    _, rc, rh = schemes.oklch(rgb(L.COL_GREEN) if ref is None else ref)
    _, tc, th = schemes.oklch(bar_tone())
    if abs((th - rh + 180.0) % 360.0 - 180.0) < 1.0 and abs(tc - rc) < 0.005:
        return c
    _, cc, ch = schemes.oklch(c)
    return tuple(float(v) for v in schemes.colour_at((ch + th - rh) % 360.0, cc * tc / rc, schemes.luminance(c)))


def liq_loader_beads():
    """A bead of Shard glass, pale, and the teal one drawn over it where the light is."""
    pad, n = 3, L.LOADER_BEAD
    side = n + 2 * pad
    c = side / 2.0
    out = []
    for name, tint, deep, alpha in zip(("LOADER_BEAD", "LOADER_BEAD_ON"), liq_tints(), (False, True), (0.62, None)):
        img = Image(side, side)
        base = dict(rim=1.0, bands=((2 * c - 3.0, 1.6, 0.50),), hair=0.36, shadow=0.34, y0=c - n / 2.0,
                    y1=c + n / 2.0)
        body, look, _ = glass_style("shard", tint, base, deep=deep, alpha=alpha)
        glass_slab(img, sd_circle(c, c, n / 2.0), 1.0, body, hair=0.40, shadow=0.36, **look)
        out.append(Sprite(name, img, (pad,) * 4))
    return out


def liq_bars(style):
    """The download's bar in a style, as the track and the fill, each stretching across (PROGRESS_H high): Clear the
    tube with teal liquid, its front round; Platin the same (its collars: liq_collar); Shard a rod of pale glass with
    glass of the scheme's bar tone in it (bar_turned of the teal), both cut on the same slant at both ends (not the
    fill's front alone)."""
    h, r, pad = L.PROGRESS_H, L.PROGRESS_R, (3, 2, 3, 5)
    if style == "shard":
        pale, teal = liq_tints()
        teal = tuple(bar_turned(c) for c in teal)

        # the slant on both sides, not only the right, and the grey track behind in the same form: the
        # track and the fill one parallelogram, the top leading by 7 px at each end, so the fill's ends lie on the
        # track's at the left and parallel inside it at the right
        def rod(b):
            return sd_poly([(b.x0 + 9.0, b.y0 + 2.0), (b.x1 - 2.0, b.y0 + 2.0), (b.x1 - 9.0, b.y1 - 2.0),
                            (b.x0 + 2.0, b.y1 - 2.0)], 2.0)

        def track(img, b):
            body, look, slab = liq_shard(pale, False, 0.62, rim=1.2, y0=b.y0, y1=b.y1)
            glass_slab(img, rod(b), 1.0, body, **slab, **look)

        def fill(img, b):
            body, look, slab = liq_shard(teal, True, rim=1.2, y0=b.y0, y1=b.y1)
            glass_slab(img, rod(b), 1.0, body, **slab, **look)
        extra = 6
    else:
        _, tube_look = liq_looks()
        look = dict(tube_look, dark_w=1.9, bright_at=2.1, bright_w=1.2, rim_w=1.1)
        wall = 1.8

        def track(img, b):
            shape = Box(b.x0, b.y0, b.x1, b.y1, r).sd
            liq_under(img, shape, liq_shade_of(shape, **LIQ_TUBE_SHADE))
            liq_glass(img, shape, lambda x, y: look)

        def fill(img, b):
            whole = Box(b.x0 - 60.0, b.y0, b.x1 + 60.0, b.y1, r).sd
            liquid = Box(b.x0 + wall, b.y0 + wall, b.x1, b.y1 - wall, r - wall).sd
            liq_glass(img, whole, lambda x, y: look, liquid=liquid, lit=liq_lit_tube((b.y0 + b.y1) / 2.0, r - wall),
                      only_liquid=True)
        extra = 0
    return [across(style_name("PROGRESS", style), h, r, pad, track, extra),
            across(style_name("PROGRESS_FILL", style), h, r, NO_PAD, fill, extra)]


def liq_collar():
    """Platin's bar: a platinum collar PROGRESS_COLLAR_W wide over each end of the tube, 1.5 px over it above and
    below."""
    h, w = L.PROGRESS_H, L.PROGRESS_COLLAR_W
    pad = (3, 4, 3, 7)
    img = Image(w + pad[0] + pad[2], h + pad[1] + pad[3])
    x0, y0 = pad[0], pad[1]
    collar = Box(x0, y0 - 1.5, x0 + w, y0 + h + 1.5, 2.5).sd
    liq_under(img, collar, liq_shade_of(collar, shadow=0.30, hair=0.36, drop=1.5, blur=2.5))
    liq_metal(img, collar, lambda x, y, e, nx, ny: (0.0, (y - y0 - h / 2.0) / (h / 2.0 + 1.5) * 0.95, 0.08), bow=0.0)
    return Sprite("PROGRESS_COLLAR", img, pad)


def liq_card(style):
    """The card as a calm pane of glass: its flat colour stays, so text on it reads as on CARD; within about 1.6 px
    of the edge a white line where it faces the light and a line of the far colour where it faces away, a faint
    shadow under it. Shard: a white rim all round; Platin: the far edge its metal's shade."""
    lit, far, far_col, drop_a = {"clear": (0.55, 0.16, rgb(L.COL_GLASS_SHADOW), 0.14),
                                 "shard": (0.80, 0.50, WHITE, 0.18),
                                 "platin": (0.70, 0.34, rgb(L.COL_PLATIN_SHADE), 0.16)}[style]
    dark = rgb(L.COL_GLASS_SHADOW)

    def draw(img, box):
        b = rounded(box, L.CARD_R)
        img.paint(lambda x, y: (max(drop_a * soft(b.sd(x, y - 1.5), 3.0), 0.12 * cover(b.sd(x, y) - 0.7)) *
                                (1.0 - cover(b.sd(x, y))), dark))
        element(img, b, rgb(L.COL_CARD))

        def edge(x, y, facing):
            d = b.sd(x, y)
            c = cover(d)
            if c <= 0.0 or d < -3.0:
                return None
            nx, ny = liq_normal(b.sd, x, y)
            f = nx * TO_LIGHT[0] + ny * TO_LIGHT[1]
            if (f > 0.0) != facing:
                return None
            return (c * clamp(1.6 + d) * (lit * min(1.0, f * 1.4) if facing else far * min(1.0, -f * 1.4)),
                    WHITE if facing else far_col)
        img.paint(lambda x, y: edge(x, y, True))
        img.paint(lambda x, y: edge(x, y, False))
    return both_ways(style_name("CARD_GLASS", style), L.CARD_R, (4, 2, 4, 6), draw)


def installing():
    """The kit's sprites of this block, in the order of their ids (the styles of a name in a row: ui.c +
    ui_glass_want)."""
    bars = [liq_bars(style) for style in GLASS_STYLES]
    return [*liq_loader_beads(), *(b[0] for b in bars), *(b[1] for b in bars), liq_collar(), *(liq_card(style) for style in GLASS_STYLES)]


# ---------------------------------------------------------------- the atlas


def pack(items, width=ATLAS_W, fill=False):
    """Shelves, tallest first, inside a BORDER on all four sides: a sprite that passes the last shelf's end starts a
    new one; with fill it goes on the first shelf that still has room for it (the atlas: with the installing
    screen's tile, Nachtglas's does not fit otherwise; the sheets and the corners keep their places).
    Returns the texture's height (a power of two)."""
    order = sorted(range(len(items)), key=lambda k: (-items[k].img.h, k))
    shelves = []            # [y, height, the next sprite's x]
    for k in order:
        s = items[k]
        if s.img.w > width - 2 * BORDER:
            raise SystemExit("%s is %d px wide: wider than its texture (%d) inside its border" % (s.name, s.img.w,
                                                                                                  width))
        shelf = next((f for f in (shelves if fill else shelves[-1:]) if f[2] + s.img.w <= width - BORDER), None)
        if shelf is None:
            shelf = [shelves[-1][0] + shelves[-1][1] + GAP if shelves else BORDER, s.img.h, BORDER]
            shelves.append(shelf)
        s.at = (shelf[2], shelf[0])
        shelf[2] += s.img.w + GAP
    height = 8
    while shelves and height < shelves[-1][0] + shelves[-1][1] + BORDER:
        height *= 2
    if height > 1024:
        raise SystemExit("the sprites do not fit a %dx1024 texture" % width)
    return height


def gpu_index(x, y, width):
    """Where the pixel x, y of a texture width px wide stands in the GPU's order: 8x8 tiles in rows from the top, Z
    order inside a tile."""
    return ((((y >> 3) * (width >> 3) + (x >> 3)) << 6) | (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2)
            | ((x & 4) << 2) | ((y & 4) << 3))


def tiled(items, height, width=ATLAS_W):
    """RGBA8 in the GPU's order (gpu_index), each pixel A, B, G, R."""
    out = bytearray(width * height * 4)
    for s in items:
        data = s.img.rgba()
        ax, ay = s.at
        for y in range(s.img.h):
            for x in range(s.img.w):
                j = 4 * (y * s.img.w + x)
                if not data[j + 3]:
                    continue
                k = gpu_index(ax + x, ay + y, width)
                out[4 * k:4 * k + 4] = bytes((data[j + 3], data[j + 2], data[j + 1], data[j]))
    return bytes(out)


def c_bytes(data):
    lines = []
    for k in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02x" % b for b in data[k:k + 16]) + ",")
    return "\n".join(lines)


def icon_mask():
    """The rounded corners of a theme's 48x48 icon, as alpha, row by row (the look's own: its icon_mask hook)."""
    if hasattr(LOOK, "icon_mask"):
        return LOOK.icon_mask()
    n, r = L.DETAIL_ICON, L.DETAIL_ICON_R
    b = Box(0, 0, n, n, r)
    return bytes(round(cover(b.sd(x + 0.5, y + 0.5)) * 255) for y in range(n) for x in range(n))


CORNERS_W = 256      # the corners' own texture, read with linear filtering


def tile_order(data, side, height=None):
    """Straight RGBA bytes of a side x side image (side x height) in the GPU's order (as tiled), every pixel kept as
    it is."""
    out = bytearray(len(data))
    for y in range(side if height is None else height):
        for x in range(side):
            j, k = 4 * (y * side + x), gpu_index(x, y, side)
            out[4 * k:4 * k + 4] = bytes((data[j + 3], data[j + 2], data[j + 1], data[j]))
    return bytes(out)


# The kit's names only Stock fills: the glass's Clear and Platin slots; every other look's atlas
# leaves them out (w = 0 in its table), and so the application's names only Stock fills (APP.STOCK_ONLY)
STOCK_ONLY = frozenset(
    [style_name(base, style) for base in ("PROGRESS", "PROGRESS_FILL", "CARD_GLASS", "STRIPES")
     for style in ("clear", "platin")]) | (APP.STOCK_ONLY if APP else frozenset())


def look_sprites():
    """The atlas's sprites as the look draws them (its sprites hook), art.py's own without a look, and the application's
    (APP.sprites, in the look): the kit's and the application's, two lists; every look but Stock without the names
    only Stock fills."""
    items = sprites()
    app = APP.sprites(SCHEME) if APP else []
    if LOOK is None:
        return stripe_ends(items), app
    items = stripe_ends(LOOK.sprites(items, SCHEME))
    if LOOK.NAME == FALLBACK:
        return items, app
    return [s for s in items if s.name not in STOCK_ONLY], [s for s in app if s.name not in STOCK_ONLY]


def scheme_part():
    """What the drawn scheme has of its own: the kit's atlas, the application's atlas (app_atlas, None without
    sprites of the application's) and every pattern's tile (PATTERN_TILE px square), each zlib-compressed in the GPU's
    order, with the sprites' table (the kit's names, then the application's, named in app; each at its place in its
    own atlas), which must not differ between schemes; and what the look's hooks give per scheme: the sheet's sprites
    (the kit's, then the application's in app_sheet), its colours, its own blobs; and the application's pictures
    (APP.pictures: their size and blob, zlib-compressed in the GPU's order)."""
    kit, app = look_sprites()
    items = kit + app
    height = pack(kit, fill=True)
    app_height = pack(app, PART_ATLAS_W, fill=True) if app else 0
    part = dict(names=[s.name for s in items], app=[s.name for s in app], height=height, app_height=app_height,
                table=[(s.at[0], s.at[1], s.img.w, s.img.h) + s.pad + s.cap for s in items],
                atlas=zlib.compress(tiled(kit, height), 9),
                app_atlas=zlib.compress(tiled(app, app_height, PART_ATLAS_W), 9) if app else None,
                tiles=[zlib.compress(tile_order(pattern_tile(shapes), L.PATTERN_TILE), 9) for shapes in PATTERNS])
    sheet = LOOK.sheet([s.name for s in glass_sheet("clear")]) if hasattr(LOOK, "sheet") else glass_sheet("shard")
    part["sheet"], part["app_sheet"] = sheet, APP.sheet() if APP else []
    pictures = APP.pictures(SCHEME) if hasattr(APP, "pictures") else None
    if pictures is not None:
        part["pictures"] = (pictures.w, pictures.h,
                            zlib.compress(tile_order(bytes(pictures.rgba()), pictures.w, pictures.h), 9))
    if hasattr(LOOK, "colours"):
        part["colours"] = LOOK.colours()
    if hasattr(LOOK, "blobs"):
        part["blobs"] = LOOK.blobs(SCHEME)
    return part


def sheet_of(sheets, width=ATLAS_W):
    """The sheet from per scheme the sprites drawn on it (or one list: the same in every scheme), at the same places
    in all: their names, table and height, and per scheme the blob."""
    height = pack(sheets[0], width)
    for items in sheets[1:]:
        for s, first in zip(items, sheets[0]):
            s.at = first.at
            assert (s.img.w, s.img.h) == (first.img.w, first.img.h)
    table = [(s.at[0], s.at[1], s.img.w, s.img.h) + s.pad + s.cap for s in sheets[0]]
    blobs = [zlib.compress(tiled(items, height, width), 9) for items in sheets]
    return dict(names=[s.name for s in sheets[0]], table=table, height=height,
                blobs=blobs * (len(schemes.SCHEMES) // len(blobs)))


def scheme_parts():
    """scheme_part() of every scheme, each drawn by a process of its own, as many at a time as there are
    processors."""
    count = len(schemes.SCHEMES)
    with tempfile.TemporaryDirectory() as tmp:
        files = [os.path.join(tmp, "scheme%d" % k) for k in range(count)]
        waiting, running = list(range(count)), []
        while waiting or running:
            while waiting and len(running) < (os.cpu_count() or 1):
                k = waiting.pop(0)
                running.append(subprocess.Popen([sys.executable, "-I", str(Path(__file__).resolve()), *LOOK_ARGS,
                                                 "--scheme", str(k), "--scheme-part", files[k]]))
            if running.pop(0).wait():
                raise SystemExit("a scheme's images could not be drawn")
        parts = [pickle.loads(Path(f).read_bytes()) for f in files]
    for part in parts[1:]:
        if [part[k] for k in ("names", "height", "app_height", "table")] != [parts[0][k] for k in
                                                                           ("names", "height", "app_height", "table")]:
            raise SystemExit("the sprites' places differ between two schemes")
    return parts


def code_colours():
    """The COL_* names of ui_layout.h itself (not of the files it includes), in its order, and per scheme their
    values in the look: what the application draws by code."""
    text = (HEADERS / "ui_layout.h").read_text(encoding="utf-8")
    names = re.findall(r"^#define\s+(COL_[A-Z0-9_]+)\s+0x[0-9A-Fa-f]{6}\b", text, re.M)
    layouts = [look_layout(k) for k in range(len(schemes.SCHEMES))]
    rows = [[getattr(layout, n) for n in names] for layout in layouts]
    return names, rows


def look_part():
    """The look's share of a build (--look NAME --part): every scheme's part, the sheet, the application's sheet and
    pictures, the icon mask, the colours drawn by code (ui_layout.h's in the look's values, then its own) and its header lines."""
    if getattr(LOOK, "NAME", None) != LOOK_NAME:
        raise SystemExit("look_%s.py: its NAME must be %r" % (LOOK_NAME, LOOK_NAME))
    parts = scheme_parts()
    first = parts[0]
    kit_names = first["names"][:len(first["names"]) - len(first["app"])] + [s.name for s in first["sheet"]]
    both = sorted(set(kit_names) & set(first["app"] + [s.name for s in first["app_sheet"]]))
    if both:
        raise SystemExit("--app: %s names sprites the kit draws too: %s" % (APP_FILE.name, ", ".join(both)))
    sheet = sheet_of([p.pop("sheet") for p in parts])
    app_sheets = [p.pop("app_sheet") for p in parts]
    app_sheet = sheet_of(app_sheets, PART_SHEET_W) if app_sheets[0] else None
    pictures = [p.pop("pictures", None) for p in parts]
    if any(pictures) and (not all(pictures) or len({q[:2] for q in pictures}) > 1 or
                          not all(w in SIDES and h in SIDES for w, h, _ in pictures)):
        raise SystemExit("--app: pictures(index) gives an image in some schemes only, or images of two sizes, or one "
                         "whose sides are not powers of two from %d to %d" % (SIDES[0], SIDES[-1]))
    pictures = dict(w=pictures[0][0], h=pictures[0][1], blobs=[q[2] for q in pictures]) if pictures[0] else None
    names, rows = code_colours()
    for row, part in zip(rows, parts):
        own = part.pop("colours", {})
        if set(own) - set(names) != set(LOOK.OWN_COLOURS):
            raise SystemExit("look_%s.py: colours() gives other colours of its own than OWN_COLOURS" % LOOK_NAME)
        row[:] = [own.get(n, v) for n, v in zip(names, row)] + [own[n] for n in LOOK.OWN_COLOURS]
    return dict(name=LOOK_NAME, own_sprites=list(LOOK.OWN_SPRITES), own_colours=list(LOOK.OWN_COLOURS), parts=parts,
                sheet=sheet, app_sheet=app_sheet, pictures=pictures, mask=icon_mask(), colours=names, rows=rows,
                header=LOOK.header() if hasattr(LOOK, "header") else "")


def padded(blob, height, want, width=ATLAS_W):
    """A blob of a texture width px wide and height px high as one want px high: the GPU's order keeps whole rows of
    tiles in sequence, so the rows added below are empty and every sprite stays where it was."""
    if height == want:
        return blob
    return zlib.compress(zlib.decompress(blob) + bytes(width * (want - height) * 4), 9)


def c_blob_table(prefix, rows, name=None):
    """Per scheme a blob, or a row of blobs: the byte arrays prefix_0, prefix_1, ... (a blob equal to an earlier one
    named once: a look's schemes may share an image) and the tables over them, [ART_SCHEMES] or [ART_SCHEMES][n]:
    prefix_z and prefix_z_size (static, reached through the look's look_art) or, given name, NAME_Z and
    NAME_Z_SIZE."""
    wide = isinstance(rows[0], list)
    flat = [b for row in rows for b in row] if wide else rows
    named, names, out = {}, [], []
    for k, b in enumerate(flat):
        if b not in named:
            named[b] = "%s_%d" % (prefix, k)
            out.append("static const uint8_t %s[] = {\n%s\n};\n" % (named[b], c_bytes(b)))
        names.append(named[b])
    dims = "[ART_SCHEMES]" + ("[%d]" % len(rows[0]) if wide else "")
    z, size = (("const uint8_t *const %s_Z" % name, "const uint32_t %s_Z_SIZE" % name) if name else
               ("static const uint8_t *const %s_z" % prefix, "static const uint32_t %s_z_size" % prefix))

    def body(values):
        if not wide:
            return ", ".join(values)
        n = len(rows[0])
        return "\n%s\n" % "\n".join("    {%s}," % ", ".join(values[k:k + n]) for k in range(0, len(values), n))
    out.append("%s%s = {%s};\n%s%s = {%s};\n" % (z, dims, body(names), size, dims, body([str(len(b)) for b in flat])))
    return "\n".join(out)


ROW = "    {%d, %d, %d, %d, {%d, %d, %d, %d}, {%d, %d, %d, %d}}"


def c_table(ids, names, table):
    """A sprite table in the order of ids: the entry of each name the look has, zeros (w = 0) for the others."""
    at = dict(zip(names, table))
    return ",\n".join(ROW % at.get(n, (0,) * 12) for n in ids)


def join(out, looks):
    """The looks' parts (OUT_DIR/NAME.part) joined into ui_art.h, ui_art.c, art_NAME.c per look, the application's
    part_art.h, part_art.c and part_art_NAME.c per look (with --app) and banner.wav. The common names are Stock's: a look draws each of them
    but those only Stock fills, every one of its own it lists, and no other; its sheet holds the common sheet's
    names."""
    if not looks or looks[0] != FALLBACK or len(set(looks)) != len(looks):
        raise SystemExit("--join: %s first, every look once" % FALLBACK)
    got = [pickle.loads((out / ("%s.part" % n)).read_bytes()) for n in looks]
    common, sheet_names, colours = got[0]["parts"][0]["names"], got[0]["sheet"]["names"], got[0]["colours"]
    own_sprites, own_colours = [], []
    for g in got:
        names = g["parts"][0]["names"]
        lacks = [n for n in common if n not in names and n not in STOCK_ONLY]
        wrong = ([n for n in names if n not in common and n not in g["own_sprites"]] +
                 [n for n in g["own_sprites"] if n not in names or n in common or n in own_sprites] +
                 [n for n in g["own_colours"] if n in colours or n in own_colours])
        if (lacks or wrong or g["sheet"]["names"] != sheet_names or g["colours"] != colours or
                (g["app_sheet"] or {}).get("names") != (got[0]["app_sheet"] or {}).get("names")):
            raise SystemExit("look_%s.py: lacks %s; names neither common nor its own once: %s" % (g["name"], lacks,
                                                                                                  wrong))
        own_sprites += g["own_sprites"]
        own_colours += g["own_colours"]
    app, sheet_app = got[0]["parts"][0]["app"], (got[0]["app_sheet"] or {}).get("names", [])
    ids, colour_ids = [n for n in common if n not in app] + own_sprites, colours + own_colours
    atlas_h = max(g["parts"][0]["height"] for g in got)
    sheet_h = max(g["sheet"]["height"] for g in got)
    corners = select_corners()
    corners_h = pack(corners, CORNERS_W)
    follows = [n for n in colours if schemes.kind_of(n) != "fixed"]
    looks_h = []
    for g in got:
        up = g["name"].upper()
        looks_h.append("extern const look_art ART_%s;" % up)
        if g["name"] == FALLBACK:
            looks_h.append("extern const uint32_t ART_%s_COLOUR[ART_SCHEMES][ART_COLOURS];   /* also the colours "
                           "before the first frame (ui.c) */" % up)
        for name, row in sorted(g["parts"][0].get("blobs", {}).items()):
            looks_h.append("extern const uint8_t *const ART_%s_%s_Z[ART_SCHEMES][%d];\n"
                           "extern const uint32_t ART_%s_%s_Z_SIZE[ART_SCHEMES][%d];"
                           % (up, name.upper(), len(row), up, name.upper(), len(row)))
        if g["header"]:
            looks_h.append(g["header"])
    (out / "ui_art.h").write_text(
        "/* Generated by art.py from ui_layout.h and the looks built (look_*.py); do not edit. */\n"
        "#ifndef UI_ART_H\n#define UI_ART_H\n\n#include <stdint.h>\n\n#include \"ui_layout.h\"\n\n"
        "/* the atlas's size: the tallest look's (a shorter look's atlas has empty rows below) */\n"
        "#define ART_ATLAS_W %d\n#define ART_ATLAS_H %d\n#define ART_MASK_SIZE %d\n\n"
        "typedef struct art_sprite {\n"
        "    uint16_t x, y, w, h;        /* where it is in the atlas */\n"
        "    uint8_t pad[4];             /* how far it reaches past its element: left, top, right, bottom */\n"
        "    uint8_t cap[4];             /* fixed edges where it stretches; 0 and 0 on an axis: not stretched */\n"
        "} art_sprite;\n\n"
        "/* the sprites of every look: the kit's names every look draws (Stock's, in its order), then each look's own;\n"
        " * an application's follow the kit's end, ART_ALL (art_part) */\n"
        "enum art_id {\n%s,\n    ART_COUNT\n};\n\n"
        "/* the selection's corners, in a texture of their own (linear filtering), the same in every look:\n"
        " * ART_CORNERS_Z, ART_CORNER_SPRITES */\n"
        "#define ART_CORNERS_W %d\n#define ART_CORNERS_H %d\n\n"
        "enum art_corner_id {\n%s,\n    ART_CORNER_COUNT\n};\n\n"
        "/* the colour schemes (schemes.py), the standard first: per look and scheme an atlas (the same sprites at\n"
        " * the same places), a sheet and the pattern tiles */\n"
        "enum art_scheme {\n%s,\n    ART_SCHEMES\n};\n\n"
        "/* the glass styles (ui.h, GLASS_*) and the patterns (ui.h, PATTERN_*) */\n"
        "#define ART_GLASSES %d\n#define ART_PATTERNS %d\n\n"
        "/* the sheet: a texture ART_GLASS_SHEET_W x ART_GLASS_SHEET_H (the tallest look's) of what is drawn as\n"
        " * glass: the start screen's word, the icons on buttons, the carousel's panes; their ids follow the atlas's,\n"
        " * so ART_SD is drawn as any sprite; ART_ALL is the kit's end */\n"
        "enum art_glass_id {\n%s,\n    ART_ALL\n};\n"
        "#define ART_GLASS_SHEET_W %d\n#define ART_GLASS_SHEET_H %d\n\n"
        "extern const art_sprite ART_CORNER_SPRITES[ART_CORNER_COUNT];\n"
        "extern const uint8_t ART_CORNERS_Z[];\n"
        "extern const uint32_t ART_CORNERS_Z_SIZE;\n\n"
        "/* The colours drawn by code: every COL_* of ui_layout.h, then each look's own, per look and scheme as\n"
        " * 0xRRGGBB. ui_layout.h's own numbers are the yellow original's; a colour that follows the scheme is read from\n"
        " * the look's table (ui.h, SC(COL_X)), and its name on its own does not compile after this header. */\n"
        "enum art_colour {\n%s,\n    ART_COLOURS\n};\n\n%s\n\n"
        "/* A look's images (art_<look>.c), per scheme; blobs zlib-compressed RGBA8 in tile order, one blob may be\n"
        " * named for several schemes */\n"
        "typedef struct look_art {\n"
        "    const art_sprite *sprites;              /* [ART_COUNT]; a name the look does not draw has w = 0 */\n"
        "    const art_sprite *sheet_sprites;        /* [ART_ALL - ART_COUNT]: word, icons, pane parts */\n"
        "    const uint32_t (*colour)[ART_COLOURS];  /* [ART_SCHEMES] */\n"
        "    const uint8_t *const *atlas_z;  const uint32_t *atlas_z_size;   /* [ART_SCHEMES] */\n"
        "    const uint8_t *const *sheet_z;  const uint32_t *sheet_z_size;   /* [ART_SCHEMES] */\n"
        "    const uint8_t *const (*pattern_z)[ART_PATTERNS];                /* [ART_SCHEMES] */\n"
        "    const uint32_t (*pattern_z_size)[ART_PATTERNS];\n"
        "    const uint8_t *mask;                    /* [ART_MASK_SIZE * ART_MASK_SIZE]: a theme icon's corners */\n"
        "} look_art;\n\n"
        "/* An application's part (ui.h, ui_part): the sprites it brings, whose ids follow the kit's from ART_ALL on, its\n"
        " * atlas's first (count of them), then its sheet's (sheet_count); per look built its two tables and its two\n"
        " * textures' blobs, in the form of look_art's: the places are in the part's own atlas and sheet, which the kit\n"
        " * makes at the sizes art_part gives (the same in every look) and unpacks as its own. Its pictures: a third\n"
        " * texture, made at their size, unpacked when the application asks for them (widget.h, ui_part_pictures). */\n"
        "typedef struct art_part_look {\n"
        "    const look_art *look;                   /* the look they are drawn in */\n"
        "    uint16_t count, sheet_count;\n"
        "    const art_sprite *sprites;              /* [count], NULL for none; a name the look does not draw has w = 0 */\n"
        "    const art_sprite *sheet_sprites;        /* [sheet_count], NULL for none */\n"
        "    const uint8_t *const *atlas_z;  const uint32_t *atlas_z_size;   /* [ART_SCHEMES]; NULL: no atlas */\n"
        "    const uint8_t *const *sheet_z;  const uint32_t *sheet_z_size;   /* [ART_SCHEMES]; NULL: no sheet */\n"
        "    const uint8_t *const *pictures_z;  const uint32_t *pictures_z_size;  /* [ART_SCHEMES]; NULL: none */\n"
        "} art_part_look;\n\n"
        "typedef struct art_part {\n"
        "    const art_part_look *looks;\n"
        "    int count;\n"
        "    uint16_t atlas_w, atlas_h, sheet_w, sheet_h;    /* its textures' sizes; 0: it has no sprites there */\n"
        "    uint16_t pictures_w, pictures_h;                /* its pictures' size; 0: it has none */\n"
        "} art_part;\n\n"
        "/* the looks built, %s first */\n%s\n\n"
        "#endif\n" % (ATLAS_W, atlas_h, L.DETAIL_ICON, ",\n".join("    ART_%s" % n for n in ids), CORNERS_W, corners_h,
                       ",\n".join("    ART_%s" % s.name for s in corners),
                       ",\n".join("    ART_SCHEME_%s" % s["name"] for s in schemes.SCHEMES), len(GLASS_STYLES),
                       len(PATTERNS), ",\n".join("    ART_%s%s" % (n, " = ART_COUNT" if not k else "")
                                                 for k, n in enumerate(sheet_names)),
                       ATLAS_W, sheet_h, ",\n".join("    ART_%s" % n for n in colour_ids),
                       "\n".join("#undef %s\n#define %s follows_the_scheme_read_it_as_SC" % (n, n) for n in follows),
                       FALLBACK.capitalize(), "\n".join(looks_h)),
        encoding="utf-8")
    cz = zlib.compress(tiled(corners, corners_h, CORNERS_W), 9)
    (out / "ui_art.c").write_text(
        "/* Generated by art.py from ui_layout.h; do not edit. What no look owns: the selection's corners. */\n"
        "#include \"ui_art.h\"\n\n"
        "const art_sprite ART_CORNER_SPRITES[ART_CORNER_COUNT] = {\n%s\n};\n\n"
        "const uint32_t ART_CORNERS_Z_SIZE = %d;\n"
        "const uint8_t ART_CORNERS_Z[] = {\n%s\n};\n" % (
            ",\n".join(ROW % ((s.at[0], s.at[1], s.img.w, s.img.h) + s.pad + s.cap) for s in corners), len(cz),
            c_bytes(cz)), encoding="utf-8")
    for g in got:
        (out / ("art_%s.c" % g["name"])).write_text(c_look(g, ids, sheet_names, colour_ids, atlas_h, sheet_h),
                                                     encoding="utf-8")
    if APP:
        c_part(out, got, app, sheet_app)
    chime(out / "banner.wav")


def c_part(out, got, app, sheet_app):
    """The application's part (art.py --app) beside the kit's files: part_art.h, its ids from ART_ALL on (its
    atlas's, then its sheet's, each in Stock's order); part_art_NAME.c per look, the blobs of its atlas, its sheet and
    its pictures (ART_PART_NAME_ATLAS_Z, ART_PART_NAME_SHEET_Z, ART_PART_NAME_PICTURES_Z: per scheme, a blob equal to
    another scheme's named once); and
    part_art.c, per look its two tables and the art_part ART_APP_PART the application hands to ui_part, with its
    textures' sizes (the tallest look's: a shorter look's blob has empty rows below; the pictures' the same in all)."""
    (out / "part_art.h").write_text(
        "/* Generated by art.py from the application's module (art.py --app); do not edit. The application's part:\n"
        " * its sprites' ids from the kit's end (ART_ALL) on, ART_APP_COUNT of its atlas, then ART_APP_GLASS_COUNT of\n"
        " * its sheet; their tables per look in part_art.c, its textures' blobs in part_art_<look>.c. */\n"
        "#ifndef PART_ART_H\n#define PART_ART_H\n\n#include \"ui_art.h\"\n\n"
        "%s"
        "#define ART_APP_COUNT %d\n#define ART_APP_GLASS_COUNT %d\n\n"
        "extern const art_part ART_APP_PART;\n\n"
        "#endif\n" % ("enum part_art_id {\n%s\n};\n" % ",\n".join("    ART_%s%s" % (n, " = ART_ALL" if not k else "")
                                                                   for k, n in enumerate(app + sheet_app))
                      if app + sheet_app else "", len(app), len(sheet_app)),
        encoding="utf-8")
    atlas_h = max(g["parts"][0]["app_height"] for g in got) if app else 0
    sheet_h = max(g["app_sheet"]["height"] for g in got) if sheet_app else 0
    sizes = {(g["pictures"]["w"], g["pictures"]["h"]) for g in got if g["pictures"]}
    if len(sizes) > 1:
        raise SystemExit("--app: the pictures differ in size between two looks")
    pictures_w, pictures_h = sizes.pop() if sizes else (0, 0)
    tables, looks, externs = [], [], []
    for g in got:
        p, up = "art_part_" + g["name"], g["name"].upper()
        has_atlas = app and g["parts"][0]["app_atlas"] is not None    # a look left without the part's sprites: none
        blobs = []
        if has_atlas:
            blobs.append(c_blob_table(p + "_atlas", [padded(q["app_atlas"], q["app_height"], atlas_h, PART_ATLAS_W)
                                                      for q in g["parts"]], "ART_PART_%s_ATLAS" % up))
        if sheet_app:
            blobs.append(c_blob_table(p + "_sheet", [padded(b, g["app_sheet"]["height"], sheet_h, PART_SHEET_W)
                                                      for b in g["app_sheet"]["blobs"]], "ART_PART_%s_SHEET" % up))
        if g["pictures"]:
            blobs.append(c_blob_table(p + "_pictures", g["pictures"]["blobs"], "ART_PART_%s_PICTURES" % up))
        (out / ("part_art_%s.c" % g["name"])).write_text(
            "/* Generated by art.py from the application's module (art.py --app %s) in the look %s; do not edit. */\n"
            "#include \"part_art.h\"\n\n%s" % (APP_FILE.name, g["name"], "\n".join(blobs)), encoding="utf-8")
        z = []
        for what, have in (("ATLAS", has_atlas), ("SHEET", sheet_app), ("PICTURES", g["pictures"])):
            if have:
                externs.append("extern const uint8_t *const ART_PART_%s_%s_Z[ART_SCHEMES];\n"
                               "extern const uint32_t ART_PART_%s_%s_Z_SIZE[ART_SCHEMES];\n" % (up, what, up, what))
            z += ["ART_PART_%s_%s_Z" % (up, what), "ART_PART_%s_%s_Z_SIZE" % (up, what)] if have else ["NULL", "NULL"]
        # a table only where the part has names (no array of none): NULL in the look's entry, as a missing blob
        tables.append(("static const art_sprite %s_sprites[ART_APP_COUNT] = {\n%s\n};\n\n" % (
                          p, c_table(app, g["parts"][0]["names"], g["parts"][0]["table"])) if app else "") +
                      ("static const art_sprite %s_sheet_sprites[ART_APP_GLASS_COUNT] = {\n%s\n};\n\n" % (
                          p, c_table(sheet_app, (g["app_sheet"] or {}).get("names", []),
                                     (g["app_sheet"] or {}).get("table", []))) if sheet_app else ""))
        looks.append("    {&ART_%s, ART_APP_COUNT, ART_APP_GLASS_COUNT, %s, %s,\n"
                     "     %s, %s, %s, %s,\n     %s, %s}," % (
                         (up, p + "_sprites" if app else "NULL", p + "_sheet_sprites" if sheet_app else "NULL")
                         + tuple(z)))
    (out / "part_art.c").write_text(
        "/* Generated by art.py from the application's module (art.py --app); do not edit. */\n"
        "#include <stddef.h>\n\n#include \"part_art.h\"\n\n%s\n%s"
        "static const art_part_look art_part_looks[] = {\n%s\n};\n\n"
        "const art_part ART_APP_PART = {art_part_looks, %d, %d, %d, %d, %d, %d, %d};\n" % (
            "".join(externs), "".join(tables), "\n".join(looks), len(got), PART_ATLAS_W if app else 0, atlas_h,
            PART_SHEET_W if sheet_app else 0, sheet_h, pictures_w, pictures_h),
        encoding="utf-8")


def c_look(g, ids, sheet_ids, colour_ids, atlas_h, sheet_h):
    """art_NAME.c: the look's blobs and tables and its const look_art ART_NAME."""
    n, parts, sheet = g["name"], g["parts"], g["sheet"]
    p, up = "art_" + n, n.upper()
    colour = "const uint32_t ART_%s_COLOUR" % up if n == FALLBACK else "static const uint32_t %s_colour" % p
    have = [dict(zip(g["colours"] + g["own_colours"], row)) for row in g["rows"]]
    blobs = [c_blob_table("%s_%s" % (p, name), [q["blobs"][name] for q in parts], "ART_%s_%s" % (up, name.upper()))
             for name in sorted(parts[0].get("blobs", {}))]
    return "".join((
        "/* Generated by art.py from look_%s.py; do not edit. */\n#include \"ui_art.h\"\n\n" % n,
        "static const art_sprite %s_sprites[ART_COUNT] = {\n%s\n};\n\n" % (
            p, c_table(ids, parts[0]["names"], parts[0]["table"])),
        c_blob_table(p + "_atlas", [padded(q["atlas"], q["height"], atlas_h) for q in parts]), "\n",
        "static const art_sprite %s_sheet_sprites[ART_ALL - ART_COUNT] = {\n%s\n};\n\n" % (
            p, c_table(sheet_ids, sheet["names"], sheet["table"])),
        c_blob_table(p + "_sheet", [padded(b, sheet["height"], sheet_h) for b in sheet["blobs"]]), "\n",
        c_blob_table(p + "_pattern", [q["tiles"] for q in parts]), "\n",
        "static const uint8_t %s_mask[ART_MASK_SIZE * ART_MASK_SIZE] = {\n%s\n};\n\n" % (p, c_bytes(g["mask"])),
        "%s[ART_SCHEMES][ART_COLOURS] = {\n%s\n};\n\n" % (colour, "\n".join(
            "    {%s}," % ", ".join("0x%06X" % row.get(c, 0) for c in colour_ids) for row in have)),
        "".join(b + "\n" for b in blobs),
        "const look_art ART_%s = {\n"
        "    .sprites = %s_sprites, .sheet_sprites = %s_sheet_sprites, .colour = %s,\n"
        "    .atlas_z = %s_atlas_z, .atlas_z_size = %s_atlas_z_size,\n"
        "    .sheet_z = %s_sheet_z, .sheet_z_size = %s_sheet_z_size,\n"
        "    .pattern_z = %s_pattern_z, .pattern_z_size = %s_pattern_z_size, .mask = %s_mask,\n"
        "};\n" % (up, p, p, colour.split()[-1], p, p, p, p, p, p, p)))


# ---------------------------------------------------------------- chime


def chime(path):
    """Two soft notes, 1.2 s, 16-bit mono at 32728 Hz (the rate of the console's sound hardware)."""
    rate, notes = 32728, ((659.25, 0.0), (987.77, 0.18))
    frames = bytearray()
    for k in range(int(rate * 1.2)):
        t = k / rate
        v = 0.0
        for freq, start in notes:
            if t >= start:
                d = t - start
                v += 0.28 * math.sin(2 * math.pi * freq * d) * math.exp(-4.0 * d) * min(1.0, d * 200)
        frames += struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767))
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(frames))


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--scheme-part":   # one scheme's share, for scheme_parts()
        Path(sys.argv[2]).write_bytes(pickle.dumps(scheme_part()))
        return
    if len(sys.argv) == 3 and sys.argv[1] == "--part" and LOOK:
        out = Path(sys.argv[2])
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(pickle.dumps(look_part()))
        return
    if len(sys.argv) >= 4 and sys.argv[1] == "--join" and not LOOK:
        join(Path(sys.argv[2]), sys.argv[3:])
        return
    if APP and APP.main(sys.argv[1:]):
        return
    sys.exit(__doc__)


if __name__ == "__main__":
    main()
