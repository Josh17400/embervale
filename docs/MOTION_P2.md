# MOTION_P2: the side walk and run (the method that shipped)

Rewritten 2026-10-22 by the planner for the rollout (docs/P2B_MOTION_ROLLOUT.md). Binding together with STYLE_P2 section 10.
Scope: rig 2, the doll's SIDE views (R, L) of every gait that moves: the walk, the run, their drawn-weapon variants and the
walking postures. Front and back walks, idles, swing keys, deaths and rig 1 do not change.

This document is the method as it stands in the tree at checkpoint f47c3af plus RUN FIX i3 (pool 7b11cfbac641b82b), the
adult man and woman the owner approved (proto6 i3, then the 2 px run bob, 2026-10-22). It replaces the 2026-10-20/21
joint-table plan (sections 5.2-5.11, targets.json, the stance and feet tables): those bent the wrong knees, left no room for
a knee under a 6-row boot, and forced corners the owner rejected. The old text is in git at f47c3af. The per-iteration notes
stay in the headers of legs/adult/loco/std.pxm, far.pxm, tips.pxm and clips/loco.clip.

Inputs: docs/ref/motion/owner_walk_ref.png (copy its mechanics, not its style), docs/ref/motion/RESEARCH.md (the recipe:
per-frame leg drawings, a planted foot that never slides, a darker far leg, a triangle bob, arms against legs, a flight
frame), the approved deliverables in %TEMP%\claude\emb_P2b\proto7\run_i3\ (walk_run_strip_8x.png, run_zoom_12x.png,
walk_run_3x.mp4, per_frame.txt) and the measuring tool .tmp/judge/proto2/check.py.

---

## 0. The owner's motion rules (binding; every body, garment and item)

1. **Short legs.** Never longer legs, never a hem lifted to show more leg, never a shorter boot.
2. **Thighs start inside the hips.** No leg pixel leaves the hem span in the three rows under the hem (a push leg may leave
   it along its bone, 1 px a row).
3. **Full leg thickness across the bone at every angle.** Rows widen with the angle (ceil(w / cos θ)); a level bone is
   4-5 rows tall. No taper, no pinch, no 1-2 px staircase.
4. **The far leg is a whole leg**: drawn darker, never thinner, clearly separated from the near leg (an outline or a gap
   of background between them).
5. **Ankles 4 px or wider.**
6. **One boot drawing**, rotated by hand into its tilts, never squashed or sheared; the two boots of a frame the same size.
7. **The head stays attached.** Any lean moves head, torso and arms together; no head offset against the torso.
8. **No stretching**: the leg's length along its bones (hip > knee > ankle) stays the idle's.
9. **Bob**: the walk 1 px at most, the run 2 px at most (owner, 2026-10-22). No squat.
10. **No skating**: the planted foot stays on its spot of ground (the clips' plant anchors).
11. **Idles pixel for pixel** (B1 parity: the A3 idles and swing keys at 0 px).
12. **Atlas budgets** (B10): RUN 64 KB, LOCO 128 KB per look.
13. **Design by eye first, then make the checks agree.** Before any walk or run is shown: trace hip to boot in every frame at
    8x, and play it at game speed on grass in both facings.

## 1. The feel

A villager crossing the square on a phone at arm's length walks like the reference: the legs open into a clean inverted V as
the front heel strikes, toe up; the weight rolls onto a flat planted boot that stays where it landed; the free leg swings
through, knee first, toe down, and reaches again. The body barely moves (one pixel down on each contact), the near arm swings
against the near leg. The run is a different animal: the upper body leans in, the arms pump hard and symmetric, the landing
foot plants flat under a bent knee, the support leg drives back onto its toe, and for one frame in three both boots hang in
the air with a real wedge of grass under them. Every leg is as thick as the idle's, from under the tunic to the boot; every
boot is the same boot, tilted. Everything the person wears or carries rides that motion: the hem leans with the body, tails
and skirts trail, sleeves stay on the arms, the stick stays in the hand.

## 2. What draws side loco (the scope)

| body | side gaits | where |
|---|---|---|
| man, woman (and half-breeds, elves; avg, slim, broad builds) | walk, run; drawn walk (ACTION variant 1), drawn run (ACTION variant 2) | legs/adult/loco, arms/adult/loco, clips/loco.clip |
| man.elder, woman.elder | walk (never run; the stoop: lean 1) | the adult legs, their own loco.clip lines |
| youth (boy, girl; 26 px) | walk, run | kin/youth/**, clips/kin_youth.clip |
| child (boy, girl; 21 px) | walk (kids run only in the Play posture) | kin/child/**, clips/kin_child.clip |
| walking postures (adults: Carry x3, Play, Dance; child: Play, Carry) | the walk / run leg keys, stepped by time (9 fps, render_anim.cpp) | clips/postures.clip, clips/kin_child.clip |

Everything a look wears or carries in those frames is in scope: torsos (`run` and `runl` views), hangs (`wide`, `rest`,
`flare`), stride panels, sleeves and drapes on the arm channels, shoe tips under long garments, cloaks (hang channel),
neckwear, headwear and hair hangs, the scabbard and belt tools (sheath channel), held items (grip), the walking stick (legs
channel) and the run staff (arm channels).

## 3. The method in one paragraph

Each side frame is a pose, drawn by hand, never a rotated leg. A leg is built from its joints at the body's own idle length
(hip, knee, ankle), as a band of the idle's width around its bones, and cut into the idle's rows (trouser, knee band, shin,
boot). The far leg is a whole leg on its own part, drawn first (z -1) a value darker, so only what the near leg covers is
hidden. The foot is one boot from a hand-drawn library of tilts, placed by `put` on the end that touches the ground. The
clip's plant anchors mark where the planted heel (walk) or the planted contact (run) stands, so the game's frame table moves
the body exactly as far as the planted foot steps back: frames are keyed to distance walked, with even holds. The body bobs
in a triangle (lowest on the walk's contact, highest in the run's flight). In the run the upper body leans: `lean` moves the
head, torso and arms together, the torso's `runl` view is sheared so the chest leads the waist, and every hip-anchored part
rides `hip=` behind. Arms swing against the legs. Garments raise their hem a row in every side walk and run frame (the
torso's `run` view) so the legs show as limbs.

## 4. Constants that interact (adult; the kin get theirs in section 12)

| constant | value | source |
|---|---|---|
| leg | 10 rows: the hip anchor F-10 to the sole row F; idle near leg 8.0 px along its bones (hip > knee > ankle joint) | loco.clip anchors, check.py |
| leg band | 5 px near (A3), the far leg the same capsule; across the bone >= 3.8 px at every angle | std.pxm, far.pxm |
| boot | K0 20 px, sole 7 px, thickness 3.0 (tilts 2.73-2.93); the shaft above it is the shin | boots_adult.stamps |
| walk | 6 frames, plants R 3:0, 0:-4, -4:-8 (L a pixel forward), holds 3 / 4 / 4 px, **cycle 22 px**, bob +1 / 0 / 0 | loco.clip (PROTO2 i1) |
| run | 6 frames, plants R 0:-6, -6:-12, -12:-18 (L a pixel forward), holds 6 / 6 / 6 px, **cycle 36 px**, bob 0 / -1 / -2 | loco.clip (RUN FIX i1) |
| run lean | lean 4 (head, shoulders, arms); torso `runl`: chest 1 px and waist 2 px behind the shoulders; hip-anchored parts `hip=2,0` (man) / `3,0` (woman) | loco.clip (PROTO6 i3) |
| hem | walk F-9 on the contact (bob +1), F-10 otherwise, span u -5..+5 (L -5..+6); run F-10 / F-11 / F-12, span u -3..+7 (L -3..+8) | per_frame.txt |
| gait switch | 54 px/s (doll.h kGaitSwitch); hero walk 40 px/s, run 74 px/s; a walk starts on frame 5, the reach | anim.cpp |
| budgets | RUN 64 KB, LOCO 128 KB per look; worst today RUN 63.4 (staff + hood man), LOCO 124.0 | B10, 2026-10-22 |

**The arithmetic.**
- Walk timing: at 40 px/s the frames hold 75 / 100 / 100 ms (1.8 cycles a second); a 20 px/s stroll 150 / 200 / 200 ms;
  an elder at 0.8 pace 94 / 125 / 125 ms. RESEARCH 5.2's "about 100 ms a frame" is met without a longer stride.
- Run timing: at 74 px/s every frame holds 81 ms (2.06 cycles a second, 12.3 fps); at the switch (54-58 px/s) 103-111 ms.
  Even holds are what cured the stutter (round 6's 8 / 4 / 6 read as a limp; RHYTHM keeps the spread at 2 px or less).
- Registration: a frame advances the body by its hold, and the planted contact steps back by exactly the same amount
  (walk heel +3 > 0 > -4 > -8; run toe at -1 through the push), so the boot stays on its ground pixel within 1 px.
- Air: the flight's bob -2 plus 1 row of leg lift puts both soles 3 rows over the ground (FLIGHT needs 2). With the short
  legs and thighs straight down for 3 hem rows, 3 rows is the most a leg can lift without the under-seat fold the owner read
  as kneeling.
- B10: a RUN atlas holds 24 trimmed frames (4 facings x 6) at 4 bytes a pixel, about 2.6 KB a frame for the widest looks.
  One extra column on the 12 side frames of a 36-row figure costs about 1.7 KB; the worst look has 0.6 KB left. **No side
  run frame of a near-budget look may grow** (section 11.3).

## 5. The adult walk, frame by frame (R view; L is the R placements mirrored, x' = -x - 1, re-lit, a pixel forward)

Near = the right leg facing right (std.pxm), far = the left (far.pxm). u = columns forward of the pivot. Measures: the
default hero on the dump (per_frame.txt; idle leg 8.0, boot 20 px / 3.0).

| F | key | plant, hold, bob | near leg | far leg | arms near / far | hang |
|---|---|---|---|---|---|---|
| 0 | walkA | 3:0, 3 px, +1 | CONTACT: in front, heel strike, K1 toe-up at u +3; knee 35 deg; length 7.5 | behind, heel-off, K2 on the ball at u -2; knee 28 deg | B / - | wide |
| 1 | walkA2 | 0:-4, 4 px, 0 | PASS: planted flat K0 under the hips (u 0), straight (0 deg), 8.0 | swing: knee forward, shin trailing into K3 toe-down, lowest pixel 1 row up; 59 deg | N / - | wide |
| 2 | walkB2 | -4:-8, 4 px, 0 | REACH: planted K0 at u -4, behind the hips; 22 deg | reaching, K1 toe-up ahead at u +3, 1 row up; 31 deg | F / back | rest |
| 3-5 | walkC, walkC2, walkD2 | as 0-2 | the roles swapped: far in front at contact, far planted at pass and reach | | F / back, N / -, B / - | wide, wide, rest |

Every frame: torso=run (the hem a row up), neck flow (flow2 on the pass), hair=swing, tuft=down and braid=swing on the pass,
sheath=rest (R) / kick (L: the scabbard swung back behind the legs). The woman: her `when woman replace` views at the same
placements; `hip=0,-1` on the bob +1 frames so her skirt stays off the ground row; braid=rest except on the pass. The
elders: the same lines with lean=1 (the stoop) and their own head anchors. The drawn walk: the same legs, plants, bob, hangs
and hair; armR=guard with held=guard; the off hand R -, -, oback, oback, -, - (L oB oN oF oF oN oB).

Measured ranges (both bodies, both facings): leg length 6.8-8.4, ankles 4-5 px (3.8-5.0 across), boots 20 px (2.93-3.0),
hem span -5..+5 inside, head dx 0 dy 0.

## 6. The adult run, frame by frame (R view)

| F | key | plant, hold, bob | near leg | far leg | arms near / far |
|---|---|---|---|---|---|
| 0 | run1 | 0:-6, 6 px, 0 | CONTACT: lands flat, K0 at u 0, under a bent knee (the knee ~3 px proud of the ankle; hip-ankle 11 deg from vertical, knee 34 deg) | TRAILING: hangs from the hip, the knee under the seat, the shin down and back at ~45 deg (rows 5, 6, 6) into K3 toe-down, its toe 2 rows up behind the planted heel; 45 deg | pB / pF |
| 1 | run2 | -6:-12, 6 px, -1 | PUSH: the support leans back ~30 deg from the hip onto its toe (K3, toe at u -1, the heel up), the knee giving ~20 deg | SWING: the thigh from the hips' front half, the knee driven forward to u +9, the shin hanging into K3 toe-down, 1 row up; a wedge of grass between the boots | pM / pK |
| 2 | run3 | -12:-18, 6 px, -2 | FLIGHT, trailing: the thigh straight down 3 rows inside the hem span, the knee breaking back under the seat (90 deg), the shin down and back into K9 pointed; sole 3 rows up | FLIGHT, lead: the thigh forward out of the hips' front half, the knee up and forward (u +8), the shin hanging into a whole K1 toe-up (reaching for the heel strike); sole 3 rows up; knee 32 deg | pF / pB |
| 3-5 | run4-run6 | as 0-2 | the roles swapped (far lands, far pushes, far trails) | | pF / pB, pK / pM, pB / pF |

Every frame: lean=4 (L -4), torso=runl, hip=2,0 (woman 3,0), hang=flare, braid=run (down her back), sheath=kick, neck flow2,
flow2, flow; hair=swing on the push; tuft=down on the contact. The arms: extremes on the contact and the flight (the arm
opposite the forward leg), passing the body on the push; the far arm a value darker; pB reaches back flatter (same length)
so the sleeve shows and the fist reads as a hand. The drawn run: the same legs, plants, bob, lean and hangs; armR=guard with
held=guard; the off hand R -, -, oback, oback, -, - (L orunB oN orunF orunF oN orunB).

Measured ranges: leg length 6.3-8.9 (folded legs to their far end), knee bends support 13-34 deg, swing 104-127 deg, lead in
flight 32 deg; ankles 5-6 px (4.5-5.4 across); boots 19-22 px (2.73-3.0; the K9 trailing boot 21-22 px); hem span -3..+7
inside; head dx 0 dy 0.

**Known open polish** (ADULT-RIG, rollout): the flight's K9 boot read as changing size by the panel (BOOTSIZE passes); the
dark outlines bunch at the crotch where the legs overlap in run3 / run6; the lean reads slight (4 px is approved; a deeper
lean is the owner's call).

## 7. Drawing a leg (every body)

1. **From the joints, at the idle's length.** Place hip, knee and ankle; the thigh and shin keep the idle's lengths (adult
   4.5 + 4 + 1.5 = 10 rows), so a pose the leg cannot reach is refused, never stretched. Bent legs read shorter along the
   straight line, never longer along the bones (LEGLEN: idle +1 / -1.5).
2. **Thighs start inside the hips**: from the seat row to 3 rows under the hem, no leg pixel outside the hem span; below that
   a reaching or trailing thigh angles out (its cut edge re-outlined).
3. **Full thickness across the bone**: the near leg 5 px across, the far leg the same capsule; rows ceil(w / cos θ) wide; a
   level shin 4-5 rows tall; the inside of a bent knee filled; THICK 3.8 px or more (sampled along thigh and shin).
4. **Ankles**: the shin whole down to its boot, every row knee > boot top 4 px or more and 3.5 px across.
5. **The far leg**: legs/adult/loco/far.pxm (z -1), the idle's far value (cloth 3 then 2, leather h then g, the first
   interior pixel of each row lit), never one flat dark; drawn full width under the near leg; where it would show as a 1-2
   px sliver beside the near leg its row slides under instead. It also carries the seat row and the hips filled to the
   idle's width. Front and back views of the shared keys are empty views (std draws both legs there).
6. **Separation**: the two legs are told apart by value and an outline or a gap; boots apart or touching with an outline
   between them, the far boot showing 60 % or more (BUNCH).
7. **The boot**: a library key placed whole by `put`; where the shin enters a tilted boot the boot goes `under` (as the
   idle's shin sits on its boot). `tags boots-stamped`: pxc refuses a hand-drawn boot pixel.
8. **Regions**: T thigh, K knee band (the run keys: every visible trouser pixel K, so a kilt's bare knee shows), S shin, B
   boot, O sole; the look recolours them (rags: bare shins and feet that tilt the same way).
9. **Elves**: the anchors sit 1 px up; the walk and run keys have no stretch row; far.pxm's `when elf` views add the seat row.
10. **The woman**: her own `when woman replace` views at the same placements; drawn and judged in trousers, bare legs and
    under skirts.
11. **L views**: the R placements mirrored and re-lit for the top-left light, never the plain mirror (pxc checks).

## 8. The boot libraries (maps/stamps)

Adult (boots_adult.stamps; `n` near, `f` far a value darker; R and L re-lit): **K0** flat (the planted foot; origin the
heel), **K1** toe-up (heel strike, and the run flight's reaching lead foot; the toe cap's top parallel to the sole, closed
over the instep), **K2** heel-off (on the
ball, the heel two rows up; origin the toe), **K3** toe-off / toe-down 1:2 (rolled onto the toe; every lifted or swinging
foot), **K9** point (the flight's trailing foot, toe down and back along the shin, K0 turned ~125 deg by hand). Retired, kept
for history: K4 (a 45 deg turn reads as a 2-row stick) and K8 (the under-seat tuck). Youth (boots_youth: Y0-Y4) and child
(boots_child: C0-C4) hold their own idle boots in the same five tilts; a body that needs a flight point adds its own key
by hand (never a scaled adult key).

## 9. The upper body

- **Lean** (run): `lean=4` moves the neck, head and shoulders; the torso part's `runl` view shears the garment so the chest
  sits 1 px and the waist 2 px behind the shoulders, which puts the hem where lean 2 would. No `head=` offsets anywhere: the
  head moves only with the whole upper body (HEAD: dx 0 against the neck, no gap row).
- **Hips**: tails, skirts, cloaks and scarves on the hip or hang anchors follow `hip=2,0` (man) / `3,0` (woman) in the run,
  so they sit under the leaned torso and trail.
- **Walk arms**: the near arm B (back) N (passing) F (forward) against the near leg; the far arm shows only when it swings
  back (`back`), a value darker. Owner-approved; frozen for the rollout.
- **Run arms**: the pump keys pB (back), pM (forward-mid), pF (forward), pK (back-mid), pN (passing) on both arms; near
  pB pM pF pF pK pB, far pF pK pB pB pM pF (R). Frozen for the rollout: sleeves, drapes and carried items fit these drawings.
- **The elder's stoop**: lean 1 on every side frame (head, torso, arms), the hips behind the shoulders.

## 10. Garments in side walk and run frames

1. **The walk takes torso=run**: each garment's side `run` view, the hem a row up (F-10; F-9 on the contact). The run takes
   torso=runl, the sheared lean view. Both views exist for every adult garment and the mail; the `runl` views are generated
   by a row shear (proto6 gen_lean.py) and are to be hand-finished so the lean reads as a body leaning (rollout).
2. **Hangs** ride the hang channel: `wide` on the walk's contact and pass, `rest` on the reach (the idle's view: never edit a
   `rest` view to fit a walk frame, ask for a key instead), `flare` on every run frame. A hang opens over the stride and
   trails; it never flies forward of the torso, never cuts a knee or a shin into a stub, never floats free of the body.
3. **Stride panels** (`*_stride`, legs channel) draw on the reach keys (walkB2, walkD2) and fit those legs.
4. **Sleeves and drapes** (`*_sleeve_l/r`, `poncho_drape_l/r`, arm channels) need a view, empty if it must vanish, for every
   arm key the look's side clips name: walk B N F back, run pB pM pF pK pN (runB runF where clips still name them), drawn
   guard and the o* keys. A part without the named key draws nothing (doll_compose.cpp), so a missing key is a silent
   bare arm.
5. **Long garments** (dress, robe, gown, kaftan, wrap) draw no legs: tips.pxm shows the shoes below the hem at the same keys
   and positions as std's feet; in the run's flight the toes of both feet still peek below the raised hem (no floating bell).
   The hem line of a long garment's run flare does not move down.
6. **Patterns and builds**: tiles ride the garment's cloth (check that stripes survive the shear); build masks (slim, broad)
   apply to `run` and `runl` views as to the rest.

## 11. Worn-over and carried things

1. **Cloaks** ride the hang channel (`wide` walk, `flare` run): they hang from the neck behind the body, stream back along
   the lean in the run, read as a cloak (not a box), stay attached, and stay inside the figure's box (11.3).
2. **Neckwear**: the man's kerchief on the neck channel (flow, flow2), the woman's kerchief and scarf on the hang channel;
   tails trail. **Headwear** rides the head box (it leans with the head); hood drapes sit on the leaned shoulders.
   **Hair hangs** ride the hair channel (rest, swing) or their own (the braid: rest, swing, run); in the run every hanging
   lock, tail and braid hangs or streams behind, never across the chest or the hands.
3. **The box rule** (B10): in every side run frame, worn-over and carried parts stay inside the frame box the body already
   makes (head top, trailing boot, scabbard tip, leading hand), 1 column of slack at most. Cloaks are the risk: a travel cloak
   with hood and staff runs at 64.4 KB today (over).
4. **The scabbard and belt tools** (sheath channel: rest, kick, lag): behind the legs, swung back (`kick`) in the L walk and
   every run frame, never across a boot, never a third boot.
5. **Held items on a grip** follow their arm automatically. **The walking stick** (legs channel, a drawing per walk and run
   key): in the walk its tip is planted and steps back exactly the frame's advance (3 / 4 / 4), never on a boot; in the run it
   is carried in the fist of the pump keys. **The run staff** (staff_run on armR, staff_runl on armL, a view per pump key):
   in the hand, upright, never across the face.
6. **Walking postures** (Carry, Play, Dance) reuse the walk and run leg keys; their frames step by time (9 fps,
   render_anim.cpp), so registration is not measured for them, but every leg rule is. Their side frames take torso=run.

## 12. The kin (the same method at their scale)

The youth and the child get the adult method, drawn at their own scale from their own idles: legs from their idle's joints
and length, a far leg on its own part (kin/youth/legs/far.pxm, kin/child/legs/far.pxm), their own boot libraries, even
holds, a triangle bob, the youth's run lean with a sheared torso view, arms against legs. Never the adult's drawings
shrunk. Targets to start from (the KIN slice decides by eye; GATES-WEAR records the final numbers here):

| | youth (26 px) | child (21 px) |
|---|---|---|
| leg | 8 rows (hip F-8): trousers F-7..F-5, boot F-4..F-1, sole F; leg 4 px, shaft 4 px | 5 rows (hip F-5): trousers 2 rows, boot 3 rows, sole; leg 4 px, shaft 4 px |
| walk | about 18 px a cycle (holds 3 / 3 / 3), bob +1 / 0 / 0 | about 12 px (holds 2 / 2 / 2), bob +1 / 0 / 0 (no hop on the pass) |
| run | about 30 px (holds 5 / 5 / 5), bob up to 2, lean 2-3 with a sheared torso view | none (Play hops in place) |
| arms | walk B N F back; run pump keys pB pM pF pK pN drawn for the youth's arms | walk B N F |
| thresholds | THICK 3.8, ANKLE 4 px, LEGLEN own idle +1 / -1.5, BOOT own idle +/-10 % | THICK 3.3, ANKLE 4 px, the rest as the youth |

The youth wears the adult hair and headwear parts (the same head box): their run views must suit the youth's shoulders too.

**Recorded at integration (GATES-WEAR, 2026-10-23; drawn by KIN-YOUTH and KIN-CHILD, read from their clips and reports):**

| | youth (26 px) | child (21 px) |
|---|---|---|
| leg | 8 rows (hip F-8), the idle leg 6.0 px along the bones, band 4 px (5-6 px rows on angled bones); a far leg part (z -1, a value darker) | 5 rows (hip F-5), near and far leg on their own parts |
| walk | 18 px a cycle, holds 3 / 3 / 3, plants R 2:-1, -1:-4, -4:-7 (L +1), bob +1 / 0 / 0 | 12 px a cycle, holds 2 / 2 / 2, plants -1:-3, -3:-5, -5:-7 (both facings), bob +1 on the contacts with hip 0,-1 (no hop on the pass) |
| run | 30 px, holds 5 / 5 / 5, plants R 0:-5, -5:-10, -10:-15 (L +1), bob 0 / -1 / -2, lean 3, hip 1,0 (L -1,0), torso `runl` (chest 1, waist 2 behind) | none (Play hops in place) |
| arms | walk B N F back; run pump pB pM pF pK pN at the adult's timing | walk B N F against the legs |
| boots | Y0 Y1 Y2 Y3 + Y9 (the flight point) | the child library |
| thresholds | the adult values pass (THICK 3.8, ANKLE 4 / 3.5, FLIGHT lift 2 + bend 40, PUSH 10, FOLD 25, POST 25): no youth-specific bar | THICK 3.3, the rest as the youth |


## 13. Measuring (the gates; docs/P2B_MOTION_ROLLOUT.md section 5 is the full specification)

`rpg_test --doll --gates` (CI, blocking) and `rpg_test --doll --motion` (the artists' tool, isolated pools) run:
- **B11 limbs**, per side frame, near and far leg each measured alone on its own layer: FARWIDTH, LEGLEN, THICK, ANKLE, HEAD,
  HIPLINE, BUNCH, REG, BOB, RHYTHM, FLIGHT, PUSH, FOLD, POST, plus leg area >= 89.5 % of the idle, hip row width, no sliver,
  no squat, continuity and joints, ground contact, the default hero's trouser luma.
- **B11 wear**: no silent vanishing (key completeness), sleeves on their arms, hangs attached and trailing, torso follows the
  lean, held items in the hand, the face clear, cloaks inside the box; long-garment shoe tips shown and registered.
- **B12 boots**: every boot a library key (BOOT: pixels within 10 % of the idle's, thickness), BOOTTILT (60 deg, thickness),
  BOOTSIZE (the two boots within 10 %), the far boot's visible pieces, `boots-stamped` on every side-loco legs part.
- **B11 wear calibrations** (GATES-WEAR, 2026-10-23, on the approved art; docs/P2B_MOTION_ROLLOUT.md 12 says why each):
  W2 trim-only sleeves pass touching their arm; W3 skips the cloak's shoulder rows (the torso's top 3); W7's face runs from
  the eye row - 1 to the row above the mouth and leaves out the profile's outer pixel, and counts a held item drawn through
  the head; W8 cloaks <= 1 column / 0 rows, neckwear and hair hangs <= max(4, the idle's reach + 1) columns; T1's flight wants
  a boot row, not a sole strip. The freeze manifest (.tmp/judge/rollout/freeze_f47c3af.txt, from the checkpoint pool) and
  its ruled exceptions (freeze_accepted.txt) guard every idle, front / back, hurt, action, death and posture frame;
  `rpg_test --doll-wear` runs B11 wear, the matrix (--list, --matrix M1..M7 | tag | prefix* | part:NAME), --b10 (M7),
  --freeze-check, --frames strips and --ascii frame dumps alone.
- The constants are check.py's (adult) and section 12's (kin). check.py stays the oracle for the adult looks; the C++ gate
  must agree with it frame for frame.

The eye comes first: every change is played at game speed (walk 40 px/s, run 74 px/s) on grass, both facings, at 3x, and
traced hip to boot at 8x in every frame, beside the approved adult reference.

## 14. What was tried and dropped (so nobody tries it again)

- Joint-angle tables and targets.json (rounds 1-4): bent the wrong knees; 3 trouser rows over a 6-row boot left no room for
  a knee. Replaced by poses drawn from joints at the idle's length.
- Rigid rotated legs ("tilting sticks"), skinny diagonal staircases, a far leg drawn as one flat dark sliver: replaced by
  per-frame poses, full thickness across the bone, the far leg whole on its own part.
- Longer legs or a raised hem (mock B): the owner keeps short legs (mock A).
- The under-seat folded flight (round 4): passed tight FOLD checks, read as kneeling in mid air. The bake-off picked the open
  scissor. Lesson: design by eye first, then make the checks agree.
- A 1 px run bob (no air) and uneven run holds 8 / 4 / 6 (a stutter): replaced by bob 0 / -1 / -2 and 6 / 6 / 6.
- Head offsets for lag (`head=`): the head read as slipping off the neck. The head moves only with the body.
- The trailing contact leg kneeling over a flat boot (i2): replaced by the shin down and back into K3 toe-down, 2 rows up.
