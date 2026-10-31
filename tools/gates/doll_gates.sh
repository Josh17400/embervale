#!/usr/bin/env bash
# tools/gates/doll_gates.sh - W4: the doll gate harness (the P2c flip's R2 regression list, committed; SOLID_PLAN 4.3).
#
#   tools/gates/doll_gates.sh --bdir <dir> --out <dir> [--only NAMES] [--jobs N] [--baseline DIR] [--record DIR]
#
# Wall-clock lines (B10 / R6 bake p50/p95) are left out of the compare through baseline/119de74/gates/unstable.txt (each
# with its cause); a TIMING line that FAILs anyway is rerun once alone (every slot held) before it counts. motion_arms
# (29 MB of output) is compared as line count + sha256 of the normalized text + its last 40 lines.
# Every line of the flip's R2 list at the default rig (no EMB_RIG), plus the rig-1 proofs (D101). Each line passes when
# its rc equals the recorded rc (<name>.rc; 0 except lines red at 119de74) and its output, timing tokens masked, equals the 119de74 record in
# tools/gates/baseline/119de74/gates/doll/<name>.txt ("every line as at baseline"). Lines run --jobs at a time (default
# 4), each through tools/slot.sh. Writes <out>/logs, <out>/res, <out>/summary.txt and <out>/build.sha (lib.sh).
# Exit 0 only when no line FAILs.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
. "$REPO/tools/gates/lib.sh"
cd "$REPO"
unset EMB_RIG
BDIR=""; OUT=""; ONLY=""; JOBS=4; BL=""; REC=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bdir) BDIR="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --only) ONLY="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --baseline) BL="$2"; shift 2 ;;
    --record) REC="$2"; shift 2 ;;
    *) echo "doll_gates: unknown arg $1" >&2; exit 2 ;;
  esac
done
[ -n "$BDIR" ] && [ -n "$OUT" ] || { echo "doll_gates: --bdir and --out are required" >&2; exit 2; }
case "$BDIR" in /*|?:*) B="$BDIR" ;; *) B="$REPO/$BDIR" ;; esac
BL="${BL:-$REPO/tools/gates/baseline/119de74/gates/doll}"
mkdir -p "$OUT/logs" "$OUT/res"; OUT="$(cd "$OUT" && pwd)"; rm -f "$OUT"/res/*.txt
[ -n "$REC" ] && { mkdir -p "$REC"; REC="$(cd "$REC" && pwd)"; }
write_sha
EXE="$B/rpg_test.exe"

# NAME|command (rpg_test args unless the command starts with '!', which is a full command line)
LIST=$(cat <<'EOF'
pxc_check|!node tools/doll/pxc.mjs --check
gates_parity|--doll --gates --parity
envtable|--envelope
doll_envelope|--doll-envelope
env_ladder|--doll-envelope --scope ladder --seeds 1..3 --report
env_all|--doll-envelope --scope all --seeds 1..3
adefects|--doll-adefects
heads|--doll-heads
arms_matrix|--doll-arms --looks tools/doll/arms_looks.txt --report
ladder_selftest|--doll-ladder --selftest
ladder_finish|--doll-ladder --finish --report
ladder_finish_L|--doll-ladder --finish --scope ladder --seeds 1..3 --report
forge_w1|--doll-forge --worlds 1
sil_w1|--doll-forge --silhouettes --worlds 1
forge_w2|--doll-forge --worlds 2
sil_w2|--doll-forge --silhouettes --worlds 2
forge_w3|--doll-forge --worlds 3
sil_w3|--doll-forge --silhouettes --worlds 3
forge_w4|--doll-forge --worlds 4
sil_w4|--doll-forge --silhouettes --worlds 4
forge_w5|--doll-forge --worlds 5
sil_w5|--doll-forge --silhouettes --worlds 5
forge_w6|--doll-forge --worlds 6
sil_w6|--doll-forge --silhouettes --worlds 6
forge_shapes|--forge-shapes --check
forge_drawn|--doll-forge-drawn
cov_all|--doll-coverage --scope all --seeds 1..20 --gates
cov_ladder|--doll-coverage --scope ladder --seeds 1..3
cov_forge|--doll-coverage --scope forge
cov_envelope|--doll-coverage --scope envelope
variety|--doll-variety --seeds 1..3 --gates
anim_gates|--anim --gates
anim2|--anim
arms2|--arms --seeds 1..3
motion_forge|--doll --motion --limbs-looks tools/doll/forge_looks.txt --report
motion_envelope|--doll --motion --limbs-looks tools/doll/envelope_looks.txt --report
motion_ladder|--doll --motion --limbs-looks tools/doll/ladder_looks.txt --report
motion_arms|--doll --motion --limbs-looks tools/doll/arms_looks.txt --report
freeze|--doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt
rig|--rig
rig_gates|--rig --gates --report
art_hash_rig1|!B/art_hash.exe --rig=1
village_rig1|!B/city_gallery.exe --rig=1 --village-art-check tests/fixtures/golden_village_art_rig1.txt
arms_rig1|--arms --seeds 1..3 --rig=1
anim_rig1|--anim --rig=1
freeze_dressed_rig1|--rig=1 --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt
art_hash|!B/art_hash.exe
village_art|!B/city_gallery.exe --village-art-check
EOF
)

# HASHED: lines whose output is too big to commit (motion_arms prints ~490k lines, 29 MB): compared as "full output: N lines,
# sha256 H" of the normalized text plus its last 40 lines (where the summary sits), so any changed line still FAILs.
HASHED=" motion_arms "
# TIMING: lines that time something on the wall clock (B10 / R6 bake p95). Their timing lines are left out through
# baseline/119de74/gates/unstable.txt; if such a line FAILs anyway it is rerun once alone (slot_alone) before it counts.
TIMING=" gates_parity rig_gates anim_gates "
reduce_big() {   # reduce_big NAME LOG > reduced text
  local t="$OUT/logs/$1.full" ex; norm "$2" > "$t"; ex=$(unstable_re "$1")
  if [ -n "$ex" ]; then grep -vE -- "$ex" "$t" > "$t.x"; mv "$t.x" "$t"; fi
  echo "full output: $(wc -l < "$t") lines, sha256 $(sha256sum "$t" | cut -c1-64)"; tail -n 40 "$t"; rm -f "$t"
}
chk_doll() {   # same rc as recorded (a line red at 119de74 stays judged as "same output as baseline") and same output
  local r ok want log="$2"
  if [[ "$HASHED" == *" $1 "* ]]; then reduce_big "$1" "$2" > "$2.red"; log="$2.red"; fi
  r=$(base_cmp "$1" "$log"); ok=$?
  [[ "$HASHED" == *" $1 "* ]] && [ $ok -eq 0 ] && rm -f "$2"   # the 29 MB raw log only stays when the line FAILs
  if [ -n "${REC:-}" ]; then echo "$3" > "$REC/$1.rc"; echo "PASS rc=$3 $r"; return; fi
  want=$(cat "$BL/$1.rc" 2>/dev/null | tr -d '\r')
  if [ "$3" = "${want:-0}" ] && [ $ok -eq 0 ]; then echo "PASS $( [ "${want:-0}" != 0 ] && echo "known red rc=$want, ")$r"
  else echo "FAIL rc=$3 (baseline rc=${want:-0}) $r"; fi
}

one() {   # one NAME SPEC
  local name="$1" spec="$2"
  if [ "${spec:0:1}" = '!' ]; then
    spec="${spec:1}"; spec="${spec//B\//$B/}"
    # shellcheck disable=SC2086
    gate_run "$name" chk_doll "${ALONE_FLAG:---slot}" -- $spec
  else
    # shellcheck disable=SC2086
    gate_run "$name" chk_doll "${ALONE_FLAG:---slot}" -- "$EXE" $spec
  fi
}

running=0
while IFS='|' read -r name spec; do
  [ -z "$name" ] && continue
  gate_selected "$name" || continue
  one "$name" "$spec" &
  running=$((running+1))
  if [ $running -ge "$JOBS" ]; then wait -n 2>/dev/null || wait; running=$((running-1)); fi
done <<< "$LIST"
wait
# timing lines that FAILed under load: rerun once alone, first try kept as logs/NAME.first.log
while IFS='|' read -r name spec; do
  [ -z "$name" ] && continue; gate_selected "$name" || continue
  [[ "$TIMING" == *" $name "* ]] || continue
  grep -q "^$name FAIL " "$OUT/res/$name.txt" 2>/dev/null || continue
  mv "$OUT/logs/$name.log" "$OUT/logs/$name.first.log"; cp "$OUT/res/$name.txt" "$OUT/logs/$name.first.res"
  ALONE_FLAG=--alone one "$name" "$spec" > /dev/null
  sed -i "s/\$/ (rerun alone; first try FAILed under load: logs\/$name.first.log)/" "$OUT/res/$name.txt"
  cat "$OUT/res/$name.txt"
done <<< "$LIST"
gates_summary
