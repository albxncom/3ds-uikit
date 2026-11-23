"""order.py DIR [ATLAS_IDS]: per draw list (.dl, rec.c's format) in DIR and per target (0 top left eye, 1 top right
eye, 2 bottom), the drawing rule's two counts as 3DStockTheme's test of its interface counts them: texts drawn while
the last texture drawn from is not the kit's atlas (a picture's texture: a ground, a pane, the sheet ..., and the
application's part's atlas), and transparent pieces needed after a picture's texture other than the part's atlas (the
kit's atlas's 8 x 8 piece at 0, 0 that ui_text puts between; after it the kit's atlas is the last texture; after the
part's atlas it comes by design and is not counted). A rectangle counts as the kit's atlas (no texture). The atlases
are the textures ATLAS_IDS, comma-separated (default 1,3: ui_init's second texture, after the icons, the kit's atlas,
and its fourth, after the kit's sheet, the application's part's atlas); the first is the kit's.
Prints every capture and target with a count above 0, then the sums; exit 1 if any count is above 0."""
import pathlib
import sys

folder = pathlib.Path(sys.argv[1])
atlases = (sys.argv[2] if len(sys.argv) > 2 else "1,3").split(",")
atlas = atlases[0]
total_after = total_piece = lists = 0
bad = []
for path in sorted(folder.glob("*.dl")):
    lists += 1
    last, after, piece, sizes = {}, {}, {}, {}
    for ln in path.read_text(encoding="utf-8").splitlines():
        p = ln.split(" ")
        if p[0] == "X":
            sizes[p[1]] = (p[2], p[3])
            continue
        if p[0] not in ("I", "R", "T"):
            continue
        t = p[1]
        last.setdefault(t, atlas)
        if p[0] == "I":
            clear = p[2] == atlas and p[3:7] == ["0.000", "0.000", "8.000", "8.000"]
            piece[t] = piece.get(t, 0) + (clear and last[t] not in atlases)
            last[t] = atlas if clear else p[2]
        elif p[0] == "R":
            last[t] = atlas
        else:
            after[t] = after.get(t, 0) + (last[t] != atlas)
    for t in sorted(set(after) | set(piece)):
        a, k = after.get(t, 0), piece.get(t, 0)
        total_after += a
        total_piece += k
        if a or k:
            bad.append("%s target %s: %d texts after a picture's texture, %d transparent pieces" % (path.name, t, a, k))
print("\n".join(bad))
print("%s: %d lists, %d texts after a picture's texture, %d transparent pieces, %d lists with a count" % (
    folder.name, lists, total_after, total_piece, len({b.split(" ")[0] for b in bad})))
sys.exit(1 if total_after or total_piece else 0)
