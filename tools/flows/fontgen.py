"""Writes font_adv.h for rec.c: the advance width of every character the renders use, in px at the console font's
scale 1 (cap height FONT_CAP px, ui_layout.h), measured on the font raster.py draws with (fontdef.py).
fontgen.py LAYOUT_H OUT_H"""
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fontdef import font_for, CHARS

layout = open(sys.argv[1], encoding="utf-8").read()
cap = int(re.search(r"#define\s+FONT_CAP\s+(\d+)", layout).group(1))
f = font_for(cap, 8)      # measured 8 times larger, for finer numbers
rows = [(ord(ch), f.getlength(ch) / 8) for ch in CHARS]
with open(sys.argv[2], "w", encoding="utf-8") as out:
    out.write("/* made by fontgen.py; px at scale 1 */\n#define FONT_ADV_COUNT %d\n" % len(rows))
    out.write("#define FONT_ADV_OTHER %.4ff\n" % (f.getlength("n") / 8))
    out.write("static const unsigned FONT_ADV_CP[] = {%s};\n" % ", ".join(str(cp) for cp, _ in rows))
    out.write("static const float FONT_ADV[] = {%s};\n" % ", ".join("%.4ff" % a for _, a in rows))
