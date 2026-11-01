# tools/gates/baseline: the gate records

How a batch moves a baseline, in five lines:
1. `CURRENT` names the root every gate reads (`119de74/` is wave 0's record; a root is never edited in place).
2. Each area lists what its change moves in `moves/<AREA>.txt`, one line per expected difference: `<gate> <item glob> <id> <reason>`.
3. At integration, ONE wave run: `tools/gates/wave_gates.sh --bdir <new dir> --out <run>`.
4. `tools/gates/rebase_baseline.sh --check-moves --run <run>` must PASS: every difference declared, every declared move seen.
5. `tools/gates/rebase_baseline.sh --from <old> --to <new> --run <run> --bdir <build>` writes `<new>/` (only the declared items re-recorded, `REASONS.md`, `REV`), flips `CURRENT`; commit both.

A root holds `gates/` (the step and doll records, `expect.txt`, `unstable.txt`), `probes/` (sim digests, PNG and WAV
hashes, perf) and `checks/` (layer allowlist, size report); a moved root adds `REV` (its git revision) and
`REASONS.md`. The gate names and item forms a moves line can use are listed in `rebase_baseline.sh --help`;
`rebase_baseline.sh --selftest` proves the mechanism in a throwaway copy. `unstable.txt` and the layer allowlist may
only shrink; never loosen a gate to make a move pass.
