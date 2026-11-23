#!/bin/bash
# alone.sh REV OUT: the proof that the kit stands alone. The kit's commit REV, exported by itself (git archive,
# nothing around it) into OUT/kit; there make test on the host, make example in the container (IMG, image.sh; no
# network), and hygiene.sh on the export before and after (HYGIENE_WORDS goes on to it). OUT must not exist; the logs
# go into it, one summary line a step is printed. REV is a commit of the kit's repository, the one this script stands
# in (two up). Where that is an application's submodule, REV may also name a commit of the application's: the kit's
# commit it pins is exported then (HEAD is the kit's own, the submodule's checked-out commit). Exit 0 only if every
# step passed.
[ $# -eq 2 ] && [ -n "$1" ] && [ -n "$2" ] || { echo "usage: tools/build/alone.sh REV OUT" >&2; exit 2; }
REV=$1; OUT=$2
[ -e "$OUT" ] && { echo "$OUT exists: give a new folder" >&2; exit 2; }
HERE=$(cd "$(dirname "$0")/../.." && pwd)
[ -z "$(git -C "$HERE" rev-parse --show-prefix)" ] || { echo "$HERE is not the root of the kit's repository" >&2; exit 2; }
KIT=$(git -C "$HERE" rev-parse -q --verify "$REV^{commit}"); FROM=
if [ -z "$KIT" ]; then
  SUPER=$(git -C "$HERE" rev-parse --show-superproject-working-tree)
  [ -n "$SUPER" ] && KIT=$(git -C "$SUPER" rev-parse -q --verify "$REV:$(git -C "$HERE/.." rev-parse --show-prefix)${HERE##*/}") \
    && FROM=" (the pin of $(git -C "$SUPER" rev-parse --short "$REV"))"
  git -C "$HERE" rev-parse -q --verify "${KIT:-none}^{commit}" > /dev/null \
    || { echo "no commit $REV in the kit's repository${SUPER:+, nor a pin of it here}" >&2; exit 2; }
fi
mkdir -p "$OUT/kit" && OUT=$(cd "$OUT" && pwd) && E=$OUT/kit
git -C "$HERE" archive "$KIT" | tar -x -C "$E" || { echo "export of $REV: failed"; exit 1; }
n=$(find "$E" -type f | wc -l | tr -d ' ')
echo "== export of $(git -C "$HERE" rev-parse --short "$KIT")$FROM: $n files"
[ "$n" -gt 0 ] || exit 1
. "$E/tools/build/image.sh"
[ -n "${IMG:-}" ] || { echo "no IMG in the export's tools/build/image.sh"; exit 1; }
fail=0

h=$("$E/tools/build/hygiene.sh" "$E") || fail=1
echo "hygiene before: $(printf "%s\n" "$h" | tail -1)"

make -C "$E" test > "$OUT/test.log" 2>&1; rc=$?
[ $rc -eq 0 ] || fail=1
bad=$(grep -c -E '[1-9][0-9]* failed|^FAILED' "$OUT/test.log")
[ "$bad" -eq 0 ] || fail=1
echo "make test: exit $rc warning lines $(grep -c 'warning:' "$OUT/test.log") summary lines" \
     "$(grep -c -E '[0-9]+ checks|[0-9]+ failed|^Ran [0-9]+ tests' "$OUT/test.log") with failures $bad"

docker run --rm --network none --platform linux/amd64 -v "$E":/src -w /src "$IMG" make example > "$OUT/example.log" 2>&1
rc=$?
[ $rc -eq 0 ] || fail=1
echo "make example: exit $rc warning lines $(grep -c 'warning:' "$OUT/example.log")"
X=$E/example/build/3ds-uikit-example.3dsx
if [ -f "$X" ]; then
  echo "example/build/3ds-uikit-example.3dsx $(stat -f %z "$X") $(shasum -a 256 "$X" | cut -d' ' -f1)"
else
  echo "example/build/3ds-uikit-example.3dsx: not built"; fail=1
fi

h=$("$E/tools/build/hygiene.sh" "$E") || fail=1
echo "hygiene after: $(printf "%s\n" "$h" | tail -1)"
echo "== alone: $([ $fail -eq 0 ] && echo passed || echo FAILED)"
exit $fail
