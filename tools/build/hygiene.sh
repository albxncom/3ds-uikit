#!/bin/bash
# hygiene.sh [DIR]: what must never stand in the repository, searched in DIR (a tree of it, such as an export by git
# archive). Without DIR: where the kit is an application's submodule, the application's root (git rev-parse
# --show-superproject-working-tree), and that search goes into the kit's checked-out files as well (git grep --no-index
# reads them as plain files and keeps to the kit's .gitignore); else the kit's own root. Searched for: a home folder
# path, a local network address, and the words of HYGIENE_WORDS (an extended regular expression, for example your own
# name; it is not written here, so that this file holds no name). Every file is searched except what the tree's
# .gitignore files leave out (the build output) and .git. Prints each hit as file:line without its text, then the
# count; exit 1 if there is any hit, 2 if the search itself failed (no such DIR, a bad expression).
DIR=${1:-}
if [ -z "$DIR" ]; then
  DIR=$(git -C "$(dirname "$0")" rev-parse --show-superproject-working-tree)
  DIR=${DIR:-$(git -C "$(dirname "$0")" rev-parse --show-toplevel)}
fi
[ -d "$DIR" ] || { echo "hygiene: no folder $DIR" >&2; exit 2; }
PATTERN='/User[s]/|192\.168\.'
[ -n "${HYGIENE_WORDS:-}" ] && PATTERN="$PATTERN|$HYGIENE_WORDS"
hits=$(git -C "$DIR" grep --no-index --exclude-standard -n -i -E -e "$PATTERN" -- . 2>&1)
rc=$?
if [ $rc -gt 1 ]; then
  echo "hygiene: the search failed (exit $rc): $(printf '%s' "$hits" | head -1)" >&2
  exit 2
fi
hits=$(printf '%s' "$hits" | cut -d: -f1,2)
n=$(printf '%s' "$hits" | grep -c .)
[ "$n" -eq 0 ] || printf '%s\n' "$hits"
echo "hygiene: $n lines"
[ "$n" -eq 0 ]
