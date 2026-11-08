# tools/gates/baseline: the gate records

How a batch moves a baseline, in five lines:
1. `CURRENT` names the root every gate reads (`119de74/` is wave 0's record; a root is never edited in place).
2. Each area lists what its change moves in `moves/<AREA>.txt`, one line per expected difference: `<gate> <item glob> <id> <reason>`.
3. At integration, ONE wave run: `tools/gates/wave_gates.sh --bdir <new dir> --out <run>`.
4. `tools/gates/rebase_baseline.sh --check-moves --run <run>` must PASS: every difference declared, every declared move seen.
5. `tools/gates/rebase_baseline.sh --from <old> --to <new> --run <run> --bdir <build>` writes `<new>/` (only the declared items re-recorded, `REASONS.md`, `REV`), flips `CURRENT`; commit both.

The roots, oldest first (each made from the one before it by `rebase_baseline.sh`, its moves files named in its
`REASONS.md`):
- `119de74/` wave 0's record (main re-recorded its view hashes for the hotfix ecdcb5f; the P3 roots never read it again);
- `p3/` the P3 batch's integration (moves in `moves/<AREA>.txt`);
- `p3f1/` the P3 fix round 1 (`moves_p3f1/FIX1.txt`);
- `p3f2/` the P3 batch merged with main's hotfix ecdcb5f (the creator preview is `Game::heroLook` as created;
  the T1 floor jerkin quilted from the wearer's shirt colour): 326 view-quick and 3607 view-all shots (3603 recorded; 4 sit in exclude.txt) with the hero or a
  companion in the floor jerkin, or a creator preview (`moves_p3f2/HOTFIX.txt`, reasons HOTFIX ecdcb5f).
- `p4/` the P4 batch (the W7 testers' first hour; `moves_p4/<AREA>.txt`, 1090 items from 274 moves): TALK's
  one dialogue order and pin (S2 bot lines, S3.save sizes, S10.digest and W6.digest_clang), ART's leather gambeson and
  body icons (S1 37 exes, S5.rig2, ART_TOTAL_RIG2, S6.pool, W4.doll), and view shots from ECONOMY, TALK, ART and VIEW.
- `p4f1/` the P4 fix round (`moves_p4f1/FIX1.txt`, 331 items from 81 moves, judge P4 r1): the menu key hint's
  own band at the panel foot (F1-6b), the DANGEROUS GROUND banner in three lines (F1-6a, `p4_view_long_banner.png` new),
  the pin moved only by the player's own take (F1-2) and its QUEST STARTED banner (F1-QS): 52 view-quick and 279 view-all
  shots; every other record is `p4`'s.
- `p5b3/` P5, P5b, P5b2 and P5b3 together (the signed peoples T1-T6, decision 8 reading 1; REV 9ab6bdc, the P5b3
  integration of GRIPS, DUNE and FINISH): 22,006 items from 109 moves, the union of `moves_p5/`, `moves_p5b/`, `moves_p5b2/` and
  `moves_p5b3/` (each file prefixed by its dir in REASONS.md). W6.perf was not re-recorded (perf stays `p4f1`'s: the machine's
  frame.shot_med was slow that night for a P4-era build too); `gates_parity` came from its alone rerun (B10 timing under load).
- `p5b4/` **CURRENT**: the owner's 2026-11-07 ruling, everyday armour is T3-T4 (STYLE_P5B 2.5, decision 8; REV 7665547):
  302 items from 31 moves of `moves_p5b4/T3T4.txt`, the view shots (271 view-all, 31 view-quick) where a capital watchman lifted
  to T4 in band-4+ land now wears his realm's first alloy in his people's signature; every other record is `p5b3`'s.

A root holds `gates/` (the step and doll records, `expect.txt`, `unstable.txt`), `probes/` (sim digests, PNG and WAV
hashes, perf) and `checks/` (layer allowlist, size report); a moved root adds `REV` (its git revision) and
`REASONS.md`. The gate names and item forms a moves line can use are listed in `rebase_baseline.sh --help`;
`rebase_baseline.sh --selftest` proves the mechanism in a throwaway copy. `unstable.txt` and the layer allowlist may
only shrink; never loosen a gate to make a move pass.

Load-sensitive lines (P4 7c) are not records: `tools/gates/alone_rerun.txt` names them (`<gate> <regex>  # why`). A
listed line that FAILs or differs under load is rerun alone with the same limits (`tools/gates/lib.sh` `alone_try`); a
line that PASSed on that rerun says "(alone rerun: ...)" and `rebase_baseline.sh` treats its gate as matching the record
(its first try's text is kept as `logs/<gate>.first.log`, never recorded). The list is no place to hide a real change:
any unlisted difference on the same line blocks the rerun.
