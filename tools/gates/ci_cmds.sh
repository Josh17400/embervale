#!/usr/bin/env bash
# tools/gates/ci_cmds.sh - the CI test list, extracted from .github/workflows/web.yml (never hand-kept).
#
#   tools/gates/ci_cmds.sh [WORKFLOW]      prints one command per line, e.g. "rpg_test --seeds 1..20"
#
# Every `./build-native/<exe> <args>` in the workflow, with the `./build-native/` prefix dropped, trailing blanks
# trimmed and comments cut. Lines that use a shell variable (the `for s in 7 99 12345` loop) are left out, as the
# harness has always done (SOLID_PLAN 4.1: "the 68 commands"). Exit 1 when the list is empty.
set -u
cd "$(dirname "$0")/../.."
WF="${1:-.github/workflows/web.yml}"
[ -f "$WF" ] || { echo "ci_cmds: $WF not found" >&2; exit 1; }
out=$(grep -oE '\./build-native/[a-z_]+( [^#]*)?' "$WF" | sed 's#\./build-native/##' | sed 's/ *$//' | grep -vF '$')
[ -n "$out" ] || { echo "ci_cmds: no commands in $WF" >&2; exit 1; }
printf '%s\n' "$out"
