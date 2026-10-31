#!/usr/bin/env bash
# tools/gates/perf_pack.sh - perf numbers (docs/SOLID_PLAN.md wave 0 step 0.2; wave gate W6).
#
#   tools/gates/perf_pack.sh [--bdir DIR] [--out DIR] [--runs N] [--check FILE | --record]
#
# Runs each perf line --runs times (default 3), one after another and alone (timing lines are rerun alone before they
# count, plan 4.4), each through tools/slot.sh:
#   cityperf   rpg_test --cityperf               step avg / p99 / worst ms per seed
#   chunkbench rpg_test --chunkbench             warm chunk cost (ms per chunk)
#   frame      node tools/gates/perf_frame.mjs: ONE hidden `embervale.exe --witness` instance (never a visible window),
#              endless_walk's opening and walk, one Witness step per frame timed (median frame.step_med) and a drawn
#              frame (screenshot) every 15 frames timed (median frame.shot_med). Wall time per request, IPC included.
#              (The plan's EMB_PERF frame time exists only in the windowed loop, which gates must not start; this is the
#              hidden-instance replacement. Witness `measure perf` would read an adapter `perf` reply that embervale's
#              witness handlers do not implement, so it would time the same wall clock.)
# perf.txt: "<metric> min <a> median <b> max <c> noise <(q3-a)/b %>" per metric, q3 the upper-quartile run (so one run
# slowed by an outside process does not widen the gate). --check (default
# tools/gates/baseline/119de74/probes/perf.txt): a metric passes when its median is at most the record's median times
# (1 + record noise / sqrt(check runs) + 3 %) (W6: within the wave-0 noise plus 3 %; the check compares a MEDIAN of
# --runs runs, whose spread is the single-run spread over sqrt(runs), so a 10 % slowdown of frame.step_med, noise 7 %,
# is still caught). Exception: frame.shot_med (a drawn frame + capture on the GPU every hidden instance shares) keeps the
# full record noise: at wave 0 a back-to-back A/A moved it +10.7 % while the machine idled 21 % instead of 14 %.
# Lower is better for every metric; a faster run passes.
# The "worst" and "p99" metrics are recorded for information and not gated (a single frame / a 0.01 ms-quantized tail;
# at wave 0 they moved 15-30 % between back-to-back runs on the same binary). A limit is never tighter than record +
# 0.005 ms (5 counts of the 0.001 ms the lines print: cityperf step_avg ~0.12 ms is then gated near +6 %, not +15 %).
# Timing depends on machine load, so the pack runs ALONE (plan 4.4): before the first timed line it takes EVERY
# tools/slot.sh slot itself (waiting until tools/game_cap.sh says 5, i.e. the owner is not playing, and all 5 slots are
# free), so no build, gate or slot-run Witness job can start while it measures; then it waits until no foreign
# embervale.exe / rpg_test.exe / compiler / linker process is busy and the machine CPU is quiet (details at the wait
# loop below; runs disturbed by outside load are redone). --record refuses to write a record that was not
# measured alone, and refuses one whose gated noise is above 15 % (PERF_MAX_NOISE) for any gated metric; the record's
# header says so. A --check that could not get the machine alone still compares but says "NOT ALONE" in its verdict.
# --record defaults to 7 runs, --check to 5.
# QUIET is relative to this machine's idle level (it idles at ~14 % with tray apps and a stuck foreign process): the
# idle level is the lowest 5 s mean CPU seen while waiting (at least 10 s of samples); quiet = idle + 5 %
# (PERF_QUIET_ABOVE), never above 25 % (PERF_QUIET_MAX); a run is tainted by 3+ samples above idle + 15 %
# (PERF_MAX_ABOVE). PERF_QUIET_CPU / PERF_MAX_CPU set absolute values instead. The wait for the machine is capped
# (PERF_ALONE_WAIT: 240 s for --check, 1800 s for --record); a --check that hits the cap measures anyway, flagged NOT ALONE.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
BDIR="${BDIR:-build}"; OUT=""; RUNS=""; CHECK=""; RECORD=0
while [ $# -gt 0 ]; do
  case "$1" in
    --bdir) BDIR="$2"; shift ;;
    --out) OUT="$2"; shift ;;
    --runs) RUNS="$2"; shift ;;
    --check) CHECK="$2"; shift ;;
    --record) RECORD=1 ;;
    -h|--help) sed -n '2,/^set -u/p' "$0" | grep '^#'; exit 0 ;;
    *) echo "perf_pack: unknown argument $1"; exit 2 ;;
  esac
  shift
done
case "$BDIR" in /*|?:*) B="$BDIR" ;; *) B="$ROOT/$BDIR" ;; esac
for x in rpg_test.exe embervale.exe; do [ -x "$B/$x" ] || { echo "perf_pack FAIL: $B/$x not found"; exit 2; }; done
[ -z "$RUNS" ] && { [ $RECORD -eq 1 ] && RUNS=7 || RUNS=5; }
[ -z "$CHECK" ] && [ $RECORD -eq 0 ] && CHECK="$ROOT/tools/gates/baseline/119de74/probes/perf.txt"
[ -z "$OUT" ] && OUT="$(mktemp -d "${TMPDIR:-/tmp}/emb_perfpack.XXXXXX")"
mkdir -p "$OUT"
# ---- run alone: hold every slot, then wait out foreign heavy processes ----
NSLOT="${EMB_SLOTS:-5}"
if [ -n "${EMB_SLOT_ROOT:-}" ]; then SROOT="$EMB_SLOT_ROOT"
elif [ -n "${LOCALAPPDATA:-}" ]; then SROOT="$(cygpath -u "$LOCALAPPDATA")/Temp/emb_slots"; else SROOT="/tmp/emb_slots"; fi
mkdir -p "$SROOT"
HELD=()
# only a slot whose owner file still names this script is removed: a peer's stale sweep can take one over (seen at
# wave 0: a peer's slot.sh judged perf_pack's slots stale and reused them), and that peer's slot must survive
release_all() { local d pid _r; for d in "${HELD[@]}"; do pid=""; read -r pid _r 2>/dev/null < "$d/owner"; [ "$pid" = "$$" ] && rm -rf "$d"; done; HELD=(); }
mine() { local d pid _r; LOST=(); for d in "${HELD[@]}"; do pid=""; read -r pid _r 2>/dev/null < "$d/owner"; [ "$pid" = "$$" ] || LOST+=("$d"); done; }
regain() {   # take back slots a peer took over (up to 120 s); 1 when it could not
  local d end=$((SECONDS + 120))
  while :; do mine; [ ${#LOST[@]} -eq 0 ] && return 0
    for d in "${LOST[@]}"; do mkdir "$d" 2>/dev/null && echo "$$ $(date +%s) perf_pack (holds every slot: timing runs alone)" > "$d/owner"; done
    [ $SECONDS -gt $end ] && return 1; sleep 2; done
}
trap release_all EXIT
trap 'release_all; exit 130' INT TERM HUP
slot_stale() {   # same rule as tools/slot.sh: owner gone, older than 3 h, or ownerless for 60 s
  local d="$1" pid t _r
  if [ -f "$d/owner" ]; then read -r pid t _r < "$d/owner" 2>/dev/null || return 1
    [ $(( $(date +%s) - ${t:-0} )) -gt "${EMB_SLOT_MAX_AGE:-10800}" ] && return 0
    [ -n "$pid" ] && ! kill -0 "$pid" 2>/dev/null && return 0; return 1; fi
  [ $(( $(date +%s) - $(stat -c %Y "$d" 2>/dev/null || echo 0) )) -gt 60 ]
}
foreign() {   # heavy processes that are not ours (the owner's Desktop game counts too: it lowers game_cap anyway)
  tasklist //FO CSV //NH 2>/dev/null | tr -d '\r' | cut -d, -f1 | tr -d '"' |
    grep -iE '^(embervale|rpg_test|cl|link|clang|clang\+\+|clang-cl|lld-link|ninja|msbuild|cmake|em\+\+|emcc|save_test|art_hash|city_gallery|doll_gallery)\.exe$' |
    sort | uniq -c | awk '{ printf("%s%s x%s", NR > 1 ? ", " : "", $2, $1) }'
}
# The machine CPU (typeperf, % of all logical processors) is sampled every second from here on: load that never takes
# a slot (a WSL build, a browser, another session's Witness instances) shows there.
CPUCSV="$OUT/cpu.csv"; : > "$CPUCSV"
typeperf "\\Processor(_Total)\\% Processor Time" -si 1 > "$CPUCSV" 2>/dev/null < /dev/null &
TYPEPERF=$!
cleanup() { rm -f "$OUT/.watching"; kill "$TYPEPERF" 2>/dev/null; release_all; }
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM HUP
nsamp() { tr -d '\r' < "$CPUCSV" 2>/dev/null | grep -cE '^"[^"]*","[0-9.]+"$'; }
cpu_avg() {   # cpu_avg N: mean machine CPU % over the last N samples ("" when typeperf gave fewer)
  tr -d '\r' < "$CPUCSV" 2>/dev/null | awk -F'","' 'NF == 2 && $2 ~ /^[0-9.]+"?$/ { v = $2; gsub(/"/, "", v); a[++n] = v }
    END { k = '"$1"'; if (n < k) exit; for (i = n - k + 1; i <= n; i++) s += a[i]; printf("%.0f", s / k) }'
}
IDLE=""   # the lowest 5 s mean machine CPU % seen so far (this machine's idle level)
set_quiet() {   # QUIET: mean machine CPU % (5 s) that counts as quiet before a run; MAXCPU: a run with 3+ samples above is tainted
  local q=$(( ${IDLE:-0} + ${PERF_QUIET_ABOVE:-5} )); [ $q -gt "${PERF_QUIET_MAX:-25}" ] && q="${PERF_QUIET_MAX:-25}"
  QUIET="${PERF_QUIET_CPU:-$q}"; MAXCPU="${PERF_MAX_CPU:-$(( ${IDLE:-0} + ${PERF_MAX_ABOVE:-15} ))}"
}
idle_update() { local c; c="$(cpu_avg 5)"; [ -n "$c" ] && { [ -z "$IDLE" ] || [ "$c" -lt "$IDLE" ]; } && IDLE="$c"; set_quiet; }
set_quiet
[ $RECORD -eq 1 ] && WDEF=1800 || WDEF=240
ALONE=1; WAITMAX="${PERF_ALONE_WAIT:-$WDEF}"; t0=$SECONDS; said=0; BASE=""
# Alone = every slot held by this script, game_cap 5 (the owner is not playing), the machine CPU quiet (5 s mean below
# PERF_QUIET_CPU) and no foreign heavy process busy. Foreign processes that are present but idle at that moment (a
# peer's parked Witness pool instance, say) become the baseline: a run is tainted when MORE of them appear, or the CPU rises.
while :; do
  idle_update
  mine; if [ ${#LOST[@]} -gt 0 ]; then nh=(); for d in "${HELD[@]}"; do case " ${LOST[*]} " in *" $d "*) ;; *) nh+=("$d") ;; esac; done
    HELD=("${nh[@]+"${nh[@]}"}"); fi
  c="$(bash "$ROOT/tools/game_cap.sh" 2>/dev/null)"
  # Yield to a peer that is also gathering every slot (lib.sh slot_alone, another perf_pack): two gatherers each
  # holding some slots would wait on each other forever, so this one lets go until the peer is done.
  peer=0
  for i in $(seq 1 "$NSLOT"); do d="$SROOT/$i"; case " ${HELD[*]} " in *" $d "*) continue ;; esac
    grep -q "holds every slot" "$d/owner" 2>/dev/null && ! slot_stale "$d" && { peer=1; break; }; done
  if [ $peer -eq 1 ]; then
    [ ${#HELD[@]} -gt 0 ] && echo "perf_pack: a peer is gathering every slot ($(cut -c1-80 "$d/owner" 2>/dev/null)); letting go of ${#HELD[@]} and waiting"
    release_all
  elif [ "$c" = "$NSLOT" ]; then
    for i in $(seq 1 "$NSLOT"); do
      d="$SROOT/$i"; case " ${HELD[*]} " in *" $d "*) continue ;; esac
      [ -d "$d" ] && slot_stale "$d" && { mv "$d" "$d.stale.$$" 2>/dev/null && rm -rf "$d.stale.$$"; }
      if mkdir "$d" 2>/dev/null; then
        # the owner file can fail when a peer's stale sweep removed the fresh directory: then it is not held
        if echo "$$ $(date +%s) perf_pack (holds every slot: timing runs alone)" > "$d/owner" 2>/dev/null; then HELD+=("$d"); fi
      fi
    done
    if [ ${#HELD[@]} -eq "$NSLOT" ]; then
      cpu="$(cpu_avg 5)"
      [ -n "$cpu" ] && [ "$(nsamp)" -ge 10 ] && [ "$cpu" -lt "$QUIET" ] && { BASE="$(foreign)"; break; }
    fi
  fi
  if [ $((SECONDS - t0)) -gt "$WAITMAX" ]; then ALONE=0; break; fi
  [ $said -eq 0 ] && { echo "perf_pack: waiting to run alone (cap ${c:-?}/$NSLOT, slots held ${#HELD[@]}/$NSLOT, cpu $(cpu_avg 1)%, foreign: $(foreign))"; said=1; }
  sleep 2
done
echo "perf_pack: machine idle ${IDLE:-?}% -> quiet below ${QUIET}%, a run tainted above ${MAXCPU}% (waited $((SECONDS - t0)) s)"
NOTALONE=""
if [ $ALONE -eq 0 ]; then
  NOTALONE="cap ${c:-?}/$NSLOT, slots held ${#HELD[@]}/$NSLOT, cpu $(cpu_avg 5)%, foreign: $(foreign)"
  [ $RECORD -eq 1 ] && { echo "perf_pack FAIL: could not get the machine alone in ${WAITMAX}s ($NOTALONE); not recording"; exit 2; }
  echo "perf_pack WARNING: NOT ALONE ($NOTALONE) - the numbers carry outside load"
fi
start=$SECONDS
if [ $ALONE -eq 1 ]; then RUNNER=""; else release_all; RUNNER="$ROOT/tools/slot.sh"; fi
: > "$OUT/samples.txt"; : > "$OUT/watch.log"; : > "$OUT/runs.txt"
# The watcher logs "<time> <phase> <phase start> <previous phase> <cpu %> |<foreign>" every second. A run is TAINTED when
# a sample shows more foreign processes than the baseline plus the line's own (rpg_test.exe for cityperf/chunkbench,
# embervale.exe --witness for frame; the previous line's process is forgiven for 2 s while it exits), or 3+ samples
# with the machine CPU above PERF_MAX_CPU. A tainted run's numbers are dropped and the run is redone once the machine
# is quiet again (alone mode only; at most 3 x --runs attempts).
phase() { echo "$1 $(date +%s) ${PREV:-none}" > "$OUT/.phase"; PREV="$1"; }
PREV=none; phase none
if [ $ALONE -eq 1 ]; then
  touch "$OUT/.watching"
  ( while [ -f "$OUT/.watching" ]; do
      # the process list FIRST, the phase after it: a line launched right after its phase was written then never shows
      # under the phase before it (that race tainted clean runs as "rpg_test.exe x1 in none")
      fl="$(foreign)"; echo "$(date +%s) $(cat "$OUT/.phase" 2>/dev/null) $(cpu_avg 1 || true) |$fl" >> "$OUT/watch.log"; sleep 1; done ) &
fi
tainted() {   # tainted <first watch line>: prints why the run is tainted, if it is
  tail -n +"$1" "$OUT/watch.log" | awk -F'|' -v maxcpu="$MAXCPU" -v base="$BASE" '
    BEGIN { nb = split(base, bi, ", "); for (k = 1; k <= nb; k++) { split(bi[k], p, " x"); allow0[p[1]] = p[2] + 0 } }
    { split($1, h, " "); t = h[1]; exe = h[2]; ts = h[3]; prev = h[4]; cpu = (h[5] == "" ? 0 : h[5] + 0)
      if (exe != "none" && cpu > maxcpu) { nc++; if (cpu > mx) mx = cpu }
      n = split($2, it, ", "); bad = ""
      for (k = 1; k <= n; k++) { if (it[k] == "") continue; split(it[k], p, " x"); nm = p[1]; cnt = p[2] + 0
        allow = allow0[nm] + (nm == exe ".exe" ? 1 : 0) + (nm == prev ".exe" && t - ts <= 2 ? 1 : 0)
        if (cnt > allow) bad = bad nm " x" cnt " " }
      if (bad != "") { print "t+" (t - ts) "s in " exe ": " bad "(baseline: " (base == "" ? "none" : base) ")"; done = 1; exit } }
    END { if (!done && nc >= 3) printf("machine CPU above %d%% in %d samples (max %d%%): load outside the slot system\n", maxcpu, nc, mx) }'
}
clean=0; att=0
while [ $clean -lt "$RUNS" ]; do
  att=$((att + 1))
  if [ $ALONE -eq 1 ]; then
    if [ $att -gt $((3 * RUNS)) ]; then
      echo "perf_pack: only $clean clean runs in $((att - 1)) attempts (outside load kept tainting runs; see $OUT/runs.txt)"
      ALONE=0; NOTALONE="outside load tainted $(grep -c TAINTED "$OUT/runs.txt") runs"; [ $RECORD -eq 1 ] && break
    else   # wait until the machine is quiet again before the next run
      while :; do cpu="$(cpu_avg 5)"; [ -n "$cpu" ] && [ "$cpu" -lt "$QUIET" ] && break; [ $((SECONDS - t0)) -gt "$WAITMAX" ] && break; sleep 2; done
    fi
  fi
  i=$att; : > "$OUT/run.txt"; wl=$(( $(wc -l < "$OUT/watch.log") + 1 ))
  phase rpg_test
  $RUNNER "$B/rpg_test.exe" --cityperf > "$OUT/cityperf_$i.log" 2>&1 < /dev/null
  sed -n 's/^cityperf seed \([0-9]*\): .*step avg \([0-9.]*\) ms, p99 \([0-9.]*\) ms, worst \([0-9.]*\) ms.*/\1 \2 \3 \4/p' "$OUT/cityperf_$i.log" |
    awk '{print "cityperf.s"$1".step_avg", $2; print "cityperf.s"$1".step_p99", $3; print "cityperf.s"$1".step_worst", $4}' >> "$OUT/run.txt"
  sed -n 's/^cityperf seed \([0-9]*\): residents at .*townsfolk lane \([0-9.]*\) ms per step.*/\1 \2/p' "$OUT/cityperf_$i.log" |
    awk '{print "cityperf.s"$1".townsfolk_lane", $2}' >> "$OUT/run.txt"
  $RUNNER "$B/rpg_test.exe" --chunkbench > "$OUT/chunkbench_$i.log" 2>&1 < /dev/null
  sed -n 's/^chunkbench: \([0-9.]*\) ms per chunk.*/chunkbench.ms_per_chunk \1/p' "$OUT/chunkbench_$i.log" >> "$OUT/run.txt"
  phase embervale
  $RUNNER node "$ROOT/tools/gates/perf_frame.mjs" --bdir "$(cygpath -w "$B")" > "$OUT/frame_$i.log" 2>&1 < /dev/null
  grep -qE '^frame\.step_med ' "$OUT/frame_$i.log" || { echo "perf_pack FAIL: frame run $i: $(tail -n1 "$OUT/frame_$i.log" | cut -c1-200)"; exit 2; }
  grep -E '^frame\.[a-z_0-9]+ [0-9.]+$' "$OUT/frame_$i.log" >> "$OUT/run.txt"
  phase none
  if [ $ALONE -eq 1 ]; then sleep 1; why="$(tainted "$wl")"; mine
    if [ ${#LOST[@]} -gt 0 ]; then why="${why:+$why; }${#LOST[@]} slot(s) taken over by a peer"
      regain || { ALONE=0; NOTALONE="peers took ${#LOST[@]} of the held slots"; }; fi
  else why=""; fi
  if [ -n "$why" ]; then echo "run $att TAINTED ($why)" >> "$OUT/runs.txt"; echo "perf_pack: run $att tainted by outside load ($why), redoing"
  else cat "$OUT/run.txt" >> "$OUT/samples.txt"; clean=$((clean + 1)); echo "run $att clean" >> "$OUT/runs.txt"; fi
done
rm -f "$OUT/.watching"
if [ $ALONE -eq 0 ] && [ $RECORD -eq 1 ]; then echo "perf_pack FAIL: not recording ($NOTALONE)"; exit 2; fi
awk '{ v[$1] = v[$1] " " $2; if (!($1 in seen)) { seen[$1] = 1; order[++n] = $1 } }
     END { for (i = 1; i <= n; i++) { k = order[i]; m = split(v[k], a, " ");
             for (x = 1; x <= m; x++) for (y = x + 1; y <= m; y++) if (a[y] + 0 < a[x] + 0) { t = a[x]; a[x] = a[y]; a[y] = t }
             med = (m % 2) ? a[(m + 1) / 2] : (a[m / 2] + a[m / 2 + 1]) / 2
             qi = int((3 * m + 3) / 4); if (qi > m) qi = m; q = a[qi]
             printf("%s min %.3f median %.3f max %.3f noise %.1f%% runs %d\n", k, a[1], med, a[m], med > 0 ? 100 * (q - a[1]) / med : 0, m) } }' \
  "$OUT/samples.txt" > "$OUT/perf.txt"
secs=$((SECONDS - start))
nmet=$(wc -l < "$OUT/perf.txt")
[ "$nmet" -gt 0 ] || { echo "perf_pack FAIL: no perf numbers parsed (logs in $OUT)"; exit 2; }
gated_re='_worst|_p99'
if [ -z "$CHECK" ]; then
  MAXN="${PERF_MAX_NOISE:-15}"
  noisy=$(grep -vE "^[^ ]*($gated_re) " "$OUT/perf.txt" | awk -v m="$MAXN" '{ n = $9; sub(/%/, "", n); if (n + 0 > m) printf("%s %s, ", $1, $9) }')
  { echo "# perf_pack record: recorded ALONE ($(date '+%Y-%m-%d %H:%M'), every one of $NSLOT tools/slot.sh slots held by perf_pack, game_cap $NSLOT, machine CPU under ${QUIET}% before each run and no run with 3+ s above ${MAXCPU}%, idle foreign processes at the start: ${BASE:-none}), $RUNS clean runs ($(grep -c TAINTED "$OUT/runs.txt") tainted runs redone), bdir $(basename "$B"), git $(git rev-parse --short HEAD 2>/dev/null). Gated noise <= ${MAXN}%."
    cat "$OUT/perf.txt"; } > "$OUT/perf.rec" && mv "$OUT/perf.rec" "$OUT/perf.txt"
  cat "$OUT/perf.txt"
  if [ -n "$noisy" ]; then echo "perf_pack FAIL: gated noise above ${MAXN}%: ${noisy%, } (record not usable; logs in $OUT)"; exit 1; fi
  echo "perf_pack RECORD: $nmet metrics x $RUNS runs alone, ${secs} s -> $OUT/perf.txt"; exit 0
fi
[ -f "$CHECK" ] || { echo "perf_pack FAIL: no record $CHECK"; exit 2; }
PERF_REC="$CHECK" awk -v runs="$RUNS" '
  BEGIN { rec = ENVIRON["PERF_REC"]; while ((getline l < rec) > 0) { split(l, w, " "); if (w[1] ~ /^#/) continue; rm[w[1]] = w[5]; rn[w[1]] = w[9] + 0 } }
  { k = $1; if (!(k in rm)) { printf("perf_pack NEW %s median %s (not in the record)\n", k, $5); next }
    sq = (k ~ /shot_med$/) ? 1 : sqrt(runs); lim = rm[k] * (1 + rn[k] / 100 / sq + 0.03); if (lim < rm[k] + 0.005) lim = rm[k] + 0.005; gated = (k !~ /_worst$/ && k !~ /_p99$/)
    st = ($5 + 0 <= lim) ? "ok" : (gated ? "SLOWER" : "info"); if (st == "SLOWER") bad++
    printf("perf_pack %-6s %s median %s vs record %s (limit %.3f = +%.1f%% noise%s +3%%)%s\n", st, k, $5, rm[k], lim, rn[k], sq == 1 ? "" : "/sqrt(" runs ")", gated ? "" : " [not gated]"); seen[k] = 1 }
  END { for (k in rm) if (!(k in seen)) { printf("perf_pack MISSING %s\n", k); bad++ } printf("%d\n", bad + 0) > "/dev/stderr" }' \
  "$OUT/perf.txt" 2> "$OUT/bad.txt" | tee "$OUT/check.txt" | grep -v '^perf_pack ok' || true
bad=$(cat "$OUT/bad.txt")
[ -n "$NOTALONE" ] && echo "perf_pack NOTE: NOT ALONE ($NOTALONE)"
if [ "$bad" = 0 ]; then echo "perf_pack PASS: $nmet metrics within the record's noise + 3%, ${secs} s"; exit 0; fi
echo "perf_pack FAIL: $bad metrics slower than the record's noise + 3% (or missing), ${secs} s - $OUT/check.txt"; exit 1
