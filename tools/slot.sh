#!/usr/bin/env bash
# tools/slot.sh - machine-wide counting lock for heavy work (builds, game runs, tests).
#
#   tools/slot.sh <command> [args...]
#
# Takes one of EMB_SLOTS (default 3) slots, runs the command, releases the slot and returns the command's exit
# code. Slots are directories created with mkdir (atomic), so every agent on the machine shares the same cap no
# matter which repo copy or build directory it works in. A slot whose holder process is gone, or that is older
# than EMB_SLOT_MAX_AGE seconds (default 3 hours), is treated as stale and removed.
#
# Examples (Git Bash):
#   tools/slot.sh cmd //c "set BDIR=build_hero&& tools\\build.bat embervale"
#   tools/slot.sh build_hero/rpg_test.exe --seeds 1..20
#   tools/slot.sh build_hero/embervale.exe --script tools/scripts/creator.txt
#   tools/slot.sh --status          # list the current holders
#
# Environment: EMB_SLOTS (cap, default 3), EMB_SLOT_ROOT (lock directory), EMB_SLOT_WAIT (max seconds to wait
# for a slot, default 3600; exit code 75 on timeout), EMB_SLOT_MAX_AGE (stale age, default 10800).

set -u
N="${EMB_SLOTS:-3}"
MAXAGE="${EMB_SLOT_MAX_AGE:-10800}"
MAXWAIT="${EMB_SLOT_WAIT:-3600}"
if [ -n "${EMB_SLOT_ROOT:-}" ]; then
  ROOT="$EMB_SLOT_ROOT"
elif [ -n "${LOCALAPPDATA:-}" ] && command -v cygpath >/dev/null 2>&1; then
  ROOT="$(cygpath -u "$LOCALAPPDATA")/Temp/emb_slots"
else
  ROOT="/tmp/emb_slots"
fi
mkdir -p "$ROOT" 2>/dev/null

now() { date +%s; }

# A slot is stale when its owner pid no longer runs, or it is older than MAXAGE, or it never got an owner file
# (a holder killed between mkdir and writing the file) and is older than 60 s.
stale() {
  local d="$1" pid t age _rest
  if [ -f "$d/owner" ]; then
    read -r pid t _rest < "$d/owner" 2>/dev/null || { pid=""; t=0; }
    case "$t" in ""|*[!0-9]*) t=0 ;; esac
    case "$pid" in *[!0-9]*) pid="" ;; esac
    age=$(( $(now) - ${t:-0} ))
    [ "$age" -gt "$MAXAGE" ] && return 0
    [ -n "$pid" ] && ! kill -0 "$pid" 2>/dev/null && return 0
    return 1
  fi
  t=$(stat -c %Y "$d" 2>/dev/null || echo 0)
  [ $(( $(now) - t )) -gt 60 ]
}

if [ "${1:-}" = "--status" ]; then
  for i in $(seq 1 "$N"); do
    d="$ROOT/$i"
    if [ -d "$d" ]; then
      if stale "$d"; then echo "slot $i: STALE ($(cat "$d/owner" 2>/dev/null))"
      else echo "slot $i: busy  pid/time $(cut -d' ' -f1-2 "$d/owner" 2>/dev/null)  $(cut -d' ' -f3- "$d/owner" 2>/dev/null)"; fi
    else
      echo "slot $i: free"
    fi
  done
  exit 0
fi
if [ $# -eq 0 ]; then
  echo "usage: tools/slot.sh <command> [args...]   |   tools/slot.sh --status" >&2
  exit 2
fi

SLOT=""
start=$(now)
announced=0
while [ -z "$SLOT" ]; do
  for i in $(seq 1 "$N"); do
    d="$ROOT/$i"
    if mkdir "$d" 2>/dev/null; then
      echo "$$ $(now) $*" > "$d/owner"
      SLOT="$d"
      break
    fi
    if stale "$d"; then
      # Rename first so two waiters cannot both clean up and both claim the same directory.
      mv "$d" "$d.stale.$$" 2>/dev/null && rm -rf "$d.stale.$$"
    fi
  done
  if [ -z "$SLOT" ]; then
    if [ $(( $(now) - start )) -gt "$MAXWAIT" ]; then
      echo "slot.sh: no free slot after ${MAXWAIT}s" >&2
      exit 75
    fi
    if [ $announced -eq 0 ]; then echo "slot.sh: all $N slots busy, waiting..." >&2; announced=1; fi
    sleep 3
  fi
done

release() { rm -rf "$SLOT"; }
trap release EXIT
trap 'release; exit 130' INT TERM HUP

"$@"
rc=$?
exit $rc
