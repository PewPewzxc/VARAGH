#!/usr/bin/env bash
# End-to-end word-select check in eink-qemu (X4 Pro): long-press a word on a hyphen, Persian, table and
# footnote page; fail if the firmware logs a word-select error or the press changes nothing on screen.
# Usage: run.sh <eink-qemu.exe> <x4-pro build dir> <StarDict folder, e.g. .../English-English> <work dir>
set -euo pipefail
QEMU=$1; BUILD=$2; DICT=$3; WORK=$4
HERE=$(cd "$(dirname "$0")" && (pwd -W 2>/dev/null || pwd))  # -W: a Windows path under Git Bash
export MSYS_NO_PATHCONV=1
mkdir -p "$WORK"
python -I "$HERE/make_book.py" "$WORK/a-word-select-test.epub"
BASE=$(basename "$(ls "$DICT"/*.idx | head -1)" .idx)
# --put nests the folder, and the reader does nothing on a long-press without a chosen dictionary.
printf '%s' "/dictionaries/$(basename "$DICT")/$(basename "$DICT")/$BASE" > "$WORK/dictionary.bin"
rm -rf "$WORK/run"
PAGE="touch:100,181:900 wait-screen wait:3 shot:select-{i} press:home wait:3 touch:440,400 wait:6"
set +e
"$QEMU" run "$BUILD" --machine x4pro --out "$WORK/run" \
  --put "$WORK/a-word-select-test.epub=/" --put "$WORK/dictionary.bin=/.crosspoint" \
  --put "$DICT=/dictionaries/$(basename "$DICT")" \
  ready wait:3 touch:150,343 wait-screen wait:3 touch:150,194 'wait-log:reader enter' wait:8 \
  repeat:5 $PAGE end > "$WORK/run.out" 2>&1
RC=$?
set -e
if grep -aE "exceeds arena|arena overflow|capacity exceeded|working-set|OOM allocating|Guru Meditation" "$WORK/run/log.txt"; then
  echo "FAIL: word-select error in the firmware log"; exit 1
fi
[ $RC -eq 0 ] || { echo "FAIL: a step failed (see $WORK/run/run.jsonl)"; grep '"ok":false' "$WORK/run/run.jsonl" | cut -c1-300; exit 1; }
echo "PASS: 5 pages, screenshots in $WORK/run/shots"
