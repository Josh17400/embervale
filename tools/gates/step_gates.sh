#!/usr/bin/env bash
# tools/gates/step_gates.sh - the step gates S1-S10 (docs/SOLID_PLAN.md 4.2, contract in section 7).
#
#   tools/gates/step_gates.sh --bdir <dir> --out <dir> [--lines "<rpg_test args>;..."] [--only S5,S7] [--all]
#                             [--web-bdir <dir>] [--copy <wsl dir name>] [--base <rev>] [--expect KEY=VAL]...
#                             [--baseline <dir>] [--record <dir>] [--move-files "<files>"]
#
#   --bdir      MSVC build dir (relative to this checkout, or absolute); S1 builds it with tools\build.bat
#   --out       output dir: logs/<NAME>.log, res/<NAME>.txt, summary.txt, build.sha
#   --lines     extra rpg_test lines for the step's own systems, ';'-separated; each is a line L<n> (rc 0 passes)
#   --only      comma list of gate names or prefixes (S5 = S5.rig2 + S5.rig1; S10.digest = that line only). Names
#               given here run even when they are wave-end only.
#   --all       also run the wave-end-only gates S4 (clang CI list) and S8 (web build). By default they are SKIPped
#               (owner ruling D2: once per wave, at the wave gates); wave_gates.sh always passes --all.
#   --web-bdir  emscripten build dir for S8 (default build-web-<basename of --bdir>)
#   --copy      WSL copy for S4 (default emb_solid_<basename of --bdir>); never ~/emb
#   --base      the baseline revision (default 119de74): S3's fixture diff and S10's move_check range start
#   --expect    override one expected value of baseline/expect.txt (the negative test passes a wrong TOTAL here)
#   --baseline  the dir holding the expected records (default tools/gates/baseline/119de74/gates)
#   --record    write the records into this dir instead of comparing (how wave 0 made the baseline; never during the
#               plan: section 4.4 says no baseline is re-recorded)
#
# Lines (each heavy one through tools/slot.sh; S4 and S8 start first and run beside S1 and the rest):
#   S1.build    tools\build.bat (all targets): rc 0, every baseline exe present, no new warning kind, warnings <= base
#   S2.seeds    rpg_test --seeds 1..20: "20 passed, 0 failed", output equal to baseline
#   S3.save     save_test: ALL OK, output equal to baseline (load-sensitive byte counts masked, see unstable.txt :mask);  S3.fixtures  git diff <base> -- tests/fixtures empty and
#               nothing untracked there;  S3.savever  SAVE_VER is 23
#   S4.clang    tools/gates/clang_ci.sh: clang build rc 0, CLANG_FAILS=0 over the 68 web.yml commands
#               (side by side in their own slots; a failing command is rerun once alone before it counts, its first
#               log kept as <out>/clang/logs/clang_fail_<n>.log)
#   S5.rig2/rig1 art_hash.exe / --rig=1: TOTAL as expected and every line equal to baseline
#   S6.freeze   rpg_test --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt: 0 changed, equal.
#   S6.freeze1  rpg_test --rig=1 --doll-wear --freeze-check tools/doll/freeze/p2c_freeze_rig1.txt (rig 1's own
#               manifest, kept by D107): 0 changed, equal.
#   S6.pool     node tools/doll/pxc.mjs --check: up to date, bytes and hash equal
#   S7.rig2/rig1 city_gallery.exe [--rig=1] --village-art-check [...rig1 fixture]: 65 groups, 0 changed, equal
#   S8.build    tools\gates\build_web.bat (emsdk 6.0.13 from EMSDK);  S8.exports  web.yml's runtime export grep
#   S9.test     witness_smoke.sh test (4 files);  S9.shot  witness_smoke.sh shot (ssim >= 0.999 vs village_start)
#   S10.digest  rpg_test --sim-digest --check baseline/119de74/probes/sim_digest_msvc.txt
#   S10.view    node tools/gates/witness_pack.mjs --quick --check probes/<quick record> (hidden --witness instances; a
#               witness_pack that can still start a visible window is REFUSED, never run);  S10.audio  audio_pack.sh
#   S10.layers  tools/gates/check_layers.sh;  S10.move  tools/gates/move_check.sh <base> WORKTREE [files] (count; for
#               review, never fails)
#   A tool of 0.2/0.3 that is not there yet makes its line "SKIP (tool missing)".
# Exit 0 only when no line FAILs. The last screen is the PASS table (also in <out>/summary.txt).
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
. "$REPO/tools/gates/lib.sh"
cd "$REPO"

BDIR=""; OUT=""; LINES=""; ONLY=""; ALL=0; WEBB=""; COPY=""; BASE=119de74; BL=""; REC=""; MOVEFILES=""
EXPECT_OVR=()
while [ $# -gt 0 ]; do
  case "$1" in
    --bdir) BDIR="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --lines) LINES="$2"; shift 2 ;;
    --only) ONLY="$2"; shift 2 ;;
    --all|--wave-end) ALL=1; shift ;;
    --web-bdir) WEBB="$2"; shift 2 ;;
    --copy) COPY="$2"; shift 2 ;;
    --base) BASE="$2"; shift 2 ;;
    --expect) EXPECT_OVR+=("$2"); shift 2 ;;
    --baseline) BL="$2"; shift 2 ;;
    --record) REC="$2"; shift 2 ;;
    --move-files) MOVEFILES="$2"; shift 2 ;;
    -h|--help) sed -n '2,40p' "$0"; exit 0 ;;
    *) echo "step_gates: unknown arg $1" >&2; exit 2 ;;
  esac
done
[ -n "$BDIR" ] && [ -n "$OUT" ] || { echo "step_gates: --bdir and --out are required (see --help)" >&2; exit 2; }
BL="${BL:-$REPO/tools/gates/baseline/119de74/gates}"
PROBES="$REPO/tools/gates/baseline/119de74/probes"
case "$BDIR" in /*|?:*) BABS="$BDIR" ;; *) BABS="$REPO/$BDIR" ;; esac
BNAME=$(basename "$BDIR")
WEBB="${WEBB:-build-web-$BNAME}"
case "$WEBB" in /*|?:*) WABS="$WEBB" ;; *) WABS="$REPO/$WEBB" ;; esac
COPY="${COPY:-emb_solid_$BNAME}"
mkdir -p "$OUT/logs" "$OUT/res"
OUT="$(cd "$OUT" && pwd)"
[ -n "$REC" ] && { mkdir -p "$REC"; REC="$(cd "$REC" && pwd)"; }
rm -f "$OUT"/res/*.txt
write_sha

# expected values: baseline/expect.txt (KEY=VALUE), then --expect overrides
declare -A EXP
EXPF="$BL/expect.txt"; [ -n "$REC" ] && EXPF="$REPO/tools/gates/baseline/119de74/gates/expect.txt"
if [ -f "$EXPF" ]; then while IFS='=' read -r k v; do k="${k%$'\r'}"; v="${v%$'\r'}"; case "$k" in ''|\#*) continue ;; esac; EXP[$k]="$v"; done < "$EXPF"; fi
for kv in "${EXPECT_OVR[@]+"${EXPECT_OVR[@]}"}"; do EXP[${kv%%=*}]="${kv#*=}"; done
exp() { echo "${EXP[$1]:-}"; }

# wave-end-only lines run with --all, or when named in --only
wave_end_ok() { [ $ALL -eq 1 ] && return 0; [ -n "$ONLY" ] && gate_selected "$1" && [[ ",$ONLY," == *",${1%%.*}"* ]] && return 0; return 1; }

# ---------------------------------------------------------------- checks
chk_build() {   # S1
  local name="$1" log="$2" rc="$3" miss="" e new tot btot
  if [ "$rc" -ne 0 ] || grep -q 'ninja: build stopped' "$log"; then echo "FAIL build rc=$rc: $(grep -m1 -E 'error|FAILED' "$log" | cut -c1-120)"; return; fi
  ls "$BABS"/*.exe 2>/dev/null | xargs -n1 basename | sort > "$OUT/logs/S1.exes"
  # warning kinds: the message without path and line, one count per kind
  tr -d '\r' < "$log" | grep -E 'warning [A-Z]+[0-9]+' | sed -E 's/^.*(warning [A-Z]+[0-9]+ ?:.*)$/\1/' | sort | uniq -c | sed -E 's/^ +//' > "$OUT/logs/S1.warnings"
  tot=$(awk '{s+=$1} END {print s+0}' "$OUT/logs/S1.warnings")
  if [ -n "$REC" ]; then
    cp "$OUT/logs/S1.exes" "$REC/S1.exes.txt"; cp "$OUT/logs/S1.warnings" "$REC/S1.warnings.txt"
    echo "PASS recorded $(wc -l < "$OUT/logs/S1.exes") exes, $tot warning lines ($(wc -l < "$OUT/logs/S1.warnings") kinds)"; return
  fi
  [ -f "$BL/S1.exes.txt" ] || { echo "FAIL no baseline $BL/S1.exes.txt"; return; }
  miss=$(comm -23 "$BL/S1.exes.txt" "$OUT/logs/S1.exes" | tr '\n' ' ')
  [ -n "$miss" ] && { echo "FAIL missing exes: $miss"; return; }
  new=$(cut -d' ' -f2- "$OUT/logs/S1.warnings" | sort -u | comm -23 - <(cut -d' ' -f2- "$BL/S1.warnings.txt" | sort -u) | head -2 | tr '\n' ' ')
  btot=$(awk '{s+=$1} END {print s+0}' "$BL/S1.warnings.txt")
  [ -n "$new" ] && { echo "FAIL new warning kind: $new"; return; }
  [ "$tot" -gt "$btot" ] && { echo "FAIL $tot warning lines > baseline $btot (fresh build)"; return; }
  echo "PASS $(wc -l < "$OUT/logs/S1.exes") exes, warnings $tot <= $btot, $(grep -oE '\[[0-9]+/[0-9]+\]' "$log" | tail -1 || true)$(grep -q 'no work to do' "$log" && echo ' no work to do')"
}
chk_seeds() {   # S2: rpg_test --seeds ends "<n> passed, 0 failed"
  local r ok; r=$(base_cmp "$1" "$2"); ok=$?
  if [ "$3" -eq 0 ] && tr -d '\r' < "$2" | tail -n3 | grep -qE '^[0-9]+ passed, 0 failed' && [ $ok -eq 0 ]; then echo "PASS $(lastline "$2"); $r"
  else echo "FAIL rc=$3 $(lastline "$2"); $r"; fi
}
chk_fixtures() {
  local u; u=$(git -C "$REPO" status --porcelain -- tests/fixtures | head -3 | tr '\n' ' ')
  if [ "$3" -eq 0 ] && [ -z "$u" ]; then echo "PASS tests/fixtures identical to $BASE"; else echo "FAIL tests/fixtures differ from $BASE: $u$(head -3 "$2" | tr '\n' ' ')"; fi
}
chk_savever() {
  local want; want=$(exp SAVE_VER); local got; got=$(grep -oE 'SAVE_VER = [0-9]+' "$2" | head -1 | grep -oE '[0-9]+$')
  [ -n "$want" ] && [ "$got" = "$want" ] && echo "PASS SAVE_VER $got" || echo "FAIL SAVE_VER '$got' (want $want)"
}
chk_clang() {
  local cmds fails want; cmds=$(grep -oE 'CLANG_CMDS=[0-9]+' "$2" | tail -1 | cut -d= -f2); fails=$(grep -oE 'CLANG_FAILS=[0-9]+' "$2" | tail -1 | cut -d= -f2)
  want=$(exp CLANG_CMDS)
  if [ "$3" -eq 0 ] && [ "${fails:-x}" = 0 ] && { [ -z "$want" ] || [ "$cmds" = "$want" ]; }; then echo "PASS $(grep -m1 'clang build rc' "$2" | cut -c1-40), $cmds commands, CLANG_FAILS=0$(grep -oE 'CLANG_RERUN_OK=[1-9][0-9]*' "$2" | tail -1 | sed 's/^/ (/; s/$/ ok on the alone rerun)/')"
  else echo "FAIL $(grep -m1 'clang build rc' "$2" | cut -c1-60), cmds=$cmds (want ${want:-any}), CLANG_FAILS=${fails:-?} $(grep -m2 '^FAIL' "$2" | cut -c1-80 | tr '\n' ' ')"; fi
}
chk_art() {   # S5: TOTAL as expected (expect.txt / --expect) and the whole output equal to baseline
  local key=ART_TOTAL_RIG2; [ "$1" = S5.rig1 ] && key=ART_TOTAL_RIG1
  local want got r ok; want=$(exp $key); got=$(grep -m1 '^TOTAL' "$2" | awk '{print $2}' | tr -d '\r')
  r=$(base_cmp "$1" "$2"); ok=$?
  if [ "$3" -ne 0 ] || [ -z "$got" ]; then echo "FAIL rc=$3 no TOTAL"; return; fi
  if [ -z "$want" ] || [ "$got" != "$want" ]; then echo "FAIL TOTAL $got, expected ${want:-<no $key in expect.txt>}"; return; fi
  [ $ok -eq 0 ] && echo "PASS TOTAL $got; $r" || echo "FAIL TOTAL $got ok but $r"
}
chk_freeze() {
  local r ok; r=$(base_cmp "$1" "$2"); ok=$?
  local line; line=$(grep -m1 'freeze:.*frames checked' "$2" | tr -d '\r' | sed 's/^ *//')
  local red; red=$(exp "KNOWN_RED_$1")   # already red at 119de74 (4.4): judged as "same output as baseline", same rc
  if [ -n "$red" ]; then
    if [ "$3" = "$red" ] && [ $ok -eq 0 ]; then echo "PASS known red at 119de74 (rc=$red), same output: ${line:0:70}; $r"
    else echo "FAIL known red at 119de74 (rc=$red) but output/rc changed: rc=$3 ${line:0:70}; $r"; fi; return
  fi
  if [ "$3" -eq 0 ] && echo "$line" | grep -q ' 0 changed, 0 gone, 0 new frames' && [ $ok -eq 0 ]; then echo "PASS ${line:0:90}; $r"
  else echo "FAIL rc=$3 ${line:0:90}; $r"; fi
}
chk_pool() {
  local r ok; r=$(base_cmp "$1" "$2" 'pxc --check'); ok=$?
  if [ "$3" -eq 0 ] && grep -q 'is up to date' "$2" && [ $ok -eq 0 ]; then echo "PASS $(grep -m1 'pxc --check' "$2" | grep -oE '\([^)]*\)')"
  else echo "FAIL rc=$3 $(grep -m1 'pxc --check' "$2" | cut -c1-120); $r"; fi
}
chk_village() {
  local want r ok; want=$(exp VILLAGE_GROUPS); r=$(base_cmp "$1" "$2"); ok=$?
  local line; line=$(tr -d '\r' < "$2" | grep -m1 -E '[0-9]+ groups?, [0-9]+ changed')
  if [ "$3" -eq 0 ] && echo "$line" | grep -qE "\b${want:-65} groups?, 0 changed" && [ $ok -eq 0 ]; then echo "PASS ${line:0:80}; $r"
  else echo "FAIL rc=$3 '${line:0:80}' (want ${want:-65} groups, 0 changed); $r"; fi
}
chk_exports() { [ "$3" -eq 0 ] && echo "PASS $(lastline "$2")" || echo "FAIL $(lastline "$2")"; }
chk_witness() {
  if [ "$3" -eq 0 ]; then echo "PASS $(tr -d '\r' < "$2" | grep -iE 'passed|pass|ssim' | tail -1 | cut -c1-120)"
  else echo "FAIL $(tr -d '\r' < "$2" | grep -iE 'fail|error|ssim' | head -2 | cut -c1-120 | tr '\n' ' ')"; fi
}
chk_probe() {   # 0.2/0.3 tools: rc 0; rc 127/126 or a missing tool = SKIP
  if grep -q 'SKIP (tool missing)' "$2"; then echo "SKIP $(lastline "$2")"; return; fi
  [ "$3" -eq 0 ] && echo "PASS $(lastline "$2")" || echo "FAIL $(lastline "$2")"
}
chk_move() {
  if grep -q 'SKIP (tool missing)' "$2"; then echo "SKIP $(lastline "$2")"; return; fi
  local n; n=$(grep -cE '^[^ ]+:[0-9]+:' "$2")
  [ "$3" -le 1 ] && echo "PASS $n new lines (review them: $OUT/logs/S10.move.log)" || echo "FAIL move_check rc=$3 $(lastline "$2")"
}

# ---------------------------------------------------------------- runners (functions so the lines can run in parallel)
run_exports() {   # web.yml's own "Check the page's runtime exports" step, extracted (never hand-kept), on $WABS
  local js="$WABS/embervale.js" body
  [ -f "$js" ] && [ -f "$WABS/embervale.wasm" ] || { echo "no $js / embervale.wasm"; return 1; }
  body=$(awk '/name: Check the page.s runtime exports/ {f=1; next} f && /^ *- name:/ {exit} f && !/run: \|/ && !/emcc --version/'          "$REPO/.github/workflows/web.yml" | sed -e 's/^ *//' -e "s#build-web/#$WABS/#g")
  [ -n "$body" ] || { echo "web.yml has no runtime export step"; return 1; }
  bash -c "$body" || return 1
  echo "exports ok (web.yml step, $(echo "$body" | grep -c .) lines): FS IDBFS addRunDependency removeRunDependency + indexedDB ($(wc -c < "$js") B js, $(wc -c < "$WABS/embervale.wasm") B wasm)"
}
run_savever() { grep -rhoE 'SAVE_VER = [0-9]+' "$REPO/rpg" --include=*.cpp --include=*.h | sort -u; }
run_fixtures() { git -C "$REPO" diff --stat "$BASE" -- tests/fixtures; git -C "$REPO" diff --quiet "$BASE" -- tests/fixtures; }
tool_or_skip() {   # tool_or_skip PATH CMD...: run CMD when PATH exists, else say SKIP (tool missing)
  local p="$1"; shift
  if [ ! -e "$p" ]; then echo "SKIP (tool missing): $p"; return 0; fi
  "$@"
}
run_digest() {
  local rec=""
  for f in "$PROBES/sim_digest_msvc.txt" "$PROBES/sim_digest.txt"; do [ -f "$f" ] && { rec="$f"; break; }; done
  [ -n "$rec" ] || { echo "SKIP (tool missing): no record $PROBES/sim_digest_msvc.txt"; return 0; }
  "$REPO/tools/slot.sh" "$BABS/rpg_test.exe" --sim-digest --check "$rec"; local rc=$?
  if [ $rc -ne 0 ] && grep -qiE 'unknown (flag|option|arg)' "$OUT/logs/S10.digest.log" 2>/dev/null; then echo "SKIP (tool missing): rpg_test has no --sim-digest"; return 0; fi
  return $rc
}
run_view_quick() {   # the quick view pack through witness_pack.mjs (hidden --witness instances only; CONTRACT)
  local wp="$REPO/tools/gates/witness_pack.mjs" rec="" f h
  [ -f "$wp" ] || { echo "SKIP (tool missing): $wp"; return 0; }
  if h=$(headed_launch "$wp"); then echo "REFUSED: witness_pack.mjs can start a visible game window ($h)"; return 9; fi
  for f in view_quick_hashes.txt view_quick/hashes.txt view_pack_quick.txt view_quick.txt; do [ -f "$PROBES/$f" ] && { rec="$PROBES/$f"; break; }; done
  [ -n "$rec" ] || { echo "SKIP (tool missing): no quick view record under $PROBES"; return 0; }
  local ex=(); [ -f "$(dirname "$rec")/exclude.txt" ] && ex=(--exclude "$(dirname "$rec")/exclude.txt")
  node "$wp" --bdir "$BABS" --quick --out "$OUT/view_pack" --jobs "${WP_JOBS:-5}" --check "$rec" "${ex[@]+"${ex[@]}"}"; local rc=$?
  find "$OUT/view_pack" -name '*.png' -delete 2>/dev/null
  return $rc
}
run_move() {
  # shellcheck disable=SC2086
  "$REPO/tools/gates/move_check.sh" "$BASE" WORKTREE $MOVEFILES
}

# ---------------------------------------------------------------- the run
echo "step_gates: bdir=$BDIR out=$OUT $( [ $ALL -eq 1 ] && echo '(--all: S4 and S8 included)' || echo '(S4 and S8 wave-end only; --all runs them)')"
BG=()
# S4 and S8 do not need the MSVC build: start them first, beside S1
if wave_end_ok S4.clang; then ( gate_run S4.clang chk_clang -- "$REPO/tools/gates/clang_ci.sh" --copy "$COPY" --out "$OUT/clang" ) &
else gate_skip S4.clang "wave-end only (D2); --all runs it"; fi
if wave_end_ok S8.build; then
  ( gate_run S8.build chk_rc --slot -- env MSYS_NO_PATHCONV=1 cmd /c "tools\\gates\\build_web.bat $(cygpath -w "$WABS")"
    if grep -q ' PASS ' "$OUT/res/S8.build.txt" 2>/dev/null; then gate_run S8.exports chk_exports -- run_exports
    else gate_skip S8.exports "web build failed"; fi ) &
else gate_skip S8.build "wave-end only (D2); --all runs it"; gate_skip S8.exports "wave-end only (D2); --all runs it"; fi

# S1: everything below needs its exes
S1OK=1
if gate_selected S1.build; then
  gate_run S1.build chk_build --slot -- env MSYS_NO_PATHCONV=1 cmd /c "set BDIR=$(cygpath -w "$BABS")&& tools\\build.bat"
  grep -q ' PASS ' "$OUT/res/S1.build.txt" || S1OK=0
fi
if [ $S1OK -eq 0 ]; then
  for n in S2.seeds S3.save S5.rig2 S5.rig1 S6.freeze S6.freeze1 S7.rig2 S7.rig1 S9.test S9.shot S10.digest S10.view S10.audio; do
    gate_selected "$n" && { mkdir -p "$OUT/res"; echo "$n FAIL rc=- 0s | not run: S1 build failed" | tee "$OUT/res/$n.txt"; }
  done
else
  R="$BABS/rpg_test.exe"
  ( gate_run S2.seeds chk_seeds --slot -- "$R" --seeds 1..20 ) & BG+=($!)
  ( gate_run S3.save chk_allok_base --slot -- "$BABS/save_test.exe" ) & BG+=($!)
  ( gate_run S5.rig2 chk_art --slot -- "$BABS/art_hash.exe"
    gate_run S5.rig1 chk_art --slot -- "$BABS/art_hash.exe" --rig=1 ) & BG+=($!)
  ( gate_run S6.freeze chk_freeze --slot -- "$R" --doll-wear --freeze-check tools/doll/freeze/p2c_freeze.txt
    gate_run S6.freeze1 chk_freeze --slot -- "$R" --rig=1 --doll-wear --freeze-check tools/doll/freeze/p2c_freeze_rig1.txt ) & BG+=($!)
  ( gate_run S7.rig2 chk_village --slot -- "$BABS/city_gallery.exe" --village-art-check ) & BG+=($!)
  ( gate_run S7.rig1 chk_village --slot -- "$BABS/city_gallery.exe" --rig=1 --village-art-check tests/fixtures/golden_village_art_rig1.txt ) & BG+=($!)
  ( gate_run S10.digest chk_probe -- run_digest
    gate_run S10.audio chk_probe -- tool_or_skip "$REPO/tools/gates/audio_pack.sh" "$REPO/tools/gates/audio_pack.sh" --bdir "$BABS" --out "$OUT/audio_pack" ) & BG+=($!)
  ( gate_run S10.view chk_probe -- run_view_quick ) & BG+=($!)
  # S9: witness_smoke takes one slot per hidden game itself (fixed-step runs: machine load does not change them)
  ( gate_run S9.test chk_witness -- "$REPO/tools/gates/witness_smoke.sh" test --bdir "$BABS" --out "$OUT/witness" ) & BG+=($!)
  ( gate_run S9.shot chk_witness -- "$REPO/tools/gates/witness_smoke.sh" shot --bdir "$BABS" --out "$OUT/witness"
    rm -f "$OUT"/witness/shots/*.png 2>/dev/null ) & BG+=($!)
  # --lines: the step's own rpg_test lines
  if [ -n "$LINES" ]; then
    ( i=0; IFS=';' read -ra LS <<< "$LINES"
      for l in "${LS[@]}"; do l="$(echo "$l" | sed 's/^ *//; s/ *$//')"; [ -z "$l" ] && continue; i=$((i+1))
        # shellcheck disable=SC2086
        gate_run "L$i" chk_rc --slot -- "$R" $l; echo "L$i = rpg_test $l" >> "$OUT/lines.txt"; done ) & BG+=($!)
  fi
fi
gate_run S3.fixtures chk_fixtures -- run_fixtures
gate_run S3.savever chk_savever -- run_savever
gate_run S6.pool chk_pool -- node tools/doll/pxc.mjs --check
gate_run S10.layers chk_probe -- tool_or_skip "$REPO/tools/gates/check_layers.sh" "$REPO/tools/gates/check_layers.sh"
gate_run S10.move chk_move -- tool_or_skip "$REPO/tools/gates/move_check.sh" run_move
wait "${BG[@]+"${BG[@]}"}" 2>/dev/null
BG=()
wait
gates_summary
