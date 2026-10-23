# P2B_MOTION_ROLLOUT: the shipped side walk and run on everything rig 2 draws

Planner, 2026-10-22. Branch p2b-motion, checkpoint f47c3af + RUN FIX i3 in the tree (pool 7b11cfbac641b82b). No
state-changing git in this batch (no commit, push, reset, checkout or stash). Binding with docs/MOTION_P2.md (the method,
rewritten today) and docs/STYLE_P2.md section 10 (the shipped-method ruling at its top). Inventory renders, the looks file,
the B10 probe and the sheet tools are in `%TEMP%\claude\emb_P2b\rollout\` (inventory\, tools\).

Rig 1 must not move: build/art_hash.exe TOTAL 317eef6efe4d8229, build/city_gallery.exe --village-art-check 65/0,
build/rpg_test --anim 0 failures.

---

## 1. Vision

The adult man and woman now walk and run the way the owner asked for: short, chunky legs that are real limbs in every frame,
a far leg as whole as the near one, one boot tilting through the stride, a planted foot that never slides, a leaning run with
pumping arms and a real moment in the air. This batch makes that true for **everyone** rig 2 draws moving sideways: the
youth and the child at their own scale, the elders, every garment (torsos, hems, tails, skirts, stride panels, sleeves),
the mail, cloaks, neckwear, hats and hair, the scabbard and belt tools, the staff and the walking stick, and the walking
postures. When it is done, a crowd crossing the square reads as one family of movers: the robed scholar's shoes peek from
under a hem that flies with his run, the traveller's cloak streams off a leaning back instead of sitting on it like a
pack, the old man's stick stays planted while he walks over it, the youth sprints with the same lean and pump as the adult,
and nothing silently vanishes, floats or crosses a face. A new C++ gate (B11 limbs, B11 wear, B12 boots) measures the method
on every look, so CI keeps it true after we leave.

## 2. In and out

**IN**
- The side (R, L) walk, run, drawn walk and drawn run of every rig-2 body: man, woman, man.elder, woman.elder, youth boy and
  girl, child boy and girl (walk), every people (human, half, elf) and build (average, slim, broad).
- Every part that draws in those frames: garment torsos (`run`, `runl`), hangs (`wide`, `rest`, `flare`), stride panels,
  sleeves and drapes, mail, shoe tips, cloaks and cloak fronts, neckwear, headwear, hair hangs, sheaths and belt tools,
  held items (sword guard, staff, stick, ledger, scroll), and the side frames of the walking postures (Carry x3, Play,
  Dance; the child's Play and Carry).
- New parts: kin/youth/legs/far.pxm, kin/child/legs/far.pxm; the youth's arm pump keys and sheared run torso views; one new
  clip key, `hair=run` (section 6.2).
- The gate rewrite: B11 limbs and B12 boots ported from .tmp/judge/proto2/check.py into rpg_test, B11 wear added, a look
  matrix that covers every part, a freeze manifest, B10 sweeps that include cloaks.
- docs: MOTION_P2.md (done, the method), STYLE_P2 section 10 (done, the numbers), this plan; at integration the results
  table here, MOTION_P2 section 12's kin numbers and the docs/PLAN.md log entry.

**OUT**
- Front and back (D, U) walks and runs, idles, hurt, swing and action keys, deaths, seated / kneeling / riding postures
  (frozen; the freeze manifest proves it).
- The adult walk and run legs and arms as drawn (frozen, owner-approved) except the listed polish (ADULT-RIG).
- Any change to the doll's code (doll.h, doll_*.cpp but doll_ramps.cpp), pxc.mjs, anim.cpp, render code, rig 1.
- Distance-keying the walking postures (Carry, Play, Dance step by time at 9 fps in render_anim.cpp, shared with rig 1):
  their side frames get every leg rule but not registration. A later rig-2-only code task if the owner wants it.
- A deeper run lean than 4 px (owner decision; section 10).
- P2c's new armour forms, helms, shields and weapons (they inherit this method and these gates when they arrive).

## 3. Inventory (what already follows the method, what does not)

Rendered from the tree pool (74 looks, both facings, `rpg_test --doll --pool tree.pool --limbs`), viewed at 3x-10x; B10 by
`doll_gallery stats` and `rpg_test --doll --gates --report`. Sheets: `%TEMP%\claude\emb_P2b\rollout\inventory\*.png`.

| # | what | state | evidence | area |
|---|---|---|---|---|
| 1 | adult legs walk + run (std, far), man and woman, elves, builds | FOLLOWS (the reference) | per_frame.txt 0/1480 on check.py | ADULT-RIG (polish only) |
| 2 | adult shoe tips under long garments (tips.pxm) | MISFIT in the run: follow proto6 i2, not RUN FIX i1-i3; in the flight no toe shows, so robes, gowns, kaftans, wraps and the dress float as a bell | run_m1, run_m2, run_w1, run_w2 | ADULT-RIG |
| 3 | elders (man.elder, woman.elder) walk | FOLLOWS (adult legs, lean 1) | walk_c (m_elder), walk_b (w_elder) | (verify in matrix) |
| 4 | drawn walk / run | legs FOLLOW; the off hand does not pump in the drawn run (hidden on contact and push R; old o* keys L) | drun, drunL | ADULT-RIG |
| 5 | far fist behind the back on pB | reads as a cuff block, worst on slim builds | run_slim10c | ADULT-RIG (hand pixels only) |
| 6 | youth legs walk + run | OLD METHOD: one-part legs from the round-4 tables, no far part, shuffling stride, run reads as a fast walk, lean 1, bob 1, arms runB / N / runF | run_y, walk_y | KIN-YOUTH |
| 7 | youth garments | rigid torsos (`rest` only, no run / runl), hangs drawn for the old legs | kin/youth/wear | KIN-YOUTH |
| 8 | child legs walk | OLD METHOD: one part, the pass frames hop up a pixel (bob -1), shuffle | walk_c | KIN-CHILD |
| 9 | child skirts, garments, Play and Carry | on the old walk keys | kin_child.clip | KIN-CHILD |
| 10 | adult torsos `run` (walk) views | FOLLOW for tunic / trousers (HIPLINE inside); other garments never measured (check.py judges 3 looks) | walk_g, walk_stride1-2 | MAN-WEAR, WOMAN-WEAR |
| 11 | adult torsos `runl` (run) views | generated by a 1-2 px row shear (proto6 gen_lean.py) for every garment and the mail: reads as a lean at 3x, but folds, belts and patterns are not drawn for it; the lean reads slight | run_m1-m3, run_w1-w2 | MAN-WEAR, WOMAN-WEAR |
| 12 | sleeves on the arm channels | MISFIT: the man's gown_sleeve and robe_sleeve lack pB pM pF pK pN; mail_sleeve (both bodies) has only the walk keys, so it lacks every run and drawn-weapon key (pB..pN, runB, runF, guard, the o* keys). A part without the named key draws nothing (doll_compose.cpp), and coverage counts the channel as covered because the arm has the key: in the run the robe's wide sleeve vanishes, and a mail wearer's sleeves vanish in the run and in every drawn frame (a bare tunic sleeve shows) | run_m1 (robe), run_m3 + runL_g2 (mail), drun (m_mail guard arm), run_w2 (w_mail) | MAN-WEAR, WOMAN-WEAR |
| 13 | the woman's sleeves and poncho drapes | have the pump keys (proto6) but were drawn before i3 re-timed the pump and redrew pB | wear/woman/*_sleeve_*, poncho_drape_* | WOMAN-WEAR (verify) |
| 14 | hang flares | the apron's flap flies forward and reads as a pouch in front of the waist (L) and a tool sticking out (R); coat, kilt, smock, rags tails trail at 3x | apronL10c, run_m3 | MAN-WEAR, WOMAN-WEAR |
| 15 | stride panels (`*_stride`, legs channel) | draw only on walkB2 / walkD2, drawn for the old pass; those keys are now the reach | walk_stride1-2 | MAN-WEAR, WOMAN-WEAR |
| 16 | the man's coat_stride | draws in the idle (buckle, guard, idle, lunge, stance) and vanishes in every walk and run frame (intent never stated) | wear/man/coat_stride.pxm | MAN-WEAR |
| 17 | long skirts in the walk | the hem stands still while the legs work (glides) | walk_g (m_robe, w_robe, woman) | MAN-WEAR, WOMAN-WEAR (depth) |
| 18 | cloaks in the run | MISFIT: the `flare` view (drawn for the pre-lean run) is a short block behind the shoulders that reads as a backpack; the fur mantle's cape floats behind the arm | run_m4, run_w4, runL_g | OVER-CARRY |
| 19 | cloaks and B10 | OVER BUDGET today, and the B10 sweeps sample no cloak: travel or fur cloak + hood + staff man RUN 64.4 KB; noble cloak + hood + staff elf man LOCO 128.4 / RUN 66.2; travel cloak + hood + stick elf man RUN 66.9 (budgets 128 / 64) | inventory\b10_cloak_probe.txt | OVER-CARRY, GATES-WEAR |
| 20 | headwear, hoods | ride the head with the lean | run_b (m_hood) | OVER-CARRY (verify all 10) |
| 21 | neckwear | kerchief and scarf flow at 3x; the woman's scarf and kerchief ride the hang channel's flare | run_w4 (w_scarf) | OVER-CARRY (verify) |
| 22 | the braid | hangs down her back in the run, on her chest in idle and walk: it switches sides at the gait change | run_w1 | ADULT-RIG (decision 10.2) |
| 23 | twin braids (and likely pigtails) | MISFIT: a braid tip dangles in front of the chest and hands in the run, both facings | run_w4, runL_g | ADULT-RIG |
| 24 | other long hair (loose, wavy, locs, ponytails) | hangs behind with the lean; no streaming in the run (no run key) | run_w3 | ADULT-RIG (depth) |
| 25 | the walking stick (legs channel) | MISFIT: drawn for the old 3 / 3 / 3 walk (now 3 / 4 / 4, the planted tip skates a pixel) and the old run arms: in the run it is not in the fist, it trails or plants like a cane | walk_m4, run_m4 | OVER-CARRY |
| 26 | the run staff (staff_run, staff_runl) | jumps behind the head and back every half cycle; crosses the woman's face on run5 | run_m4, run_w4 | OVER-CARRY |
| 27 | scabbard and belt tools (sheath channel) | ride kick / rest behind the legs at 3x | walk_g, drun | OVER-CARRY (verify) |
| 28 | the drawn sword (held guard) | rides the guard arm; the blade passes close to the face in L | drunL | OVER-CARRY (verify) |
| 29 | walking postures | Carry's side frames show the new leg keys under torso=rest (the hem a row low over the thighs); Play / Dance reuse run / walk keys; time-keyed | carry_R_12x | OVER-CARRY |
| 30 | gates | B11 limbs (R L) 400 failures and B12 68: the superseded table checks; B11 covers 9 looks, check.py 3; nothing measures garments, sleeves, cloaks, held items; coverage counts a channel as covered when ANY part has the key, so silent vanishing passes | gates_report_f47c3af_runi3.txt | GATES-LIMBS, GATES-WEAR |

## 4. Constants that interact

| constant | value | consequence |
|---|---|---|
| walk | 22 px cycle, holds 3 / 4 / 4, bob +1 / 0 / 0 | at 40 px/s frames of 75 / 100 / 100 ms; the stick's planted tip must step back 3 / 4 / 4 |
| run | 36 px cycle, holds 6 / 6 / 6, bob 0 / -1 / -2, lean 4, hip 2 (woman 3) | at 74 px/s 81 ms a frame; hems and hangs rise 0 / 1 / 2 rows; the lean moves the hem span to u -3..+7 |
| RUN budget | 64 KB per look; worst civil look 63.4 (staff + hood man), armed 60.6 (elf man); cloaked looks up to 66.9 | 24 frames x ~2.6 KB: one extra column on the 12 side frames costs ~1.7 KB, so cloaks, staffs, hair and the lean may not widen a near-budget look's side run boxes at all; the over-budget cloak looks must lose 0.4-2.9 KB |
| LOCO budget | 128 KB; worst 124.0 (staff + hood man), cloaked 128.4 | the walk's cloak `wide` views and stick drawings may not widen the worst looks' walk boxes |
| youth budgets | RUN 41.9, LOCO 71.3 KB today | a 3 px lean (~+4 KB) and a far leg fit easily |
| pool | 750,832 bytes of 1.0 MB | the additions (two far parts ~9 KB, sleeve pump views ~15 KB, hair run views ~6 KB, youth torso views ~10 KB, empty views) ~45 KB: 0.80 MB |
| idle leg (adult) | 10 rows, 8.0 px along the bones, 5 px band, boot 20 px / 3.0 | the thresholds below are check.py's |
| youth leg | 8 rows (hip F-8), 4 px band, 4 px shaft | walk ~9 px a step (3 / 3 / 3, 18 px), run ~15 a half (5 / 5 / 5, 30 px): stride scales with leg length (adult 11 / 18 at 10 rows); lean ~3 px (adult 4 px on 33 px = 12 %) |
| child leg | 5 rows (hip F-5): 2 trouser rows, 3 boot rows, sole; 4 px band | walk ~6 px a step (2 / 2 / 2, 12 px); no run |
| gate runtime | about 200 looks, 6,000-8,000 side frames | must add 60 s or less to `rpg_test --doll --gates` (MSVC Release); CI runs it on clang |

## 5. The gate rewrite (GATES-LIMBS and GATES-WEAR; tools/tests)

### 5.1 What stays, what goes
- **Stay unchanged:** B1 parity, B2, faces, hands, B8, B9, B10 (budgets), ramps, B11 leg mass (D U), the doll contract.
- **Go:** B11 limbs checks 5 (down frames: the walk has none now), 9 (envelope table), 10 (stance table), 12 (feet on the
  table), 13 (phase shapes / targets.json), the synth.py / `--limbs-synth` path and the old B11 selftest cases built on them;
  B12's sole-run table (K1 as runs 3, 2, 2 and the like) gives way to check.py's boot measures.
- **Kept and re-homed into the new B11 limbs:** leg area >= 89.5 % of the idle (1), hip row width (2), no sliver (3),
  continuity and joints (6, 6b), ground (7), the default hero's trouser luma (11). "Hip row within 1 of contact" (4)
  becomes SQUAT (the run's flight rises 2 px by design).
- **New:** check.py's checks in C++, B11 wear, B12 boots rebuilt on check.py's boot measures, the look matrix, the freeze
  manifest, cloak-aware B10 sweeps.

### 5.2 B11 limbs (per side frame; near and far leg each composed ALONE on its own layer, probe ramps)
The meaning is check.py's (its header is the reference text); the C++ must give the same verdict on its three looks.

| check | per frame | adult | youth | child |
|---|---|---|---|---|
| FARWIDTH | the far leg as drawn: every shaft row below the seat >= the idle leg's width and >= the near leg's row where both bones are upright; any far run beside the near leg >= 3 px | idle 5 | own idle | own idle |
| LEGLEN | each leg hip > knee (K centroid) > ankle joint along the bones | idle +1 / -1.5 | same | same |
| THICK | thigh and shin across the bone (rows / sqrt(1 + slope^2); level bones by columns) | 3.8 | 3.8 | 3.3 |
| ANKLE | shin rows knee > boot top | >= 4 px, >= 3.5 across | same | same |
| HEAD | headX 0; head shift minus neckwear shift 0; no clear pixel between head and body in the idle's neck columns | exact | exact | exact |
| HIPLINE | no leg pixel outside the hem span from the hem down 3 rows (the push: 1 px a row along the bone) | exact | exact | exact |
| BUNCH | run contact and push: boots apart or outlined; the far boot >= 60 % shown | exact | exact | (no run) |
| REG | at each frame change inside a stance, the planted contact within 1 px (heel while down in both, else toe) | 1 px | 1 px | 1 px |
| BOB | the clip's bob range over a cycle | walk 1, run 2 | same | walk 1 |
| RHYTHM | holds spread (plant0 - plant1) | run 2 px, walk 1 px | same | walk 1 |
| FLIGHT | run flight: both soles >= 2 rows up; the trailing knee bend >= 40 deg | exact | set by the KIN slice | - |
| PUSH | run push: the support leg hip > ankle >= 10 deg back from vertical | exact | set by the slice | - |
| FOLD | run flight trailing leg: nothing outside the coat to hem + 2 rows; a knee >= 25 deg (joints, or the far-end bend when the rows agree) | exact | set by the slice | - |
| POST | run: a support / lead leg vertical within 4 deg and bent < 15 deg in two adjacent frames fails; flight lead knee >= 25 deg | exact | set by the slice | - |
| AREA, HIPW, SLIVER | visible leg area >= 89.5 % of the same facing's idle (skirt area for long garments); hip row <= 1 px narrower; no visible run < 3 px without a boot pixel | exact | exact | exact |
| SQUAT | bob <= +1 in every frame | exact | exact | exact |
| JOINTS, GROUND | each leg 4-connected, no empty row, boot-shin-thigh-seat joined; a sole on row F in every walk frame and run contact / push, none in flight | exact | exact | exact |
| LUMA | the default hero's trouser body (Cloth2 step 1) luma >= 52 | once | - | - |

The kin thresholds marked "set by the slice" start at the adult values; KIN-YOUTH proposes changes with 8x evidence, the
judge rules, GATES-WEAR records them in MOTION_P2 section 12 (design by eye first, then the checks agree).

### 5.3 B11 wear (new; per side frame of every matrix look)
| check | meaning | bar (proposed; calibrated on the eye-approved art) |
|---|---|---|
| W1 no silent vanishing | a part that draws in the look's side idle has a view (an empty one declares intent) for every key its channel takes in every side walk, run, drawn idle, drawn walk and drawn run frame of the look (`-` excepted); parts that never draw in the idle (stride panels, the drawn sword) are exempt | exact |
| W2 sleeves on arms | sleeve and drape parts on arm channels lie within the arm's silhouette dilated by 3 px and overlap its U / F pixels | 3 px |
| W3 hangs trail | in run frames no hang, cloak or scarf pixel more than 1 px forward of the torso layer's front edge in the same rows | 1 px |
| W4 torso leans | in run frames the torso layer's top rows shift by the lean (+/-1) and its hem rows by the hip offset (+/-1) against the idle | 1 px |
| W5 attached | every 8-connected piece of a hang, cloak, neckwear or held layer touches the body (torso, legs, arms, head) | exact |
| W6 in hand | held parts not on a grip (the stick, the run staff) touch the holding hand's H pixels | 1 px |
| W7 face clear | no held, sheath, belt or cloak-front pixel over a face or eye pixel | exact |
| W8 the box | cloak, neckwear and hair-hang pixels extend the frame's box beyond the body's own box (legs, torso, arms, head, garment) | <= 1 column, 0 rows |
| W9 hair behind | in run frames no hair-hang pixel below the chin lies forward of the neck anchor + 2 | exact |
| W10 belt clear | sheath-channel items cover no boot pixel and at most 1 column of a leg below the knee row | exact |
| T1 tips shown | long-garment looks: every foot within 3 rows of the ground shows >= 2 rows of shoe below the hem; in flight both feet show >= 1 row | exact |
| T2 tips registered | the planted shoe tip registers as REG | 1 px |

### 5.4 B12 boots
BOOT (each boot as drawn on its own leg layer: pixels within 10 % of the body's idle near boot, thickness perpendicular to the
sole >= idle - 0.5, the far boot's visible pieces none smaller than 2 x 3), BOOTTILT (long axis within 60 deg of the idle's,
thickness >= idle - 0.3), BOOTSIZE (the two boots within 10 % of the larger), LIBRARY (every side-loco legs part of every
body, std / far / tips, tagged `boots-stamped`, with a view for every side walk / run key its clips name).

### 5.5 The look matrix (every look, made tractable)
Built in code from the pool's role vocabulary plus a fixed reference list, so a new part enters automatically. Families:
M1 reference (hero, defhero, woman (dress), womant, womanb, elfm, elfw, halfm, broadm, slimw, elderm, elderw, hdrawn, wdrawn);
M2 every garment on every body that wears it, walk + run (child walk), adults also slim and broad; mail with the sword
(walk, run, dwalk, drun); M3 cloaks x man / woman (walk, run, drawn run), headwear x man (+ woman veil and headscarf, + youth
hood and cap), neckwear x every body; M4 every hanging hair style on woman, man, youth girl (+ child hangs); M5 sword
sheathed and drawn (man, woman, elf), axe, bow, hammer, horn (civil), staff civil and player, stick (man, woman, elder),
ledger, scroll; M6 postures Carry 0-2, Play, Dance (man, woman), child Play and Carry (leg checks only: no REG, BOB,
RHYTHM); M7 the B10 watch list (cloak x hood x staff / stick / sword x human / elf x man / woman; atlas only).
**Part coverage**: every part with an R or L walk, run, pump, flare, run or runl view is drawn in at least one judged frame;
an unjudged part FAILS.

### 5.6 Selftest, parity, freeze
- **Selftest** (`--doll --motion --selftest`, also inside `--gates`): port check.py `--synth` (a synthetic good look passes;
  each synthetic failure fails exactly its own check, both facings) at adult, youth and child scale, plus a synthetic for each
  wear check (a deleted sleeve view, a hang shifted 3 px forward, a staff pixel over an eye, a stick 3 px off the hand, a
  cloak 2 columns past the box, a braid on the chest). Prints ALL CAUGHT.
- **Parity**: `.tmp/judge/rollout/parity.py` runs check.py `--dir` and `--doll --motion --report` on hero, womant and
  defhero of the same pool: 0 disagreements per frame and check, measures within 0.1.
- **Freeze manifest**: `--doll --freeze-write FILE` hashes, per matrix look, every frame outside the side walk / run / drawn
  frames and the walking postures' side frames (idles, D / U walks and runs, hurt, actions, deaths, other postures); written
  from the checkpoint pool (`--pool %TEMP%\claude\emb_P2b\rollout\checkpoint\checkpoint_7b11cfbac641b82b.pool`; its maps and
  clips beside it in checkpoint\doll\) to `.tmp/judge/rollout/freeze_f47c3af.txt`, so it never depends on when areas deliver;
  `--freeze-check FILE` at integration: 0 changed.
- **B10 sweeps**: add the M7 watch list to the armed and civil sweeps.

### 5.7 Command line and report (fixed so the artists can script against it)
- `rpg_test --doll --gates`: everything, as CI runs it (lines `B11 limbs (R L)`, `B11 wear (R L)`, `B12 boots`).
- `rpg_test --doll --motion [--pool FILE] [--limbs-looks FILE | --matrix FILTER] [--report] [--frames DIR] [--selftest]`:
  only B11 limbs, B11 wear and B12 on the given looks (default the matrix); exit 0 iff 0 failures. `--report` prints one line
  per frame in per_frame.txt's format plus `FAIL <check> <look> <gait> <F><i>: <why>` and an ASCII map of the failing legs;
  `--frames DIR` writes per look a 4x strip (idle, walk 0-5, run 0-5, both facings, on grass) for the eye.
- Looks files keep the `tag who spec [gaits]` format; gaits add `carry carry1 carry2 play dance`; spec adds `hood`,
  `jewellery`, `amulet`, `beardStyle`, `head`, `helmStyle` (whatever the matrix needs).
- Published for the artists (they never build): `%TEMP%\claude\emb_P2b\rollout\bin\rpg_test_m<N>.exe` and the G0 dumper
  `proto2_dump.exe`; `LATEST.txt` names the newest. Static binaries; run them from the repo root.

### 5.8 The code contract between GATES-LIMBS and GATES-WEAR (fixed now, so both start at once)
`tools/tests/doll_motion.h` (GATES-LIMBS writes it in its first hour, exactly this shape; GATES-WEAR asks for changes):
```cpp
namespace dollmotion {
struct Look { std::string tag; int who = 0; std::string spec; std::vector<std::string> gaits; };  // walk run dwalk drun carry carry1 carry2 play dance
struct Img { int w = 0, h = 0, px = 0, py = 0; std::vector<uint32_t> c; };   // pivot at (px, py); probe codes or RGBA
struct Frame {
  std::string gait; int facing = 2, i = 0;                        // facing 2 R, 3 L
  int plant0, plant1, bob, lean, hipX, hipY, headX, headY;         // the clip frame's moves (kNoPlant when none)
  std::map<std::string, std::string> keys;                         // channel -> key
  std::vector<std::pair<std::string, Img>> parts;                  // each part of the recipe ALONE on its layer, probe codes
  Img raw, fin;                                                    // the whole figure: probe codes; finished in the look's ramps
};
struct LookFrames { Look look; art::doll::DollRecipe recipe; std::vector<Frame> idle, frames; };   // idle: R then L
struct Fail { std::string check, where, why; };
std::vector<Look> readLooks(const char* file);                         // GATES-LIMBS (the existing format, new gaits and fields)
LookFrames shoot(const Look& l, const char* poolFile);                // GATES-LIMBS
void limbsChecks(const LookFrames& f, std::vector<Fail>& out, bool report);   // GATES-LIMBS: B11 limbs, B12 boots
int  limbsSelftest(bool report);                                      // GATES-LIMBS
std::vector<Look> matrixLooks(const std::string& filter);             // GATES-WEAR: section 5.5, part coverage
void wearChecks(const LookFrames& f, std::vector<Fail>& out, bool report);    // GATES-WEAR: B11 wear, T1, T2
int  wearSelftest(bool report);                                       // GATES-WEAR
int  freezeCmd(const char* file, bool write);                         // GATES-WEAR
void b10Watch(std::vector<Fail>& out, bool report);                   // GATES-WEAR: the M7 sweep (called by test_doll.cpp's B10)
void writeStrips(const LookFrames& f, const char* dir);               // GATES-WEAR: --frames
}
```
GATES-WEAR lands stubs for its functions in its first hour so the tree always links; test_doll.cpp (GATES-LIMBS) calls
them from `--doll --gates` and `--doll --motion`. Both engineers keep every tools/tests file compiling in the tree at all
times (the CMake glob builds both files in both build dirs).

## 6. Contracts

### 6.1 Frozen (no area edits; a needed change is a re-plan)
rpg/art/doll/doll.h, doll_compose/fit/pool/resolve/walk.cpp, doll_internal.h, tools/doll/pxc.mjs; maps/legs/adult/pose/**,
maps/arms/adult/pose/**, maps/head/**, maps/beard/**, maps/fx/**, maps/legend/**; clips/death.clip, clips/actions/**; rig 1
(rpg/art/art_*.cpp/h, rpg/sim/appearance.h) and all game code; .tmp/judge/proto2/check.py (the oracle: read-only).
Frozen inside owned files: every idle view; every D / U view; the adult walk and run leg drawings (std, far: ADULT-RIG may
touch only the listed polish); the arm drawings B N F back pB pM pF pK pN runB runF guard o* (hand pixels only, inside the
silhouette); every number in loco.clip's side walk and run lines (plants, bob, lean, hip, holds, arm keys) except the
listed additions; any `rest` view a hang shares with the idle (ask for a key instead of editing it).

### 6.2 Keys
Names are frozen: legs idle walkA walkA2 walkB2 walkC walkC2 walkD2 run1-run6 (+ D / U walkB walkD, stance, guard, sit...);
arms as above; torso rest idle swing run runl lie; hang rest wide flare sit floor kneel ride lie idle; hair rest swing lie;
braid rest swing run lie; sheath rest kick lag sit stride; neck rest flow flow2 lie; tuft down. **One new key: `hair=run`**,
named by loco.clip's side run frames (both bodies, sheathed and drawn) once every adult hair-hang part has R / L `run` views
(empty where a style needs none); kin_youth.clip may name it after ADULT-RIG delivers. Any other new key needs the clip's
owner and the owners of every part on that channel to agree (announced in their reports); W1 catches any part that lacks it.

### 6.3 Shared files and their single owners
loco.clip: ADULT-RIG. postures.clip: OVER-CARRY. kin_youth.clip: KIN-YOUTH. kin_child.clip: KIN-CHILD. doll_ramps.cpp and
gen/doll_pool.cpp: GATES-WEAR, the integrator (the pool only at integration; ramps only if a gate demands it, rig 2
only). tools/tests/test_doll.cpp and doll_motion.h: GATES-LIMBS; test_doll_wear.cpp: GATES-WEAR. The adult hair and
headwear also serve the youth (one head box): ADULT-RIG and OVER-CARRY own them, KIN-YOUTH reviews them on the youth.

### 6.4 How an artist works (preview, measure, deliver)
1. Root: `%TEMP%\claude\emb_P2b\rollout\<area>\doll\` = copies of rpg/art/doll/maps and clips; edit only your files there.
2. Compile: `node tools/doll/pxc.mjs --root <root>\doll --no-cpp --bin <root>\x.pool` (0 errors in your maps).
3. Dump: `build-proto/rpg_test.exe --doll --pool <root>\x.pool --limbs <out> --limbs-looks <looks.txt>` (or the newest
   rollout\bin exe); strips: `python -I %TEMP%\claude\emb_P2b\rollout\tools\strip.py <out> s.png 5 R run tag...` (sheet.py
   for many looks); motion at game speed: proto2\work3\render3.py and preview.py, proto7\run_i3\w\render7.py (copy them and
   point them at your dump).
4. Measure: until G1 lands, `rollout\bin\proto2_dump.exe <out> --pool x.pool --looks looks.txt` then
   `python -I .tmp/judge/proto2/check.py --dir <out> --looks <tags>` (adult looks); from G1, `rollout\bin\rpg_test_m<N>.exe
   --doll --motion --pool x.pool --limbs-looks looks.txt --report`.
5. Eye: hip-to-boot trace in every frame at 8x; 3x motion in both facings beside the adult reference (proto7\run_i3).
6. Deliver: copy your files into the tree, re-copy a fresh root from the tree (everyone's current files), compile and
   measure again; write `<root>\report.txt` (what changed, measures, strips, open items). Never write gen/doll_pool.cpp, never
   build into build/ or another area's build dir, never edit another area's file; validate a clip with pxc before saving it
   into the tree (a clip error stops every area's compile).

## 7. Areas (8; disjoint files; one owner each)

### 7.1 GATES-LIMBS (engineer; BDIR build-gates)
**Files:** tools/tests/test_doll.cpp, tools/tests/test_doll_motion.cpp (new), tools/tests/doll_motion.h (new),
.tmp/judge/proto2/dump.cpp and build_dump.bat (options only; check.py read-only), .tmp/judge/rollout/parity.py (new).
**Brief:** port check.py into rpg_test as B11 limbs and B12 boots at parity, retire the superseded table checks, own the
`--doll --motion` command, and give the artists a measuring tool on isolated pools in the first hour.
- core: **G0** (first hour): dump.cpp gains `--pool FILE` and `--looks FILE`; publish rollout\bin\proto2_dump.exe so check.py
  judges any adult look on an isolated pool; doll_motion.h exactly as 5.8. **G1**: test_doll_motion.cpp with every 5.2 and
  5.4 check (adult constants, the kin and tips parameters as a per-body table), `shoot()` composing each part alone, the
  superseded table checks and the synth.py path removed from test_doll.cpp, `--doll --motion` and the gate lines wired,
  `limbsSelftest` (check.py `--synth` ported, adult, youth and child scale), parity.py at 0 disagreements; publish
  rollout\bin\rpg_test_m1.exe.
- depth: intermediate dumps (skeleton, joints, bone rows) per frame behind `--report` for diffing against check.py; the ASCII
  leg map on failing frames; a per-check timing line.
- delight: `--motion --report` prints each look's per_frame.txt block in check.py's exact words, so old and new diff cleanly.
**Done (measured):** parity.py 0 disagreements (every frame, every check; measures within 0.1) on hero, womant and defhero of
the tree pool; `limbsSelftest` ALL CAUGHT (every synthetic failure fails exactly its own check, both facings, three scales);
on the checkpoint tree the trousered adult reference looks (hero, defhero, womant, womanb, elfm, elfw, halfm, broadm, slimw,
elderm, elderw, hdrawn, wdrawn) pass B11 limbs and B12 with 0 failures and the kin fail on the old legs; m1 published within
the first half of wave 1.

### 7.2 GATES-WEAR (engineer; BDIR build-wear; the integrator)
**Files:** tools/tests/test_doll_wear.cpp (new), rpg/art/doll/gen/doll_pool.cpp (integration only),
rpg/art/doll/doll_ramps.cpp (only if a gate demands; rig 2 only), .tmp/judge/rollout/** except parity.py,
.github/workflows/web.yml (comments), docs/PLAN.md (log entry), this file's section 12 and docs/MOTION_P2.md sections 12-13
(recorded numbers) at integration, rollout\bin publishing from m2 on.
**Brief:** the B11 wear checks, the look matrix with part coverage, the freeze manifest and the cloak-aware B10 sweep; then
integrate everyone's delivery and prove the whole thing on both compilers.
- core: stubs for every 5.8 function in the first hour; the freeze manifest written from the checkpoint pool (5.6:
  `.tmp/judge/rollout/freeze_f47c3af.txt`); **G2**: W1-W10, T1, T2 (5.3), `matrixLooks` (5.5, part coverage),
  `wearSelftest`, `b10Watch` (M7), `writeStrips`; publish rpg_test_m2.exe and keep publishing as the checks calibrate on the
  eye-approved art. **Integration** (wave 2): compile the pool from the tree (`node tools/doll/pxc.mjs`, then `--check`), build
  build-wear then build/, run every gate and invariant, the freeze check, clang in WSL (rsync to ~/emb, `ninja -C
  build-native rpg_test`, `--doll --gates`), fill section 12, record the kin numbers in MOTION_P2 12, write the docs/PLAN.md
  entry.
- depth: per-look RUN / LOCO KB before and after for the 20 nearest-budget looks and every M7 look; the runtime line (added
  seconds); `--matrix part:<name>` (every look that wears a part).
- delight: `--frames` strips laid out like the inventory sheets (idle, walk 0-5, run 0-5, both facings, on grass) so the judge
  compares before and after side by side.
**Done (measured):** `wearSelftest` ALL CAUGHT; on the checkpoint pool the inventory misfits fail their named checks (W1 robe,
gown and mail sleeves and the coat_stride; W8 cloaks; W6 / W7 stick and staff; W9 twin braids; T1 long-garment flight) and
the M7 sweep reports the four over-budget cloak looks; at integration: `pxc --check` clean, `rpg_test --doll --gates` exit 0
on build/ (MSVC) and build-native (clang), `--freeze-check` 0 changed, M7 within budget, `--doll-coverage --seeds 1..3` no
missing values, rig 1 invariants intact, added gate time <= 60 s, section 12 filled.

### 7.3 ADULT-RIG (artist)
**Files:** rpg/art/doll/maps/legs/adult/loco/** (std, far, tips), maps/arms/adult/loco/**, maps/hair/adult/**,
maps/stamps/boots_adult.stamps, clips/loco.clip.
**Brief:** keep the approved adult motion intact and finish what only it can: the shoe tips, the hair in the run, the drawn
run's off hand, the run polish; own loco.clip.
- core: tips.pxm follows the shipped feet in every side walk and run frame (man and woman, R and L) and in the flight both
  toes peek below the raised hem (T1, T2; no floating bell); every adult hair-hang style hangs or streams behind in the run
  (twin braids and pigtails off the chest: W9) through the new `hair=run` key (R / L `run` views on every hair-hang part
  first, then the loco.clip lines, in one delivery); the drawn run's off hand pumps against the legs with the sheathed timing
  using existing keys (or a ruled exception), never crossing the blade or the face; loco.clip keeps every frozen number.
- depth: the flight's K9 trailing boot reads the same size as the other boot (by eye; BOOTSIZE); run3 / run6 thighs separated
  below the seat (no bunched outlines at the crotch); long hair lags a frame (swing on the push, run in the flight); the far
  fist on pB reads as a hand on slim builds (hand pixels only).
- delight: a pixel of hair bounce on the run's landing; the braid's side switch made gentle if the owner wants it (10.2).
**Done:** long-garment looks (man robe, gown, kaftan, wrap; woman dress, robe, gown, kaftan, wrap) pass T1 / T2 and B11 in
every side frame, both facings; every hair-hang part has `run` views (W1) and the M4 looks pass W8 / W9 in every side run
frame; the drawn run passes W7 and the judge's eye; a diff of loco.clip shows only the `hair=run` names and announced lines;
pxc 0 errors; report with 8x strips and 3x motion of each.

### 7.4 MAN-WEAR (artist)
**Files:** rpg/art/doll/maps/wear/man/**, maps/armour/man/**.
**Brief:** every man's garment and the mail ride the shipped walk and run: hems up and clear of the legs, the lean drawn, tails
trailing, sleeves complete and on the arms.
- core: each garment's side `run` view fits the walk legs and its `runl` view is hand-finished so the lean reads as a body
  leaning (chest forward, a straight back diagonal, belt and hem following) for every build, people and the elder's
  `when elder` views; hangs `wide` / `flare` trail and stay attached (W3, W5; the apron's flap never flies forward or reads as
  a pouch); stride panels fit the reach frames or draw empty; gown_sleeve, robe_sleeve and mail_sleeve gain every arm key the
  side clips name, fitted on the frozen arms (W1, W2); coat_stride gets explicit empty views where it should vanish; the mail
  torso's run views read as mail over a leaning body.
- depth: tails, pleats and robe hems trail a pixel more in the flight than at the contact (drawn into `flare`); patterns
  survive the shear; no garment look grows its RUN or LOCO atlas (before / after list).
- delight: the robe's wide sleeves flutter back on pB / pK; the apron strings flick back in the run.
**Done:** `--doll --motion` 0 failures on every matrix look that wears a wear/man or armour/man part (B11 limbs, B11 wear,
B12); every `runl` view differs from its generated shear (the report lists each) and passes W4; B10 before / after per
garment with no growth on the near-budget looks; 3x motion and 8x strips of all 14 garments and the mail, both facings, in
the report.

### 7.5 WOMAN-WEAR (artist)
**Files:** rpg/art/doll/maps/wear/woman/**, maps/armour/woman/**.
**Brief:** the same for every woman's garment and her mail, plus her skirts and her already-pumping sleeves.
- core: as MAN-WEAR for her 15 garments (incl. rags) and mail; her sleeves and poncho drapes re-fitted to the i3 pump (pB
  redrawn, re-timed) (W2); mail sleeves gain every key (W1); long garments (dress, robe, gown, kaftan, wrap) keep their hem line
  in the run so the tips show (T1) and their skirt area (AREA); her stride panels (apron, coat, culturetunic, jerkin, kilt,
  poncho, rags, smock) fit the reach frames or draw empty.
- depth: her skirts stop gliding in the walk (the hem sways by whole rows over the stride, within `wide` / `rest`); patterns
  (tiles) survive the shear; no atlas growth.
- delight: the dress's flare lifts a row more at the back in the flight; the coat's tails kick on the landing.
**Done:** as MAN-WEAR for every matrix look wearing a wear/woman or armour/woman part.

### 7.6 KIN-YOUTH (artist)
**Files:** rpg/art/doll/maps/kin/youth/**, maps/stamps/boots_youth.stamps, clips/kin_youth.clip.
**Brief:** the youth walks and runs by the adult method at 26 px: the kin slice first, then every youth garment.
- core: the slice (youth boy and girl in tunic and trousers, walk and run, R and L) drawn from the youth idle's joints:
  std near + new kin/youth/legs/far.pxm (z -1, the far value, `boots-stamped`), full thickness, ankles >= 4 px, the Y boot keys
  (+ a hand-drawn flight point if needed), `when girl replace` where her legs differ; kin_youth.clip: even holds (about
  3 / 3 / 3 walk, 5 / 5 / 5 run), bob walk <= 1 / run <= 2, the run lean (2-3 px: head, torso, arms) with the youth torsos
  switched to the torso channel (`keys torso`, new `run` / `runl` views, the `rest` view untouched), hips on `hip=`, the arms
  pumping on new pB pM pF pK pN keys (kin/youth/arms) with the adult timing; shown to an `eyes` check beside the adult
  reference before the garments roll; then all 11 garments and their hangs fit walk and run (HIPLINE, AREA, W3-W5); the youth's
  neckwear flows; the adult hair (with `hair=run` once delivered) and headwear reviewed on the youth (findings to their
  owners).
- depth: the youth's flight and push read as a sprint, not a hop; long-garment youths (robe, gown, kaftan, wrap, dress) show
  their shoes (youth tips follow); B10 youth RUN / LOCO within budget.
- delight: a youthful bounce (the full 2 px bob, arms a touch wilder than the adult's).
**Done:** `--doll --motion` 0 failures on every youth matrix look with the youth thresholds (5.2; slice-set values recorded);
REG <= 1, BOB, RHYTHM green; pxc 0 errors (L views not mirrors); the slice note (eyes) and 3x motion / 8x strips of boy and
girl in every garment, both facings, in the report; the numbers handed to GATES-WEAR for MOTION_P2 section 12.

### 7.7 KIN-CHILD (artist)
**Files:** rpg/art/doll/maps/kin/child/**, maps/stamps/boots_child.stamps, clips/kin_child.clip.
**Brief:** the child walks by the method at 21 px, and its walking postures follow.
- core: std near + new kin/child/legs/far.pxm from the child idle's joints (5-row legs), THICK 3.3, ankles >= 4 px, the C boot
  keys, R and L (+ girl views where needed); kin_child.clip side walk with even holds (about 2 / 2 / 2), bob +1 on the contacts
  only (no hop on the pass), arms B N F against the legs; the `hop` key and the Play and Carry side frames on the new legs; the
  child skirts (gown, kaftan, robe, wrap skirts) and garments clear of the new legs (AREA, W5); child tips follow.
- depth: the child's hair hangs (braid, pigtails, ponytail) swing on the pass; consistent with the youth slice (far value, boot
  use, bob).
- delight: a toddler-ish rhythm, the arms a touch wide.
**Done:** `--doll --motion` 0 failures on every child matrix look (walk; postures with the leg checks only); REG <= 1; pxc 0
errors; 3x motion / 8x strips of boy and girl in every garment and in Play and Carry, both facings, in the report.

### 7.8 OVER-CARRY (artist)
**Files:** rpg/art/doll/maps/cloak/**, maps/headwear/**, maps/neck/**, maps/held/**, maps/belt/**, clips/postures.clip.
**Brief:** everything worn over the body or carried rides the shipped walk and run inside the atlas budget: cloaks that stream
like cloaks, hats on the leaning head, neckwear that trails, the stick and staff in the hand, belt items behind the legs, and
the walking postures on the new legs. One owner, because the over-budget looks are cloak + hood + staff or stick together.
- core: the four cloaks' `flare` views redrawn for the leaned, 2 px-bobbing run (streaming back from the shoulders along the
  lean, long enough to read as a cloak, the hood lump at the nape, attached, inside the body's box: W3, W5, W8) and the `wide`
  walk views behind the shipped strides; the fur mantle's floating cape gone; cloak fronts on the leaned chest; headwear and
  hood drapes on the leaned head and shoulders (HEAD, W5); kerchiefs and scarves trail (W3, W5); the walking stick redrawn for
  the shipped walk (holds 3 / 4 / 4: its planted tip steps back exactly each frame's advance, never on a boot) and carried in
  the fist on every run pump key (W6), man and woman, R and L; the run staff in the hand on every pump key, upright, never over
  a face (W6, W7); scabbard, hilt and belt tools behind the legs (W10); the drawn sword clear of the face (W7); postures.clip:
  Carry's side frames on torso=run with the walk's hang keys, Play and Dance read on the run and walk keys, the loads (sack,
  basket, crate) carried (leg checks only: these frames step by time); **every M7 look within B10** (the four over-budget
  cloak looks fixed by trading pixels between cloak, hood, staff and stick).
- depth: the cloak's hem trails a row more in the flight than at the contact (inside `flare`); the fur rim reads in motion;
  ledger and scroll swing with the arm; no stick or staff frame wider than today's.
- delight: the travel cloak's hood lump bounces a pixel on the landing; the elder's hand leans onto the stick's crook on the
  plant.
**Done:** `--doll --motion` 0 failures on every M3, M5 and M6 look (incl. W6-W8, W10); the stick's tip registers within 1 px
over the walk (reported per frame); the M7 sweep within budget (before / after KB per look); the judge sees no backpack read
(travel, fur, noble, cape; man and woman; both facings) and no stick or staff out of the hand in 3x motion; 3x motion / 8x
strips of the cloaks, the stick (man, woman, elder), the staff (man, woman) and Carry / Play in the report.

## 8. Waves

- **Wave 0 (done by the planner):** MOTION_P2.md rewritten as the shipped method; STYLE_P2 section 10's numbers; this plan;
  the inventory, looks file, B10 probe and sheet tools in `%TEMP%\claude\emb_P2b\rollout\`.
- **Wave 1 (all eight areas at once):** GATES-LIMBS lands doll_motion.h and G0 in its first hour and G1 by the middle of the
  wave; GATES-WEAR lands its stubs and the freeze manifest (from the checkpoint pool) early and G2 by three quarters. Every
  artist starts at once by eye (renders + check.py on isolated pools from G0) and adopts the newest published exe for its
  final self-check; KIN-YOUTH builds its slice first inside its area; KIN-CHILD starts its legs at once and matches the
  slice's conventions before it delivers. Areas deliver into the tree as they pass (own files only).
- **Wave 2 (barrier: every area delivered):** GATES-WEAR integrates (pool, builds, every gate and invariant, B10, freeze,
  clang), then the judge. Fix rounds follow the build loop; a fixer reruns `--doll --motion` on its looks and the judge's
  harness before claiming a fix.

## 9. What the judge focuses on (and how each done is measured)

1. **Look and feel first.** Play at game speed (walk 40 px/s, run 74 px/s) on grass, 3x, both facings, beside the adult
   reference: the youth boy and girl (walk, run), the child boy and girl (walk, Play), the robed man and the woman in dress and
   gown (run: shoes under the hem), the four cloaks (run), twin braids, long hair and the braid (run), the elder with his stick
   (walk), the staff (man, woman; run), the drawn run, Carry. Then 8x strips of every frame with the hip-to-boot trace. The
   question each time: is it the same method, and does anything float, vanish, slide, stretch or cross a face?
2. **The inventory**: each of the 30 rows closed with its evidence (a gate line or a render).
3. **The gates**: `rpg_test --doll --gates` exit 0 (MSVC build/ and clang); `--doll --motion --report` 0 failures over the
   matrix; selftest ALL CAUGHT; parity 0 disagreements; freeze 0 changed; M7 B10 within budget; pxc --check clean; rig 1
   (art_hash TOTAL 317eef6efe4d8229, village-art-check 65/0, `--anim` 0).
4. **The blind panel** (motion, thickness, naive lenses) on the youth, the child and six garment / over / carry looks against
   the adult reference.
5. **Owner pack** (the owner checks in from the road): before / after GIFs of the youth run, child walk, robe run, cloak run,
   stick walk, staff run and twin braids, plus the 8x strips.

Each area's "Done" in section 7 lists its measures; the judge reruns them, it never takes an area's word.

## 10. Decisions taken by default (confirm or overrule)

1. **Kin numbers** (youth walk 18 / run 30 px, lean 2-3, run bob up to 2; child walk 12 px): drawn by eye inside these bounds
   by the KIN areas, then recorded.
2. **The braid** stays on her chest in idle and walk (A3's idle is locked) and hangs down her back in the run; it switches at
   the gait change.
3. **The lean stays 4 px** (approved); garments hand-finish their `runl` views at that lean. A deeper lean is the owner's.
4. **Cloaks are re-cut to fit the budget**: the run flare streams back no further than the body's box, a steeper, shorter drape
   rather than a flying sheet.
5. **The drawn run's off hand pumps** if it reads; otherwise the guard run stays as it is.
6. **Gate names**: `B11 limbs (R L)`, `B11 wear (R L)`, `B12 boots` inside `--doll --gates` (VISION_PLAN's P2b done-gate list
   uses B11 / B12 for other things; test_doll's names win in code).
7. **Walking postures stay time-keyed** (no registration) in this batch.

## 11. Risks and fallbacks

- **The C++ port drifts from check.py** (skeleton heuristics): port function by function with intermediate dumps to diff;
  after two failed attempts escalate to engineer-high; fallback: ship with check.py as the adult oracle and the C++ checks for
  kin and wear, recorded here.
- **Checks force corners** (proto4's FOLD): design by eye first; a check that contradicts an eye-approved pose is re-tuned by
  GATES-LIMBS / GATES-WEAR with the judge's ruling and recorded, never the art bent to it.
- **B10** has 0.6 KB of RUN headroom: every OVER-CARRY change is measured per look; fallback for cloaks: drop the hood lump in
  side runs, shorten the trailing hem; raising a budget is the owner's call only.
- **Youth THICK 3.8 at steep angles**: if the slice proves it unreadable, the judge rules a measured youth bar.
- **Parallel edits**: isolated roots, own files only, clips validated before saving, one integrator (GATES-WEAR); the two
  engineers keep every tools/tests file compiling in the tree at all times (the CMake glob builds both files in both BDIRs).
- **Idle drift**: the freeze manifest catches any idle or D / U change; revert it.
- **`hair=run` lands before every hair hang has a view**: W1 fails at integration; ADULT-RIG adds the views first and the clip
  lines last, in one delivery.

## 12. Results (GATES-WEAR fills at integration)

Integration 2026-10-23 01:00 (GATES-WEAR): every area's files in the tree (ADULT-RIG, MAN-WEAR, WOMAN-WEAR, KIN-YOUTH,
KIN-CHILD, OVER-CARRY, GATES-LIMBS); `node tools/doll/pxc.mjs` -> pool 9c637a21586cf561 (789,222 bytes, 427 parts, 7,470
views), `--check` clean; build-wear and build/ (every target, MSVC) built; build-native (WSL clang 18) built rpg_test.
Logs and renders: `%TEMP%\claude\emb_P2b\proto7\areas\gates-wear\` (int_*.txt, freeze\, ck_strips\, *_findings.txt).

| gate | target | measured |
|---|---|---|
| B11 limbs / B11 wear / B12 boots over the matrix | 0 failures | `--doll --gates`: B11 limbs 38 / 447 (elfm, elfw FARWIDTH THICK LEGLEN: ADULT-RIG; childm, childw AREA: KIN-CHILD); B11 wear 202 / 5,218 (below); B12 16 / 442 (elfm, elfw BOOT BOOTSIZE: ADULT-RIG). On the checkpoint pool the same wear gate failed 1,621 times |
| selftest / parity / freeze | ALL CAUGHT / 0 / 0 | wear selftest ALL CAUGHT (11 frame cases x R L, a deleted sleeve view (W1), a sliding shoe (T2)); limbs SYNTH ALL CAUGHT (GATES-LIMBS); parity: GATES-LIMBS' parity.py; freeze 144 changed of 15,844 (the youth girl's fall frame 2 and lying frame 0, every facing, 7 px: KIN-YOUTH's youth wear `keys torso` / `lie` views) + 16 changed as ruled (mail sleeves now drawn in the drawn idle and the hurt where they vanished: `.tmp/judge/rollout/freeze_accepted.txt`) |
| B10 incl. M7 | RUN <= 64, LOCO <= 128 | every non-M7 matrix look within (worst RUN 63.3 hat_man_conical, LOCO 113.5); M7 9 of 72 over: the elf man with hood and cloak, RUN 64.4 (staff) / 65.0 (stick) / 65.3 (sword) for all three cloaks (checkpoint: 24 of 72 over, RUN up to 67.1, LOCO 128.4): OVER-CARRY |
| `--doll --gates` MSVC / clang | exit 0 | exit 1 on both, identical lines (B10 9, B11 limbs 38, B11 wear 202, B12 16); B1 B2 faces hands B8 B9 ramps B11 leg mass pass; runtime 21.7 s against 1.8 s before (+20 s; the limit +60 s) |
| rig 1 | 317eef6efe4d8229, 65/0, `--anim` 0 | TOTAL 317eef6efe4d8229; village-art 65 groups, 0 changed; `--anim` 0 failures (A1 21,630 samples identical), `--anim --gates` exit 0 |
| kin numbers (youth, child) | recorded in MOTION_P2 12 | recorded |
| `--doll-coverage --seeds 1..3` | no missing values | 9,400 of 9,400 looks covered (100.0 %), 44 dead parts (reported, not gated) |
| part coverage | every side-moving part judged | 175 parts, 0 unjudged |

B11 wear at integration, by check (rerun: `rpg_test --doll-wear --report`; per look: tree_findings.txt):
W7 120 (the woman's drawn guard over her chin, R and L, every drawn look of hers, and every cloaked woman's drawn run:
OVER-CARRY, held/weapons/sword `guard`); T2 20 (the child's long skirts in the walk, toe +3 against the 2 px advance:
KIN-CHILD); W10 19 (hammer 12, axe 7: the belt tool over the shin: OVER-CARRY); W8 16 (the woman's four cloaks in the walk,
2 columns past her box: OVER-CARRY); W2 12 (wear/man/robe_sleeve on the robe and kaftan, run 4, R and L, 2 px off its arm: MAN-WEAR); W9 12 (the
youth's twin braids on the chest in the run: kin_youth.clip lacks `hair=run`: KIN-YOUTH); W5 3 (the ledger and scroll
float behind in run L2: OVER-CARRY). Closed since the checkpoint: W1 381 -> 0 (sleeves, strides), W3 90 -> 0, W4 552 ->
0, W6 17 -> 0, T1 120 -> 0, the staff through the face (W7) and the twin braid on the adult chest (W9).

Calibrations made on the approved art (design by eye, then the check agrees): W1 reads the standing idle frame (the blink
and breath overlays are not "drawn in the idle"); W2's overlap is waived for a sleeve view that is only trim touching its
arm (the robe, the gown) and for an arm drawn without U / F regions; W3 skips the cloak's rows on the shoulder (the torso's
top 3 rows: the drape hangs from them); W5 joins pieces on the item's own image (a scarf tail under a cloak front is one
piece); W7's face is the eyes, lashes and eye whites and the skin forward of the eye from the eye row - 1 to the row above
the mouth, not the profile's outermost pixel (A3's hero guard L crosses the jaw and the mouth's corner); W7 also counts a
held item drawn through the head (under it, showing above and below: the run staff); W8 holds cloaks to the bar (1
column, 0 rows) and neckwear and hair hangs to max(4, the same facing idle's reach + 1) columns and the idle's rows (the A3
scarf trails 4 columns behind a bare head, a ponytail swings a column past its idle); T1's flight asks for a row of the boot
itself (the sole strip alone reads as nothing under a hem).

Integration round 2, 2026-10-23 (INTEGRATION): pool 71fed774b7de202a (793,221 bytes, 427 parts, 7,494 views), `--check`
clean. Every gate green on build/ (MSVC): `rpg_test --doll --gates --parity` exit 0 (B10 0 / 1,324, B11 limbs 0, B11 wear 0
/ 5,016, B12 0); `--doll --motion --report --selftest` 0 failures over the 213-look matrix (5,016 side frames), 0 misses;
`--doll-wear --selftest` ALL CAUGHT; `--freeze-check` 0 changed; parity 0 disagreements (15 looks, 4,696 checks);
check.py `--dump build` PASS 1480/1480, `--synth` ALL CAUGHT; rig 1 TOTAL 317eef6efe4d8229, village-art 65/0, `--anim` 0;
the full `rpg_test` ALL OK; M7 worst RUN 63.6 KB, LOCO 127.6 KB.

Art fixed in place (each file's header note says why): the elf's seat (legs/adult/loco std + far: run3's near thigh fuller,
left walkC's seat pixel on the near leg); the woman's guard in her side drawn walk and run on a new held key `gmove`
(sword.pxm, loco.clip; her drawn idle keeps `guard`, frozen); the robe sleeve's pK cuff (R, L); the youth run names
hair=run (twin braids, W9); the child walk without its contact drop (the 1 px bob covered a fifth of a 5-row leg: AREA);
the ledger and scroll ride the pumping far hand in the left run (`when run`); hammer and axe swing back off the shin on
`kick` (L, a steeper `when run` variant); the four cloaks and the fur front narrowed for her walk's passing frames; the coat
hem over the passing knee and the run's lifted thigh; the apron flap a pixel forward (L); the staff's foot carried 3 rows
up in the walk (R); the legacy hood's peak tip folded for an elf running (D, U: the M7 RUN atlas); the youth girl's `lie`
torso views restored to her rest views (the frozen fall and lying frames, 144 lines).

Engine: doll_compose.cpp sets the flag `walk` on the walk clips of every part but RUN (doll.h documents it), so a part can
fit the stepping body (`when walk replace`) without touching the frozen idles, hurts, swings and falls that share its keys.

Checks calibrated (C++ and check.py together where both have the check; parity holds): BOOT credits an elf's `when elf`
overlay cells as the drawing's; an elf's top leg row is the seat; HEAD's loose test reads the skull (head, face, ears), not
hanging hair, hoods or veils, and its neck columns likewise; HEAD falls back to the torso's top rows where the neckwear's
shape moved (the scarf and kerchief flow); GROUND counts the tips part on whatever layer it draws; SLIVER skips a run the
garment's cover bounds on both sides, or on one side on a fringe row (the rags' tatters); T2's contact reads only what one
shoe can reach (sole - n columns past its contact); the child's AREA bar is 75 % (its idle boots are one double boot, the
poncho's idle fringe shows the thighs).
