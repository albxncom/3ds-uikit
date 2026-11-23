#!/bin/bash
# installed.sh REV OUT: the proof that a program can take the kit as installed, with nothing of its sources. The
# kit's commit REV, exported by itself (git archive) into OUT/kit; in the container (IMG, image.sh; no network):
#   1. make lib there;
#   2. make install DESTDIR=/stage, with the container's own files and the export read-only (only OUT/stage and a
#      fresh /tmp can be written: a write outside DESTDIR fails the step), the staged files listed in OUT/staged.txt,
#      and no string of the staged library names the build folder (/src in the container) or OUT;
#   3. the example, copied alone into OUT/consumer/example, built with UIKIT set to the staged prefix (into its
#      build/installed/) and with no copy of the kit in the container: its compile lines and dependency files name no
#      kit source;
#   4. the example built from the kit's sources (make example, as alone.sh), and the two .3dsx compared: size, bytes,
#      and the symbols each defines that the other does not (every look's art, ART_<LOOK>, and every sound, snd_*
#      and track_*, must be in both);
#   5. the package description, pacman/PKGBUILD: bash -n, makepkg --printsrcinfo (as nobody: makepkg refuses root),
#      its build() and package() run with the export read-only into OUT/pkgdir and the file list compared with
#      step 2's; then makepkg itself, into OUT/pkgfile: where the image can build the package file, its file list is
#      compared with step 2's; where it cannot, makepkg's own error is printed (devkitarm:20260610 has no fakeroot,
#      which makepkg needs for package(), nor zstd, the compressor of makepkg.conf's PKGEXT .pkg.tar.zst).
# OUT must not exist; the logs go into it, one summary line a step is printed. Exit 0 only if every step passed.
[ $# -eq 2 ] && [ -n "$1" ] && [ -n "$2" ] || { echo "usage: tools/build/installed.sh REV OUT" >&2; exit 2; }
REV=$1; OUT=$2
[ -e "$OUT" ] && { echo "$OUT exists: give a new folder" >&2; exit 2; }
HERE=$(cd "$(dirname "$0")/../.." && pwd)
KIT=$(git -C "$HERE" rev-parse -q --verify "$REV^{commit}") || { echo "no commit $REV in the kit's repository" >&2; exit 2; }
mkdir -p "$OUT/kit" "$OUT/stage" "$OUT/pkgdir" "$OUT/pkgfile" "$OUT/consumer" && OUT=$(cd "$OUT" && pwd) && E=$OUT/kit
git -C "$HERE" archive "$KIT" | tar -x -C "$E" || { echo "export of $REV: failed"; exit 1; }
echo "== export of $(git -C "$HERE" rev-parse --short "$KIT"): $(find "$E" -type f | wc -l | tr -d ' ') files"
. "$E/tools/build/image.sh"
[ -n "${IMG:-}" ] || { echo "no IMG in the export's tools/build/image.sh"; exit 1; }
PREFIX=/opt/devkitpro/portlibs/3ds   # 3ds_rules' PORTLIBS in the image, make install's default prefix there
RUN=(docker run --rm --network none --platform linux/amd64)
fail=0

"${RUN[@]}" -v "$E":/src -w /src "$IMG" make lib > "$OUT/lib.log" 2>&1
rc=$?; [ $rc -eq 0 ] || fail=1
members=$("${RUN[@]}" -v "$E":/src:ro "$IMG" sh -c '$DEVKITARM/bin/arm-none-eabi-ar t /src/build/3ds/lib3ds-uikit.a | wc -l')
[ "$members" -gt 0 ] || fail=1
echo "make lib: exit $rc warning lines $(grep -c 'warning:' "$OUT/lib.log")," \
     "build/3ds/lib3ds-uikit.a $(stat -f %z "$E/build/3ds/lib3ds-uikit.a" 2>/dev/null || echo no) bytes, $members members"

"${RUN[@]}" --read-only --tmpfs /tmp -v "$E":/src:ro -v "$OUT/stage":/stage -w /src "$IMG" \
  sh -c 'make install DESTDIR=/stage && echo "written in /tmp: $(find /tmp -mindepth 1 | wc -l)"' > "$OUT/install.log" 2>&1
rc=$?; [ $rc -eq 0 ] || fail=1
(cd "$OUT/stage" && find . -type f -exec stat -f '%z %N' {} + | sed 's| \./| |' | sort -k2) > "$OUT/staged.txt"
outside=$(cd "$OUT/stage" && find . -mindepth 1 | grep -v -c -E "^\.(/opt(/devkitpro(/portlibs(/3ds(/.*)?)?)?)?)?$")
[ "$outside" -eq 0 ] || fail=1
paths=$(strings -a "$OUT/stage$PREFIX/lib/lib3ds-uikit.a" 2>/dev/null | grep -c -e /src -e "$OUT")
[ "$paths" -eq 0 ] || fail=1
echo "make install DESTDIR (read-only container and export): exit $rc $(tail -1 "$OUT/install.log")," \
     "$(wc -l < "$OUT/staged.txt" | tr -d ' ') files $(awk '{s+=$1} END {print s}' "$OUT/staged.txt") bytes," \
     "outside $PREFIX: $outside; strings of the library naming /src or OUT: $paths"

cp -R "$E/example" "$OUT/consumer/example"
"${RUN[@]}" -v "$OUT/consumer":/c -v "$OUT/stage":/stage:ro -w /c/example "$IMG" make UIKIT=/stage$PREFIX \
  > "$OUT/consumer.log" 2>&1
rc=$?; [ $rc -eq 0 ] || fail=1
C=$OUT/consumer/example/build/installed
own=$(cd "$OUT/consumer/example" && command ls *.c | wc -l | tr -d ' ')
compiled=$(grep -c -E '^arm-none-eabi-gcc .* -c ' "$OUT/consumer.log")
stray=$(grep -E '^arm-none-eabi-gcc .* -c ' "$OUT/consumer.log" | grep -c -v -E ' -I\. -I/stage[^ ]*/include/3ds-uikit -MMD -MP -c [a-z_]+\.c ')
deps=$(cat "$C"/*.d 2>/dev/null | tr ' \\' '\n\n' | grep -E '\.[ch]:?$' | tr -d : | sort -u)
foreign=$(printf '%s\n' "$deps" | grep -c -v -E "^(build/)?[a-z_]+\.[ch]$|^/stage$PREFIX/include/3ds-uikit/")
kit_headers=$(printf '%s\n' "$deps" | grep -c "^/stage$PREFIX/include/3ds-uikit/")
lib=$(grep -o -E "^/stage$PREFIX/lib/lib3ds-uikit\.a\([a-z_]+\.o\)" "$C/3ds-uikit-example.map" | sort -u | wc -l | tr -d ' ')
{ [ "$stray" -eq 0 ] && [ "$foreign" -eq 0 ] && [ "$compiled" -eq "$own" ] && [ "$lib" -gt 0 ]; } || fail=1
echo "example against the staged kit alone: exit $rc warning lines $(grep -c 'warning:' "$OUT/consumer.log")," \
     "compiled $compiled of its own $own .c, other compile lines $stray; dependencies: $kit_headers installed" \
     "headers, $foreign other; map: $lib members of /stage$PREFIX/lib/lib3ds-uikit.a"

"${RUN[@]}" -v "$E":/src -w /src "$IMG" make example > "$OUT/example.log" 2>&1
rc=$?; [ $rc -eq 0 ] || fail=1
echo "make example (from the sources): exit $rc warning lines $(grep -c 'warning:' "$OUT/example.log")"
S=$E/example/build/3ds-uikit-example
I=$C/3ds-uikit-example
for x in "$S" "$I"; do
  [ -f "$x.3dsx" ] && echo "  ${x#$OUT/}.3dsx $(stat -f %z "$x.3dsx") $(shasum -a 256 "$x.3dsx" | cut -c1-16)" \
    || { echo "  ${x#$OUT/}.3dsx: not built"; fail=1; }
done
if cmp -s "$S.3dsx" "$I.3dsx"; then echo "  the two .3dsx: equal bytes"; else echo "  the two .3dsx: the bytes differ"; fi
"${RUN[@]}" -v "$OUT":/out:ro "$IMG" sh -c \
  'for x in kit/example/build consumer/example/build/installed; do $DEVKITARM/bin/arm-none-eabi-nm --defined-only /out/$x/3ds-uikit-example.elf \
     | awk "{print \$3}" | sort -u > /tmp/$(echo $x | cut -d/ -f1); done; cd /tmp
   echo "  symbols: $(wc -l < kit) from the sources, $(wc -l < consumer) against the staged kit;" \
        "only from the sources: $(comm -23 kit consumer | wc -l) $(comm -23 kit consumer | head -12 | tr "\n" " ")" \
        "only against the staged kit: $(comm -13 kit consumer | wc -l) $(comm -13 kit consumer | head -12 | tr "\n" " ")"
   for f in kit consumer; do
     echo "  $f: looks $(grep -c -x -E "ART_(STOCK|FROST|NIGHT|PRISM)" $f) sounds $(grep -c -E "^(snd|track)_[a-z_]+$" $f | tr -d " ")" \
          "($(grep -E "^(snd|track)_" $f | grep -v _end | tr "\n" " "))"
   done' > "$OUT/compare.log" 2>&1
cat "$OUT/compare.log"
looks=$(grep -c -E '^  (kit|consumer): looks 4 ' "$OUT/compare.log")
[ "$looks" -eq 2 ] || fail=1
[ "$(grep -E '^  kit: ' "$OUT/compare.log" | sed 's/^  kit//')" = "$(grep -E '^  consumer: ' "$OUT/compare.log" | sed 's/^  consumer//')" ] \
  || { echo "  the looks or sounds differ between the two"; fail=1; }

P=$E/pacman/PKGBUILD
bash -n "$P"; rc=$?; [ $rc -eq 0 ] || fail=1
# makepkg as nobody (it refuses root), its folders in the container's /tmp (it wants each writable, the export is not)
MKPKG=(--read-only --tmpfs /tmp:mode=1777 --user 65534:65534 -e BUILDDIR=/tmp/build -e SRCDEST=/tmp/src
       -e SRCPKGDEST=/tmp/srcpkg -e LOGDEST=/tmp/log -e PKGDEST=/pkgfile -v "$E":/src:ro -v "$OUT/pkgfile":/pkgfile
       -w /src/pacman "$IMG" dkp-makepkg)
"${RUN[@]}" "${MKPKG[@]}" --printsrcinfo > "$OUT/srcinfo.txt" 2>&1
rs=$?; [ $rs -eq 0 ] || fail=1
"${RUN[@]}" --read-only --tmpfs /tmp -v "$E":/src:ro -v "$OUT/pkgdir":/pkgdir -w /src/pacman "$IMG" \
  bash -c 'set -e; startdir=$PWD; . ./PKGBUILD; srcdir=/tmp/src; pkgdir=/pkgdir; build; package' > "$OUT/pkgfn.log" 2>&1
rp=$?; [ $rp -eq 0 ] || fail=1
(cd "$OUT/pkgdir" && find . -type f -exec stat -f '%z %N' {} + | sed 's| \./| |' | sort -k2) > "$OUT/pkgdir.txt"
if cmp -s "$OUT/staged.txt" "$OUT/pkgdir.txt"; then same=equal; else same=DIFFERENT; fail=1; fi
echo "pacman/PKGBUILD: bash -n exit $rc, makepkg --printsrcinfo exit $rs ($(grep -E '^\s*(pkgver|arch|license|groups) =' "$OUT/srcinfo.txt" | tr -d '\t' | tr '\n' ' '))," \
     "build() and package() into a read-only container: exit $rp, $(wc -l < "$OUT/pkgdir.txt" | tr -d ' ') files, list and sizes $same to make install's"
"${RUN[@]}" "${MKPKG[@]}" --nodeps > "$OUT/makepkg.log" 2>&1
rk=$?
pkgfile=$(find "$OUT/pkgfile" -name '3ds-uikit-*.pkg.tar*' | head -1)
if [ $rk -eq 0 ] && [ -n "$pkgfile" ]; then
  "${RUN[@]}" -v "$OUT/pkgfile":/pkgfile:ro "$IMG" bsdtar -tf "/pkgfile/${pkgfile##*/}" | grep -v -E '^\.|/$' | sort \
    > "$OUT/pkgfile.txt"
  if [ "$(awk '{print $2}' "$OUT/staged.txt" | sort)" = "$(cat "$OUT/pkgfile.txt")" ]; then same=equal; else same=DIFFERENT; fail=1; fi
  echo "package file: ${pkgfile##*/} $(stat -f %z "$pkgfile") bytes, $(wc -l < "$OUT/pkgfile.txt" | tr -d ' ') files, list $same to make install's"
else
  zst=$("${RUN[@]}" "$IMG" bash -c '. /opt/devkitpro/pacman/etc/makepkg.conf; echo "PKGEXT $PKGEXT, zstd $(command -v zstd || echo absent)"')
  echo "package file: not built: makepkg exit $rk, \"$(grep -m1 'ERROR' "$OUT/makepkg.log" | sed 's/^==> //')\" ($zst)"
fi
echo "== installed: $([ $fail -eq 0 ] && echo passed || echo FAILED)"
exit $fail
