"""The UI kit's documentation (docs/) stays complete: every function a header of the kit declares is named in a
file of docs/, every file of sound/ and every icon (icons/*.svg) is named in sound.md, icons.md or
credits.md, and every relative link in a page of the folder (docs/, the READMEs, sound/CREDITS.md) leads to a file
that is there, inside the folder. Every picture in docs/img/ (make
docs-pictures draws them) is linked from a page, is a PNG of 400 x 480 (both screens, true size) with no chunk but
IHDR, IDAT and IEND (no text: no path or name inside), and is at most IMG_MAX bytes (the largest, a dark look's start
page, is about 150 KB).

A function counts when a line of an include/*.h starts at its first column with one or more type words ("static",
"inline", "const", "struct", "unsigned" and the type's own), each followed by stars or blanks, then a name, then "(":
a declaration or a definition. Lines that start with "#", "extern", "typedef" or blanks do not count, so macros,
function pointers and struct members are not functions here; a declaration whose type stands on the line before its
name is not seen (the kit's headers have none). A name counts as named when it stands in a doc as a whole word.

Run: python3 -I host/test_kit_docs.py      (make test runs it)
"""
import re
import struct
import sys
import unittest
from pathlib import Path

KIT = Path(__file__).resolve().parent.parent
DOCS = KIT / "docs"
ICONS = KIT / "icons"
IMG = DOCS / "img"
IMG_MAX = 200_000
DECL = re.compile(r"^(?!extern\b|typedef\b|return\b)(?:[A-Za-z_]\w*[ \t*]+)+([A-Za-z_]\w*)[ \t]*\(", re.M)
LINK = re.compile(r"\]\(([^)\s]+)\)")


def declared():
    """{header name: [function names]} for every include/*.h."""
    return {h.name: DECL.findall(h.read_text()) for h in sorted((KIT / "include").glob("*.h"))}


def pages():
    """Every Markdown page of the folder but what builds leave and git's own .git."""
    return sorted(p for p in KIT.rglob("*.md") if not {".git", "build", "__pycache__"} & set(p.relative_to(KIT).parts))


def docs_text(*names):
    files = [DOCS / n for n in names] if names else sorted(DOCS.glob("*.md"))
    return "\n".join(f.read_text() for f in files if f.is_file())


def named(name, text):
    return re.search(r"(?<![\w.-])" + re.escape(name) + r"(?![\w-])", text) is not None


class KitDocs(unittest.TestCase):
    def test_every_function_named(self):
        text = docs_text()
        for header, functions in declared().items():
            with self.subTest(header=header):
                missing = [f for f in functions if not named(f, text)]
                self.assertEqual(missing, [], f"{header}: not named in docs/")

    def test_every_sound_and_icon_named(self):
        text = docs_text("sound.md", "icons.md", "credits.md")
        files = [p.name for p in sorted((KIT / "sound").iterdir()) if p.is_file()] + \
                [p.name for p in sorted(ICONS.glob("*.svg"))]
        missing = [f for f in files if not named(f, text)]
        self.assertEqual(missing, [], "not named in sound.md, icons.md or credits.md")

    def test_links_resolve(self):
        for doc in pages():
            name = doc.relative_to(KIT).as_posix()
            for target in LINK.findall(doc.read_text()):
                if "://" in target or target.startswith(("#", "mailto:")):
                    continue
                with self.subTest(doc=name, link=target):
                    dest = (doc.parent / target.split("#")[0]).resolve()
                    self.assertTrue(dest.is_relative_to(KIT), f"{name}: {target} leaves the folder")
                    self.assertTrue(dest.exists(), f"{name}: {target}")

    def test_every_picture_linked(self):
        linked = {(doc.parent / t.split("#")[0]).resolve() for doc in sorted(DOCS.glob("*.md"))
                  for t in LINK.findall(doc.read_text()) if "://" not in t}
        orphans = [p.name for p in sorted(IMG.iterdir()) if p.resolve() not in linked]
        self.assertEqual(orphans, [], "docs/img/: linked from no page")

    def test_pictures_are_plain_small_pngs(self):
        for p in sorted(IMG.iterdir()):
            with self.subTest(picture=p.name):
                data = p.read_bytes()
                self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n", "not a PNG")
                self.assertEqual(struct.unpack(">II", data[16:24]), (400, 480), "not 400 x 480")
                self.assertLessEqual(len(data), IMG_MAX, "larger than IMG_MAX")
                chunks, i = set(), 8
                while i < len(data):
                    chunks.add(data[i + 4:i + 8])
                    i += 12 + struct.unpack(">I", data[i:i + 4])[0]
                self.assertEqual(chunks, {b"IHDR", b"IDAT", b"IEND"}, "a chunk besides the picture's own")


if __name__ == "__main__":
    if "--count" in sys.argv:
        text = docs_text()
        for header, functions in declared().items():
            print(f"{header}: {len(functions)} declared, {sum(named(f, text) for f in functions)} named")
    else:
        unittest.main()
