#!/usr/bin/env bash
# tools/gates/clang_ci.sh - S4: the CI list on WSL clang (SOLID_PLAN 4.2).
#
#   tools/gates/clang_ci.sh [--copy NAME] [--out DIR] [--only-build] [--cmds FILE]
#
# Runs from Git Bash. Copies this checkout (whichever worktree the script lives in) into a WSL dir ~/<NAME>
# (default emb_solid_${GATES_NAME:-gates}; rsync, so ninja stays incremental), configures it like CI on first use
# (clang, Release, SDL_UNIX_CONSOLE_BUILD=ON, SDL from ~/sdl3-src = release-3.4.16 when present), builds rpg_test and
# save_test (holding ONE tools/slot.sh slot), then runs every command of tools/gates/ci_cmds.sh (the web.yml list) from
# the copy's root, CI_JOBS (default 5) side by side, each in its own slot (lib.sh ci_pool). A command that fails is rerun
# once alone (slot_alone: every slot held) before it counts; its first log stays as <out>/logs/clang_fail_<n>.log.
# Writes <out>/clang_build.log, <out>/clang_ci.log (one "ok|FAIL rc=N <cmd> <secs>s | <last line>" per command) and
# prints "clang build rc=N", one line per command and "CLANG_FAILS=N" last. Exit 0 only when the build and every
# command pass. Do NOT wrap the call in tools/slot.sh: it takes its own slots (and the alone rerun needs all of them).
# Environment: CLANG_JOBS (ninja -j, default 12), CI_JOBS (commands at once, default 5), WSL_DISTRO (default Ubuntu).
set -u
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
REPO="$HERE"; . "$HERE/tools/gates/lib.sh"
NAME="emb_solid_${GATES_NAME:-gates}"; OUT=""; ONLYB=0; CMDS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --copy) NAME="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --only-build) ONLYB=1; shift ;;
    --cmds) CMDS="$2"; shift 2 ;;
    *) echo "clang_ci: unknown arg $1" >&2; exit 2 ;;
  esac
done
case "$NAME" in emb|emb_judge|emb_dollc|"") echo "clang_ci: refusing to use shared WSL dir ~/$NAME" >&2; exit 2 ;; esac
OUT="${OUT:-$(mktemp -d "${TMPDIR:-/tmp}/clang_ci.XXXXXX")}"; mkdir -p "$OUT"
[ -n "$CMDS" ] || { CMDS="$OUT/ci_cmds.txt"; bash "$HERE/tools/gates/ci_cmds.sh" > "$CMDS" || exit 1; }
SRC_W=$(cygpath -m "$HERE")                      # C:/Users/...
SRC_L="/mnt/$(echo "${SRC_W:0:1}" | tr 'A-Z' 'a-z')${SRC_W:2}"
OUT_W=$(cygpath -m "$(cd "$OUT" && pwd)"); OUT_L="/mnt/$(echo "${OUT_W:0:1}" | tr 'A-Z' 'a-z')${OUT_W:2}"
CMDS_W=$(cygpath -m "$(cd "$(dirname "$CMDS")" && pwd)")/$(basename "$CMDS"); CMDS_L="/mnt/$(echo "${CMDS_W:0:1}" | tr 'A-Z' 'a-z')${CMDS_W:2}"
INNER="$OUT/clang_inner.sh"
cat > "$INNER" <<INNER_EOF
set -u
D=~/$NAME
mkdir -p "\$D"
rsync -a --delete --exclude='/build*' --exclude='/.git' --exclude='/.tmp' --exclude='/site*' "$SRC_L/" "\$D/" || { echo "clang build rc=98 (rsync)"; echo CLANG_FAILS=1; exit 1; }
cd "\$D"
SDLOPT=""; [ -f ~/sdl3-src/CMakeLists.txt ] && SDLOPT="-DFETCHCONTENT_SOURCE_DIR_SDL3=\$HOME/sdl3-src"
if [ ! -f build-native/build.ninja ]; then
  cmake -S . -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_UNIX_CONSOLE_BUILD=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \$SDLOPT > "$OUT_L/clang_build.log" 2>&1 || { echo "clang build rc=97 (configure)"; echo CLANG_FAILS=1; exit 1; }
fi
ninja -C build-native -j ${CLANG_JOBS:-12} rpg_test save_test >> "$OUT_L/clang_build.log" 2>&1; brc=\$?
echo "clang build rc=\$brc | \$(tail -n1 "$OUT_L/clang_build.log" | cut -c1-120)"
exit \$brc
INNER_EOF
INNER_W=$(cygpath -m "$INNER"); INNER_L="/mnt/$(echo "${INNER_W:0:1}" | tr 'A-Z' 'a-z')${INNER_W:2}"
# the build: one slot
MSYS_NO_PATHCONV=1 "$HERE/tools/slot.sh" wsl -d "${WSL_DISTRO:-Ubuntu}" -- bash "$INNER_L" < /dev/null 2>&1 | grep -vE "$SLOT_NOISE"
brc=${PIPESTATUS[0]}
[ "$brc" -eq 0 ] || { echo "CLANG_FAILS=1"; exit 1; }
[ $ONLYB -eq 1 ] && { echo CLANG_FAILS=0; exit 0; }
# the commands: CI_JOBS side by side, each in its own slot, failures rerun alone (lib.sh ci_pool)
ci_exec() {
  local c="$1" ln
  if [ "${c%% *}" = node ]; then ln="$c"; else ln="./build-native/$c"; fi
  MSYS_NO_PATHCONV=1 wsl -d "$CI_DISTRO" -- bash -c "cd ~/$CI_COPY && $ln" < /dev/null
}
export -f ci_exec; export CI_DISTRO="${WSL_DISTRO:-Ubuntu}" CI_COPY="$NAME"
REPO="$HERE"
ci_pool "$CMDS" "$OUT/logs" clang | tee "$OUT/clang_ci.log"
exit "${PIPESTATUS[0]}"
