#!/bin/sh
# example.sh OUT ART: the kit's example's host walk (example.c) into OUT/driver, the compiler's output in
# OUT/driver-build.log. The kit is the one this script stands in (two folders up): the example's sources are its
# example/, and of the kit itself (source/, its headers in include/) ui.c, the looks of ART and the input parts
# (audio.c not: example.c counts the sound calls); ART an art folder the kit made alone (art/art.py without --app,
# with looks_built.h). Nothing of an application is compiled.
# Run: OUT/driver OUT_DIR (the draw lists of the walk's captures as OUT_DIR/<name>.dl); exit 0 when the walk held.
set -eu
OUT=$1; ART=$(cd "$2" && pwd); HERE=$(cd "$(dirname "$0")" && pwd)
K=$(cd "$HERE/../.." && pwd)
mkdir -p "$OUT/src"
python3 -I -B "$HERE/fontgen.py" "$K/include/ui_layout.h" "$OUT/src/font_adv.h"
python3 -I -B "$HERE/art_names.py" "$ART/ui_art.h" "$OUT/src/art_names.h"
ART_C=$(ls "$ART"/art_*.c)
LOOK_C="$K/source/look.c $(for f in $ART_C; do n=${f##*/art_}; echo "$K/source/look_${n%.c}.c"; done)"
# shellcheck disable=SC2086
cc -std=gnu11 -Wall -Wextra -Werror -Wshadow -Wvla -O1 -I"$HERE/fake" -I"$OUT/src" -I"$ART" -I"$K/example" \
    -I"$K/include" -o "$OUT/driver" "$HERE/example.c" $(ls "$K"/example/page_*.c) "$HERE/rec.c" "$HERE/names_kit.c" \
    "$K/source/ui.c" "$K/source/hits.c" "$K/source/wrap.c" "$K/source/scroll.c" "$K/source/touch.c" \
    "$K/source/popup.c" "$K/source/tracks.c" $LOOK_C "$ART/ui_art.c" $ART_C \
    -lz -lm > "$OUT/driver-build.log" 2>&1
