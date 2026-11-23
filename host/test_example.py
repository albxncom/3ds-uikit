"""The kit's example (example/), a gallery built on the kit alone:
- every element once: every function ui.h declares (found as host/test_kit_docs.py finds them) is called in
  example/*.c or stands, with a reason, in the table of what is left out in example/README.md; the same
  for every interface sound (SFX_* of audio.h) and for audio_menu, audio_sounds, audio_work and audio_ready;
- nothing from outside: no file of example/ includes or names a path outside the kit (the application's
  folders, the kit's host or tools folders, or a way up past the kit), and every example/*.c compiles with only the kit, the kit's own art (ART,
  made without --app) and the kit's SDK headers (host/gpu) on the include path;
- the walk: the example's main.c on the host under the flows' recorder (tools/flows/example.sh and example.c) with
  scripted keys and touches exits 0 (every page reached, every sound asked for, a touch held across a popup or a page
  opening presses nothing, the clock read once a frame, START left), once with the 3D slider down and once up; every
  page is captured in every look the build holds; every look and every colour scheme really switched (the start
  page's lists differ between looks, the settings page's between schemes); the drawing rule holds on every list of
  both walks (tools/flows/order.py, the kit's atlas being texture 1 with no part); no list is empty; with the slider
  up the two eyes' lists of every capture differ only where the eye's offset moves a piece (its x, or a repeated
  texture's u); every face drawn at a target's place is as high as that target (a face has its own height, ui.h); the
  list's thumb takes a touch beside the drawn bar. It prints the walk's frames and the largest list.

Run: python3 -I host/test_example.py ART      (ART: an art folder the kit made alone, with looks_built.h)
"""
import itertools
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HOST = Path(__file__).resolve().parent
sys.path.insert(0, str(HOST))
from test_kit_docs import DECL  # noqa: E402  (the same declarations the docs test finds)

KIT = HOST.parent
HEADERS = KIT / "include"
EXAMPLE = KIT / "example"
FLOWS = KIT / "tools" / "flows"
if len(sys.argv) < 2:
    sys.exit("usage: test_example.py ART [unittest arguments]")
ART = Path(sys.argv.pop(1)).resolve()
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
LEFT_OUT = re.compile(r"^\|\s*`([A-Za-z_]\w*)`\s*\|\s*([^|]*?)\s*\|\s*$", re.M)
OUTSIDE = re.compile(r"(?<![\w.-])(?:app|core|host|tools)/|\.\./\.\.")
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
GENERATED = {"ui_art.h", "looks_built.h"}
FACE_PAD_X, FACE_PAD_LOW = 3, 7     # a face's shadow beside it and below it (ui_layout.h)


def sources():
    return sorted(EXAMPLE.glob("*.c")) + sorted(EXAMPLE.glob("*.h"))


def files():
    """Every file of example/ but what its build makes (build/, ignored by git)."""
    return sorted(p for p in EXAMPLE.rglob("*") if p.is_file() and "build" not in p.relative_to(EXAMPLE).parts)


def code():
    return "\n".join(COMMENT.sub(" ", p.read_text(encoding="utf-8")) for p in sources())


def left_out():
    return dict(LEFT_OUT.findall((EXAMPLE / "README.md").read_text(encoding="utf-8")))


class EveryElement(unittest.TestCase):
    def test_every_function_of_ui_h(self):
        text, out = code(), left_out()
        declared = DECL.findall((HEADERS / "ui.h").read_text(encoding="utf-8"))
        self.assertGreater(len(declared), 80, "too few functions found in ui.h")
        missing = [f for f in declared if not re.search(r"\b%s\s*\(" % f, text) and not out.get(f)]
        self.assertEqual(missing, [], "ui.h functions neither called in example/ nor left out by name")
        called = [f for f in declared if re.search(r"\b%s\s*\(" % f, text)]
        print("\n%d functions of ui.h: %d called, %d left out" % (len(declared), len(called), len(declared) - len(called)),
              file=sys.stderr)

    def test_every_sound(self):
        text, out = code(), left_out()
        sounds = [s for s in re.findall(r"\b(SFX_[A-Z]+)\b", (HEADERS / "audio.h").read_text(encoding="utf-8"))
                  if s != "SFX_COUNT"]
        self.assertEqual(len(set(sounds)), 7, "the seven interface sounds")
        missing = [s for s in dict.fromkeys(sounds) if not re.search(r"\b%s\b" % s, text) and not out.get(s)]
        missing += [f for f in ("audio_menu", "audio_sounds", "audio_work", "audio_ready")
                    if not re.search(r"\b%s\s*\(" % f, text) and not out.get(f)]
        self.assertEqual(missing, [], "sounds neither used in example/ nor left out by name")

    def test_left_out_names_exist(self):
        known = set(DECL.findall((HEADERS / "ui.h").read_text(encoding="utf-8"))) | \
            set(re.findall(r"\b(SFX_[A-Z]+|audio_\w+)\b", (HEADERS / "audio.h").read_text(encoding="utf-8")))
        out = left_out()
        self.assertTrue(out, "no table of what is left out in example/README.md")
        self.assertEqual(sorted(set(out) - known), [], "left out but not an element of the kit")
        self.assertEqual([n for n, why in out.items() if len(why) < 10], [], "left out without a reason")


class NothingFromOutside(unittest.TestCase):
    def test_no_path_outside_the_kit(self):
        for p in files():
            name = p.relative_to(KIT).as_posix()
            with self.subTest(file=name):
                text = p.read_text(encoding="utf-8")
                self.assertEqual(OUTSIDE.findall(text), [], "%s names a path outside the kit" % name)

    def test_includes_only_the_kit(self):
        own = {p.name for p in HEADERS.iterdir() if p.is_file()} | {p.name for p in EXAMPLE.iterdir()} | GENERATED
        for p in sources():
            with self.subTest(file=p.name):
                stray = [h for h in INCLUDE.findall(p.read_text(encoding="utf-8")) if h not in own]
                self.assertEqual(stray, [], "example/%s includes from outside the kit" % p.name)

    def test_compiles_against_the_kit_alone(self):
        self.assertTrue((ART / "ui_art.h").is_file() and not list(ART.glob("part_art*")),
                        "%s is not an art folder made by the kit alone" % ART)
        cc = os.environ.get("HOST_CC", "cc")
        for p in sorted(EXAMPLE.glob("*.c")):
            with self.subTest(file=p.name):
                run = subprocess.run([cc, "-fsyntax-only", "-Wall", "-Wextra", "-Werror", "-I%s" % HEADERS,
                                      "-I%s" % ART, "-I%s" % (HOST / "gpu"), str(p)], capture_output=True, text=True)
                self.assertEqual(run.returncode, 0, "example/%s:\n%s" % (p.name, run.stderr[-800:]))


class Walk(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        tmp = Path(cls.tmp.name)
        cls.lists, cls.lists3d = tmp / "lists", tmp / "lists3d"
        cls.lists.mkdir()
        cls.lists3d.mkdir()
        cls.build = subprocess.run(["sh", str(FLOWS / "example.sh"), str(tmp / "drv"), str(ART)],
                                   capture_output=True, text=True)
        log = tmp / "drv" / "driver-build.log"
        cls.build_log = log.read_text(encoding="utf-8") if log.is_file() else ""
        cls.walk, cls.walk3d = (subprocess.run(["perl", "-e", "alarm 120; exec @ARGV", str(tmp / "drv" / "driver"),
                                                str(out)] + more, stdin=subprocess.DEVNULL, capture_output=True,
                                               text=True) if cls.build.returncode == 0 else None
                                for out, more in ((cls.lists, []), (cls.lists3d, ["3d"])))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        self.assertEqual(self.build.returncode, 0, "the walk's driver did not build:\n%s%s" % (
            self.build.stderr[-600:], self.build_log[-1200:]))
        self.assertEqual(self.build_log.count("warning:"), 0, "the walk's driver built with warnings")

    def dl(self, name):
        return (self.lists / ("%s.dl" % name)).read_text(encoding="utf-8")

    def test_the_walk_held(self):
        for walk in (self.walk, self.walk3d):
            out = walk.stderr
            self.assertEqual(walk.returncode, 0, "the walk failed:\n%s" % "\n".join(
                ln for ln in out.splitlines() if "walk" in ln))
            summary = [ln for ln in out.splitlines() if ln.startswith("walk: ") and "frames" in ln]
            self.assertTrue(summary and summary[-1].endswith("held"), "no summary line")
            m = re.search(r"(\d+) of (\d+) pages reached", summary[-1])
            self.assertTrue(m and m.group(1) == m.group(2), "not every page reached: %s" % summary[-1])
            print("\n%s" % summary[-1], file=sys.stderr)
        sizes = {p.stem: sum(1 for ln in p.read_text(encoding="utf-8").splitlines() if ln[:1] in "CRIT")
                 for p in self.lists.glob("*.dl")}
        big = max(sizes, key=sizes.get)
        print("the largest list %s, %d draw calls" % (big, sizes[big]), file=sys.stderr)

    def test_every_page_in_every_look(self):
        held = int(re.search(r"LOOKS_HELD (\d+)", (ART / "looks_built.h").read_text()).group(1))
        shots = re.findall(r"^captured look(\d+)-\S+: page (\w+), look (\d+)", self.walk.stderr, re.M)
        pages = {page for _, page, _ in shots}
        self.assertEqual(len(pages), 11, "not every page captured: %s" % sorted(pages))
        for look in range(held):
            with self.subTest(look=look):
                self.assertEqual({page for n, page, at in shots if int(n) == look == int(at)}, pages,
                                 "look %d: not every page captured in it" % look)

    def test_no_list_empty(self):
        names = sorted(p.stem for p in self.lists.glob("*.dl"))
        self.assertGreater(len(names), 30, "too few captures")
        for name in names:
            with self.subTest(capture=name):
                lines = self.dl(name).splitlines()
                for target in "02":     # the top screen's left eye, the bottom screen
                    self.assertTrue([ln for ln in lines if re.match(r"[RIT] %s " % target, ln)],
                                    "%s draws nothing on target %s" % (name, target))

    def test_every_look_and_scheme_switched(self):
        looks = sorted(p.stem for p in self.lists.glob("start-look*.dl"))
        held = int(re.search(r"LOOKS_HELD (\d+)", (ART / "looks_built.h").read_text()).group(1))
        self.assertEqual(len(looks), held, "the start page not captured in every look: %s" % looks)
        schemes = sorted(p.stem for p in self.lists.glob("settings-scheme*.dl"))
        self.assertEqual(len(schemes), 8, "the settings page not captured in every scheme: %s" % schemes)
        for group in (looks, schemes):
            for a, b in itertools.combinations(group, 2):
                with self.subTest(pair=(a, b)):
                    self.assertNotEqual(self.dl(a), self.dl(b), "%s and %s draw the same" % (a, b))

    def test_the_drawing_rule(self):
        for lists in (self.lists, self.lists3d):
            run = subprocess.run([sys.executable, "-I", "-B", str(FLOWS / "order.py"), str(lists), "1"],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, "text after a picture's texture:\n%s" % run.stdout[-1200:])
            print("\n%s" % run.stdout.strip().splitlines()[-1], file=sys.stderr)

    def test_the_eyes_agree(self):
        """Both eyes draw the same frame: only a piece's x (R, T: field 2; I: 3) moves, or a repeated texture's u0
        and u1 by as much (an I line's fields 7 and 9, its repeat flag set). A page that changed its state or read the
        clock while drawing shows something else to the right eye (the first camera frame, a counter)."""
        names = sorted(self.lists3d.glob("*.dl"))
        self.assertGreater(len(names), 30, "too few captures with the 3D slider up")
        for p in names:
            eyes = {"0": [], "1": []}
            for ln in p.read_text(encoding="utf-8").splitlines():
                f = ln.split(" ")
                if f[0] in ("C", "R", "I", "T") and len(f) > 1 and f[1] in eyes:
                    eyes[f[1]].append(f)
            with self.subTest(capture=p.stem):
                self.assertTrue(eyes["1"], "%s: nothing on the right eye" % p.stem)
                self.assertEqual([f[0] for f in eyes["0"]], [f[0] for f in eyes["1"]],
                                 "%s: the eyes draw different calls" % p.stem)
                for a, b in zip(eyes["0"], eyes["1"]):
                    diff = [i for i in range(2, max(len(a), len(b))) if a[i:i + 1] != b[i:i + 1]]
                    x = 3 if a[0] == "I" else 2
                    moved = [i for i in diff if i != x]
                    if a[0] == "I" and moved == [7, 9] and a[14] == "1":
                        span = float(a[9]) - float(a[7]), float(b[9]) - float(b[7])
                        moved = [] if abs(span[0] - span[1]) < 1e-3 else moved
                    self.assertEqual(moved, [], "%s: the eyes differ beyond the offset:\n%s\n%s" % (
                        p.stem, " ".join(a)[:200], " ".join(b)[:200]))

    def test_the_thumb_beside_the_bar(self):
        """The walk holds the stylus at x 316, beside the drawn scroll bar but where it takes touches (from x 298, as
        the application's), and moves it to the track's end: the list shows its last row."""
        self.assertRegex(self.dl("list-thumb"), r"(?m)^T 2 .* Row 40$", "the thumb did not take the touch at x 316")

    def test_faces_at_their_heights(self):
        """A face's image starts at its rectangle's top, FACE_PAD_X left of it, and reaches FACE_PAD_LOW below its
        own height: where one stands at a target's place, the target must be as high as the face."""
        found = 0
        for p in sorted(self.lists.glob("*.targets")):
            images = [ln.split(" ") for ln in self.dl(p.stem).splitlines() if ln.startswith("I 2 ")]
            for ln in p.read_text(encoding="utf-8").splitlines():
                x, y, w, h = (float(v) for v in ln.split())
                # the face's left column: its first piece at the target's top, then those of the same call (the next
                # few in the list) that start where it ends
                at = [k for k, f in enumerate(images) if abs(float(f[3]) - (x - FACE_PAD_X)) < 0.5
                      and abs(float(f[4]) - y) < 0.5]
                if not at:
                    continue
                low = y + float(images[at[0]][6])
                for f in images[at[0] + 1:at[0] + 9]:
                    if abs(float(f[3]) - (x - FACE_PAD_X)) < 0.5 and abs(float(f[4]) - low) < 0.5:
                        low += float(f[6])
                found += 1
                with self.subTest(capture=p.stem, target=ln):
                    self.assertAlmostEqual(low - y - FACE_PAD_LOW, h, delta=0.5, msg="%s: a face %s high on a "
                                           "target %s high" % (p.stem, low - y - FACE_PAD_LOW, h))
        self.assertGreater(found, 200, "too few faces found at their targets")
        print("\n%d faces at their targets" % found, file=sys.stderr)


if __name__ == "__main__":
    unittest.main()
