"""The UI kit's boundary as any user of it sees it: the kit's files include only each other and the two generated
headers, every kit source (source/*.c) compiles with only the kit's headers (include/), the generated art and the
kit's fake SDK headers (host/gpu) on the include path; the kit's example (example/) is held the same way: its files
include only each other, the kit's and the generated headers, and each of its .c files compiles with the same include
path; so are the kit's host tests (host/*.c, host/*.h: test_kit, test_sound and the lines they share with an
application's), their fake SDK headers (host/gpu/) and the flow renderer (tools/flows/*.c, compiled as
tools/flows/example.sh compiles them, with the two headers it makes). And no file of the folder (code, scripts,
Makefiles, pages) names a path outside it: none of the application's folders, none of the sibling repositories, no
path with ".." that leads out of the folder (resolved against the file's own folder, through the variables the file
sets), not the folder's old path in the application, and
every host/... or tools/... it names is there; nor does one hold the wording retired when the kit got its name, its
licence and its version, nor tells the history of the application the kit was cut from (its versions, its
decision numbers). And make install (read by make -n, the library taken as built) writes only under
$(DESTDIR)$(prefix): the library into lib/, every header of the kit and the two generated ones into
include/3ds-uikit/, the art generator with what it reads into share/3ds-uikit/ in the repository's layout (art/ its
modules, include/ the headers it reads, icons/ every icon and its LICENSE), and the kit's LICENSE, icons/LICENSE and
sound/CREDITS.md into licenses/3ds-uikit/; not the docs. It stops with one line, before any command, unless prefix
and DESTDIR are absolute paths without a space.

Run: python3 -I host/test_kit_bounds.py ART      (ART: an art folder the kit made, with looks_built.h)
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

KIT = Path(__file__).resolve().parent.parent
HEADERS = KIT / "include"
SOURCE = KIT / "source"
GENERATOR = KIT / "art"
if len(sys.argv) < 2:
    sys.exit("usage: test_kit_bounds.py ART [unittest arguments]")
ART = Path(sys.argv.pop(1)).resolve()
GENERATED = {"ui_art.h", "looks_built.h"}
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
# the application's folders, the repository's others, the kit by its path from the application's and by its old path
# in it; the wording retired: each spelt with a bracket so that this file does not name them itself
OUTSIDE = re.compile(r"(?<![\w.-])(?:ap[p]|cor[e])/|3ds-theme-installe[r]|3ds-badge[s]|3ds-theme-pac[k]|\.\./uiki[t]\b|"
                     r"(?<![\w.-])uiki[t]/|make ap[p]\b")
RETIRED = re.compile(r"working nam[e]|not confirme[d]|no licenc[e]|licence of its own ye[t]|meant to becom[e]|"
                     r"its own repositor[y] later", re.I)
# the history of the application the kit was cut from: its versions (three parts from 0.4 to 0.9; the kit's own,
# VERSION, is taken out), this project's decision numbers (a C and two or three digits, with a suffix such as -L),
# review rounds and items, its stage labels (an S, a digit, a dash, a number), its owner, a probe target the kit does
# not have, an empty code span. Case-sensitive and bounded, so that a library's version, -std=c11, a colour in hex or
# a code fence is not one (test_history_pattern holds both sides).
HISTORY = re.compile(r"(?<![\w.])0\.[4-9]\.\d+(?!\.?\d)|\bC\d{2,3}(?:-[A-Z]\w*)?\b|\bround \d+\b|\bitem \d+\b|"
                     r"\bS\d-\d+\b|the owne[r]\b|PROB[E]=1|(?<!`)`[`](?!`)")
VERSION = (KIT / "VERSION").read_text().strip()
# a path with ".." in it, alone (../x, a/../b) or after a base: $(dirname "$0") or $startdir (the file's own folder), or
# a variable the same file sets to a path (make NAME := PATH; shell NAME=PATH or NAME=$(cd "PATH" && pwd))
COMP = r"(?:\.\.|[\w-][\w.-]*)"
UP = re.compile(r'(\$\(dirname "\$0"\)|\$\(\w+\)|\$\{\w+\}|\$\w+)((?:/' + COMP + r')+)/?(?![\w.])|'
                r"(?<![\w.$/-])(" + COMP + r"(?:/" + COMP + r")*)/?(?![\w.])")
SETS = re.compile(r'(?<![\w$])(\w+)[ \t]*(?::=|\?=|=)[ \t]*(?:\$\(cd "(.+?)" && pwd\)|"?([^"\s;]+)"?)')
# the two paths up that are meant to leave the folder: the submodule's URL, relative to the application's repository,
# and the folder that holds the kit where it is a submodule (alone.sh, to find its pin)
LEAVES = {("README.md", "../3ds-uikit"), ("tools/build/alone.sh", "$HERE/..")}
HARNESS = re.compile(r"(?<![\w./-])((?:hos[t]|tool[s])/[\w./-]*\w)")
DOC = {".svg", ".png", ".wav", ".bcstm", ".ttf", ".pyc"}

EXAMPLE = KIT / "example"
HOST = KIT / "host"
FLOWS = KIT / "tools" / "flows"
# the headers tools/flows/example.sh makes for its renderer (fontgen.py, art_names.py)
FLOWS_MADE = {"font_adv.h", "art_names.h"}


def kit_files(*suffixes, folder=KIT):
    return sorted(p for p in folder.iterdir() if p.is_file() and (not suffixes or p.suffix in suffixes))


def held_files(*suffixes):
    """The kit's files (its top level, include/, source/), its example's, its host tests' and their fake SDK headers',
    the flow renderer's and its fakes'."""
    folders = (KIT, HEADERS, SOURCE, EXAMPLE, HOST, HOST / "gpu", FLOWS, FLOWS / "fake")
    return [p for folder in folders for p in kit_files(*suffixes, folder=folder)]


def where(expr, sets, depth=0):
    """The path expr names, relative to its file's folder; None where it starts at a folder that is not the file's."""
    m = re.fullmatch(r'(\$\(dirname "\$0"\)|\$\(\w+\)|\$\{\w+\}|\$\w+)(/.*)?', expr)
    if not m:
        return None if expr.startswith(("/", "$", "~")) else expr
    if m.group(1) in ('$(dirname "$0")', "$startdir"):
        head = "."
    else:
        value = sets.get(m.group(1).strip("$(){}"))
        head = where(value, sets, depth + 1) if value and depth < 4 else None
    return None if head is None else head + (m.group(2) or "")


def paths_up(p, text):
    """Each path with ".." in the file's text that leads out of the kit, resolved against the file's folder."""
    sets = {m.group(1): m.group(2) or m.group(3) for m in SETS.finditer(text)}
    out = []
    for m in UP.finditer(text):
        expr = m.group(0).rstrip("/")
        if ".." not in re.split(r"/", m.group(2) or m.group(3)) or (p.relative_to(KIT).as_posix(), expr) in LEAVES:
            continue
        rel = where(expr, sets)
        target = os.path.normpath(os.path.join(p.parent, rel)) if rel is not None else None
        if target is not None and target != str(KIT) and not target.startswith(str(KIT) + os.sep):
            out.append(expr)
    return out


def code_files():
    """Every file of the folder but its pictures, sounds, icons and font, what builds and runs leave (build/,
    __pycache__/; tools/build/ holds scripts) and git's own .git (a folder in the kit's repository, a file naming
    one outside the folder in a submodule's checkout)."""
    out = []
    for p in sorted(KIT.rglob("*")):
        parts = p.relative_to(KIT).parts
        left = parts[0] == ".git" or "__pycache__" in parts or p.name == ".DS_Store" or \
            ("build" in parts and parts[:2] != ("tools", "build"))
        if p.is_file() and not left and p.suffix not in DOC:
            out.append(p)
    return out


class KitBounds(unittest.TestCase):
    def test_includes_stay_in_kit(self):
        own = {p.name for folder in (KIT, HEADERS, SOURCE) for p in kit_files(folder=folder)} | GENERATED
        for p in held_files(".c", ".h"):
            name = p.relative_to(KIT).as_posix()
            mine = own | {q.name for q in kit_files(folder=p.parent)}
            if p.parent == FLOWS:
                mine |= FLOWS_MADE | {q.name for q in kit_files(folder=EXAMPLE)}
            with self.subTest(file=name):
                stray = [h for h in INCLUDE.findall(p.read_text()) if h not in mine and
                         not (os.path.isfile(p.parent / h) and
                              os.path.normpath(p.parent / h).startswith(str(KIT) + os.sep))]
                self.assertEqual(stray, [], f"{name} includes {stray}")

    def test_compiles_alone(self):
        self.assertTrue((ART / "ui_art.h").is_file(), f"no {ART}/ui_art.h: build the art first")
        cc = os.environ.get("HOST_CC", "cc")
        with tempfile.TemporaryDirectory() as made:
            # the flow renderer's files compile as tools/flows/example.sh compiles them, with the headers it makes
            for tool, src in (("fontgen.py", HEADERS / "ui_layout.h"), ("art_names.py", ART / "ui_art.h")):
                out = Path(made) / ("font_adv.h" if tool == "fontgen.py" else "art_names.h")
                run = subprocess.run([sys.executable, "-I", "-B", str(FLOWS / tool), str(src), str(out)],
                                     capture_output=True, text=True)
                self.assertEqual(run.returncode, 0, f"tools/flows/{tool}:\n{run.stderr[-800:]}")
            flows = [f"-I{FLOWS / 'fake'}", f"-I{made}", f"-I{ART}", f"-I{EXAMPLE}", f"-I{HEADERS}"]
            for p in held_files(".c"):
                name = p.relative_to(KIT).as_posix()
                include = flows if p.parent == FLOWS else [f"-I{HEADERS}", f"-I{ART}", f"-I{HOST / 'gpu'}"]
                with self.subTest(file=name):
                    run = subprocess.run([cc, "-fsyntax-only", *include, str(p)], capture_output=True, text=True)
                    self.assertEqual(run.returncode, 0, f"{name} does not compile alone:\n{run.stderr[-800:]}")

    def test_no_path_outside(self):
        files = code_files()
        self.assertGreater(len(files), 60, "too few code files found in the kit")
        for p in files:
            name = p.relative_to(KIT).as_posix()
            with self.subTest(file=name):
                text = p.read_text(encoding="utf-8")
                self.assertEqual(OUTSIDE.findall(text), [], f"{name} names a path outside the kit")
                self.assertEqual(paths_up(p, text), [], f"{name} names a path that leads out of the kit")
                self.assertEqual(RETIRED.findall(text), [], f"{name} holds retired wording")
                told = [f"{text.count(chr(10), 0, m.start()) + 1}: {m.group()}" for m in HISTORY.finditer(text)
                        if m.group() != VERSION]
                self.assertEqual(told, [], f"{name} tells the application's history")
                missing = sorted({h.rstrip(".") for h in HARNESS.findall(text) if not (KIT / h.rstrip(".")).exists()})
                self.assertEqual(missing, [], f"{name} names a host/ or tools/ path the kit does not hold")

    def test_history_pattern(self):
        tick = "`"
        told = ["0.9." + "3", "since 0.9." + "2.", "(0.5." + "0)", "C1" + "04", "C" + "110-L", "C1" + "35",
                "round " + "12", "item " + "25", "S5" + "-1", "the owne" + "r", "make PROBE" + "=1",
                "only " + tick * 2 + ", the"]
        kept = [VERSION, "libctru 2.7.0", "citro2d 1.7.0", "Tabler Icons 3.51.0", "10.9.3", "-std=c11", "-std=c99",
                "0xC0C0C0", "#C0C0C0", "C2D_Text", "C0, C1", "c104", tick * 3 + "c", "UI_PROBE"]
        self.assertEqual([t for t in told if not HISTORY.search(t)], [], "history the pattern misses")
        self.assertEqual([t for t in kept if HISTORY.search(t) and t != VERSION], [], "what the pattern must not take")

    def test_install_prefix(self):
        for args in (["prefix=opt/kit"], ["prefix=/opt/a kit"], ["DESTDIR=stage", "prefix=/P"],
                     ["DESTDIR=/a stage", "prefix=/P"]):
            with self.subTest(args=args):
                run = subprocess.run(["make", "-n", "-s", "-C", str(KIT), "-o", "build/3ds/lib3ds-uikit.a", "install",
                                      *args], capture_output=True, text=True)
                self.assertNotEqual(run.returncode, 0, run.stdout[-400:])
                self.assertEqual(run.stdout.strip(), "", "make install names a command before it stops")
                self.assertRegex(run.stderr, r"\*\*\* (prefix|DESTDIR)=.*: give an absolute path without a space")

    def test_install_layout(self):
        run = subprocess.run(["make", "-n", "-s", "-C", str(KIT), "-o", "build/3ds/lib3ds-uikit.a", "install",
                              "DESTDIR=/D", "prefix=/P"], capture_output=True, text=True)
        self.assertEqual(run.returncode, 0, run.stderr[-800:])
        lines = [line.split() for line in run.stdout.replace("\\\n", " ").splitlines() if line.strip()]
        self.assertTrue(lines and all(w[0] == "install" for w in lines), run.stdout[-800:])
        made, into = set(), {}
        for w in lines:
            if w[1] == "-d":
                made.update(w[2:])
            else:
                self.assertEqual(w[1:3], ["-m", "644"], " ".join(w))
                into.setdefault(w[-1], set()).update(w[3:-1])
        outside = sorted(d for d in made | set(into) if not d.startswith("/D/P/"))
        self.assertEqual(outside, [], "make install writes outside $(DESTDIR)$(prefix)")
        unmade = sorted(d for d in into if not any(m == d or m.startswith(d + "/") for m in made))
        self.assertEqual(unmade, [], "make install copies into a folder it does not make")
        missing = sorted(f for files in into.values() for f in files
                         if not f.startswith("build/") and not (KIT / f).is_file())
        self.assertEqual(missing, [], "make install names files the kit does not hold")
        names = {d: {Path(f).name for f in files} for d, files in into.items()}
        looks = {p.name for p in HEADERS.glob("look_*.h")}
        self.assertEqual(names.get("/D/P/lib"), {"lib3ds-uikit.a"})
        self.assertEqual(names.get("/D/P/include/3ds-uikit"),
                         {p.name for p in kit_files(".h", folder=HEADERS)} | GENERATED)
        self.assertEqual(sorted(d for d in into if d.startswith("/D/P/share/")),
                         ["/D/P/share/3ds-uikit/" + d for d in ("art", "icons", "include")],
                         "make install puts into share/ other than the generator's art/, icons/ and include/")
        self.assertEqual(names.get("/D/P/share/3ds-uikit/art"), {p.name for p in kit_files(".py", folder=GENERATOR)})
        self.assertEqual(names.get("/D/P/share/3ds-uikit/include"), {"ui_layout.h", "popup_band.h"} | looks)
        self.assertEqual(names.get("/D/P/share/3ds-uikit/icons"),
                         {p.name for p in (KIT / "icons").glob("*.svg")} | {"LICENSE"})
        self.assertEqual(sorted(f for files in into.values() for f in files if f.startswith("docs/")), [],
                         "make install installs the docs, which stay in the repository")
        self.assertEqual(into.get("/D/P/licenses/3ds-uikit"), {"LICENSE"})
        self.assertEqual(into.get("/D/P/licenses/3ds-uikit/icons"), {"icons/LICENSE"})
        self.assertEqual(into.get("/D/P/licenses/3ds-uikit/sound"), {"sound/CREDITS.md"})


if __name__ == "__main__":
    unittest.main()
