"""The kit's half of the image generator without an application's (S3-4a): art.py runs with no --app, each look's
part (--look NAME --part) and their join (--join), and then writes no file of an application's part (part_art*);
and every kit source compiles alone against the art made without it (host/test_kit_bounds.py's rule, on that
folder). That these files are the same bytes as a build with the application's half is the application's test.

Run: python3 -I host/test_art_alone.py ART [LOOK...]      (ART: an art folder the kit made, with its looks' parts
     and looks_built.h; the looks are every one ART holds, Stock first)
"""
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

KIT = Path(__file__).resolve().parent.parent
if len(sys.argv) < 2:
    sys.exit("usage: test_art_alone.py ART [LOOK...]")
ART = Path(sys.argv.pop(1)).resolve()
LOOKS = sys.argv[1:] or sorted((p.stem for p in ART.glob("*.part")), key=lambda n: (n != "stock", n))
del sys.argv[1:]


def art(*args):
    return subprocess.run([sys.executable, "-I", str(KIT / "art" / "art.py"), *args], cwd=KIT, capture_output=True,
                          text=True)


class ArtAlone(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name)
        start = time.monotonic()
        steps = [["--look", n, "--part", str(cls.out / ("%s.part" % n))] for n in LOOKS]
        cls.runs = [(args, art(*args)) for args in steps + [["--join", str(cls.out)] + LOOKS]]
        cls.seconds = time.monotonic() - start

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_runs_without_the_application(self):
        self.assertTrue(LOOKS and LOOKS[0] == "stock", "no looks in %s" % ART)
        for args, run in self.runs:
            with self.subTest(args=" ".join(args)):
                self.assertEqual(run.returncode, 0, "art.py %s without --app:\n%s" % (" ".join(args), run.stderr[-800:]))
        print("\n%d looks without --app in %.0f s" % (len(LOOKS), self.seconds), file=sys.stderr)

    def test_no_file_of_the_part(self):
        self.assertEqual(sorted(p.name for p in self.out.glob("part_art*")), [], "a file of the application's part")

    def test_kit_compiles_alone_against_it(self):
        (self.out / "looks_built.h").write_bytes((ART / "looks_built.h").read_bytes())
        run = subprocess.run([sys.executable, "-I", str(KIT / "host" / "test_kit_bounds.py"), str(self.out),
                              "KitBounds.test_compiles_alone"], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, "*.c against the art made without --app:\n%s" % run.stderr[-1200:])


if __name__ == "__main__":
    unittest.main()
