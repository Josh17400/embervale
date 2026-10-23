# MOTION_P2: the side walk and run, rebuilt to the owner's motion reference

Written 2026-10-20/21 by the planner for the motion rebuild (workflow wf_abc83929-af6). Binding together with STYLE_P2
section 10's ruling "The owner's motion reference (2026-10-20)". Scope: rig 2, the doll's SIDE views (R, L) of the walk
and the run, for every body that walks. Front and back walks, the idles, the swing keys and rig 1 do not change.
**Round 4 re-plan (2026-10-21, 5.11) is the current plan.** Its three owner rulings bind everything below: every leg keeps
the idle's thickness across its bones at every angle; the legs stay short (A3's hem and boot shaft); the stride rides on
the leg angle and the boot keys, with knees implied. The tables in 5.2, 5.3, 5.5 and .tmp/judge/motion/targets.json carry
it; 5.8-5.10 are history.

Inputs: the owner's reference docs/ref/motion/owner_walk_ref.png; the studies in
C:\Users\joshu\AppData\Local\Temp\claude\emb_P2b\motion\study\ (reference_mechanics.md, ours_vs_reference.md,
ref_vs_ours_same_scale_4x.png, sticks_ours_vs_ref.png); the planner's feasibility work in ...\emb_P2b\motion\plan\
(targets.json = section 5.7, targets_R.png = a schematic of sections 5.2-5.3, feet_rot.png = the five tilts' geometry).

---

## 0. Why, and what this supersedes

The owner circled the side run ("the feet are completely disconnected from the body"). Three leg-pass rounds joined the
boots to trouser stubs but kept the old language: flat boots moved and lifted, a compact stride, legs mostly under the hem,
a flight that splays into a stiff inverted V. The owner then sent an 8-frame side walk in another art style and said:
"Not the art style but the walking animation and how it works, this is what you need to shoot for. See how the feet art
constantly flat but can be angled throughout the strides."

**What we copy** (measured in the studies):
1. Legs are continuous limbs, hip > thigh > knee > shin > boot; both always visible.
2. A scissor stride from one pelvis: an inverted V at contact (each leg about 20 degrees from vertical, nearly straight),
   the legs crossing at the passing pose, the planted leg vertical under the body.
3. The foot is ONE rigid drawing with a straight sole that tilts: toe up at heel strike, flat while it carries the body,
   heel up behind, rolled onto the toe at push-off, toe down while it swings. It pivots on the end that touches the
   ground; a supporting foot always sits on the ground row.
4. Almost no bob; the torso upright; the arms swing opposite the legs at waist height (ours already do).

**What we do not copy:** its style; its proportions (its leg is 19 px from the crotch to the sole, ours 10; its boot is
0.5 L long, ours 0.7 L, so our tilts are smaller, section 4); its timing (it is not a clean cycle: copy its poses).

**Superseded** (STYLE_P2 sections 10 and 20, the std.pxm / tips.pxm / loco.clip headers, the old judge harnesses):
"boots are never drawn rotated", "a flat sole, the idle's boot moved and lifted", "a compact stride", stance at most
13 px (legpass J2, J3e), "no heel more than 5 px behind the pivot" (J2), "the two legs apart in every row" (J3d) as a
rule for push and flight frames. **Still binding:** chunky legs (90 % or more of the idle's visible leg area, the idle's
hip width at most 1 px narrower, no leg run under 3 px above the feet), clean knees and ankles, hip bob 1 px at most and
no squat, the planted leg under the hips, the down frame no wider than its contact, women developed and judged in trousers,
bare legs and skirts, variety (her legs are her own), triple-A craft, the 18 / 36 px cycles and 6 frames, registration.

## 1. The feel

On a phone at arm's length, a villager crossing the square WALKS: the legs open into a clean inverted V as the front heel
strikes, toe up; the back boot peels onto its toe; the weight rolls onto a flat planted boot; the free leg swings through
past it, knee first and toe down, and strikes again. Nothing slides: the planted boot stays where it landed. The run is a
different animal: it lands flat on a bent knee, the trailing heel kicks up behind, and in the flight both feet leave the
ground, the leading knee up and the trailing foot pointed. Every leg reads as a leg, as thick as the idle's, from under the
tunic to the boot; every boot is the same boot, tilted. As solid and deliberate as the reference, at A3's chunky scale.

## 2. In and out

**IN**
- The side (R, L) walk keys walkA walkA2 walkB2 walkC walkC2 walkD2 and run keys run1-run6, redrawn for the adult man and
  woman (legs/adult/loco/std.pxm and her `when woman replace` views), the shoe tips under long garments (tips.pxm), the
  youth (walk, run) and the child (walk). The key NAMES stay, so no part, posture or coverage table gains a key.
- One boot drawn once per tilt key (stamps, section 8) and placed in every frame.
- The clips' side walk and run frames: the new plants (heel convention, section 5.4) and torso=run in the side walk, for
  the man, the woman, both elders, the drawn walk and run (ACTION variants 1 and 2), the youth and the child; the postures
  that reuse the keys (carry, dance, play) checked.
- Garments, armour, belt and held items whose side walk/run views hide or cut the new legs (section 7).
- The pxc stamp directive (tooling), the gates (B11 limbs, B12 boots), the judge harness, the blind panel.
- The default hero's dark trousers, inside rig 2 (section 14). STYLE_P2 sections 10 and 20.

**OUT**
- Front and back (D, U) walks and runs; the idles (A3-locked, B1); the stance, guard, lunge, buckle, sit and lie keys; every
  swing and action key; arms, heads, hair, cloaks.
- Frame counts (6 walk, 6 run) and cycles (18, 36 px): kept. LOCO has no room for more frames (worst look 127.6 / 128 KB),
  and the front and back walks share the side views' frame table.
- Rig 1 (v1 sprites, the default): art_hash TOTAL 317eef6efe4d8229, golden_village_art 65/0 and `--anim` golden must not
  move. rpg/sim/appearance.h and anything rig 1 reads: untouched.
- New anchors, channels or runtime code in the doll: stamps are compile time only.

## 3. Constants that interact

| constant | value | source |
|---|---|---|
| leg length L | 10 rows: hip anchor F-10 to the sole row F | loco.clip anchors |
| visible leg, side walk and run | F-9..F under a hem at F-10 (torso=run); F-8.. on the walk's down frames (bob +1); F-10.. in the run's flight (hem F-11) | section 7 |
| leg band | 5 px near, 4-5 px far (A3) when vertical; across the bone the idle's 4 px at every angle (rows ceil(4 / cos θ) wide; a flat bone 4-5 rows tall): THICK >= 3.8 px (youth 3.8, child 3.3) | std.pxm, 6.4, 5.11 |
| A3 boot | 7 px sole; foot 2 rows + sole (F-2..F); shaft 3 rows (F-5..F-3) = the shin | A3 |
| walk cycle | 18 px = 2 steps of 9; 6 frames; each frame 3 px of travel | unchanged |
| run cycle | 36 px = 2 x (5 + 5 + 8); 6 frames | unchanged |
| walk contact | heel to toe 16 px = 1.6 L (reference 1.5-1.67 L); ankle gap 7 = 0.7 L (0.67-0.84); hip centres 3 apart = 0.3 L (0.28); legs about +/-20 degrees (18-27) | section 5 |
| frame envelope, R view (L mirrored) | walk x in [-7, +9] contact, [-8, +9] down, [-10, +9] pass (fix r3: the swing K3 behind the planted heel); run [-9, +9] in all three frames (round 4: the push's heel kicked up behind reaches -9); youth and child walk pass -9 | B10, 5.11 |
| bob | walk 0 / +1 / 0; run 0 / 0 / -1 | owner rule |
| timing | walk: fps = speed / 3 (40 px/s: 13.3 fps, 2.2 cycles a second); run at 74 px/s: frames of 68 / 68 / 108 ms, 2.06 cycles a second | anim.cpp |

**The arithmetic that makes it fit.**
- Stance = step + boot length = 9 + 7 = 16 px at walk contact, whatever the placement. The registration chain fixes the
  placement: the planted heel goes +3 > 0 > -3 (3 px a frame); at the end of the pass it stands at -6, its toe at
  -6 + 6 = 0, which is exactly the trailing toe the next contact draws (K2 at 0). The front heel at +3 puts the toe at +9,
  today's front edge. So the walk needs no column beyond today's except -7 on the down frames, inside the man's hair and
  scarf span (-10/-11) and on the woman's own body edge (-7).
- B10 is about frame boxes, not parts: an atlas holds trimmed composed frames (study 5c). RUN's worst look, the armed elf
  man (63.8 / 64 KB), has his back edge set by the scabbard tip (-12 R, +11 L), so the run's new back reach (-7; -9 for the
  kick) costs him nothing and his front stays +9. LOCO's worst, the robe + staff + hood man (127.6 / 128 KB), wears a long
  garment: only tips.pxm draws his feet, and the tips stay inside today's -6..+9. Std-legs women run at about 42 KB, room
  for the kick. If B10 still fails, the fallback is fixed in advance (section 5.3, the K3 kick inside -7).
- Pool: stamps expand at compile time; about +0.1 KB per view (the pool is 0.666 of its 1.0 MB).
- Leg area: the side walk's hem rises a row, so every side frame shows one leg row more than the idle (hem F-9). The
  feasibility sketch measured contact 128 %, down 113 %, pass 91-108 % of the idle's 80 px; the pass stays above 90 % only
  with the offset thigh tops of section 5.2.

---

## 4. The boot: one drawing, five tilts

### 4.1 Which part is rigid
- The rigid drawing is the **foot**: A3's B and O regions, rows F-2..F of a flat boot (6 and 7 px of leather over a 7 px
  Ink sole). The shaft (region S, F-5..F-3) is the **shin**: it follows the shin's angle like a riding boot's leg. The knee
  is the boot top (the trouser/leather seam); the ankle is where the shin's last row sits on the foot.
- Why: our leg is 10 rows and the A3 boot 6 of them. Tilting the whole boot turns half the leg into a rotated block (the
  2026-10-19 round the owner rejected: "the feet are getting rotated, joints look a little off"). Tilting only the foot at a
  clean ankle is what the reference does with its low boot, and what "the feet art constantly flat but can be angled" means.

### 4.2 The keys
| key | used for | pivot on the ground | tilt | sole (Ink) | footprint | ankle socket, from the pivot |
|---|---|---|---|---|---|---|
| K0 flat | the planted foot; the run's landing; the flight's leading foot (lifted) | the whole sole | 0 | 7 px on one row | 7 | +2 px, 3 rows up (A3) |
| K1 toe-up | the walk's heel strike | heel | about 18 (fix r1, r2) | a 3 px heel run on F, then runs of 2 rising two rows to the toe (3,2,2) | 7 | +1, about 3.5 up; the toe cap (two leather rows, its top parallel to the sole) rises clear of the shin past an open instep (fix r2) |
| K2 heel-off | the walk contact's back foot, on its ball (fix r2) | toe | about 18 | runs 2,2,3 from the heel: the heel two rows up, the ball's 3 px on F (the gates class it with K3's slope, as K3; its stamp is K2) | 7 | -3, about 4 up |
| K3 toe-off | the walk down frame's trailing foot; the run push's swing foot and the flight's trailing foot (lifted) | toe | about 27 | 1:2 steps (runs of 2) rising two rows to the heel | 6-7 | -2, about 4.5 up |
| K4 hang | the run contact's trailing foot, kicked up behind (lifted 2) | toe, in the air | about 45 | a clean 1:1 diagonal | 5-6 | -1, about 5 up |

**Fix round 1 (R1-04, 2026-10-21):** the 1:4 K1 rose one row over 7 px and read as a flat boot at 1x-8x (lens A, JX4: every
contact frame a 1-row rise). K1 is now a 7 x 5 key, its sole 3 px on F at the heel then runs of 2 rising two rows (B12 must
class a toe-up sole of runs of 2 as K1: the gates' analyseSole change listed in 5.8), its toe cap's dome one row above the
instep so it stands clear of the reaching shin; 20 px like K0. The front shin leans into it (the reaching leg of 5.2).

**Fix round 2 (R2-01, 2026-10-21):** the r1 K1's toe cap was one isolated leather pixel ringed by outline, with an outline
notch at the instep: a dark cluster that read toe-DOWN and jagged. K1 is now the clean 1:2 rotation of K0 about the heel:
every K0 column lifted with the sole (0,0,0,1,1,2,2), so the toe cap keeps K0's two leather rows (h g g over the sole) and
its top edge runs parallel to the sole, the tip a square 2 px outline cap (no notch, no nub; 21 px, K0 20). The reaching
shin stops a pixel short of the toe cap on F-4, so the instep opens and the toe visibly rises past it. K2 (the 1:4 heel-up,
one row of rise: "the trailing foot never lifts its heel") is redrawn as the same rotation about the toe: heel two rows
up, the ball's 3 px on the ground, K0's toe slope kept (20 px). Its sole (2,2,3) is a runs-of-2 slope, which the gates
class with K3's; targets.json names its class (K3) and its drawing (`"stamp": "K2"`, which synth.py honours). The
foot rolls K1 > K0 under the front leg and K2 (18 degrees, on its ball) > K3 (27 degrees, on its toe) under the back leg.

Measured on the rotated A3 foot (feet_rot.png): toe-up 14 is 8 x 4 px, heel-up 14 is 7 x 4, toe-off 27 is 7 x 5, hang 45
is 6 x 7. A rotation brings the ankle over the pivot (heel-up: 3 px behind the toe; toe-off: 2; hang: 1), which is what
keeps the trailing and swinging legs short enough for our 10 rows. A shear would keep the ankle 4 px behind the toe and
stretch the boot: never shear.

**Fix round 3 (R3-03, R3-04, 2026-10-21):** K1's top outline runs back over the instep (`....fff` over `...hggf`; 22 px,
K0 20 + 2): the shin's front edge closes into the top of the foot, so the heel strike is one closed boot whose toe cap
rises two rows from the heel (INSTEP 0; r2's open instep was the "claw / hook / notch"). K4 is retired from the run: a 45
degree turn of the 20 px boot is a 2-row-deep diagonal bar (the panel's "stick", "boomerang", "plank"); every lifted or
pointed foot is the K3, its long straight sole tilted 27 degrees under a shin steep enough for an 80-90 degree ankle. The
K4 stamps stay in the library (unused).

**Rules that keep them ONE boot**
- Each key is a hand-drawn ROTATION of K0: the foot keeps its length along its own axis and its footprint shortens with the
  tilt. Never a shear (it stretches the boot), never a squash, never a tool or runtime rotation (jaggies). Draft from the
  rotated outline if you like; the committed pixels are placed by hand.
- The same parts in the same places on every key: a square heel, the leather step-0 outline all round, the Ink sole as one
  straight line along the bottom edge in a clean slope (0, 1:4, 1:2, 1:1; a 1 px run only at an end), the step-3 shine on
  the lit toe cap and the step-1 shade under the instep where K0 has them. The pixel count within 2 of K0's.
- The ankle socket: the shin's last row sits on the key's top-back 4-5 columns, offset 1 px at most; the shaft's back edge
  runs into the heel's back edge with no notch and no 2 px step.
- Near and far tones: the far key is the near key one value darker, the same silhouette (A3's convention: near
  fiihf / fhhgf / fhhhgf / fihhggf, far fihgf / fhggf / fhhggf / fhhgggf).
- L keys are the R keys mirrored and RE-LIT for the top-left light (painted, never the plain mirror; pxc errors on one).
- The woman wears the same keys (her foot rows equal his); her slimmer ankle is her shin's last row, not the stamp.

**Counts.** Adult: 5 keys x near/far x R/L = 20 stamps (K2 the heel-off since fix r2). Youth (its boot: shaft 4 px, foot 5 and 6 px, sole 6): Y0 flat,
Y1 toe-up, Y2 heel-up, Y3 toe-off = 16 (a Y4 hang only if its run kicks). Child (a 3-row, 6 px boot): C0 flat, C1 toe-up,
C2 heel-up = 12.

### 4.3 Drawn once, shared
The .pxm format has no instancing, so the tooling area adds a compile-time stamp to pxc (section 8): each key is drawn once
in a stamp library and placed per frame with `put`. pxc expands stamps into the views' pixels, so the pool, the compositor
and the runtime do not change; one edit to a key updates every frame and body that uses it; and pxc refuses a hand-drawn
boot pixel in a stamped side walk/run view, so every boot IS the boot. (Sharing saves no atlas bytes; B10 is held by the
envelope of section 3. The brief's "angled keys must be shared parts to fit B10" is answered there.)

---

## 5. Key poses

### 5.1 Conventions
- R view. x = columns relative to the pivot (+ forward). F-n = n rows above the sole row F.
- A joint is the centre of the leg's band on its row. **hip** = the leg on the first visible row under the hem.
  **knee (x, F-n)** = the thigh's last trouser row (the shaft starts on the row below). **ankle** = the shin's last row,
  sitting on the boot's socket.
- **gx** = the boot key's ground pivot column (heel for K0 / K1, toe for K2 / K3 / K4); **lift** = rows between F and the
  key's lowest pixel; **span** = the key's columns.
- near = drawn over far; the far leg a value darker. Frames 3-5 are frames 0-2 with near and far swapped.
- L view = the R placements mirrored (x' = -x - 1) and re-lit; L plants = R plants + 1 (forward-positive, as today).
- The woman: the same placements, her back hip a pixel fuller on the first visible row, her ankle a pixel in.
- Tolerances (the judge's): joints +/-1 px and +/-1 row; key, gx and lift exact (gx +/-1 only where marked); stance +/-1.
- Before anyone claims a frame is right: trace hip > knee > boot in EVERY frame at 8x, both facings, both bodies.

### 5.2 Walk (R view; cycle 18 px). Round-4 re-plan (5.11): short legs, full thickness, the stride on the leg angle and the boot keys
Each leg is a skeleton, **bone H > K > A** (H the hip joint on the seat row, K the knee, A the ankle on its boot's socket),
drawn as two capsules 4 px thick across the bone (section 6.4). The knee angle is the bone's (180 = straight). "Trace" is
what trace.py measures on such a drawing (the hip on the first visible row, the knee on the last trouser row, the ankle on
the shin's last row over its boot): J2's +/-1 joint targets. "Thick" is the planner's drawing measured with THICK (5.11).
x = u from the pivot, + forward; F-n = n rows above the sole row.

| f | key | phase | bob / hem | leg | role | bone H | bone K | bone A | knee | trace knee / ankle | boot | gx | lift | span | thick thigh / shin | stance | trouser rows |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | walkA | contact | 0 / F-10 | near | front: one straight reaching line into the toe-up K1 | +2, F-10 | +3, F-6.5 | +4, F-3 | 180 | +3, F-6 / +4.5, F-3 | K1 | +3 heel | 0 | +3..+9 | 4.8 / 4.8 | -6..+9 (16) | 4 |
| | | | | far | back: one straight line onto the heel-off K2 | -1, F-10 | -2.5, F-6.5 | -4, F-4 | 172 | -3, F-6 / -4.5, F-4 | K2 (class K3) | 0 toe | 0 | -6..0 | 4.6 / 4.3 | | |
| 1 | walkA2 | down | 1 / F-9 | near | planted: straight over the flat K0 | +2, F-9 | +2, F-6 | +2, F-3 | 180 | +2, F-6 / +2, F-3 | K0 | 0 heel | 0 | 0..+6 | 5.0 / 5.0 | -8..+6 (15) | 3 |
| | | | | far | trailing: angled back, rolled onto its toe; the knee implied | -1.5, F-9 | -2, F-6.5 | -5, F-4 | 141 | -3, F-6 / -5.5, F-4 | K3 | -3 toe (registered) | 0 | -8..-3 | 4.9 / 4.6 | | |
| 2 | walkB2 | pass | 0 / F-10 | near | planted: straight under the hips over the flat K0 | +1, F-10 | 0, F-6.5 | -1, F-3 | 180 | 0, F-6 / -1, F-3 | K0 | -3 heel | 0 | -3..+3 | 4.8 / 4.8 | -10..+3 (14) | 4 |
| | | | | far | swing: tucked behind the planted thigh, the K3 lifted toe down behind the planted heel | -3, F-10 | -3, F-7 | -6.5, F-5 | 120 | -4, F-7 / -7, F-5 | K3 | -5 toe | 1 | -10..-5 | 5.0 / 5.2 | | |
| 3-5 | walkC, walkC2, walkD2 | as 0-2 | | | near and far swapped (walkD2: the near swing over the far planted leg) | | | | | | | | | | | | |

- **f0 contact** (the reference's frames 1, 2, 4): an inverted V of two straight, full-thickness legs from one pelvis
  (the hip row -3..+4, the idle's). The front leg is one reaching line: its shin leans into the toe-up K1, the ankle a
  pixel ahead of the knee (CONTACTREACH), the K1's top outline closing over the instep (INSTEP 0). The back leg runs
  straight onto the K2, its heel two rows up (HEELRISE). Background between the legs from F-6 down; no knee shows.
- **f1 down** (frame 6): the weight on one straight column over the flat K0; the trailing leg angles back from the hip
  and rolls onto the toe of its K3 at its registered -3. Its knee is a 1-row turn where the shin leans back harder than
  the thigh, not a drawn joint. The thighs touch down to F-5 (apart from F-4); the down frame is no wider than contact.
- **f2 pass** (frame 3): one straight leg over its flat K0 carries the body; the swing leg is **tucked**: its thigh hangs
  behind the planted thigh (one mass from the hip, F-9..F-7), its knee level with the planted knee (3 px behind it), its
  shin turning back to the toe-down K3 lifted 1 behind the planted heel, one column of daylight between the boots
  (BOOTSPLIT 0, the whole swing boot visible). In walkD2 the near swing thigh is drawn over the far planted one.
- **Every leg**: 4 px across the bone at every angle, down to the boot (THICK); the shin's last row as wide as the rows
  above, on its boot by 3 px or more.

### 5.3 Run (R view; cycle 36 px; lean +1). Round-4 re-plan (5.11)
| f | key | phase | bob / hem | leg | role | bone H | bone K | bone A | knee | trace knee / ankle | boot | gx | lift | span | thick thigh / shin | stance | trouser rows |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | run1 | contact | 0 / F-10 | near | landing: a flat K0 under a bent knee | +2, F-10 | +5, F-6.5 | +5, F-3 | 139 | +5, F-6 / +5, F-3 | K0 | +3 heel | 0 | +3..+9 | 4.6 / 5.0 | -7..+9 (17) | 4 |
| | | | | far | trailing: back from the hip, the knee soft, the K3 lifted toe down behind | -1, F-10 | -4, F-7 | -4.5, F-5 | 149 | -3.5, F-7 / -4.5, F-5 | K3 | -2 toe | 1 | -7..-2 | 4.2 / 4.9 | | |
| 1 | run2 | push | 0 / F-10 | near | planted: under the body, the knee soft | +1, F-10 | +1.5, F-6.5 | 0, F-3 | 149 | +1, F-6 / 0, F-3 | K0 | -2 heel | 0 | -2..+4 | 4.0 / 4.6 | -9..+4 (14) | 4 |
| | | | | far | swing: the heel kicked up behind, the K3 high behind the planted heel | -2, F-10 | -2.5, F-8 | -7, F-6.5 | 122 | -4.5, F-8 / -7, F-6 | K3 | -4 toe | 2 | -9..-4 | 7.8 / 4.7 | | |
| 2 | run3 | flight | -1 / F-11 | near | trailing: just pushed off, back, the K3 pointed behind | -1, F-11 | -2, F-7.5 | -5, F-5 | 146 | -3, F-7 / -5.5, F-5 | K3 | -3 toe | 1 | -8..-3 | 4.8 / 5.4 | -8..+9 (18) | 4 |
| | | | | far | leading: the knee up, the shin hanging over the landing spot | +2, F-11 | +5, F-8.5 | +5, F-5 | 130 | +4.5, F-8 / +5, F-5 | K0 | +3 heel | 2 | +3..+9 | 7.7 / 5.0 | | |
| 3-5 | run4, run5, run6 | as 0-2 | | | near and far swapped | | | | | | | | | | | | |

- **run1 contact**: a landing, not a stride: the front boot lands flat under a knee bent forward (139); behind, the
  trailing leg runs back from the hip with a soft knee (149) and its K3 is lifted 1, toe down, behind the hips: the
  recovery begins. Its shin stays steep over the boot, so the ankle reads 75-135 (KICKANGLE 95); a folded contact leg at
  4 px would lay the shin flat over a toe-forward boot (the round-2 "club").
- **run2 push** (the owner's 2026-10-21 direction): one leg under the body, the knee soft; the other **heel kicked up
  behind**: its thigh hangs from the hip, its shin turns back nearly level (a 4-5 row block, full thickness), the K3 high
  behind the planted heel (lift 2, toe -4, a column of daylight to the planted heel). The swing foot's path is now behind
  (flight -3, contact -2, push -4 lifted 2), then forward in one swing to the next landing (+3..+9): no foot hangs in
  front (round 4's push toe +9 is gone).
- **run3 flight**: both feet off the ground (no sole on F): the leading knee up in front (F-8.5), its shin hanging over a
  flat K0 lifted 2 above the landing spot (it drops straight into the next contact); the trailing leg back with a soft
  knee, its K3 pointed behind (lift 1). Each bone's knee sits 1 px or more off its hip-ankle line (lead 1.75, trail 1.33):
  never a stiff inverted V. The silhouette differs from the contact it precedes: the lead foot two rows up with the knee
  high, the trail foot behind instead of tucked.
- RUNSTIFF holds in every frame (contact 139, push 122, flight 130: a bent bone each time).

### 5.4 Plants and registration
The plant is now the planted foot's **heel**, forward-positive (R: the heel's x; L: minus the heel's x). The differences,
and so the walk table and the cadence, are today's.

| frame | walk R | walk L | run R | run L |
|---|---|---|---|---|
| 0 / 3 (contact) | 3:0 | 4:1 | 3:-2 | 4:-1 |
| 1 / 4 (down / push) | 0:-3 | 1:-2 | -2:-7 | -1:-6 |
| 2 / 5 (pass / flight) | -3:-6 | -2:-5 | -7:-15 | -6:-14 |
| cycle | 18 | 18 | 36 | 36 |

- The same lines for the man, the woman, man.elder and woman.elder (their lean stays), the drawn walk (ACTION variant 1)
  and the drawn run (ACTION variant 2), sheathed and drawn alike.
- **Registration**: at every frame change of the walk, and contact > push of the run, the planted heel lands within 1 px
  of the new frame's plant0; the trailing toe (K2 > K3, frames 0 > 1 and 3 > 4) lands within 1 px of where it stood: the
  down frame's toe drawn at -2 against a registered -3 is that tolerance, used nowhere else. Within a held frame a planted
  foot slips by the frame's advance at most (3 px walk, 5 px run). The flight registers nothing (no foot is down); its
  leading boot is drawn where the next contact lands (+3), lifted 1, so it drops straight down.

### 5.5 The youth and the child. Round-4 re-plan (5.11): full hip, knee and ankle tables
The same language at their scale, the same columns as 5.2 (bone H > K > A drawn as capsules 4 px thick across the bone for
the youth, 3.5 px for the child: each body's own idle leg). Plants unchanged. Their legs are 3 rows (youth) and 1-2 rows
(child) above a boot as tall as the shin, so their knees are implied entirely by the leg angle and the boot key.

Youth (L = 8: the hip joint on the seat F-8, the walk hem F-8 (F-7 on the down frames), the knee row F-5 standing; boots Y0
flat, Y1 toe-up, Y2 heel-off, Y3 toe-off; walk cycle 16, plants R 2:-1, -1:-4, -4:-6; run cycle 30, plants R 2:-3, -3:-7,
-7:-13; L views +1):

| f | key | phase | bob / hem | leg | role | bone H | bone K | bone A | knee | trace knee / ankle | boot | gx | lift | span | thick | stance | trouser rows |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | walkA | contact | 0 / F-8 | near | front: one reaching line into the toe-up Y1 | +1.5, F-8 | +2.5, F-5.5 | +3.5, F-3 | 180 | +3, F-5 / +3.5, F-3 | Y1 | +2 heel | 0 | +2..+7 | 4.6 / 4.6 | -6..+7 (14) | 3 |
| | | | | far | back onto the heel-off Y2 | -1.5, F-8 | -2.5, F-5 | -4, F-4 | 142 | -3, F-5 / -4.5, F-4 | Y2 | -1 toe | 0 | -6..-1 | 4.7 / 5.8 | | |
| 1 | walkA2 | down | 1 / F-7 | near | planted: straight | +1.5, F-7 | +1.5, F-5 | +1.5, F-3 | 180 | +1.5, F-5 / +1.5, F-3 | Y0 | -1 heel | 0 | -1..+4 | 4.0 / 4.0 | -7..+4 (12) | 2 |
| | | | | far | trailing: angled back onto its toe (registered -4, drawn -3) | -1, F-7 | -1.5, F-5 | -5, F-4 | 120 | -3, F-5 / -5.5, F-4 | Y3 | -3 toe | 0 | -7..-3 | 6.8 / 3.8 | | |
| 2 | walkB2 | pass | 0 / F-8 | near | planted: straight under the hips (drawn at heel -3, its plant -4: gx_tol 1) | 0, F-8 | -0.5, F-5 | -1, F-3 | 175 | -0.5, F-5 / -1, F-3 | Y0 | -3 heel | 0 | -3..+2 | 3.9 / 3.9 | -9..+2 (12) | 3 |
| | | | | far | swing: tucked behind the planted thigh, the Y3 lifted toe down behind the planted heel | -3, F-8 | -3, F-6 | -6.5, F-5 | 106 | -4.5, F-6 / -7, F-5 | Y3 | -5 toe | 1 | -9..-5 | 7.0 / 3.8 | | |
| 0 | run1 | contact | 0 / F-8 | near | landing: a flat Y0 under a bent knee | +1.5, F-8 | +3.5, F-5 | +3.5, F-3 | 146 | +3, F-5 / +3.5, F-3 | Y0 | +2 heel | 0 | +2..+7 | 4.2 / 4.0 | -6..+7 (14) | 3 |
| | | | | far | trailing: back from the hip, the Y3 lifted toe down | -2, F-8 | -3.5, F-6 | -3.5, F-5 | 143 | -3, F-6 / -4, F-5 | Y3 | -2 toe | 1 | -6..-2 | 4.0 / 5.0 | | |
| 1 | run2 | push | 0 / F-8 | near | planted: under the body, the knee soft | +0.5, F-8 | +0.5, F-5 | -1, F-3 | 143 | 0, F-5 / -1, F-3 | Y0 | -3 heel | 0 | -3..+2 | 4.0 / 4.0 | -3..+7 (11) | 3 |
| | | | | far | swing: the knee driven forward, the Y3 lifted toe down under it | +2, F-8 | +4.5, F-6.5 | +4, F-5 | 103 | +4, F-6 / +4.5, F-5 | Y3 | +7 toe | 1 | +3..+7 | 5.1 / 4.7 | | |
| 2 | run3 | flight | -1 / F-9 | near | trailing: just pushed off, the Y3 pointed behind | -1, F-9 | -2, F-6.5 | -5.5, F-5 | 135 | -3.5, F-6 / -5.5, F-5 | Y3 | -3 toe | 1 | -7..-3 | 4.6 / 4.6 | -7..+7 (15) | 3 |
| | | | | far | leading: the knee up, the shin hanging | +1.5, F-9 | +3.5, F-6.5 | +3.5, F-5 | 141 | +3.5, F-6 / +3.5, F-5 | Y0 | +2 heel | 2 | +2..+7 | 3.9 / 4.0 | | |

The youth's run push drives the knee forward instead of kicking the heel up behind: with 3 rows of leg above a 5-row boot,
a heel folded behind the planted leg at 4 px thickness has no room (its shin would lie flat and 3 rows deep: THICK 2.8).
The youth's swing foot still travels behind > under the knee > in front > down, never back.

Child (legs 5 rows: the hip joint on the seat F-5, F-6 on the pass where the child rises a row (its own bob 0 / 0 / -1);
the knee row F-4 (F-5 on the pass); boots C0-C3; walk cycle 12, plants R 1:-1, -1:-3, -3:-5; the child does not run):

| f | key | phase | bob / hem | leg | role | bone H | bone K | bone A | knee | trace knee / ankle | boot | gx | lift | span | thick | stance |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | walkA | contact | 0 / F-5 | near | front: reaching into the toe-up C1 | +0.5, F-5 | +1.5, F-4 | +2.5, F-3 | 180 | +1.5, F-4 / +2.5, F-3 | C1 | +1 heel | 0 | +1..+6 | 4.2 / 4.2 | -7..+6 (14) |
| | | | | far | back onto the heel-off C2 | -1.5, F-5 | -2.5, F-4 | -3.5, F-3 | 180 | -2.5, F-4 / -3.5, F-3 | C2 | -2 toe | 0 | -7..-2 | 5.7 / 4.9 | |
| 1 | walkA2 | down | 0 / F-5 | near | planted: straight | +0.5, F-5 | +0.5, F-4 | +0.5, F-3 | 180 | +0.5, F-4 / +0.5, F-3 | C0 | -1 heel | 0 | -1..+4 | 4.0 / 4.0 | -7..+4 (12) |
| | | | | far | trailing: angled back onto its toe | -2, F-5 | -2.5, F-4 | -4, F-3 | 150 | -3, F-4 / -4, F-3 | C3 | -3 toe | 0 | -7..-3 | 6.3 / 5.0 | |
| 2 | walkB2 | pass | -1 / F-6 | near | planted: straight | -0.5, F-6 | -0.5, F-5 | -1.5, F-3 | 153 | -0.5, F-5 / -1.5, F-3 | C0 | -3 heel | 0 | -3..+2 | 4.0 / 3.6 | -9..+2 (12) |
| | | | | far | swing: one thick column angled back, the C3 lifted toe down behind the planted heel | -3.5, F-6 | -4.5, F-5 | -5.5, F-4 | 180 | -4.5, F-5 / -6, F-4 | C3 | -5 toe | 1 | -9..-5 | 5.7 / 4.9 | |

The child's swing leg at the pass is one straight column angled back to its lifted boot: a 1-row shin cannot turn without
going flat and thin. The child's knee measures (CONTACTREACH, PASSTUCK's angle and knee distance) are info, not bars.
The kin pass reaches u -9 behind (it was -7): B10 is rechecked on the youth and child looks (5.11).

### 5.6 How each pose must read (the gesture, for the artists and the blind panel). Round-4 re-plan (5.11)
At A3's chunky scale (a 10-row leg, 4 px thick) the stride is carried by the **angle of each leg and the boot keys**, the
way chibi 16-bit sprites carry it; a knee is a 1-row turn of a thick leg or the angle of the shin over its boot, never a
thin joint. Every leg keeps the idle's thickness across the bone, to the boot top.
- **Walk contact** = the reference's frames 1/2/4/7: two straight legs open in an inverted V from one pelvis; the front
  boot toe-up on its heel with the shin leaning into it (the toe cap rising past the instep), the back boot up on its
  ball, the heel two rows up; daylight between the legs from the knees down.
- **Walk down** = its frame 6: the weight on a flat front boot over one straight column; the back leg angles back and the
  boot rolls onto its toe: the push.
- **Walk pass** = its frame 3 at our proportions: one straight leg carries the body over a flat boot; the other is
  tucked behind it, the thighs one mass from the hip, the knee at the planted knee, the shin turning back to the boot
  lifted toe down behind the planted heel, daylight between the two boots.
- **Run contact**: a landing, not a stride: a flat boot under a bent knee; behind, the trailing leg runs back from the
  hip and its boot is lifted toe down: the recovery begins.
- **Run push**: one leg under the body; the other heel kicked up behind, the boot high behind the planted heel (the
  youth drives that knee forward instead).
- **Run flight**: both feet off the ground: a high front knee with the shin hanging over the landing spot, the trailing
  leg back with its foot pointed.

### 5.7 Machine-readable targets (the judge's targets.json)
The planner writes .tmp/judge/motion/targets.json (round 4: 5.11); the gates area generates test_doll.cpp's tables from it
(gen_tables.py --write) at integration. Per frame: f, key, bob, hem, plant [start, end], stance [u0, u1], apart_from,
trouser_rows, and the phase's own fields (walk pass overlap_rows and heel_behind; run push `kick` {lift_min, behind} for
the adult, knee_ahead for the youth's knee drive; flight lead_knee_above and lead_shin_dx). Per leg (near / far as drawn in
frames 0-2): hip, knee, ankle (trace.py's measures of the drawing: J2's +/-1 targets), `bone` {H, K, A} (the skeleton:
the angle bars, ALIGN and THICK use it), `angle` (the bone's knee angle), `thick` [thigh, shin] (the planner's drawing),
boot (the gates' class: K0 K1 K3; heel-offs carry `stamp` K2), gx, lift, span, gx_tol where a foot uses the registration
tolerance. `_rules` holds the round-4 bars and definitions (THICK, ALIGN, PASSTUCK, KICKUP, CONTACTLIFT, RUNSTIFF, FLIGHT,
TROUSERROWS, PATH, the envelopes). Example (adult walk pass, frame 2):
```json
{"f": 2, "key": "walkB2", "bob": 0, "hem": 10, "plant": [-3, -6], "stance": [-10, 3],
 "near": {"hip": [1, 9], "knee": [0, 6], "ankle": [-1, 3], "boot": "K0", "gx": -3, "lift": 0, "span": [-3, 3],
          "bone": {"H": [1, 10], "K": [0, 6.5], "A": [-1, 3]}, "angle": 180, "thick": [4.8, 4.8]},
 "far": {"hip": [-3, 9], "knee": [-4, 7], "ankle": [-7, 5], "boot": "K3", "gx": -5, "lift": 1, "span": [-10, -5],
         "bone": {"H": [-3, 10], "K": [-3, 7], "A": [-6.5, 5]}, "angle": 120, "thick": [5.0, 5.2]},
 "overlap_rows": [9, 8, 7], "heel_behind": 5, "trouser_rows": 4}
```

### 5.8 Fix round 1 re-plan (2026-10-21, legs-adult; R1-01 R1-02 R1-04 R1-07 R1-10)
*History (5.8-5.10): their tables are superseded by 5.11 (round 4) wherever they differ; kept for the reasoning.*
The judge's round 1 found the tables themselves wrong at our proportions (a 10-row leg on a 7 px boot): the walk pass hid
the swing boot behind the planted heel (36 % visible, a toe stub), the run push set the swing K3 beside the planted boot
on F-1/F-2 (it read as standing), the flight's lift 1 did not read and its trailing leg ran straight, the 1:4 K1 read as
a flat boot. 4.2, 5.2, 5.3, 5.6 and 5.7 above carry the re-plan; targets.json (.tmp/judge/motion) carries 5.7. In short:
- **K1** toe-up: sole runs 3,2,2 (two rows of rise), the toe cap clear of the reaching shin.
- **Walk pass**: the swing boot toe down IN FRONT of the planted shin (K3 toe +8, lift 1), the knee raised 6 px ahead.
- **Walk down**: the trailing knee bent about 30 degrees; the legs apart from F-5.
- **Run contact**: the kicked shin a 3-row block, the K4 hanging from its end on 4 px.
- **Run push**: the swing knee driven 6 px ahead, its K3 toe down under it, 2 rows clear (toe +9).
- **Run flight**: both boots lift 2; the legs apart from F-8; the trailing thigh down from the hip, the knee bent 2 px off
  the hip-ankle line.
- **Sockets**: a tilted key's own shin covers at most 1 of its pixels (its ankle row abuts the key's heel top), so B12's
  "within 2 of the flat boot" holds without counting a boot's own shin as cover.
- **The woman's ankle** a pixel in on the side that keeps a 1 px staircase into the key under it (see std.pxm's header).
- **L views**: the hips 1-2 px fuller behind, at most 1 px past the frame's back edge; the human seat inside u -3..+4, an
  elf's seat carried to u -4 behind by its `when elf` overlay.

**The gates follow targets.json**: B11 limbs' feet, stances and phase shapes in tools/tests/test_doll.cpp are generated from
targets.json by .tmp/judge/motion/gen_tables.py (`--write`; its plain run checks them), and B12 classes a toe-up sole of
runs of 2 as K1. After this re-plan the adult stances are walk 16 / 14 / 13 and run 19 / 14 / 17 (the pass and the push
widened by the L view's and the woman's fuller hips), and the flight keeps the legs apart from F-8 down (the knees
separable, as drawn). Proven with a scratch copy of the gates synced to this table: B11 limbs 0 on every adult look,
B12 0 but the pxc `boots-stamped` tag (the tooling's), B10 RUN worst 62.9 KB, LOCO 127.6 KB; the harness's J2 SELFTEST
0 fails on synth.py's frames of this table.
---


### 5.9 Fix round 2 re-plan (2026-10-21, legs-adult; R2-01 R2-02 R2-03)
The judge's round 2 named three structural faults in 4.2, 5.2 and 5.3 (the tables above carry the re-plan; targets.json
carries 5.7 and the gates follow it through gen_tables.py):
- **K1** (R2-01): the clean rotation of K0 (two leather rows over a 3,2,2 sole, the top parallel to the sole, a square tip);
  the reaching shin's F-4 row a pixel short of the cap, so the toe rises past an open instep. STAMPCRAFT 0, JX4 2 rows.
- **K2 heel-off** (R2-01): the back foot on its ball, the heel two rows up (HEELRISE 2 in every contact frame). Its 2,2,3
  sole is a runs-of-2 slope: the gates class it as K3 (targets.json `"boot": "K3", "stamp": "K2"`). The 3 px gap on F-2
  between the boots holds because the K2 keeps K0's toe slope (K3's square toe at 0 would close it to 2).
- **Walk pass** (R2-02): the swing toe at +5 (2 px ahead of the planted toe), lifted 2, its heel behind the planted shin;
  a kneecap corner at +7 on F-7, the shin angled back; spans sharing 2-4 columns; travel 10 / 7 (59 %); stance 12 +/-1.
  The legs divide by the near leg's own outline (no double outline where the thighs meet), so the swing knee is measured
  on its own pixels (J2 pass, knee_ahead 6).
- **Run contact kick** (R2-03): the shin lies back level from the knee (u -5..-1, F-6..F-5), the K4 at its end (toe -5,
  lift 2), the heel behind a row up: the ankle about 129 degrees (KICKANGLE 75-135); B10 RUN worst 59.9 KB armed, 62.8
  civil.
- Unchanged: the down frames, the run push and flight, the plants and cycles, the envelope, every front and back key.

### 5.10 Fix round 3 re-plan (2026-10-21, legs-adult; R3-01 R3-02 R3-03 R3-04)
The judge's round 3 found the pass geometry itself wrong (BOOTSPLIT: the swing boot on the planted boot in 116 of 116
pass frames), the heel strike's open instep a notch, and the run's tilted feet sticks. The tables above carry the
re-plan (targets.json carries 5.7; the gates follow it through gen_tables.py, which now also writes the walk pass's
`heel_behind`):
- **Walk pass** (R3-01): the reference's frame 3. The swing K3 lifted 1 BEHIND the planted heel (toe -5, span -10..-5,
  one column of daylight at u -4), the swing knee behind the planted knee on F-7, the shin angled back; `knee_ahead` no
  longer applies to the walk pass: `heel_behind` 5 (the swing heel 5+ px behind the planted heel, a boot of its own) in
  trace.py and B11's shape check 13, plus the judge's BOOTSPLIT. The planted thigh a pixel forward at the hip (+1, F-9).
- **Walk down**: the trailing K3 at its registered toe -3 (no tolerance used), the knee on F-6 at -3, the shin 2 px back
  from its corner; the legs apart from F-6.
- **Knees** (R3-02): every bent knee is a corner: the knee row 1-2 px out from the shin under it (down, pass, run), the
  gap under a back-bent knee open; the near leg lit down its thigh and shin.
- **Heel strike** (R3-03): K1's closed instep (4.2).
- **Run** (R3-04): the contact's trailing leg in recovery (knee F-7, the K3 pointed under it, toe -3, lift 2, ankle about
  80 degrees), the flight's trailing leg just pushed off (knee F-7 under the leading F-8, the K3 at the shin's end, toe -4,
  lift 2, the knee 1.75 px off its hip-ankle line). No K4.
- **Envelope** (section 3; trace.py ENV and B11's ruleOf): walk -7 / -8 / -10 .. +9, run -9 / -7 / -9 .. +9. B10 held:
  LOCO worst 127.6 KB (unchanged: the robe's tips sit inside its hem), RUN worst 59.9 KB.
- **Tips** (tips.pxm): the walk pass's swing toe at -6..-5 lifted 1 (the K3's toe columns), the run contact's and
  flight's trailing toe at the K3's toe (-5..-4 / -4..-3, lifted 2), the down frame's at -3.
- **Women** (one table): her seat a pixel fuller at the back except in the pass, where the seat already reaches u -4 (a
  leaning elder's hem must bare no seat pixel); the L views' hips a pixel fuller at the back on the first two visible rows
  (L hip row 9-11 px, at most 1 under the idle's 10/11), the elf overlay carrying the seat a pixel past the base seat.
- **The scabbard, facing left** (loco.clip): every side L walk frame (sheathed and drawn, man, woman, elders) names
  sheath=kick, the run's steady swung-back hang behind the legs: the pass's swing boot lies along the belt-layer rest's
  line (JX3 walk L2 / L5). The sword's walk-only `stride` key (rest's drawing, layer back) reads better, but naming it
  needs a `stride` view on every sheath-channel part (the axe, bow, hammer, horn too), or the coverage gate loses their
  looks (B6 99.8 %, 3 dead parts): AREA garments-gear adds those, then loco.clip names stride.
- The youth and the child follow (AREA kin): the same pass behind the planted heel at their scale.
- Unchanged: the contact frames (but the K1), the run push, the plants and cycles, every front and back key.

### 5.11 Fix round 4 re-plan (2026-10-21, planner; R4-01..R4-06; three owner rulings the same day)
**What failed.** The tables of 5.2, 5.3 and 5.5 bent the wrong knees, against 5.6's own gesture. The heel-strike front leg
was bent at 146 deg, with its knee and ankle in one column. The pass and run swing legs were straight (169), and the run
push's planted leg was a post (162). The art followed the tables exactly (J2 0), so the art had no knees. The legs also
thinned on every diagonal: the round-4 trailing and swing shins measure 2.2-2.9 px across the bone, against the idle's
4. The owner's words: "The legs also seem to get extremely skinny toward the end."

**Owner rulings (binding):**
1. **Thickness.** Every thigh and shin keeps the idle leg's thickness, measured across the bone, at every angle, with no
   taper to the ankle. The idle legs measure, outline included: adult 4 px, youth 4 px, child 3.5 px (A3's side idles).
   The owner rejected a walk for this once before ("his legs all of a sudden get super skinny"), so treat it as a hard
   rule.
2. **Short legs.** "For the current art style I think you have to keep the legs short. Longer legs would make everything
   weird." A3's hem stays where it is: F-10 in the side walk and run (torso=run), F-9 on the walk's down frames, F-11 in
   flight. The boot shaft stays the shin. Variant B was a boot shaft 2 rows shorter in motion, giving 6 trouser rows; it
   was planned and is dropped. Do not revisit it.
3. **The mechanics ride on the leg angle and the boot keys**, as chibi 16-bit sprites carry a stride without long limbs
   (Sea of Stars, Chained Echoes, Eastward). A knee may be implied by a 1-row turn of a thick leg, or by the shin and boot
   angle.

**The method: thick bones.**
- Each leg is a bone H > K > A, drawn as two capsules 4 px thick across the bone (the planner's raster uses radius
  2.4 px; youth 2.2, child 1.9).
- Row widths follow from the angle: a vertical bone is 4 px (far) or 5 px (near). A bone at θ from vertical needs rows
  ceil(4 / cos θ) px wide: 5 px up to 37°, 6 px to 48°, 7 px to 55°. A flatter bone is instead 4-5 rows tall all along
  its length.
- The knee's inside corner is filled (no pinch). The shin's last row is as wide as the rows above it and sits on its
  boot's top by 3 px or more.

This is how the folded and diagonal legs keep their thickness, as measured by THICK on the planner's drawing:
- the pass's swing shin, 58° from vertical: a 4-5-row block, 5.0 / 5.2 px across;
- the push's kicked shin, 79°: a 4-row block, 4.7 px;
- the flight's lead thigh, 50°: 6-7 px rows, 7.7 px;
- the down frame's trailing shin, 50°: 6 px rows, 4.6 px;
- every vertical leg: 4.8-5.0 px.

The angle bars are read from the bone, which is a plan property. ALIGN and THICK then hold the art to the bone.

**Why the bars move to the bone.** On 4 px legs the old joint measures wash bends out:
- trace.py's knee is the centre of the last trouser row, and that row spans both bones. So the pass's 120 deg bone
  measures 150-161, and the push's 122 measures 117-143 depending on what covers it.
- The flight's "knee off its hip-ankle line" measures 0.7 px for a bone 1.75 px off.
- Exaggerating the bones until the old measures pass makes the legs thin again. In the planner's flight sweep, every
  pose that passes the old check fails THICK.

So the knee angles, PASSTUCK's knee distance and FLIGHT's knee-off-line come from targets.json's `bone`. ALIGN (the bone
inside every row, with 0.5 px to spare) and THICK (3.8 px or more across; child 3.3) make the art bend where the bone
bends, at full thickness. J2's +/-1 joint checks stay on the trace values (`hip`, `knee`, `ankle`).

**The new tables** (5.2, 5.3, 5.5) against round 4:
- **Walk contact:** the front leg is straight (180), its ankle 1 px ahead of the knee; round 4 had 146, with knee and
  ankle in one column. The back leg is straight (172).
- **Walk down:** the planted leg is straight (180; was 135) and the trailing leg bent (141; was 180), as 5.6's own text
  asks.
- **Walk pass:** the swing leg is tucked. Its knee is 3 px behind the planted knee, on F-7 (was 4 px, at 169), and its
  angle is 120. The K3 stays at toe -5, lift 1, keeping one column of daylight (BOOTSPLIT).
- **Run contact:** the trailing leg runs back from the hip with a soft knee (149), its K3 lifted 1 at toe -2; KICKANGLE
  95. Round 4 had a straight diagonal (169) with the K3 lifted 2 at -3.
- **Run push:** the heel kicks up behind (122), the K3 lifted 2 at toe -4. This replaces round 4's knee drive with the K3
  at toe +9 (169). The planted knee is soft (149; was 162).
- **Run flight:** both feet are up. The trailing K3 is lifted 1 at toe -3 (was lift 2 at -4); the lead knee is up (130).
- **Kin:** full hip, knee and ankle (bone) tables. The youth and child pass behind the planted heel at their scale (was in
  front: R4-05). The youth's run push drives the knee forward, because its leg cannot fold behind at 4 px.
- **Unchanged:** plants, cycles, bob (walk 0 / +1 / 0, run 0 / 0 / -1), hems, and stamps (K0-K3; K4 unused).

**The bars** (run_r4.sh, B11, J2): what short legs can honestly hold, and why.

| check | round 4 | round 5 (this re-plan) | why |
|---|---|---|---|
| CONTACTREACH | the front knee >= 165 and the ankle >= 1 px ahead (measured) | kept, on the bone (with ALIGN) | the one bar short legs hold fully: the reference's frames 1, 2 and 4, and the owner's "a straight reaching front leg into a toe-up boot" |
| PASSKNEE becomes PASSTUCK | swing <= 150, the knee <= 3 px behind | the swing bone <= 155 and its knee <= 3 px behind the planted knee; the thighs one mass on overlap_rows; the swing boot lifted >= 1, toe down, its toe behind the planted heel; BOOTSPLIT 0. Child: the angle and distance are info | the owner wants "the swing leg tucked close under the body". At 4 px a 1-2 px bend is only the contour, so the tuck and the boot angle carry the pose. <= 155 still forbids round 4's straight trailing leg (169) |
| SWINGFOLD (push) becomes KICKUP | push swing <= 120 | adult push: the swing bone <= 135, the boot lifted >= 2, its toe >= 2 px behind the planted heel. Youth knee drive: <= 125, lift >= 1, the boots apart | the owner's push is the heel kicked up behind with the boot high. The bone is 122 (it would pass 120); 135 leaves room for a 1-row knee |
| SWINGFOLD (contact) becomes CONTACTLIFT | contact trailing <= 130 | the trailing boot lifted >= 1, toe down, behind the hips; KICKANGLE 75-135 kept | a contact leg folded at 4 px lays its shin flat over a toe-forward boot: KICKANGLE < 75, round 2's "club". The chibi contact runs the leg back with the boot raised |
| RUNSTIFF | no run frame with both knees >= 155 (measured) | kept, on the bone | holds (139 / 122 / 130) |
| TROUSERROWS | >= 4 in every frame (youth >= 3) | >= 4 on contact, pass and run; >= 3 on the walk's down frames (youth 3 / 2; child info) | the short legs stay. The down frame's 1 px bob drops the hem to the idle's own row, where A3 shows 3 trouser rows (youth 2). Getting 4 there would need the hem held on every garment or the bob dropped, both owner rules |
| THICK (new) | none | every bone >= 3.8 px across (adult, youth), >= 3.3 (child), sampled every 0.25 px; the shin's last row on its boot by >= 3 px | the owner's ruling. It replaces B11 check 3's horizontal >= 3 px run as the thinness bar (check 3 stays as a floor): a diagonal drawn 3 px wide passes check 3 but is 2.1 px thick |
| ALIGN (new) | none | on every visible row, the bone lies inside the leg's run with 0.5 px to spare | holds the art to the bone, so the bone's angles are the art's |
| FLIGHT | the measured knee >= 1 px off its line | on the bone (lead 1.75, trail 1.33) | the measured knee washes out on 4 px legs |
| PATH | info for adults; kin d1 >= -1 and d2 >= 3 | d1 info for every body; d2 >= 3 for every body | the toe-off foot lifts where it stood: d1 = -2 px body-relative is +1 px in the world, because the frame advances 3 px. The pass behind the heel needs this |
| envelope | run push -7..+9; kin -7 | run push -9..+9 (the kicked boot); youth and child pass -9 | |

Kept unchanged: BOOTSPLIT, INSTEP, HEELRISE, STAMPCRAFT, KICKANGLE, JX1-JX5, B10, B11 checks 1-2 and 4-11, B12, and
registration. The definitions are in targets.json `_rules`.

**Matched-leg check against the reference.** The reference's 18 sp leg is drawn as our 10 px leg; x is measured from the
pelvis centre + 0.5.

| pose | reference (matched) | ours (bone) | note |
|---|---|---|---|
| walk contact, front | hip +1.9 > knee +3.5, F-5.5 > ankle +4.4, F-2.8; straight, +19° | +2 > +3, F-6.5 > +4, F-3; 180, +18° | match |
| walk contact, back | -0.9 > -2.4 > -3.3, F-2.8; straight | -1 > -2.5 > -4, F-4; 172 | our ankle is higher: the K2's heel is two rows up |
| stance: contact / down / pass | 1.64 L / 1.46 L / 0.84 L = 16 / 15 / 8 px | 16 / 15 / 14 | our pass is wider: our K3 hangs at 27° with a column of daylight; the reference's boot hangs at 45-55° and overlaps |
| walk down, trailing | the knee bent about 25° (about 155), the heel up 34° | 141; K3 at 27° | our planted foot is under the hips (registration plants the down frame's heel at 0); the reference's front leg still reaches |
| walk pass, swing | the knee hidden at the planted knee; the shin about 45° back; the toe 1.4 px behind the planted heel | the knee 3 px behind, on F-7; 120; the shin 58° back; the toe 2 px behind | ours is stretched back by the K3's length and low tilt |

The reference has no run. Our run poses are the standard chibi run: a landing, the heel kicked up behind, both feet up.

**Integration (R4-06).** One owner, the gates area (engineer-high), does exactly this, after the art round's last map edit:
1. Do not merge fix_r3/kin/targets_kin_amend.json. The 5.11 kin tables supersede it, and its in-front pass and joint-less
   feet are what R4-05 rejected. Merging it into a scratch copy only reproduces the round-4 baseline ("J2 0 on today's
   kin art").
2. Extend gen_tables.py:
   1. write the kin ShapeT tables (yw, yr, cw), and make test_doll.cpp's shapeOf return them;
   2. write a BoneT table from `bone` (H, K and A per phase and leg);
   3. for the adult run push, set kneeAhead to 0 (the swing leg is now behind) and add ShapeT fields kickLift and
      kickBehind, read from `kick`;
   4. take ruleOf's envelopes from `_rules.envelope` (adult run push -9; youth and child walk pass -9) instead of the
      hand-written numbers;
   5. make its check mode compare the new tables too.

   Then run `gen_tables.py --write`. Today it reports TABLES 12 checks, 11 fails against the new targets.json, as
   expected.
3. In test_doll.cpp B11 limbs, keep check 3 as a floor. Add check 14 THICK and check 15 ALIGN, using the `_rules`
   definitions and BoneT. Check 13 gains the push's KICKUP: the swing sole is lifted at least kickLift, and its toe is at
   least kickBehind px behind the planted heel. Checks are updated, never deleted.
4. In trace.py (J2):
   - take ENV from `_rules.envelope`;
   - read the flight's knee-off-line and the knee angles from `bone`;
   - drop the adult push's knee_ahead branch and add KICKUP;
   - add THICK and ALIGN per traced leg, skipping chords that end on the other leg and merged rows;
   - keep the +/-1 joint checks on hip, knee and ankle.
5. In synth.py, draw the proof frames as 4 px capsules along `bone`, so that J2 SELFTEST exercises THICK and ALIGN. Add
   two broken variants that must fail: bad_thin (a 2 px diagonal shin) and bad_straightpass (the swing bone straight).
6. The judge's run_r4.sh and bends.py follow `_rules`. The judge owns that harness; the gates area mirrors the same
   definitions in run_all.sh's J2, so fixers can rerun them.
7. Add THICK, ALIGN and TABLES lines to integrate.sh. Run it LAST on the frozen tree
   (`BDIR=build bash .tmp/judge/motion/integrate.sh OUT`) and report INTEGRATE TOTAL 0, with gen/doll_pool.cpp newer than
   every map. Nobody edits a map after that.

**The next art round, by area and file:**
- **legs-adult (artist-xhigh):** redraw std.pxm's side walk and run views (R, L and `when woman replace`) as
  full-thickness legs on the 5.2 and 5.3 bones. Move tips.pxm's toes to the new feet: the run contact's trailing toe -2,
  lift 1; the push's kicked toe -4, lift 2; the flight's trailing toe -3, lift 1. The walk tips are unchanged.
- **kin (artist):** redraw kin/youth/legs/std.pxm (and its `when girl replace` views) and tips.pxm, and
  kin/child/legs/std.pxm and tips.pxm, on the 5.5 bones; adjust the youth hangs.
- **garments-gear (artist):** fix the hangs over the new run legs (the coat, kilt and rags back panels over the heel
  kicked up behind at the push), the scabbard (JX3: its L view over the kicked leg) and the elder's stick at the pass.
- **Unchanged:** no stamp, clip, plant, garment hem or rig-1 change.
- **B10:** the adult push's boot reaches u -9 (the contact already reaches -7 and the flight -8), so recheck RUN; the
  civil hood woman, at 62.8 of 64 KB, is the closest. The kin pass reaches -9, so recheck the youth and child LOCO.

**Round 5 waves.** Keep them few, and run independent work in parallel:
1. **In parallel:**
   - legs-adult draws the slice: the man's R walk and run, full thickness on the bones, judged at 1x-8x and on
     THICK/ALIGN.
   - The gates area extends gen_tables, B11, trace and synth (integration steps 2-5). It proves the work on synth frames
     and on today's tree. Today's tree must FAIL THICK where the owner saw skinny legs: the round-4 trailing and swing
     shins measure 2.2-2.9 px across.
2. **After the slice passes,** in parallel: the L views and the woman's views, tips, the kin, and garments-gear.
3. **Last:** run integrate.sh (step 7) on the frozen tree, then the verifier, the blind panel and the judge.

**The mock** is the planner's composite, not game art. It shows the hero's real head, torso and arms, with the legs drawn
from these tables, the real boot stamps and the hero's own ramps:
- C:\Users\joshu\AppData\Local\Temp\claude\emb_P2b\motion\replan\mock_A.png: the default hero and the A3 hero, 6 walk and
  6 run frames, at 8x and 4x, beside the reference at matched leg length;
- anim_A_defhero_4x.gif and anim_A_hero0_4x.gif in the same folder, at game speed.

The scripts are in ...\replan\scripts: frames.py holds these tables, check3.py the bars, mock.py the mock.

## 6. Drawing the legs (rules for every leg drawing in the side walk and run)

1. **A limb in every frame**: the trouser thigh from the seat to the knee, the leather shin from the knee to the ankle, the
   boot key on the socket. Each row of a leg touches the row above (4-connected). There is no gap and no hard step at the
   knee or the ankle; a boot more than 2 px off its knee reads "disconnected" (runfix_r1's rule, kept in the new harness).
   Since round 4 (5.11), a leg is a thick band around its bone (targets.json `bone`), so its edges step by the bone's angle.
   A 2 px step is fine on a diagonal bone; a 1 px pinch is never fine.
2. **The seat**: the drawing's top row, one above its first visible row (F-10 on the walk's contact and pass keys, whose
   origin y grows from 9 to 10; F-9 on the down keys; F-10 on the run's contact and push keys; F-11 on the flight keys), is
   one solid seat row across both thighs, finished: humans keep it under the hem, elves show it (their non-pivot anchors sit
   1 px up). No stretch rows in walk and run keys: a doubled row would jag a diagonal limb.
3. **The pelvis keeps the idle's width**: the first visible row at most 1 px narrower than the idle's (man R 8 / L 10; the
   woman per her own idle) in every frame; at the pass the two thigh tops sit side by side 3 px apart, not stacked.
4. **Thickness, measured across the bone** (owner ruling 2026-10-21, a hard rule, 5.11). Every thigh and shin keeps the
   idle leg's thickness at every angle, with no taper to the ankle: adult 4 px, youth 4 px, child 3.5 px, outline
   included.
   - A vertical bone is 4 px wide (far) or 5 px (near).
   - A bone at θ from vertical needs rows at least ceil(4 / cos θ) px wide: 5 px up to 37°, 6 px to 48°, 7 px to 55°.
   - A flatter bone (the kicked shin, the swing shin) is instead 4-5 rows tall all along its length.
   - The inside corner of a bent knee is filled, never pinched.
   - The shin's last row is as wide as the rows above it and sits on its boot's top by 3 px or more.
   - Measured by THICK: 3.8 px or more across (child 3.3). No leg run is ever under 3 px.
5. **Knees** (short legs, 5.11). At this scale the knee is implied, not drawn as a joint: it is a 1-row turn where the
   shin leans harder than the thigh, or the angle of the shin over its boot. The leg's outline carries it, with the outer
   side of the bend a corner and the inner side a filled notch. A straight leg has no corner, and there is never a mushy
   two-step knee. The bend is where the table's bone bends (ALIGN).
6. **Ankles**: the shin's last row on the key's socket (section 4.2). The far shin may end under the near leg, never short
   of its boot.
7. **Light**: the near leg lit on its top-left edge (A3's band: step 3 left, step 1 and the outline right), the far leg a
   value darker; where they overlap the near leg's own outline separates them (no banding, no pillow shading); the finishing
   pass lights the outline (never paint it).
8. **Regions**: T above the knee row, K on it (the run keys: every visible trouser pixel K, so a kilt's bare leg shows under
   its flare, as today), S the shin, B and O from the stamps only. Looks recolour them: rags give bare shins and bare feet
   that tilt the same way; armour boots and greaves recolour S and B.
9. **The woman**: her own `when woman replace` views at the same placements (one walk table); her back hip a pixel fuller on
   the first visible row, her shin's last row a pixel in. Drawn and judged in trousers and bare legs; under a long skirt
   only tips.pxm shows her feet (and every long-garment wearer's).
10. **L views**: the R placements mirrored (x' = -x - 1), every run re-lit for the top-left light; never the plain mirror.
11. **Variety, not recolours**: the youth's and child's legs are their own drawings at their scale, never the adult's
    shrunk; her legs are hers.

## 7. Hems and garments in side walk and run frames

1. **The side walk takes torso=run** in every frame (man, woman, elders, the drawn walk): each garment's side `run` view,
   the hem a row up (F-10; F-9 on the down frames). Every man and woman garment and the mail already has R/L run views, so
   this starts with no map work. The woman's side down frames drop torso=swing: her swing views equal rest for every garment
   but the dress, and the dress is long (tips only); garments-gear checks the dress's run view in a walk. Front and back
   walks keep their torsos.
2. **The run keeps torso=run** (F-10; F-11 in flight).
3. **Hangs over the legs** (hangFront coat, kilt, poncho, apron, smock, culture tunic, the woman's jerkin, rags; the mail
   skirt in the mail torso, to F-6) in the side walk/run keys (wide on contact and down, rest on the pass, flare in the run):
   the hem never hides a leg's knee and shin. Over the scissor the hang opens: its front edge rides the leading thigh, its
   back edge the trailing thigh, with a notch or a lifted edge between the legs so both knees and shins show; at the pass it
   closes over the overlapping legs; in the flight the leading knee pushes the front panel forward and up. A hang's rows
   below the leading knee lift; they never cut a shin into a stub.
4. **Long garments** (dress, robe, gown, kaftan, wrap: longGarment()) draw no legs. tips.pxm shows the toes below the hem
   at the same keys and positions as the legs (the front toe tipped up at heel strike, the trailing toe on the ground, the
   swing toe pointed down, a kicked foot hidden under the skirt), each tip being the stamp's toe columns (`put ... cols`).
5. **Belt and held items**: the scabbard's R view (layer back) stays behind the legs; its L view (belt layer, over the legs)
   must not cut the near leg or read as a third boot: its tip rides above the near knee row, or behind the shin's edge, in
   every side walk/run frame. The belt tools (axe, hammer F-3, horn F-7), the bow and the staff (staff_run / staff_runl)
   never cover more than a leg's edge column below the knee. The elder's stick keeps its planted tip (the advance stays
   3 / 3 / 3) and never lands on a boot.
6. **Arms**: the hands (F-11..F-10) may overlap the thigh tops (B11 measures legs under garments, not arms), but a hand never
   erases a knee.
7. **The youth's skirts are hangs** (kin/youth/wear/*_hang: rest / wide / flare): their wide and flare keys lift the hem a
   row in the side walk and run (AREA kin).

## 8. The stamp contract (pxc; AREA tooling)

```
# rpg/art/doll/maps/stamps/boots_adult.stamps   (also boots_youth, boots_child; pxc reads maps/stamps/*.stamps)
use person
# (illustrative pixels only: the artist draws the real keys to section 4.2)
stamp K1n R origin 0 3        # name, facing, origin = the key's ground pivot in its own grid (heel or toe, sole row)
|....ff.
|fhhhhgf
|fihhKKK
|KKKK...
regions
|....BB.
|BBBBBBB
|BBBBOOO
|OOOO...
```
- Names: `<key><tone>`: K0n K0f K1n ... K4f (adult), Y0n ... (youth), C0n ... (child); one entry per facing (R, L).
- In a part: `stamps boots_adult` (a directive before the first view, like `use`). In a view block:
  `put <name> <dx> <dy> [under] [cols <a>-<b>]`: the stamp's origin lands pivot-relative at (dx, dy), y down like the
  clips' anchors (dy = -lift); the view's facing picks the stamp's facing entry; `under` draws it beneath the view's own
  pixels (the far boot under the near leg), the default draws it over them; `cols a-b` keeps only those stamp columns (the
  shoe tips). The stamp's regions land with its pixels.
- pxc expands puts into the view's cells and region plane before the craft checks; the pool format does not change; the
  incremental cache keys on the stamp files too.
- `tags boots-stamped` (std.pxm, tips.pxm, the youth's and child's legs): in an R or L view of a walk or run key, a B or O
  region cell not written by a put is an error naming the part, view and cell.
- Library checks: every stamp a part puts exists for that facing; an L stamp equal to the plain mirror of its R stamp is an
  error; the craft checks (orphans, lone-colour noise) run on each stamp; each stamp's O cells are Ink.
- `node tools/doll/pxc.mjs --placements FILE` writes one JSON record per put: part, facing, key, when, stamp, dx, dy (the
  judge's boot tracer). `--report` lists each stamp's uses.
- doll.h's ".pxm format" block documents all of it.

## 9. Gates (`rpg_test --doll --gates`; updated to these rules, never deleted to pass)

B11 splits in two. **B11 (front and back)**: the existing checks, unchanged. **B11 limbs (side walk and run frames, R and
L)** for the looks: the A3 hero, the A3 woman (dress: tips; her skirt check stays), the woman in shirt and trousers, the
woman bare-legged, and, new, the youth boy and girl, the man elder (walk), the elf man and the default hero.

| # | check, per frame | bar | status |
|---|---|---|---|
| 1 | visible leg area vs the same facing's idle | 90 % or more | kept |
| 2 | hip row (the first visible leg row of 3 px or more; strays ignored) vs the idle's | at most 1 px narrower | kept (fixes the woman's L idle, whose first row is a 1 px stray) |
| 3 | narrowest run of leg pixels holding no B/O pixel (a probe recipe colours B and O apart, test side only) | 3 px or more | a floor only since round 4: THICK (14) is the thinness bar |
| 4 | hip row height vs the contact frame's | within 1 | kept |
| 5 | walk down frames' span vs contact | no wider | kept |
| 6 | continuity: the legs one 4-connected piece; no empty row from the hip row to the lowest leg row | 0 faults | new |
| 7 | ground: an Ink sole pixel on row F in every walk frame and run contact/push; none in run flight | exact | new |
| 8 | registration: a sole run on row F whose heel end matches plant0 (R: its least x = plant0; L: minus its greatest x = plant0), walk frames and run contact/push | 1 px | new |
| 9 | envelope: every leg pixel inside R walk [-7, +9] / [-8, +9] / [-10, +9], run [-9, +9] in all three (round 4); youth and child walk pass -9; L mirrored (targets.json `_rules.envelope`) | exact | updated (round 4) |
| 10 | stance vs section 5 (round 4: walk 16 / 15 / 14; run 17 / 14 / 18; youth walk 14 / 12 / 12, run 14 / 11 / 15; child 14 / 12 / 12) | table + 1 | new (replaces "13 px") |
| 11 | the default look's trouser body (Cloth2 step 1) luma | 52 or more | new (section 14) |
| 12-13 | the feet on the table; the phase shapes (contact apart_from, the pass's overlap and heel_behind, the push's KICKUP: the swing sole lifted `kick.lift_min`, its toe `kick.behind` px behind the planted heel; the flight) | table | round 4: KICKUP replaces the adult push's knee_ahead; the kin get shapes too |
| 14 | THICK: each bone of the table (`bone`), every 0.25 px, the leg's run through the axis pixel projected across the bone (along the row x cos θ within 45° of vertical, down the column x sin θ beyond) | >= 3.8 px (youth 3.8, child 3.3); the shin's last row on its boot by >= 3 px | new (round 4, the owner's thickness ruling) |
| 15 | ALIGN: the bone's x on every visible row inside the leg's run, 0.5 px to spare | exact | new (round 4) |

`--report` prints every frame's numbers (area, hip, narrowest run, bob, stance, pieces, sole on F, heel vs plant, envelope).
**B12 boots**: pxc refuses hand-drawn boot pixels and bad stamp libraries (`pxc --check`, CI); `rpg_test --doll` checks the
`boots-stamped` parts are in the pool with their side walk/run views (so the tag cannot quietly vanish).
**Kept**: B1 parity (the idles, the swing keys and `stance` at 0 px), B2, faces, hands, B8 craft, B9, B10 (LOCO 128, RUN 64,
DEATH 48, POSTURE 64 KB per look, the armed and civil sweeps), the ramps' delta E 6, the walk table's defaults contract.
**The old harness rules, rewritten** in the new harness: J2's "contact stance over 13 fails" and "back heel more than 5 px
behind the hip anchor fails" become the table's stance and envelope; the boot finder (4+ Ink sole pixels on a leather blob)
also accepts tilted Ink soles and must find exactly two boots a frame; J3d's "apart in every row" becomes "apart from the
table's `apart_from` row in contact and flight frames, overlapping at the pass"; J3e's "flight 13 px" becomes the flight
rules of 5.3; runfix's "knee step over 2 px = DISCONNECTED" is kept.

---

## 10. Areas (one owner each; disjoint files)

**Start state.** The tree holds the three uncommitted leg-pass rounds: the garments' side `run` and `flare` views, the mail's
run views, the youth's legs, std.pxm's side run keys and loco.clip's torso=run in the run. Keep the garment and mail work
(the side walk now uses those run views); the side walk and run leg keys are replaced. gen/doll_pool.cpp is stale (std.pxm
and loco.clip were edited after the last compile) and the tree as it stands fails B10 (the armed elf man's RUN at 64.1 KB):
nobody "fixes" that state; the slice's first compile replaces those keys.

Every area: wrap heavy runs in tools/slot.sh; build with its own BDIR
(`tools/slot.sh cmd //c "set BDIR=<dir>&& tools\\build.bat <target>" < /dev/null`, check the ninja lines); test map edits
on a scratch pool (`node tools/doll/pxc.mjs --bin <scratch>/x.pool --no-cpp`, then `rpg_test --pool` / `doll_gallery --pool`);
run pxc (which rewrites gen/doll_pool.cpp, never hand-edited) right before any build of the game; screenshots under
C:\Users\joshu\AppData\Local\Temp\claude\emb_P2b\motion\<area>\; no git commands that change state; never kill a Desktop
Embervale.exe; foreground checks; no screenshot or iteration caps. **Shared and owned by no area**: rpg/art/doll/gen/doll_pool.cpp
(generated; the integrator regenerates it and rebuilds BDIR=build all targets at the end of each stage).

### AREA tooling (engineer; wave 1; BDIR build-mo-tool)
Files: tools/doll/pxc.mjs; rpg/art/doll/doll.h (the format comment block only); rpg/view/script_p2b_slice.cpp (slicelook
gains `civil=`, test only, so the judge can walk an elder with the stick).
- core: section 8's `stamps`, `stamp`, `put` (dx, dy, under), the `boots-stamped` check, the library checks.
- depth: `cols` windows, `--placements`, cache keying on stamp files, the craft checks on stamps, `--report` stamp uses.
- delight: an error that names the stamp, view and cell and prints the offending row with a caret.
- done: `pxc --check` passes on the tree; a scratch map using stamps compiles to the same pool bytes as its hand-expanded twin
  (a script left in the area folder, rerun by the verifier); a hand-drawn B pixel in a `boots-stamped` side walk view and a
  plain-mirror L stamp each fail with a named error; `--placements` writes JSON; `rpg_test --doll --gates --parity` no worse
  than before; art_hash TOTAL 317eef6efe4d8229.

### AREA tone (engineer; wave 1; BDIR build-mo-tone)
Files: rpg/art/doll/doll_ramps.cpp. Brief: section 14.
- done: the default look's Cloth2 step 1 has luma 52 or more (doll_gallery `ramps` / `--palette look` with
  bottomColor=#4E3C2C); A3 parity 0 (B1); the ramps' delta E 6 gate passes; art_hash and golden_village_art unchanged.

### AREA gates (engineer; wave 2, parallel with the slice; BDIR build-mo-gates; engineer-high after two failures)
Files: tools/tests/test_doll.cpp; .tmp/judge/motion/** (the harness, section 12).
- core: B11 limbs and B12's test side (section 9); the harness stages j1-j4 with targets.json, trace.py, boots.py,
  sheets.py, the witness configs, the blind panel's prompts and image lists.
- depth: the probe recipe; the `--report` per-frame lines; the target skeleton drawn over every traced frame at 8x; the
  reference-beside-ours sheet at one scale.
- delight: one summary line per stage (`J2 TOTAL <frames> frames, <fails> fails`), and a fail list that names the frame,
  the leg, the measure, the target and the value.
- done: run on the CURRENT tree first: B11 limbs and the J2 trace together fail the old side walk and run for the right
  reasons (J2 names flat boots where K1 / K2 / K3 / K4 are planned, the flight's two straight legs, the pass with no
  crossing, the stance off the table) and pass the unchanged front and back walks; the harness runs end to end (every sheet
  and the in-game clips); then the same on the slice.

### AREA legs-adult (artist-xhigh; waves 2 and 3; BDIR build-mo-legs)
Files: rpg/art/doll/maps/legs/adult/loco/std.pxm, tips.pxm; rpg/art/doll/maps/stamps/boots_adult.stamps (new);
rpg/art/doll/clips/loco.clip; rpg/art/doll/clips/postures.clip.
- **Wave 2, the motion slice** (the style slice of this rebuild; everything else must match it): the 10 R stamps (K0-K4,
  near and far) drawn as one boot; the man's R walk and run keys and the woman's R replace views for them, per sections 5
  and 6; the man's and woman's sheathed R side walk and run clips (torso=run, the new plants). Order: the stamps at 12x
  until they read as one boot, the man's walk, his run, her views, the clips.
- **Wave 3**: the 10 L stamps; the L views (man, woman) and L clips; tips.pxm (R, L) from the stamps' toe columns; the drawn
  walk and run clips (ACTION variants 1 and 2) and both elders' side walks (same keys, plants and lean); postures.clip:
  carry, dance and play composed and checked at 4x, changed only if they read wrong.
- core: every frame on the table within tolerance; stamps only; continuity; chunky; registration.
- depth: the near/far value split, kneecaps, the lit toe caps, the hem's seat row for elves, her legs her own.
- delight: the swing toe flicks down a pixel before the strike; the landing boot in the run sits a hair flatter than the walk's
  heel strike, so the two gaits feel different underfoot.
- done: B11 limbs and B12 at 0 for the hero, the woman (all three), the elder and the elf; the J2 trace 0 fails on every adult
  side frame (R, L; sheathed, drawn, elder); B10 within budget; coverage 100 %, 0 dead; the blind panel passes on the slice
  (R) and on the full set.

### AREA kin (artist; wave 3; BDIR build-mo-kin)
Files: rpg/art/doll/maps/kin/youth/** and rpg/art/doll/maps/kin/child/** (their legs; the youth's and child's wear where it
covers the legs); rpg/art/doll/maps/stamps/boots_youth.stamps, boots_child.stamps (new); rpg/art/doll/clips/kin_youth.clip,
kin_child.clip.
- core: the youth's side walk and run and the child's side walk per 5.5, their stamps, their plants.
- depth: the youth girl's own hips; the youth's skirt hangs lifting in the walk and run (section 7.7).
- delight: the child's step a touch bouncier within its own bob.
- done: the youth boy and girl in B11 limbs at 0; J2 0 fails on the youth and child side frames; coverage 100 %, 0 dead.

### AREA garments-gear (artist; wave 3, after the slice fixes the legs; BDIR build-mo-garb)
Files: rpg/art/doll/maps/wear/man/**, wear/woman/**, armour/man/**, armour/woman/**, belt/**, held/civil/**, held/props/**,
arms/** (only views that collide with the new legs).
- core: section 7 for every garment, armour piece, belt and held item that shows over the side legs.
- depth: hang edges that follow the scissor; the mail skirt split over the knees; the scabbard's L view cleared.
- delight: a coat's back panel lags the trailing thigh by a pixel in the run.
- done: the harness's garment check (each leg's knee row and 2 or more shin rows visible below any hang; long garments: tips
  on the planned toe columns) at 0 on the garment looks of section 12; the spur check at 0; exactly two boots a frame in
  every armed L frame; blind lens B passes on the garment sheet.

## 11. Waves

1. **Wave 1** (parallel, short): tooling (pxc stamps; the slicelook field), tone.
2. **Wave 2** (parallel): legs-adult's motion slice; gates (B11 limbs and the harness, proven on the old tree first).
   **The slice check**: J1 and J2 on the R frames, the J3 sheets, the blind panel on R images, the judge's eye. Up to three
   slice rounds; wave 3 starts only on a passed slice.
3. **Wave 3** (parallel): legs-adult's L / tips / clips / postures; kin; garments-gear.
4. **Integrate**: pxc, rebuild BDIR=build all targets, every gate. Then the verifier, the blind panel and the judge
   (4 rounds at most; a code item to engineer-high after failing twice, a visual item to artist-xhigh after failing three
   times).

## 12. The judge protocol (stated in full for round 1; additions only for new, real problems, named as such)

The judge tests from the state a user starts in (the default hero in the start village, rig 2, a phone window), measures
before it believes, and fixers rerun the judge's own stage before claiming a fix. Look and feel first.
Harness: .tmp/judge/motion/run_all.sh STAGE OUTDIR (Git Bash; each stage under 10 min in the foreground; outputs in
C:\Users\joshu\AppData\Local\Temp\claude\emb_P2b\motion\judge_rN\).

**J1 build and gates**: `node tools/doll/pxc.mjs --check` (fresh) and `--report` (stamp uses); build BDIR=build, all
targets; `build/rpg_test --doll --gates --parity` 0 failures, with the `--report` B11 limbs, B12 and B10 worst-look lines;
`--doll-coverage --gates` 100 %, 0 dead; `--doll-variety --gates` passes; `build/art_hash.exe` TOTAL 317eef6efe4d8229;
`build/city_gallery.exe --village-art-check` 65/0; `build/rpg_test --anim --gates` 0 (golden).

**J2 trace** (per look x facing R, L x every side walk/run frame; the drawn walk and run, the elder's walk, the youth's and
child's tables): dumps from `rpg_test --doll --frames` (A3 palettes) plus the probe twins, and doll_gallery renders of the
look list with `--palette look`; `pxc --placements` for the boots.

| measure | bar |
|---|---|
| knee row and ankle (row and band centre) of each separable leg vs targets | +/-1 |
| hip row span vs the table's union | +/-1 |
| each foot's key (placements + its composed pixels equal the stamp's, minus what the near leg covers), gx, lift | exact (gx_tol where marked) |
| stance | +/-1 |
| contact frames: background between the legs on every row from `apart_from` down; 3 px or more at F-2 (walk) | exact |
| pass: the legs one run on overlap_rows; the swing heel behind the planted heel (round 4: PASSTUCK on the bone, 5.11) | heel_behind or more |
| run push: round 4 KICKUP (adult: the swing sole lifted `kick.lift_min`, its toe `kick.behind` px behind the planted heel); the youth's knee drive: the swing knee ahead of the planted leg | as stated; 3 px or more |
| flight: lead knee row above the trail knee row; lead shin dx; both lifts; each bone's knee off its hip-ankle line (round 4: read on `bone`) | 1 or more; 1 at most; 1 or more; 1 px or more |
| continuity: one piece, no empty row, no 1 px pinch, no boot over 2 px off its knee (round 4: edge steps follow the bone's angle) | 0 faults |
| THICK and ALIGN (round 4, 5.11; targets.json `_rules`): every bone 3.8 px or more across (child 3.3), the shin's last row on its boot by 3 px; the bone inside every row | 0 faults |
| registration from frames + plants (planted heel; trailing toe K2 > K3) | 1 px |
| chunky: area, hip row, narrowest non-boot run (the floor; THICK is the bar) | 90 %, idle - 1, 3 px |
| envelope; B10 echo | section 3; the gate's budgets |
| garments: each leg's knee row and 2+ shin rows visible below any hang; long garments' tips on the planned toe columns | 0 faults |
| spurs (legpass_r2 spur.py): 1 px garment spurs of 3+ px into empty space | 0 |
| boots per frame (Ink-sole finder + placements) | exactly 2 |

Prints `J2 TOTAL <frames> frames, <fails> fails`: PASS at 0.
Looks: the A3 hero, the woman in trousers, the woman bare-legged, the A3 woman (dress, tips), the default hero, the armed man
and woman drawn, the youth boy and girl, the man and woman elder, the elf man, the half-breed man, and the garments coat,
kilt, poncho, smock, apron, mail, culture tunic, rags, robe (tips), gown (tips).

**J3 look**: sheets (sheets.py): (a) legs at 8x with the traced skeleton (white) and the target skeleton (near yellow, far
violet) over every frame, per look, R and L, walk and run; (b) the boot key sheet at 12x: every stamp, R and L, near and far,
beside each of its uses in situ; (c) game-colour strips at 1x, 2x and 3x (full figures: hero, woman in trousers, default
hero, youth, elder, elf); (d) the reference beside our walk at one scale (the study's 4x layout); (e) onion skins
(registration at a glance; doll_gallery `onion`); (f) the garment sheet (the garment looks, walk and run R, 4x). The judge
looks at 1x-3x first (does it walk like the reference? does the run read as a run?), then traces hip > knee > boot in EVERY
frame at 8x. Then the blind panel (section 13).

**J4 in game** (WITNESS_MAX_PROCS=5, headless): witness tests in .tmp/judge/motion/w/ from the legpass_r1/w and owner-pack
wclip patterns: seed 7, params {rig 2, hour 11, god, at [-140, -432], window [2556, 1179]}, calls weather clear, ui 0, ring 0,
then camhold 1 (registration) or the follow camera (the look); inputs d (east) and a (west) held 0.9-2.6 s; `skill sneak 1`
for the walk; `j` before moving for the drawn walk and run; looks by slicelook: the default hero (none), a3, a3woman outfit=0,
age=2 (and body woman), age=3 civil=1 weapon=9, people=2, and the garment looks (outfit / garb / bodyform values).
- clips: per case an APNG (`shot --clip --follow "state.player.pos" --size 96x64 --zoom 4`) and a filmstrip sheet of every
  frame over 1.2 s; the default hero's 4x and 8x crops (legpass_r3 ingzoom.py).
- measures: `witness measure registration --pos "state.player.pos" --hold-camera` on the default hero walking and running
  east and west: foot slide at frame changes 1 px at most; glide within a frame at most the frame's advance (3 walk,
  5 run stance). The default hero at 11:00 on grass: the visible trouser pixels' median luma 52 or more (legtone.py), the
  leg outline ring against the grass ring 40 luma or more (STYLE_P2 8).

**J5 owner pack** (after a PASS; the lead sends it): the walk and run APNGs at game speed (default hero, the woman in
trousers, the youth, the elder), the reference beside our walk at one scale, the 8x strips, the boot key sheet, and before
(275229c) beside after.

## 13. The blind panel (three lenses)

Three fresh agents, one per lens, told nothing about the history, the fix, the reference's origin or each other. Each gets
its prompt unchanged and only its images (made by sheets.py into judge_rN\panel\), and replies with JSON only. The panel
passes when all three lenses pass in the same round; "mostly" fails the lens, and its worst frames or defects become fix
items, each with the harness measure that would catch it.

**Lens A, motion.** Images: docs/ref/motion/owner_walk_ref.png; walk_legs_8x.png (the default hero and the woman in
trousers, idle + walk 0-5, game colours); walk_full_3x.png; ingame_walk_strip_4x.png; run_legs_8x.png;
ingame_run_strip_4x.png. Prompt: "Image 1 is a reference walk cycle. The other images show a different pixel-art character
walking and then running sideways, frames in order. Ignore the art style and the proportions; compare how the legs and feet
MOVE. Answer only from what you see. Reply with JSON only: {"walk_scissor": "yes|mostly|no" (the legs open into an
inverted V at contact and cross at the passing pose), "walk_feet_angled": "yes|mostly|no" (the front foot toe-up as it
lands, the planted foot flat, the back foot rising onto its toe, the swinging foot toe-down), "walk_planted_leg_carries_body":
"yes|mostly|no", "walk_matches_reference": "yes|mostly|no", "run_reads_as": "run|jog in place|walk|other", "run_not_stiff":
"yes|mostly|no" (the knees bend; no two straight legs splayed from the hips), "run_flight": "<what the legs do when both
feet are off the ground>", "differences_from_reference": "...", "worst_frames": "..."}". Bar: the five yes/no fields "yes"
and run_reads_as "run".

**Lens B, limbs.** Images: legs_8x_all.png (idle, walk and run, R and L: the default hero, the woman in trousers, bare legs,
the youth, the elder, the elf); z4_hero.png and z8_hero.png (the default hero walking and running in the game); garments_4x.png.
Prompt: "These images show small pixel-art characters from a game walking and running sideways. Look at every image and every
frame. Answer only from what you see. Reply with JSON only: {"feet_attached": "yes|mostly|no" (every boot visibly joined to
its leg in every frame), "legs_read_as_limbs": "yes|mostly|no" (thigh, knee, shin and ankle, from under the clothes to the
boot), "legs_as_thick_as_standing": "yes|mostly|no", "legs_hidden_or_cut_by_clothes": "<frames and garments, or none>",
"dark_band": "yes|no" (do any character's legs read as a dark band rather than legs), "worst_frames": "..."}". Bar: the
three yes/no fields "yes", dark_band "no", legs_hidden_or_cut_by_clothes names nothing (long robes and dresses showing only
shoe tips are fine); a remark that the legs are short passes only if it applies equally to the standing frames (A3's
proportions are locked).

**Lens C, craft.** Images: boots_12x.png (the boot keys and their uses in situ); frames_12x.png (walk 0-2 and run 0-2, the
man and the woman in trousers, 12x with the pixel grid); strips_1x2x.png (game colours). Prompt: "These images show
pixel-art legs and boots from a game, enlarged. Judge the pixel craft only. Reply with JSON only: {"boot_one_drawing":
"yes|mostly|no" (the boot keeps the same shape and size at every angle), "soles_clean": "yes|mostly|no" (each sole one
clean straight line), "jaggies_or_squash": "<where, or none>", "joints_clean": "yes|mostly|no" (knees and ankles),
"consistent_with_standing_art": "yes|mostly|no", "defects": "<or none>"}". Bar: every yes/no field "yes"; jaggies_or_squash
and defects name nothing material.

## 14. The default hero's dark trousers (decision; AREA tone)

**Decision: a trouser-body floor in rig 2's ramps.** In rpg/art/doll/doll_ramps.cpp, where the Cloth2 ramp is built from
L.bottomColor (about line 429, `rampA3(L.bottomColor, Material::Cloth, ...)`): when the ramp's step 1 (the trouser body
A3's legs paint, char '2') is darker than A3's own trouser body (#33304E, luma 52), lift the base, keeping its hue and
saturation, until step 1 reaches luma 52; brighter picks are unchanged. The default #4E3C2C goes from a body of #301F1C
(luma 36) to about luma 52, its lit steps rising with it, so the legs read as legs on grass (luma about 110) and part from
the tan boots, as A3's do. The mapping stays monotonic (no colour renders darker than a darker pick).
- Inside rig 2 only: not appearance.h, not rig 1, not the generator itself (the ramps' delta E 6 gate) and not A3's
  palettes (B1 parity). It also lifts every rig-2 look's near-black trousers to the same floor, by design (and the legacy
  hood, which takes its cloth from the hose).
- Measured: B11 limbs check 11 (step 1 luma 52 or more); J4's legtone and outline-ring numbers on grass; lens B "dark_band":
  "no".
- Rejected: repainting the near leg a step lighter in the walk keys only (the A3-locked idle stays dark: a flicker on every
  start and stop); a rim light (the finishing pass already lights the outline).

## 15. Risks and fallbacks

| risk | sign | fallback |
|---|---|---|
| B10 RUN or LOCO grows | B10 fails on a look | the run kick falls back to K3 (inside x >= -7); a std-legs LOCO failure pulls the down frame's trailing heel to -6 (K3 drawn 5 px wide); never a frame removed |
| the far-swing pass (walkB2) reads as standing | lens A or B names walkB2 | the swing boot lifted 2, its heel 3 px behind; the swing knee 3 px ahead |
| the K4 hang reads as a slab at 1x | lens C or the judge at 1x | K3 lifted 2 (the fallback in 5.3) |
| a hang can't clear a knee without looking torn | garments-gear | split the hang at the knee into front and back panels (the coat's own construction) rather than lift it whole |
| stamps land late | wave 1 slips | the slice artist drafts the 10 R keys in the stamp file format meanwhile; nothing compiles without the directive |
| gen/doll_pool.cpp written by two areas at once | a torn or stale gen file | areas test on scratch pools; only builders run pxc, immediately before building; the integrator regenerates last |
| the woman's swing torso dropped | her side down frames look different in a dress | garments-gear tunes the dress's run view |
| (round 4) a thick bent leg reads as a blob or a bean at 1x | the panel says blob, bean, lump or club about a pass or push leg | keep the bone, sharpen the outline: the outer corner of the bend a clean step, the inner notch one row deep, the near leg lit along its top-left edge; never thin the leg to sharpen it |
| (round 4) the push's kicked heel reads as a Z or a boomerang (round 2's level shin) | the panel names the push | the kicked shin is a 4-5 row block, not a 2-row bar; if it still reads as a Z, lower the boot to lift 1 and steepen the shin by a pixel (KICKUP's lift_min stays 2 on the adult: lower it only on the owner's word) |
| (round 4) B10 RUN grows with the push's boot at -9 | B10 fails on a look | the kicked K3 at toe -3 (span -8..-3, a corner touch with the planted heel accepted in the run), the envelope back to -8 |
| (round 4) a gate still measures bends on the trace knee | PASSTUCK, KICKUP, FLIGHT or RUNSTIFF fail on art that follows the bone | the gates read `bone` (5.11 step 2-6); never exaggerate a bone to satisfy a washed-out measure (the legs go thin) |

## 16. What the judge focuses on

1. At 1x-3x and in the game on a phone-sized window: does the walk move like the reference (scissor, crossing, the foot
   angled through the stride, nothing skating), and does the run read as a run (a landing, a kick, a flight with knees)?
2. Continuity at 8x in EVERY frame, both facings, every body: hip > knee > boot, no gap, no glued or floating boot, no leg
   under a hem.
3. The boot is one drawing at every angle (the 12x key sheet and its uses).
4. The legs as chunky as the idle (no sliver, no skinny frame) and the woman's legs her own. Since round 4: the idle's
   4 px measured across every bone, the folded pass and push legs and the shin above every boot included (THICK, ALIGN),
   with short legs (A3's proportions) and the stride carried by the leg angle and the boot keys.
5. The default hero's legs on grass: legs, not a dark band.
6. Garments, the scabbard and held items never hide or cut a leg.
7. Rig 1 untouched; every gate at 0; B10 within budget.
