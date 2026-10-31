#!/usr/bin/env bash
# tools/gates/audio_pack.sh - the audio identity probe (docs/SOLID_PLAN.md wave 0, step 0.2).
#
# Runs audio_preview in all of its modes (the full run: every sfx, every music mode, the culture pieces, the tavern,
# the beds and moods, the magic voices; then --sea and --falls, which the full run leaves out), each into its own temp
# dir, and hashes every WAV written.
#
#   tools/gates/audio_pack.sh [--bdir DIR] [--out DIR] [--check FILE | --record] [--keep]
#
#   --check FILE  default tools/gates/baseline/119de74/probes/audio_hashes.txt
#   --record      write hashes.txt only (how the baseline was made)
#   --keep        keep the WAVs (default: deleted after hashing)
#
# hashes.txt: "<mode> <wav> <sha256>" per WAV and "<mode> __rc__ <exit code>" per mode, sorted. On a mismatch it
# names every differing WAV. Exit 0 only when every line is identical; the last line is "audio_pack PASS|FAIL: ...".
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
BDIR="${BDIR:-build}"; OUT=""; CHECK=""; RECORD=0; KEEP=0
while [ $# -gt 0 ]; do
  case "$1" in
    --bdir) BDIR="$2"; shift ;;
    --out) OUT="$2"; shift ;;
    --check) CHECK="$2"; shift ;;
    --record) RECORD=1 ;;
    --keep) KEEP=1 ;;
    -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
    *) echo "audio_pack: unknown argument $1"; exit 2 ;;
  esac
  shift
done
EXE="$BDIR/audio_preview.exe"
case "$EXE" in /*|?:*) ;; *) EXE="$ROOT/$EXE" ;; esac
[ -x "$EXE" ] || { echo "audio_pack FAIL: $EXE not found (build it first)"; exit 2; }
[ -z "$CHECK" ] && [ $RECORD -eq 0 ] && CHECK="$ROOT/tools/gates/baseline/119de74/probes/audio_hashes.txt"
[ -z "$OUT" ] && OUT="$(mktemp -d "${TMPDIR:-/tmp}/emb_audiopack.XXXXXX")"
mkdir -p "$OUT"
start=$SECONDS
pids=()
for mode in full sea falls; do
  (
    d="$OUT/wav_$mode"; rm -rf "$d"; mkdir -p "$d"
    flag=""; [ "$mode" != full ] && flag="--$mode"
    t0=$SECONDS
    "$ROOT/tools/slot.sh" "$EXE" "$(cygpath -w "$d")" $flag > "$OUT/$mode.log" 2>&1 < /dev/null; rc=$?
    {
      echo "$mode __rc__ $rc"
      ( cd "$d" && find . -name '*.wav' -type f | sed 's|^\./||' | LC_ALL=C sort | while read -r w; do echo "$mode $w $(sha256sum "$w" | cut -c1-64)"; done )
    } > "$OUT/$mode.hash"
    echo "$mode $((SECONDS - t0)) s rc $rc" > "$OUT/$mode.secs"
    [ "$KEEP" = 1 ] || rm -rf "$d"
  ) &
  pids+=($!)
done
wait "${pids[@]}"
cat "$OUT"/full.hash "$OUT"/sea.hash "$OUT"/falls.hash | LC_ALL=C sort > "$OUT/hashes.txt"
nwav=$(grep -vc ' __rc__ ' "$OUT/hashes.txt"); secs=$((SECONDS - start))
if [ -z "$CHECK" ]; then echo "audio_pack RECORD: $nwav WAVs, ${secs} s -> $OUT/hashes.txt"; exit 0; fi
[ -f "$CHECK" ] || { echo "audio_pack FAIL: no record $CHECK"; exit 2; }
LC_ALL=C sort "$CHECK" > "$OUT/rec.sorted"
LC_ALL=C join -j1 -a1 -a2 -e MISSING -o 0,1.2,2.2 <(awk '{print $1"|"$2, $3}' "$OUT/rec.sorted" | LC_ALL=C sort) \
  <(awk '{print $1"|"$2, $3}' "$OUT/hashes.txt" | LC_ALL=C sort) | awk '$2 != $3 {
    split($1, k, "|"); if ($2 == "MISSING") why = "new, not in the record"; else if ($3 == "MISSING") why = "missing";
    else if (k[2] == "__rc__") why = "exit code " $2 " -> " $3; else why = "samples differ";
    print "audio_pack DIFF " k[1] " " k[2] ": " why }' > "$OUT/diff.txt"
ndiff=$(wc -l < "$OUT/diff.txt"); nrec=$(wc -l < "$OUT/rec.sorted")
head -20 "$OUT/diff.txt"
if [ "$ndiff" -eq 0 ]; then echo "audio_pack PASS: $nwav WAVs identical ($nrec lines), ${secs} s"; exit 0; fi
echo "audio_pack FAIL: $ndiff of $nrec lines differ, ${secs} s - $OUT/diff.txt"; exit 1
