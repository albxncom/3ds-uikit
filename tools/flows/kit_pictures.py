"""kit_pictures.py ART IMG_DIR [CAPTURE...]: the pictures of the kit's docs (docs/img/), drawn from the kit's
example. Builds the example's host walk (example.sh, from this kit and its art ART), runs it into a folder of its own
that is gone afterwards, and draws the captures PICTURES names with raster.py as IMG_DIR/<name>.png, or, given
CAPTUREs (any of the walk's, such as look1-text0: example.c names them), those as IMG_DIR/<capture>.png: 400 x 480,
the top screen above the bottom one, which stands centred under it at x 40, grey 30 beside it (as pics.py). The text
is set in the renderer's font (fontdef.py), not the console's. The PNGs hold no text chunk; the same tree gives the
same bytes. Run by make docs-pictures, not by make test, which checks the pictures' files only."""
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from raster import render  # noqa: E402
from PIL import Image  # noqa: E402

# the picture's name: the walk's capture (example.c)
PICTURES = {
    "start-stock": "start-look0",
    "start-frost": "start-look1",
    "start-night": "start-look2",
    "start-prism": "start-look3",
    "list": "list-selected",
    "popup-warning": "popup2",
    "settings-scheme": "settings-scheme4",
    "loader": "loader-known",
    "text": "text",
    "top": "top0",
    "buttons": "buttons",
}


def main():
    art, out = (pathlib.Path(a).resolve() for a in sys.argv[1:3])
    pictures = {name: name for name in sys.argv[3:]} or PICTURES
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        work = pathlib.Path(tmp)
        drv, lists = work / "drv", work / "lists"
        lists.mkdir()
        if subprocess.run(["sh", str(HERE / "example.sh"), str(drv), str(art)]).returncode:
            sys.exit("the walk's driver did not build: %s" % (drv / "driver-build.log").read_text()[-2000:])
        walk = subprocess.run(["perl", "-e", "alarm 120; exec @ARGV", str(drv / "driver"), str(lists)],
                              stdin=subprocess.DEVNULL, capture_output=True, text=True)
        if walk.returncode:
            sys.exit("the walk failed:\n" + walk.stderr[-2000:])
        for name, capture in pictures.items():
            if not (lists / (capture + ".dl")).is_file():
                sys.exit("the walk has no capture %s" % capture)
            top, bottom = render(str(lists / capture), 1)
            img = Image.new("RGB", (400, 480), (30, 30, 30))
            img.paste(top.convert("RGB"), (0, 0))
            img.paste(bottom.convert("RGB"), (40, 240))
            img.save(out / (name + ".png"), optimize=True)
    print(len(pictures), "pictures in", out)


main()
