"""pics.py WALK_DIR OUT_DIR [SCHEME]: every capture <SCHEME, default weiss>-<name>.dl of a walk whose textures were
kept as one true-size picture OUT_DIR/<name>.png (400 x 480: the top screen above the bottom one, which stands
centred under it at x 40; grey 30 beside it), drawn by raster.py."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from raster import render  # noqa: E402
from PIL import Image  # noqa: E402

walk, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
pre = (sys.argv[3] if len(sys.argv) > 3 else "weiss") + "-"
out.mkdir(parents=True, exist_ok=True)
n = 0
for dl in sorted(walk.glob(pre + "*.dl")):
    if not pathlib.Path(str(dl)[:-3] + ".tex1").exists():   # only captures whose textures were kept (REC_TEX)
        continue
    name = dl.name[len(pre):-3]
    top, bottom = render(str(dl)[:-3], 1)
    img = Image.new("RGB", (400, 480), (30, 30, 30))
    img.paste(top.convert("RGB"), (0, 0))
    img.paste(bottom.convert("RGB"), (40, 240))
    img.save(out / (name + ".png"))
    n += 1
print(n, "pictures in", out)
