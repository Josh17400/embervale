#!/usr/bin/env bash
# tools/game_cap.sh - how many heavy jobs (builds, test runs, headless Witness game instances) may run right now.
#
#   tools/game_cap.sh            # prints 2 while the owner is playing a game, else 5
#   tools/game_cap.sh --why      # also prints the game process that triggered the low cap
#
# Owner rule (2026-10-30): "if i have a game open im playing instead of witness having 5 headless windows open only
# allow 2 so i can keep playing my game". A game counts as open when a process runs from a Steam, Epic, GOG, Xbox or
# EA game folder, or the owner's own Embervale (Desktop\Embervale.exe). The answer is cached for 30 s in the slot
# directory, so many waiters share one check. Witness 0.7.12+ applies the same rule to its own game instances
# (WITNESS_MAX_PROCS_GAMING); this script covers everything else that goes through tools/slot.sh (builds, rpg_test ...).
#
# Environment: EMB_SLOTS (normal cap, default 5), EMB_SLOTS_GAMING (cap while a game is open, default 2),
# EMB_GAME_RE (extra regex of exe paths that count as a game).

set -u
HI="${EMB_SLOTS:-5}"
LO="${EMB_SLOTS_GAMING:-2}"
if [ -n "${EMB_SLOT_ROOT:-}" ]; then
  ROOT="$EMB_SLOT_ROOT"
elif [ -n "${LOCALAPPDATA:-}" ] && command -v cygpath >/dev/null 2>&1; then
  ROOT="$(cygpath -u "$LOCALAPPDATA")/Temp/emb_slots"
else
  ROOT="/tmp/emb_slots"
fi
mkdir -p "$ROOT" 2>/dev/null
CACHE="$ROOT/gaming"

now=$(date +%s)
game=""
if [ -f "$CACHE" ] && [ $(( now - $(stat -c %Y "$CACHE" 2>/dev/null || echo 0) )) -lt 30 ]; then
  game="$(cat "$CACHE" 2>/dev/null)"
else
  re='\\steamapps\\common\\|\\Epic Games\\|\\GOG Galaxy\\Games\\|\\GOG Games\\|\\XboxGames\\|\\EA Games\\|\\Desktop\\Embervale\.exe$'
  [ -n "${EMB_GAME_RE:-}" ] && re="$re|$EMB_GAME_RE"
  game="$(powershell.exe -NoProfile -NonInteractive -Command "\$re='$re'; Get-CimInstance Win32_Process -Property Name,ExecutablePath | Where-Object { \$_.ExecutablePath -and \$_.ExecutablePath -match \$re -and \$_.Name -notmatch '^crashpad|crash_?handler|CrashReport|Launcher|Helper|Updater|EasyAntiCheat|redist|webhelper|Lossless|wallpaper|vrserver|vrmonitor' } | Select-Object -First 1 -ExpandProperty Name" 2>/dev/null | tr -d '')"
  printf '%s' "$game" > "$CACHE.$$" && mv -f "$CACHE.$$" "$CACHE"
fi

if [ -n "$game" ]; then cap="$LO"; else cap="$HI"; fi
if [ "${1:-}" = "--why" ]; then
  if [ -n "$game" ]; then echo "$cap (game open: $game)"; else echo "$cap (no game open)"; fi
else
  echo "$cap"
fi
