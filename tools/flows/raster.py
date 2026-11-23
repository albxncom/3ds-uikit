"""Draws a recorded frame (rec.c's draw list and textures) as pictures: the top screen 400 x 240 and the bottom one
320 x 240, K times enlarged (default 2). Sprites are read as the GPU would (nearest or linear, as the application set
each texture), text is set in fontdef.py's font at the size the application asked for, and every picture of a theme
(the 64 x 1024 icon texture) is replaced by a plain violet square.
  from raster import render;  top, bottom = render("DIR/1-start", 2)"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw  # noqa: E402
from fontdef import font_for  # noqa: E402

FONT_CAP = 19
VIOLET = (0x7A, 0x5C, 0xC8)
_tex_cache = {}


def _untile(data, w, h):
    """RGBA8 in the GPU's order (8 x 8 tiles from the top, Z order inside, each pixel A B G R) to a picture."""
    out = bytearray(w * h * 4)
    lut = [(x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)
           for y in range(8) for x in range(8)]
    for ty in range(h >> 3):
        for tx in range(w >> 3):
            base = (ty * (w >> 3) + tx) << 6
            for y in range(8):
                row = ((ty * 8 + y) * w + tx * 8) * 4
                for x in range(8):
                    k = 4 * (base + lut[y * 8 + x])
                    j = row + 4 * x
                    out[j] = data[k + 3]
                    out[j + 1] = data[k + 2]
                    out[j + 2] = data[k + 1]
                    out[j + 3] = data[k]
    return Image.frombytes("RGBA", (w, h), bytes(out))


def _texture(prefix, tid, w, h):
    data = open("%s.tex%d" % (prefix, tid), "rb").read()
    key = (w, h, hash(data))
    if key not in _tex_cache:
        _tex_cache[key] = _untile(data, w, h)
    return _tex_cache[key]


def _rgba(s):
    return tuple(int(s[i:i + 2], 16) for i in (0, 2, 4, 6))


def _faded(c, fade):
    f = fade[3] / 255.0
    return tuple(round(c[i] + (fade[i] - c[i]) * f) for i in range(3)) + (c[3],)


def _tinted(patch, corners):
    """citro2d's tint, per corner (top left, top right, bottom left, bottom right) a colour whose alpha multiplies the
    image's and a blend towards the colour, both interpolated across the quad as the GPU does."""
    w, h = patch.size
    def field(values, mode):
        small = Image.new(mode, (2, 2))
        small.putdata(values)
        return small.resize((w, h), Image.BILINEAR, box=(0.5, 0.5, 1.5, 1.5))
    blend = field([round(b * 255) for _, b in corners], "L")
    colour = field([col[:3] for col, _ in corners], "RGB")
    alpha = field([col[3] for col, _ in corners], "L")
    rgb = Image.composite(colour, patch.convert("RGB"), blend)
    from PIL import ImageChops
    a = ImageChops.multiply(patch.getchannel("A"), alpha)
    out = rgb.convert("RGBA")
    out.putalpha(a)
    return out


def render(prefix, k=2, skip=()):
    """skip: the draw list's line numbers (from 0) to leave out."""
    screens = {0: Image.new("RGB", (400 * k, 240 * k)), 2: Image.new("RGB", (320 * k, 240 * k))}
    tex = {}
    for number, line in enumerate(open(prefix + ".dl", encoding="utf-8").read().splitlines()):
        if number in skip:
            continue
        p = line.split(" ")
        kind = p[0]
        if kind == "X":
            tid, w, h, linear, repeat = (int(v) for v in p[1:6])
            tex[tid] = (w, h, linear, repeat)
            continue
        target = int(p[1])
        if target not in screens:
            continue
        img = screens[target]
        if kind == "C":
            img.paste(_rgba(p[2])[:3], (0, 0) + img.size)
        elif kind == "R":
            x, y, w, h = (float(v) for v in p[2:6])
            cs = [_faded(_rgba(v), _rgba(p[10])) for v in p[6:10]]
            x0, y0, x1, y1 = round(x * k), round(y * k), round((x + w) * k), round((y + h) * k)
            if x1 <= x0 or y1 <= y0:
                continue
            if cs[0] == cs[1] == cs[2] == cs[3]:
                patch = Image.new("RGBA", (x1 - x0, y1 - y0), cs[0])
            else:       # a vertical gradient (the application draws no other)
                col = Image.new("RGBA", (1, y1 - y0))
                n = y1 - y0
                col.putdata([tuple(round(cs[0][i] + (cs[2][i] - cs[0][i]) * (j + 0.5) / n) for i in range(4))
                             for j in range(n)])
                patch = col.resize((x1 - x0, n), Image.NEAREST)
            img.paste(patch, (x0, y0), patch)
        elif kind == "I":
            tid = int(p[2])
            x, y, w, h, u0, v0, u1, v1, alpha = (float(v) for v in p[3:12])
            ccx, ccy, angle = (float(v) for v in p[23:26]) if len(p) > 25 else (0.0, 0.0, 0.0)
            x, y = x - ccx, y - ccy                     # citro2d places the params' centre at pos
            fade = _rgba(p[12])
            tw, th = tex[tid][:2]
            linear, repeat = int(p[13]), int(p[14])     # the drawn image's own filter and wrap (rec.c)
            x0, y0 = round(x * k), round(y * k)
            dw, dh = round((x + w) * k) - x0, round((y + h) * k) - y0
            if dw <= 0 or dh <= 0:
                continue
            src = _texture(prefix, tid, tw, th)
            sx0, sx1, sy0, sy1 = u0 * tw, u1 * tw, (1 - v0) * th, (1 - v1) * th
            if repeat:
                # one texel a pixel: the tile enlarged k times, laid out from the offset the coordinates give
                tile = src.resize((tw * k, th * k), Image.BILINEAR)
                ox, oy = round(sx0 * k) % (tw * k), round(sy0 * k) % (th * k)
                patch = Image.new("RGBA", (dw, dh))
                for py in range(-oy, dh, th * k):
                    for px in range(-ox, dw, tw * k):
                        patch.paste(tile, (px, py))
            else:
                patch = src.resize((dw, dh), Image.BILINEAR if linear else Image.NEAREST, box=(sx0, sy0, sx1, sy1))
            corners = [(_rgba(p[15 + 2 * c]), float(p[16 + 2 * c])) for c in range(4)] if len(p) > 22 else []
            if corners and any(b > 0 or col[3] < 255 for col, b in corners):
                patch = _tinted(patch, corners)
            if fade[3]:
                f = fade[3] / 255.0
                r, g, b, a = patch.split()
                r, g, b = (ch.point(lambda v, t=fade[i]: round(v + (t - v) * f)) for i, ch in enumerate((r, g, b)))
                patch = Image.merge("RGBA", (r, g, b, a))
            if angle:       # turned about the centre, clockwise on the screen for a growing angle (C2Di_CalcQuad)
                import math
                big = patch.rotate(-math.degrees(angle), resample=Image.BILINEAR, expand=True)
                px, py = (x + ccx) * k - big.width / 2.0, (y + ccy) * k - big.height / 2.0
                img.paste(big, (round(px), round(py)), big)
                continue
            img.paste(patch, (x0, y0), patch)
        elif kind == "T":
            x, y, scale = float(p[2]), float(p[3]), float(p[4])
            align = int(p[5])
            color = _faded(_rgba(p[6]), _rgba(p[7]))
            text = line.split(" ", 8)[8]
            f = font_for(FONT_CAP, scale * k)
            # Pillow 12 ignores a text fill's alpha on an RGB image: draw the glyphs opaque on a layer, scale the
            # layer's alpha by the colour's, then blend (a plain fill would draw faded text at full tone)
            layer = Image.new("RGBA", img.size, color[:3] + (0,))
            ImageDraw.Draw(layer).text((x * k, y * k), text, font=f, fill=color[:3] + (255,),
                                       anchor=("ls", "ms", "rs")[align])
            layer.putalpha(layer.getchannel("A").point(lambda v, a=color[3]: v * a // 255))
            img.paste(layer, (0, 0), layer)
    return screens[0], screens[2]


if __name__ == "__main__":
    prefix, out = sys.argv[1], sys.argv[2]
    k = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    top, bottom = render(prefix, k)
    sheet = Image.new("RGB", (400 * k, 480 * k), (24, 24, 24))
    sheet.paste(top, (0, 0))
    sheet.paste(bottom, (40 * k, 240 * k))
    sheet.save(out)
