"""The font the renders are set in (the console's own font is not used anywhere): Nunito Medium, the file
font/Nunito.ttf beside this one, under the SIL Open Font License (font/OFL.txt). font_for(cap_px, k) gives it at the
size whose capitals are cap_px * k high."""
import functools
import os
from PIL import ImageFont

CHARS = "".join(chr(c) for c in range(32, 127)) + "ÄÖÜäöüéèêàáâçñíóúôûîïëÉÈ·…–—’‘“”„«»°€×•©®™¡¿"
PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "font", "Nunito.ttf")


def _load(size):
    f = ImageFont.truetype(PATH, size)
    f.set_variation_by_name("Medium")
    return f


@functools.lru_cache(maxsize=None)
def _cap_ratio():
    f = _load(1000)
    box = f.getbbox("H", anchor="ls")
    return -box[1] / 1000.0


@functools.lru_cache(maxsize=None)
def font_for(cap_px, k=1.0):
    return _load(cap_px * k / _cap_ratio())
