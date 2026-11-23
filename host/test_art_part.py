"""The hook of the image generator (art.py --app) on small parts written for this test, none of the application's:
- a part with pictures only (no sprite in the atlas or the sheet) generates, and part_art.h has no enum;
- a part whose one sprite only Stock fills (STOCK_ONLY), built with a second look, generates, and that look's entry
  has no atlas (NULL, no blob);
- each of the two compiles beside the kit (part_art.c, part_art_<look>.c and a file that hands it to ui_part);
- a module that lacks an entry the hook needs, and one whose sprite has a name of the kit's, each end the run with
  one line, "--app: ...", before any file is written.

Run: python3 -I host/test_art_part.py ART      (ART: an art folder the kit made, for its first look's name)
"""
import os
import re
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

KIT = Path(__file__).resolve().parent.parent
if len(sys.argv) < 2:
    sys.exit("usage: test_art_part.py ART [unittest arguments]")
ART = Path(sys.argv.pop(1)).resolve()
SECOND = "night"
WARN = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wshadow", "-Wvla"]    # the Makefiles' WARN

HEAD = """from pathlib import Path
import art
LAYOUT = Path(%r)
""" % str(KIT / "include" / "ui_layout.h")
MODULES = {
    "small_pictures": HEAD + """STOCK_ONLY = frozenset()
def sprites(index):
    return []
def sheet():
    return []
def pictures(index):
    return art.Image(16, 8, (40 * index % 256, 90, 160))
def main(args):
    return False
""",
    "small_stock": HEAD + """STOCK_ONLY = frozenset(["SMALL_DOT"])
def sprites(index):
    return [art.Sprite("SMALL_DOT", art.Image(6, 6, (250, 250, 250)))]
def sheet():
    return []
def main(args):
    return False
""",
    "small_lacks": HEAD + """STOCK_ONLY = frozenset()
def sprites(index):
    return []
def main(args):
    return False
""",
    "small_clash": HEAD + """STOCK_ONLY = frozenset()
KIT_NAME = %r
def sprites(index):
    return [art.Sprite(KIT_NAME, art.Image(6, 6, (250, 250, 250)))]
def sheet():
    return []
def main(args):
    return False
""",
}
HAND_OVER = '#include "ui.h"\n#include "part_art.h"\n\nvoid hand_over(void)\n{\n    ui_part(&ART_APP_PART);\n}\n'


def kit_name():
    """The first of the kit's sprite names (ui_art.h, enum art_id): one every look draws."""
    return re.search(r"enum art_id \{\n\s*ART_(\w+)", (ART / "ui_art.h").read_text(encoding="utf-8")).group(1)


def art(module, *args):
    return subprocess.run([sys.executable, "-I", str(KIT / "art" / "art.py"), "--app", str(module), *args],
                          cwd=KIT, capture_output=True, text=True)


def build(module, out, looks):
    """Each look's part and the join, as the Makefiles run them; the runs."""
    runs = [art(module, "--look", n, "--part", str(out / ("%s.part" % n))) for n in looks]
    runs.append(art(module, "--join", str(out), *looks))
    (out / "looks_built.h").write_text("#define LOOKS_HELD %d\n#define LOOKS_BUILT(X) %s\n" % (
        len(looks), " ".join("X(%s, %s)" % (n, n.upper()) for n in looks)), encoding="utf-8")
    return runs


class ArtPart(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        root = Path(cls.tmp.name)
        (root / "mods").mkdir()
        cls.mod = {}
        for name, text in MODULES.items():
            cls.mod[name] = root / "mods" / ("%s.py" % name)
            cls.mod[name].write_text(text % kit_name() if name == "small_clash" else text, encoding="utf-8")
        start = time.monotonic()
        cls.out = {"small_pictures": (root / "pictures", ["stock"]), "small_stock": (root / "stock", ["stock", SECOND])}
        cls.runs = {}
        for name, (out, looks) in cls.out.items():
            out.mkdir()
            cls.runs[name] = build(cls.mod[name], out, looks)
        cls.stops = {}
        for name in ("small_lacks", "small_clash"):
            (root / name).mkdir()
            cls.stops[name] = (root / name, art(cls.mod[name], "--look", "stock", "--part",
                                                 str(root / name / "stock.part")))
        cls.seconds = time.monotonic() - start

    @classmethod
    def tearDownClass(cls):
        print("\nthe small parts in %.0f s" % cls.seconds, file=sys.stderr)
        cls.tmp.cleanup()

    def test_small_parts_generate(self):
        for name, runs in self.runs.items():
            for run in runs:
                with self.subTest(part=name, args=" ".join(run.args[5:7])):
                    self.assertEqual(run.returncode, 0, "%s:\n%s" % (name, run.stderr[-800:]))

    def test_pictures_only(self):
        out = self.out["small_pictures"][0]
        self.assertNotIn("enum part_art_id", (out / "part_art.h").read_text(encoding="utf-8"))
        part = (out / "part_art.c").read_text(encoding="utf-8")
        self.assertIn("ART_PART_STOCK_PICTURES_Z", part)
        self.assertIn("const art_part ART_APP_PART = {art_part_looks, 1, 0, 0, 0, 0, 16, 8};", part)

    def test_stock_only_sprites(self):
        out = self.out["small_stock"][0]
        part = (out / "part_art.c").read_text(encoding="utf-8")
        self.assertIn("ART_PART_STOCK_ATLAS_Z", part)
        self.assertNotIn("ART_PART_%s_ATLAS_Z" % SECOND.upper(), part)
        self.assertIn("    {&ART_%s, ART_APP_COUNT, ART_APP_GLASS_COUNT, art_part_%s_sprites, NULL,\n     NULL, NULL,"
                      % (SECOND.upper(), SECOND), part)
        self.assertNotIn("ATLAS", (out / ("part_art_%s.c" % SECOND)).read_text(encoding="utf-8"))

    def test_compiles_beside_the_kit(self):
        cc = os.environ.get("HOST_CC", "cc")
        for name, (out, looks) in self.out.items():
            (out / "hand_over.c").write_text(HAND_OVER, encoding="utf-8")
            for c in ["part_art.c", "hand_over.c"] + ["part_art_%s.c" % n for n in looks]:
                with self.subTest(part=name, file=c):
                    run = subprocess.run([cc, *WARN, "-fsyntax-only", "-I%s" % (KIT / "include"), "-I%s" % out,
                                          "-I%s" % (KIT / "host" / "gpu"), str(out / c)], capture_output=True, text=True)
                    self.assertEqual(run.returncode, 0, "%s %s:\n%s" % (name, c, run.stderr[-800:]))

    def test_stops_with_one_line(self):
        want = {"small_lacks": "--app: small_lacks.py lacks sheet (",
                "small_clash": "--app: small_clash.py names sprites the kit draws too: %s" % kit_name()}
        for name, (out, run) in self.stops.items():
            with self.subTest(module=name):
                self.assertNotEqual(run.returncode, 0)
                lines = run.stderr.strip().splitlines()
                self.assertEqual(len(lines), 1, "%s: not one line:\n%s" % (name, run.stderr[-800:]))
                self.assertTrue(lines[0].startswith(want[name]), lines[0])
                self.assertEqual(sorted(p.name for p in out.iterdir()), [], "%s wrote a file" % name)


if __name__ == "__main__":
    unittest.main()
