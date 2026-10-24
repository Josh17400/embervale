# STYLE_P2: Clear Sight people (art direction for P2b, P2c and P2d)

Rewritten 2026-10-18 by the planner after the owner's art-direction change (VISION_PLAN 15.36 "The art-direction
change"). The owner rejected the look of P2a's procedural 32 px rig ("they look bad, they just aren't sharp or clear":
proportions, shading, faces) and approved art study **A3**: art study A's chunky 32 px silhouette, rendered inside the
way Sea of Stars and Chained Echoes render (eye whites with an iris and a glint, brows, a mouth, hue-shifted 5-step
ramps, cloth, leather and metal kept apart, coloured outlines lit on the top-left edges, left facings painted for the
top-left light, no orphan pixels, secondary motion). People are now **hand-placed pixel art**: hand-drawn layered part
maps, composed by the doll (VISION_PLAN 15.36 "The production method"). Geometry code never draws a limb, a face or a
fold again.

**The reference is locked.** docs/ref/style_a3/ holds A3's maps (hero.py, woman.py, swing.py, with pal.py and
pixlib.py), the build and check scripts, the sheets and GIFs, A3's notes, and the 64 frames at 1x (frames/, frames.json,
palettes.json, exported from the maps). In that folder `python build.py sheet gifs` and `python checks.py hero woman
swing` run as they did in the study; its scene, before_after and silhouettes targets need the study's scratch inputs,
which were not kept. Every number marked "A3" below was measured on those maps. For anything A3 drew, the reference's pixels
win over this text; for anything A3 did not draw, this text wins. Once the P2b slice passes and the owner signs it off,
this file changes only to record what the slice measured.

**Kept from P2a** (unchanged): the lens (one pixel grid, whole-number world scale, ZOOM, layers, the ring, the night
lift, the pips, text in the UI layer), the body-holders at k = 32 / 22, four painted facings, the clip list and its
timing model (frames by distance walked, swings by the family's pace), the swing families, and the corpse rules.

## What changed on 2026-10-18 (superseded P2a rules)

| P2a rule (procedural rig) | now (A3, the doll) | section |
|---|---|---|
| proportions: crown F-31, legs 11 rows, shoulders 15 / 16 / 18 (man) vs 13 / 14 / 16 (woman) | A3's measured grid: crown F-31 (man) / F-30 (woman); a hem at F-9 over 8 rows of leg; 16 px across the shoulders and arms for both bodies | 3, 14 |
| eyes 1 x 2 at face columns 3 and 6; "No whites: no sclera, no anime eyes" | eyes 2 x 3: lash, white + iris, white + glint. The ban on eye whites is LIFTED | 4 |
| a 2 px mouth over a dark chin row | a 1 px mouth (the woman: lip tint + mouth), a clean skin chin | 4 |
| Ink #1E162A outer outline with Lit Ink on every person | coloured outlines: each material outlined in its own darkest step; the finishing pass lights the outline pixels that face the top-left to the next step. Ink only on the sole line. The Ink outline stays on v1 sheets and creatures until those are redrawn | 8 |
| ramps from the creator hexes, spread 0.8-1.3 | A3's hue-shifted 5-step ramps: curated tables for skin and hair, a generator calibrated on A3 for every other colour | 6 |
| dither at light-band edges on wide cloth | no dither on people: solid lit bands, clean folds, no single-pixel trims | 7 |
| walk: 6 frames over a 28 px cycle | the cycle comes from the drawings' planted feet; after the owner's walk fix (2026-10-19) both bodies walk an 18 px cycle with chunky legs and flat boots; a RUN (36 px) above 54 px/s; side views since the owner's motion reference (2026-10-20): limbs, a scissor stride, one boot in five angled keys; since 2026-10-22 the shipped method (docs/MOTION_P2.md): walk 22 px, run 36 px with a 2 px bob and a 4 px lean | 10 |
| weapons rasterized procedurally at 16 angles | weapons are authored maps at 16 angles (drafted from P2a's rasterizer, then cleaned by hand) | 11 |
| fighters hold the weapon in every clip | out of combat the blade rides sheathed (A3: the sword at the hip, the hand on the hilt); in hand while fighting (the owner confirms at the slice) | 11 |
| fall: recoil, buckle, a painted topple | recoil, buckle, then the lying figure itself lifted for one frame; lying is the composed side view turned a quarter | 13 |
| the P2a style slice (procedural; passed 2026-10-18) | the A3 doll slice, P2b wave 1, signed off by the owner; P2a's record kept in the appendix | 19 |

**2026-10-23, the gear ladder.** After approving the P2c slice's direction, the owner asked for a rags-to-riches
progression: beginner gear okay / mid, steadily cooler, late gear animated, built on the culture armour generator. Section
27 is that ladder. "Bands are ramps, never shapes" now reads "materials are ramps; tiers are finish and the culture's
dials" (22, 25.2, 25.12).

---

## 0. The feel

Sea of Stars and Chained Echoes at 32 px. On a phone at arm's length you know everyone. A chunky, warm figure, about
2.7 heads tall, with a big readable head: eyes with whites and a blue or green glint, brows, a small mouth, the woman's
lips and blush. The hair is a lit volume, not a cap. Cloth, leather and steel each have their own colour family, and
every material is outlined in its own darkest shade, never in a black line. The edges facing the light are a step lighter.
Hems are solid lit bands. A braid swings a pixel behind the step, a scarf tail flies, a cowlick stretches as the head
drops. The hero walks with the sword sheathed at the hip and a hand on the hilt; in a fight it comes out in a coiled
back-swing and a thick, three-tone arc. Thousands of people in a procedural world, and no two neighbours look like copies:
different jaws, noses, brows, 24 hair styles, beards, cuts and cultures, and armour that visibly changes a man or a woman.
The hero's gear tells the story of the road: a mended gambeson at the start, living legendary steel at the end (27).
Everything stays crisp: one pixel grid, whole-number scale, no blur, no mixels.

---

## 1. Scale: one pixel grid, the zoom, and the rig (P2a, kept)

**One pixel grid.** Tiles stay 16 px. Every world sprite is drawn 1 art px = 1 world px, and the world layer is scaled by
a whole number D. Nothing is ever drawn at another scale: no 2x people on a 1x world, no runtime rotation or scaling of
sprites. UI art lives only in the UI layer.

**Footprints never change**, so world generation, collision, pathing and saves are untouched by P2.

**The zoom (world layer only).** D0 = round(device px height / 240), ties up, kept inside V = h / D in [150, 300] world
rows; ZOOM offers FAR (D0 - 1), NORMAL, NEAR (D0 + 1), CLOSE (D0 + 2) where V allows. The reference-screen table
(P2a, measured and passing L1-L5) stands:

| reference screen (device px) | UI (kept) | world D0 | world view | 32 px body box at NORMAL | zoom levels |
|---|---|---|---|---|---|
| desktop 1440x810 | 480x270 @3 | 3 | 480x270 | 11.9 % | N3 E4 C5 |
| desktop 1920x1080 | 480x270 @4 | 5 | 384x216 | 14.8 % | F4 N5 E6 C7 |
| laptop 1366x768 | 683x384 @2 | 3 | 456x256 | 12.5 % | N3 E4 C5 |
| desktop 2560x1440 | 512x288 @5 | 6 | 427x240 | 13.3 % | F5 N6 E7 C8 |
| iPhone 15 Pro app 2556x1179 | 639x295 @4 | 5 | 512x236 | 13.6 % | F4 N5 E6 C7 |
| iPhone 15 Pro Safari 2556x1020 | 721x287 @3.54 | 4 | 639x255 | 12.5 % | N4 E5 C6 |
| iPhone 11 1792x828 | 615x284 @2.91 | 3 | 598x276 | 11.6 % | N3 E4 C5 |
| iPhone SE 1334x750 | 480x270 @2.78 | 3 | 445x250 | 12.8 % | N3 E4 C5 |
| iPad landscape 2360x1640 | 480x333 @4.92 | 7 | 338x235 | 13.7 % | F6 N7 E8 C9 |
| Witness window 960x540 | 480x270 @2 | 2 | 480x270 | 11.9 % | N2 E3 |

**The body.** The A3 man is 32 px from the sole row F to the crown's outline row (F-31), with up to 2 px of hair above it
(the hero's cowlick: 34 px in all). The A3 woman is 31 px (crown F-30). A3 sits exactly on P2a's 32 px rig, so **every
body-holder P2a sized at k = 32 / 22 = 1.45 stays**: doors 0.70-0.76 of the figure, the one-storey wall 1.00-1.12, seats
0.14-0.20 (restated P2b fix r1: at A3's knee, not P2a's 0.31-0.37; A3's legs are 8 rows, so a P2a-height seat put a
sitter's hips above a standing man's), tables 0.44-0.50, horse withers 0.80-0.90, gunwale 0.35-0.45, rails 0.45-0.55, gate clearance 1.25 or more,
fences 0.45-0.60, wheels 0.45-0.55, rungs every 6 px. The ratios are measured against the 32 px body box, never against
hair. Everything else (trees, rocks, crops, roofs' pitch, wall courses) keeps its size.

---

## 2. Shape language (people)

Chunky and warm (A3): a big round head (12 rows of 32), a compact torso, short sturdy legs in big boots (each boot 7 px
at the toe), hands as solid 3 x 2 blocks. One clear silhouette per figure; details live inside it. Shoulders are square
on both bodies; the woman reads through her face, hair, waist and skirt, never through exaggeration. Hair is a volume
with lit locks and a crown highlight, gathered into braids and tails that hang and swing. Clothes have planes: a lit
chest plane, one fold under a strap, clean vertical skirt folds, a solid lit hem. Gear sits on the body's planes at the
body's anchors, never floating.

---

## 3. Proportions (A3's measured grid)

Rows count up from **F, the sole row** (the Ink line under the shoes; A3 row 34 of a 24 x 36 canvas, whose pivot is
(12, 34)). Measured on docs/ref/style_a3 hero.py and woman.py, front idle.

| rows (man) | A3 row | content |
|---|---|---|
| F-33 .. F-32 | 1-2 | the hair's rise: tufts, the cowlick (2 px at most above the crown) |
| F-31 | 3 | crown: the hair's outline, 12 px |
| F-30 .. F-26 | 4-8 | hair mass, 13-14 px wide; the fringe shows forehead skin at F-26 |
| F-25 | 9 | brows: 2 px each in hair step 1, inside the fringe |
| F-24 | 10 | lash row; the ears (the head 16 px wide with ears) |
| F-23 | 11 | eye white + iris |
| F-22 | 12 | eye white + glint; the nose pixel between the eyes |
| F-21 | 13 | mouth (1 px); the face 10 px wide |
| F-20 | 14 | chin, 8 px incl. the outline |
| F-19 | 15 | neck and collar (the hero's scarf) |
| F-18 | 16 | shoulders, 13 px |
| F-17 .. F-13 | 17-21 | chest and arms: 16 px across (torso 10, each arm 3 incl. the outline) |
| F-12 | 22 | belt |
| F-11 .. F-10 | 23-24 | hands (3 x 2 incl. the outline) beside the hips |
| F-9 | 25 | the tunic's hem |
| F-8 .. F-6 | 26-28 | trousers: 10 px across both legs |
| F-5 .. F-1 | 29-33 | boots: each 7 px at the toe, 12-14 px across both; front view feet together (the inner outlines touch) |
| F | 34 | sole: Ink, 14 px |

| rows (woman) | content |
|---|---|
| F-30 | crown: 4 px, a rounder top (F-29 8 px wide) |
| F-29 .. F-19 | hair, 10-14 px; it falls beside the face to F-18 |
| F-23 | lash: 3 px, lifted one pixel outward |
| F-22 .. F-21 | eye white + iris, eye white + glint |
| F-20 | lip tint + mouth (2 px); a blush pixel under each eye (A3's columns) |
| F-19 | chin, 6 px incl. the outline |
| F-18 | neck, 4 px of skin |
| F-17 .. F-15 | shoulders: the chemise's puff sleeves, 16 px across |
| F-14 .. F-10 | bodice and apron; arms and hands at the sides |
| F-9 .. F-2 | skirt: 12 px at the waist, 14 at F-8, 16 from F-5; the hem's lit band at F-2 |
| F-1 | shoe tips under the hem (3 px each) |
| F | soles (Ink, 3 px each) |

**The head** is 12 rows from crown to chin on both bodies (man F-31..F-20, woman F-30..F-19): the woman's head and torso
sit one row lower, her hem and feet do not move. Shoulders to crown: 13 rows on both. Face 10 px wide between the hair;
hair 14 px; with ears 16 px. Head to body 12 : 20, about 2.7 heads.

**Side views** (A3 right idle): head and hair 18 px deep (the hair overhangs 3 px behind the skull and falls to the nape
at F-22); the face 6 px from the ear to the nose tip, the nose 1 px proud; chest 10 px deep with the arm; boots 7 px long.

**The hair's envelope.** Close styles stay inside the 14 px hair box; volume styles (curls, big curls, long wavy, locs)
may reach 18 px (2 px past each ear). Hair rises 2 px at most above the crown (F-33 man, F-32 woman). Behind the head in
side views, up to 4 px. Long hair falls to the shoulders at the sides and to F-9 down the back; braids and tails hang to
F-9 in front (A3's braid ends there). Headwear may rise to F-35; crests, plumes and horns to F-37 (markers sit 4 px over
the frame's own top: section 17).

**Kids** (painted natively; P2a's numbers kept): youth 26 (head 12: the adult head box; torso 6, legs 7; shoulders 12),
child 21 (head 11, rounder; torso 5, legs 5; shoulders 10).

**Peoples, builds, age.** Elves stand 1 px taller, all of it in the legs (each leg drawing carries one stretch row,
section 24), and one build slimmer; half-breeds one build slimmer at average. Builds change only the chest, waist and the
arms' placement: average = A3; slim = 1 px narrower on the right edge (15 px); broad = 1 px wider on each side (18 px with
arms) and 1 px deeper in side views. Elders keep their body: the head stack sits 1 px lower (1 px forward in side views)
and the shoulders round by a pixel (section 14).

---

## 4. Heads and faces

**A3's face (binding, front view):** a 10 px face under a fringe.
- **Eyes** 2 px wide x 3 rows: the lash (Lash #24142A) over both columns; then eye white (Eye White #F6EEE3) + iris; then
  eye white + glint (the iris's light step). The irises sit on the inner columns (toward the nose). Man: eyes on face
  columns 1-2 and 7-8, 4 px of skin between the irises. Woman: the lash 3 px, one pixel outward and up (a lifted line).
- **Brows**: 2 px over each eye in hair step 1, on the row above the lash, part of the fringe's edge.
- **Nose**: one skin-shadow pixel between the eyes on the glint row (the light is top-left).
- **Mouth**: 1 px (Mouth, skin-derived: A3 #8E3A40) centred one row under the eyes. Woman: a lip-tint pixel (A3 #D46470)
  beside it, and a blush pixel (A3 #F49A8A) under each eye on the mouth row (A3's columns).
- **Chin**: a clean skin row (man 8 px, woman 6 px incl. the outline); no dark chin row (A's read as a beard).
- **Side view**: one eye 2 px (white + iris) under its lash, the brow above, the nose bump 1 px proud with the outline
  round it, a 1 px mouth, the ear at the middle of the head. The face turns into the light in the left view (lit
  forehead, nose and cheek) and falls into shadow at the back of the head.
- **Back view**: no face; hair over the head with a highlight ring across the crown; the ears; 1-2 px of nape.
- **Nothing is outlined inside the face.**

**Head types (owner, 2026-10-18: distinct shapes, not one head recoloured).** Every head type keeps A3's eye rows and
columns, the nose pixel and the mouth row, so expressions, marks and paint fit them all; it differs in the jaw and chin
outline (F-21..F-19), the cheek shading, the skull top (seen when bald), the brow ridge and the profile.

| set | head types | noses | brows |
|---|---|---|---|
| man, human | Oval (A3), Square (6 px chin, flat jaw corners), Round (full cheeks), Long (narrow, the chin 1 row lower, 6 px), Craggy (brow ridge, hollow cheeks, cheekbones), Heavy (jowls, a thick neck) | 5: button, straight, long, hooked, broad | 4: straight, heavy, arched, thin |
| woman, human | Oval (A3), Heart (wide cheekbones, a pointed 4 px chin), Round, Long, Square (a strong jaw), Full (soft and plump) | 4: button, straight, upturned, aquiline | 4: arched, straight, thin, full |
| elven (both bodies) | Fine (narrow oval, high cheekbones), Sharp (angular, pointed chin), Long-elven (long face, sleek) | the straight and button noses, finer | thin, swept up |
| youth | the soft adult types of the same sex (Oval, Round, Long, Fine) | button, straight | thin, arched |
| child (11 px head) | Round, Oval, Pointed, Elfin | button, small straight | 2 |

Humans draw from the 6 human types, elves from the 3 elven ones, half-breeds from all 9 (mixed heritage) with their own
ears. Man: 9 x 5 x 4 = 180 faces; woman: 9 x 4 x 4 = 144, before ears, marks, age and eye colour.

**Ears**: human (round, 1 px inner shadow), half-breed (a 1 px tip up), elf (2-3 px swept up and back, breaking the
head's outline). Human ears are drawn under the hair (long hair covers them); half-breed and elf ears over it.

**Marks** (per person, by the look): freckles (a 3-5 px scatter over the nose and cheeks in skin step 1), a scar (men: a
1 px skin-step-3 line through a brow or cheek), a beauty mark (women: 1 px), weathered (sun lines at the eyes). **Age**
(elders): lined (a 1 px crease at each outer eye corner and from nose to mouth) or gaunt (hollow cheeks, a sharper jaw);
brows and hair grey through their ramps; the receding and bald styles are common. Never on youths and children.

**Expressions** (overlays at the face anchor; the only face changes): neutral; blink (the lash drops to the iris row, no
white: 0.12 s); hurt (eyes squeezed to 2 px dark lines, a 2 px open mouth); fierce (A3's swing face: brows down a pixel,
the mouth open 2 px: on strike frames); closed (a 2 px lash line: lying, sleeping); talk (the mouth 2 px, alternating with
neutral while a speech plate shows). The hero gets nothing extra in the face: the ring marks the hero.

---

## 5. Hands

A3's hand is a 3 x 2 block at the end of the arm (skin step 2-3 with the outline on its outer side and bottom); an open
hand is 3 x 3. The hand is drawn OVER what it grips: the grip vanishes under the fist, the guard shows on one side and
the pommel on the other. Front and back views: both hands show in idle and walk at F-11..F-10. Side views: the near hand
always; the far hand peeks past the hip on the back-swing (A3's left walk) or when it holds something. A hand at rest on
a sheathed hilt (A3) is the fighter's idle. Gloves recolour the hand region (leather, metal); gauntlets add the cuff
flare region (section 22).

---

## 6. Palette and ramps

**Every material is a 5-step ramp: 0 outline, 1 shadow, 2 base, 3 light, 4 highlight.** Shadows lean cool (toward violet
and blue), lights lean warm (toward yellow): hue-shifted, never just darkened. Step 0 is the material's outline and is
as deep as the world's darks (timber #453035): luma 70 or less, and at least 40 below the ramp's base.

**A3's ramps** (outline > shadow > base > light > highlight), tuned against the world: green roofs #44655A, olive grass
#8C8D50, terracotta #B76B49, timber #643C2C, darks #453035.

| ramp | hero | woman |
|---|---|---|
| hair | copper `#3A1622 #742E2C #AA4A2C #D9733A #F5B060` | wheat `#3E2418 #8C5A2A #C79346 #EBC26C #FFEBA8` |
| skin | `#5E2A36 #B8644F #E9A077 #F8C597 #FFE6C2` | `#5E2C36 #C4705A #EEAA86 #FCD2AA #FFEAD0` |
| cloth 1 | tunic teal `#162540 #1F4A68 #28728C #3E9FA7 #8BD8C6` | dress berry `#2C1030 #5E1D48 #922F60 #C24F76 #EC8E9E` |
| cloth 2 | trousers `#1C1732 #33304E #4F4B6B #777493` | (under the skirt) |
| accent | scarf `#44122C #87232F #C83E38 #F47650` | ribbon `#1F4A68 #3E9FA7` (the hero's teal) |
| linen | | chemise `#4B3C55 #ADA4C0 #DCD6E2 #FAF6EE` |
| cloth 3 | | apron `#4E3A30 #A99070 #D9C69E #F3E6C4` |
| leather | `#2C1621 #5B3129 #8D5536 #BF8249 #E7B672` | the same |
| metal | steel `#201C31 #525C7A #95A2BC #D5DEEA #FFFFFF` | |
| metal 2 | brass `#704020 #CB9034 #F8DA80` (steps 1-3) | |
| eyes | iris `#3A78B0`, glint `#8FC6E8` | iris `#3F8A5A`, glint `#9FDCA8` |

A3's villager swaps (docs/ref/style_a3/palettes.json: traveller, moss lad, Mira, Hedda) are the first entries of the
curated tables: two deeper skins, black and auburn hair, rust, moss, blue and ochre cloth.

**Slots.** A look resolves to these ramps; every map pixel names a slot (or a region that resolves to one, section 24)
and a step:

| slot | from the look | ramp source |
|---|---|---|
| skin | skin | curated: the culture's 12 skin steps (cult::skinRamp) and the creator's 8, anchored on A3's four skins; elf tints; (P2c) draugr, goblin, bone |
| hair | hairColor (+ grey with age) | curated for the creator's 10 colours and the cultures' hair lists; the generator for any other; elder greys |
| eyes | eyeColor | the creator's 6 (Brown #603A20, Blue #3C70C4, Green #388C50, Grey #788492, Amber #CE9228, Violet #8850B0) as iris + glint pairs; 0 picks by skin and hair |
| cloth 1 | topColor | generator |
| cloth 2 | bottomColor | generator |
| accent | tabardColor (scarf, sash, tabard, trim) | generator |
| linen | the culture's linen (default A3's chemise) | curated: white, unbleached, grey |
| cloth 3 | aprons, overskirts | generator (default A3's apron) |
| leather | the culture's leather (default A3's) | curated: 4 browns, black, red-brown |
| metal | armour band / armourTint / sheen | the 7 bands curated on A3's steel; alloys through metalRampKeys re-tuned to A3 |
| metal 2 | trimColor / gold | curated brass, gold, silver, bronze |
| head | headColor (headwear) | generator |
| cloak | cloakColor | generator |
| pattern | patternColor | generator |
| plume, field, device, glow | plumeColor, shieldField, shieldDevice, glowColor | generator (P2c) |

**The generator** (`rampA3(base, material)`) makes a ramp from a base colour: step 1 darker and turned toward violet,
step 0 deeper still (luma 70 or less), step 3 lighter and turned toward yellow, step 4 lighter, warmer and a little
desaturated; metals with a wider spread and a white-hot step 4. It is calibrated on A3's 14 hand ramps: given each A3
base it reproduces the hand ramp within delta E 6 per step (CIE76), and that is a gate.

**Fixed colours.** Ink #21152A (the sole line, the darkest colour of a person; never #000000); Lash #24142A; Eye White
#F6EEE3; Mouth, Lip and Blush derived from the skin ramp (A3's values for A3's skins). The ring, pips and night lift keep
P2a's: Ember Gold #F2C14E, Ring Shadow (Ink 45 %), Warning Ember #E8603C, Moonwash #8FA6D8.

**Effects.** The sword's arc is A3's (edge #7FA8D8, body #C9E6F4, core #FFFDF4). The other families keep P2a's named
colours re-cut to A3's three-tone construction: Heavy Arc (core #FFF2D0, edge #C8A878), Chip (wood #B07A4A, bone
#E6DCC0), Smash Dust #C8B496 with sparks #FFD87A, Glint (#FFFFFF, halo #FFE9A8), Thrust Streak (#E8F0FF to #9FB4D8),
Swish (#D8CDB8), Punch Star (#FFF0C0, rim #F08A3C), Twang #E8E0C8; the finisher tints #FFE6A0.

**Budget**: 48 colours or fewer per composed figure (A3: hero 40 plus the arc, woman 37).

---

## 7. Light, shading and materials

- **Light from the top-left** in every facing, a little toward the viewer. The head is a sphere with a lit crown and a
  crown highlight ring seen from behind; limbs are cylinders (lit left, shadow right); the torso a box with a lit plane.
- **Contact shadows**: the fringe onto the forehead, the chin onto the neck, arms onto the torso, under the belt.
- **Cloth**: matte; one lit plane on the chest, folds as single step-1 lines (one under a strap, clean vertical skirt
  folds, never alternating 7 / 6 pixels); hems and trims are **solid lit bands**, never dotted.
- **Leather**: its own warm ramp (belts, bracers, boots, straps, scabbards); a shine pixel on the lit toe of a boot.
- **Metal**: its own cool ramp; steel gets a white glint pixel; brass marks buckles, guards and pommels. Mail is the
  metal ramp in a brick of steps 2 and 3 on the lit side and 1 and 2 on the shadow side; plate is broad steps with a lit
  ridge; scale and lamellar are rows of 2-3 px units with a step-1 lower edge.
- **Hair**: a volume: lit locks on the left, a darker lower right, strands that gather into the braid or tail; streaks
  of step 3 follow the style's flow.
- **No pillow shading, no banding** (neighbouring steps never run as parallel stripes), **no dither** on people, no
  orphan pixels (a pixel all eight of whose neighbours are transparent), no lone-colour noise outside the face and FX.
- Ground shadows are never painted into a sprite: the renderer draws the contact ellipse.

---

## 8. Outlines and contrast

- **Coloured outlines (people).** Every material is outlined in its own step 0: hair in hair step 0, skin in skin step
  0, the tunic in tunic step 0. The **finishing pass** (the doll's, after all layers are composed) recolours each outline
  pixel whose outward edge faces the top-left light to step 1 of its material (A3's selout rule: the outward normal from
  the transparent neighbours, orthogonal weight 1.0, diagonal 0.7; lit when its dot with (-0.707, -0.707) exceeds 0.35).
  It recolours existing pixels only; the silhouette never moves. Artists draw step 0 everywhere on the silhouette and
  never paint the lit outline by hand. Ink appears only on the sole line.
- **Inner contours**: where a part lies over another part of a similar value (an arm over the torso, a hand over cloth,
  a braid over a shoulder), the part's own step-0 edge separates them; no double lines; nothing outlined inside the face.
- **The world** keeps its selective outlines; creatures and v1 sheets keep P2a's Ink outline until P2d redraws them.
- **Night**: the night lift (P2a, measured and passing L6) adds each visible person again in Moonwash after the light map;
  its interior mask is the figure minus its outer ring.
- **The hero's ring** (P2a): 24 x 9 Ember Gold under the feet, 80 % alpha, flaring to 95 % for a second when hit or in a
  crowd; PLAYER RING in SETTINGS.
- **Measured targets** (P2a L6 restated, kept): by day the outline ring against the ground ring at least 40 luma on
  grass, road, sand and snow (tools/p2_measure.mjs contrast); at 23:00, 5 tiles or more from any light, for the hero and
  an unlit villager on each ground: |figure median - ground median| at least 10, or the edge at least 40. Ring contrast
  35 or more. A3's coloured outlines meet these (step-0 luma: hair 34, skin 59, tunic 36, trousers 28, leather 30, steel
  32, dress 28, apron 63, chemise 67); the lit step-1 pixels are the minority on any ring.

---

## 9. Facings

Four facings, painted natively: down, up, right and LEFT. The left facing is painted, never mirrored at runtime, so the
light stays top-left and the weapon stays in the right hand (A3: the sword on the left hip, the hand on the hilt; facing
left it is the near hip and the scabbard crosses in front of the legs). An artist may start a left view from a mirror
and relight draft (section 24), then repaints it; the committed map is hand-checked. Facing right the weapon hand is the
near hand; facing left the far hand, and at least 4 px of blade or haft shows beyond the body. FX may be mirrored and
turned (they carry no light).

---

## 10. Animation: clips, frames, timing

The doll composes every frame from parts (section 24) and packs them into trimmed atlases with a pivot per frame (the
point between the feet on the sole row). Atlases per look: LOCO (everyone: idle, walk, hurt; sheathed), RUN (fast
movers), ACTION (fighters: drawn-weapon idle and walk, the family's swings, cast, bow, block, roll), DEATH (on falling),
POSTURE (per posture in use). The clip list and its timing model are P2a's (anim.cpp):

| clip | frames per facing | timing | reads as |
|---|---|---|---|
| Idle | 3: neutral, breathe, blink | neutral and breathe alternate 0.55 s (the upper body 1 px up); a 0.12 s blink every 3-6 s, phase per actor | alive while standing |
| Walk | 6: contact, down, pass, contact, down, pass | keyed to the whole px walked (below) | feet planted |
| Run | 6: contact, push, flight, contact, push, flight | keyed to the whole px moved, above 54 px/s | a purposeful jog |
| Hurt | 1 | held through the Hurt state | the flinch face, shoulders up |
| Windup / Strike / Follow | A3's sword: 1 / 1 / 2 (follow-through, recover); a family may use 2 / 2 / 2 | the states' times x the family's pace: A3's sword 70 / 110 / 70 + 60 ms | anticipation, blow, weight |
| Combo variants | hit 1, hit 2, hit 3 (finisher) | as above per hit | three different blows |
| Cast | 2: gather, release | | magic |
| Draw / Release (bow) | 3 + 1 | draw over the windup, release 0.08 s | a real draw |
| Block, Roll (depth) | 1, 4 | parry window; 0.085 s each | |
| Fall | 3: recoil, buckle, topple | 0.10, 0.12, 0.12 s | the body gives way |
| Lying | head west, head east | 60 s, fading the last 5 | the dead lie down |

**The shipped method (2026-10-22; binding; wins over every number below that disagrees).** docs/MOTION_P2.md is the side
walk and run as the owner approved them (proto6 i3, then the 2 px run bob on 2026-10-22, then RUN FIX i1-i3): legs drawn per
frame from their joints at the idle's length, full thickness across the bone, the far leg a whole leg on its own part, one
boot library placed by `put`. The **walk** is 6 frames on a **22 px** cycle (holds 3 / 4 / 4 px, bob +1 / 0 / 0, lowest on
the contact); the **run** is 6 frames on a **36 px** cycle (holds 6 / 6 / 6 px, **bob 0 / -1 / -2**: the run may bob 2 px,
the walk 1), the upper body leaning 4 px (head, torso and arms together; the torso's sheared `runl` view; hip-anchored parts
on `hip=2,0`, the woman `3,0`), the arms pumping on their own keys (pB pM pF pK pN). docs/P2B_MOTION_ROLLOUT.md carries the
method to every body, garment and item; the gates are B11 limbs, B11 wear and B12 boots (MOTION_P2 section 13).

**The owner's motion reference (2026-10-20; binding for the SIDE walk and run; supersedes the walk fix's boot and stance
rules below, and anything else in this section that disagrees).** The owner circled the side run ("the feet are
completely disconnected from the body"), then sent docs/ref/motion/owner_walk_ref.png, an 8-frame side walk in another art
style: "Not the art style but the walking animation and how it works, this is what you need to shoot for. See how the feet
art constantly flat but can be angled throughout the strides." docs/MOTION_P2.md holds the key poses (every frame, both
facings, every body), the boot keys, the plants and the gates; these are its rules:
- **Legs are limbs.** In every side walk and run frame both legs show, continuous from under the hem to the boot: the
  trouser thigh, the knee at the boot top, the leather shin, the ankle on the boot. The hem rides a row up while walking and
  running (the garments' `run` views); hangs open over the stride; long garments show the shoe tips.
- **A scissor stride from one pelvis**, scaled to our 10-row leg: an inverted V at contact (each leg about 20 degrees from
  vertical, 16 px heel to toe), the legs crossing at the passing pose, the planted leg carrying the body. The run lands flat
  on a bent knee, kicks the trailing heel up, and flies with the leading knee up and the trailing foot pointed: never a
  stiff inverted V.
- **One boot, angled.** The foot is one hand-drawn boot with a clean straight Ink sole that tilts through the stride: toe up
  at heel strike, flat while it carries the body, heel up behind, rolled onto the toe at push-off, toe down while it
  swings. Five keys (flat, toe-up, heel-up, toe-off, hang), each a hand-drawn rotation of the idle's foot (the same size and
  parts; never a shear, a squash or a tool rotation), drawn once as a stamp and placed in every frame. It pivots on the end
  that touches the ground; a supporting foot sits on the ground row. Only the foot tilts, at a clean ankle: the shaft is
  the shin. (The round the owner rejected on 2026-10-19, "the feet are getting rotated, joints look a little off", turned
  whole boots and broke the joints.)
- **Still binding:** chunky legs (90 % of the idle's visible leg area, the idle's hip width at most 1 px narrower, no leg
  run under 3 px above the feet), clean knees and ankles, a hip bob of 1 px at most in the walk and 2 px in the run, no
  squat, the planted leg under the hips, women drawn and judged in trousers, bare legs and skirts, the 22 / 36 px cycles and
  6 frames, registration. Elves show the leg drawing's top row: it is a finished seat row (walk and run keys use no stretch
  row). Front and back walks are unchanged.
Gated by `rpg_test --doll --gates` (B11 limbs, B11 wear and B12 boots, MOTION_P2 section 13) and the judge's trace of every
frame.

**The owner's walk fix (2026-10-19; binding, supersedes the stride notes below where they differ; its boot and stance
rules are superseded for side views by the motion reference above).** After three review
rounds the owner rejected the walk drawn from A3's leg drawings ("his legs all of a sudden get super skinny") and signed
off a redrawn walk and run. The A3 idles and the swing keys stay pixel for pixel (0 px parity); **every walk and run frame,
side, front and back, was redrawn** and is no longer A3's. The redrawn walk keeps A's chunky legs and the A3 silhouette:
- every walk and run frame keeps 90 % or more of the idle's visible leg area and the idle's hip width (the hip row, the
  first visible leg row, at most 1 px narrower); no visible leg run under 3 px (no sliver of a leg);
- ~~boots are never drawn rotated: a flat sole, the idle's boot moved and lifted~~ (front and back views still; side views
  since 2026-10-20: one boot in five hand-drawn angled keys, above); clean knee and ankle joints, the bend at the knee;
- the hip bob at most 1 px (0 / +1 / 0; the hip row within 1 px of the contact frame's, no squat); the planted leg straight
  under the hips; no splay beyond MOTION_P2's key poses (a down frame no wider than its contact; the "compact stride" and
  the 13 px stance caps are superseded by MOTION_P2's table);
- **both bodies walk an 18 px cycle; the run is 36 px** (above 54 px/s, signed off); the frame table still comes from the
  plant anchors (section 24);
- the woman's legs are drawn and gated in trousers and bare legs as well as under the skirt (her skirt keeps 90 % of the
  idle's area).
These are gated by `rpg_test --doll --gates` (B11 leg mass: leg area, hip row, narrowest run, hip bob, stance; side views
since 2026-10-20 B11 limbs and B12 boots; `--report` prints every frame), not by A3 parity. Every new leg set, garment and armour piece that shows legs passes the same checks.

**The walk fits A3's stride** (the study's measurements, kept as history; the cycle lengths above win). Measured on A3's side walks (docs/ref/style_a3, the soles' Ink spans): the man's planted
foot travels 9 px from heel strike to toe-off (centre 18 > 15 > 9 on the near leg; 18.5 > 14 > 9 on the far leg), the
woman's 10 px (16.5 > 13.5 > 6.5). A walk cycle is two steps, so the study gave the man a 19 px cycle and the woman 20 px (the owner's walk fix set both to 18 px).
(A2's notes asked for a longer cycle for A2's longer legs; A3 keeps A's shorter legs. On the 28 px cycle A3's feet skate
forward about 4.5 px a step, and A3's contact and down frames share one leg drawing, so the planted foot cannot move
between them.) The fix, painted in the slice: each side view gets 6 leg drawings. A3's contact drawings stay pixel for
pixel; the pass drawings are re-registered with the planted foot under the hips (1-3 px further back than A3's); and two
new down drawings bend the knees with the planted foot 3 px behind contact. The planted foot then steps 3 / 3 / 3.5 px
(man) and 3 / 3.5 / 3.5 px (woman). The frame table is not a constant: it comes from the clip's plant anchors
(section 24). At 40 px/s the man's walk runs at 12.6 fps (2.1 cycles a second); a 20 px/s stroll at 6.3 fps. Front and
back views walk in place (the 16-bit convention, A3's drawings).

**The run (signed off by the owner, 2026-10-19).** A3's walk at the hero's 74 px/s would be 3.9 cycles a second, a
scurry. Above 54 px/s people run: 6 frames, the torso leaning 1 px forward in side views, the hips bobbing 1 px at most
(the owner's walk fix above; the first plan's 2 px bob is superseded; **since 2026-10-22: lean 4, bob 0 / -1 / -2, holds
6 / 6 / 6, the shipped method at the top of this section and docs/MOTION_P2.md 6**), the legs keeping the idle's mass, arms bent and pumping, every hanging part lagging 2 px. The planted foot travels about 10 px in the two stance
frames and the flight adds about 8 px a half-cycle, so the cycle is **about 36 px** (the drawings' anchors decide): at
74 px/s, 2.06 cycles a second and 12.3 fps, close to the cadence of A3's approved walk GIF (2.6 cycles a second). Kids
run with the Play posture's frames; elders never run. Its side poses (landing, push, flight) are MOTION_P2 5.3's.

**Registration** (P2a, kept): at every frame change of a side walk or a run's stance frames, the planted foot lands within
1 px of where it stood (since 2026-10-20 measured at its ground contact: the heel from heel strike to mid-stance, the toe
from heel-off to toe-off; the clips' plant is the planted heel, MOTION_P2 5.4); within a held frame it slips one frame step or less; a walk starts on the pass frame; the body
stays steady on screen (pivot changes on 2 or fewer of 119 frames on W, E and S).

**Secondary motion** (A3): the cowlick stretches a pixel on the down frames and flattens on the up frames; the scarf tail
trails in two strengths in side views; the braid tip swings a pixel behind the bob in every facing; the hem sways by
whole rows, never jagged; the scabbard lags the bob a frame; the kerchief knot sways from behind. Every hair style with a
hanging part has its hang keys (section 21).

---

## 11. Weapons: size, grip, depth, sheathed and drawn

| weapon (art id) | length on the 32 px body | grip |
|---|---|---|
| dagger (5) | blade 5 + hilt 3 | one hand |
| sword (1) | blade 11 + hilt 4 = 15 (A3: a 2 px blade, steel ramp, white glint, brass guard and pommel) | one hand; guard and pommel either side of the fist |
| greatsword (7) | blade 18 + hilt 6 | two hands, 3 px apart |
| axe (2) | haft 13, bearded head 5 x 6 | one hand |
| mace / hammer / pick (6) | haft 11, head 4 x 5 / haft 13, head 6 x 3 | one hand |
| spear (8), glaive, halberd | shaft 34, leaf head 5; glaive blade 7; halberd axe and spike | two hands, 6-8 px apart |
| staff (4) | 30 with a 3 x 3 head | two hands, or one and planted |
| walking stick (9) | 24 with a crook | one hand |
| bow (3), longbow, recurve, crossbow | 22 / 28 / 20 with curled tips / 12 x 10 | off hand holds the bow |
| ledger (10), scroll case (11), horn (12) | held items | one hand |

- **Authored, not rasterized.** Each weapon and form is a set of hand-cleaned maps at the 16 directions (22.5 degree
  steps), drafted once from P2a's rasterizer: clean lines, the lit edge toward the top-left in every direction, a bright
  tip pixel on blades, no doubled pixels or L-shaped jaggies. Culture blade, polearm and bow forms are their own maps on
  the base hilts. Tier tint is the metal slot.
- **Depth**: the clip says per frame whether the weapon is behind the body or in front (STYLE P2a rule kept).
- **Sheathed and drawn.** Out of combat a fighter's weapon rides on the body (A3): the sword in its scabbard on the left
  hip with the hand on the hilt; the dagger at the hip; the axe and mace through the belt; the greatsword across the
  back; the spear, staff and stick carried upright in the hand; the bow on the back. In combat (the hero for 4 s after a
  swing or while a hostile is within its aggro radius; foes while aggro'd; guards while fighting) the weapon is in hand
  and the scabbard hangs empty and steeper (A3's swing). A legendary or Mythic hilt glints in its glow colour while
  sheathed, so the weapon still reads. The owner confirms sheathing at the slice; the fallback is P2a's "always in hand".

---

## 12. Swing families

P2a's family table stands (each family its own three blows, tip paths, effect and pace), painted on A3's body:

| family | hit 1 | hit 2 | hit 3 (finisher) | effect |
|---|---|---|---|---|
| Unarmed | jab | cross | rising hook | Punch Star |
| Dagger | stab from the hip | short slash | two quick stabs | Glint at the tip |
| Sword | A3's swing: the coiled back-swing, then a level sweep with the arc over the head into the blade | the mirror path | overhead cut to the low front | A3's Steel Arc |
| Greatsword | heavy overhead cleave, stance 2 px wider | waist-high sweep | rising sweep into a ground slam | Heavy Arc with a ghost blade |
| Axe | chop from behind the head to knee height | diagonal from the other side | two-handed overhead chop | short heavy arc, Chips |
| Mace / hammer | overhead smash onto the ground line | heavy hook | bigger smash | Smash ring, dust, sparks |
| Spear / glaive / halberd | thrust | higher thrust | lunge (spear); sweep (glaive, halberd) | Thrust Streak |
| Staff / stick | diagonal two-handed strike | rising strike | overhead smash | Swish |
| Bow | draw and release in every facing | | | Twang |

- **A3's sword is the reference swing.** Wind-up: weight on the back foot, the hand behind the back shoulder, the blade
  cocked up and back, clear of the jaw. Strike: the arc rebuilt from a 1 px wisp where the blade was to 6 px at the
  blade, in three tones, drawn behind the body over the head. Follow-through: the blade cuts down in front with a
  thinning tail. Recover: a guard at 45 degrees. Facing down: a horizontal sweep whose arc passes in front of the feet.
  The blow's face is fierce.
- **The arm's reach on this body** (A2's finding, binding): shoulder to crown is 13 px and the arm about 12, so a raised
  one-handed wind-up lands the hand at the neck. Raised wind-ups cock the hand behind the back shoulder; overhead blows
  (greatsword, axe, mace) hold the hands at crown height with the weapon behind the head.
- Two families never share a strike frame (25 % or more of the union differs, per facing); a family's three variants
  differ by 25 % or more; tip paths as P2a's table; the effect ends with the strike state within a frame.

---

## 13. Hurt, fall, lying, corpses

- **Fall** (every human, every facing, both sides, 0.34 s): recoil (head snapped back, arms flung, the hurt face),
  buckle (knees bent on the kneel legs, the torso slumped, the head bowed), topple (the lying figure itself, 2 px above
  its rest and 2 px toward where the body stood: the body in the air just before it lands).
- **Lying**: the composed side view (the left view: the face toward the top of the screen, lit) with the lying arms and
  legs (splayed keys painted in the side view), turned a quarter clockwise as exact pixels (head east), mirrored for
  head west, then finished again (the finishing pass relights the outline for its new edges). Eyes closed. The weapon
  dropped beside the hand at its own angle, a shield flat beside the body; a helm stays on the head, a cloak lies under
  the body. 16 px tall or less. A part that reads wrong turned may carry its own painted lying view (section 24).
- **Corpses** (P2a, kept): 60 s, fading the last 5 s, blocking nothing, 24 at most; a small dark stain (Ink 25 %) under
  them; no gore. The hero falls under the death screen.
- **Kneel** (one knee down, head at about 22 px) and **Downed** (slumped on one knee, a hand to the wound, head low, two
  slow breaths) are postures.

---

## 14. Bodies: man and woman, kids, elders, peoples

**Man and woman** (A3 measured):

| aspect | man | woman |
|---|---|---|
| height | 32 (crown F-31) + hair rise | 31 (crown F-30) + hair rise; head and torso one row lower, feet the same |
| shoulders with arms | 16 | 16 (the chemise's puff sleeves) |
| torso, arms | 10, 3 each | 10, 3 each; the waist 12 px across at F-9 under the arms |
| neck | hidden by the collar or scarf | 4 px of skin at F-18 |
| chin | 8 px | 6 px |
| lash | 2 px | 3 px, lifted outward |
| mouth | 1 px | lip tint + mouth, blush |
| legs | 3 rows of trousers, 5 of boot below a hem at F-9; feet together | the same legs in trousers (a few painted overrides); under a long skirt, shoe tips only |
| beard | allowed | never |
| hair | any style | any style |
| clothing and armour | fitted to the man | fitted to the woman: the same pieces cut to her torso, a 1 px waist and a shaped breastplate by one step-1 row, never sculpted cups |

**Kids**: youth (26) and child (21) bodies have their own legs, arms, garments and hangs; the youth uses the adult head
box (adult faces of the soft types, adult hair and headwear); the child has its own head, faces and hair. No beards,
marks or age lines on kids; no armour on kids (goblins excepted, P2c).

**Elders** (census age 60 or more, Job::Elder): the adult body with the head stack 1 px lower (1 px forward in side views),
the shoulders rounded a pixel, age marks, grey or white hair ramps, the receding and bald styles favoured, a slower walk
(0.8 x speed) and often a walking stick.

**Peoples**: human, half-breed (ear tips, any of the 9 head types), elf (long swept ears over the hair, the elven head
types, one build slimmer, +1 px leg).

---

## 15. Postures, things in hand, riders, rowers, sleepers

All 31 postures (art_life.h) keep their variants, frame counts and paces, in four facings, plus **Row** (new, P2b: seated
on the thwart, reach, catch and pull, synced to the oars' stroke). A posture is the body's leg mode (stand, chair, floor,
kneel, ride, ground) + its arm keys + a prop in hand + its offsets. Props (mug, spoon, bread, hammer, hoe, broom, axe,
ladle, sack, basket, crate, rod, lute, drum, flute and the culture instruments, lamp pole, book, ledger, bowl, linen,
reins) are maps in their own ramps, the hand drawn over the grip. A chair-height sitter's hips sit on the seat (0 px) with
the feet on the floor; a cross-legged sitter 2 px above the cushion; a rider's hips on HOLDERS' saddle with the hands at
the reins on every horse frame; a rower's hips on the thwart; a sleeper is the head (closed eyes) and its hair on the
berth's pillow with the body a mound under the berth's own blanket (north-south beds: the front head; west-east berths:
the side head turned a quarter like a corpse). Skeletons, draugr and goblins are P2c's: a bone body set, and the adult
and youth bodies with their own skins, heads and gear.

---

## 16. How bodies meet the world (P2a, kept)

Feet on the ground line in every contact frame (0 px); hips on the seat; a rider on the saddle, a rower on the thwart, a
sleeper's head on the pillow. Door openings 0.70-0.76 of the 32 px body; a walker's head may overlap the lintel by 9 px
as it steps through; the hero never looks taller than the eaves of a one-storey house. Sorting by the feet; a lying body
by its own baseline; roofs, crowns and vaults still turn a hidden hero into the silhouette.

---

## 17. Composition in the world (P2a, kept)

People are the focal points; nothing person-sized crowds a path (ground clutter by roads and doors under 0.6 of the
figure). Markers float 4 px over the frame's own top (hair, a hat, a crest); bubbles above the markers. In a crowd the
hero is found by the ring first, then by the outline and the face. Nothing floats. A3's scene shots
(docs/ref/style_a3/scene.png, scene_phone_zoom.png) are the reference for people in the world.

---

## 18. UI for P2 (P2a, kept)

ZOOM and PLAYER RING in SETTINGS; text never in the world layer; off-screen pips. Menu figures (the paper doll, the
creator preview, the HUD hero, the party list, portraits) show doll frames at whole multiples (P2c).

---

## 19. The style slice (P2b wave 1, artist-xhigh; the owner signs it off)

The smallest finished piece that locks the look on the production pipeline. Nothing else is drawn for production until
the owner signs it off from the screenshots.

**Contents**
1. **A3 in the engine.** The A3 hero and woman as layered part maps, composed by the doll at `rig 2`: idle, walk and the
   sword's hit 1 in all four facings (A3's right and down swings, plus up and left painted new); the scabbard, the hand on
   the hilt, the scarf, the braid, every secondary motion. With A3's palettes, every frame A3 drew that this file keeps
   (the idles and the swing keys) is pixel-identical to docs/ref/style_a3/frames. (As built: the walk frames, side, front
   and back, were redrawn at the owner's request on 2026-10-19, section 10 "The owner's walk fix"; they are gated by the
   leg-mass, hip-row and stance checks of `rpg_test --doll --gates`, not by A3 parity.)
2. **The walks redrawn** (section 10): chunky legs, flat boots, clean knees, a 0 / +1 / 0 bob, both bodies on an 18 px
   cycle (first planned as the side walks re-registered: the pass drawings moved, the down drawings new).
3. **The run**: the hero (and the woman) in four facings.
4. **Variety on the same pipeline** (idle and walk, four facings): 2 more head types per body (man Square and Long; woman
   Heart and Round) with 2 noses and 2 brows each; elf and half-breed ears; 2 more hair styles per body (man Cropped and
   Long loose with its hang; woman Bun and Twin braids with their hangs); 2 beards (Full, Moustache); a garment swap per
   body (the man in a Robe with its long hang; the woman in shirt and trousers, showing the shared legs); **Mail on both
   bodies** with a Nasal helm, a Hood on the woman; an elder man (Receding, grey, lined, the stoop); a child (idle and
   walk); A3's four villager swaps and three cultures' ramps.
5. **The fall and lying** for the hero, both sides, the sword dropped beside the hand; **Sit** on a chair for both bodies.
6. **The owner pack**: the parity sheet (engine beside reference), a 12-person variety lineup of three cultures, the
   armour lineup (man and woman, mail and helm), the walk, run and swing GIFs at game timing, the fall, and in-world shots
   (seed 7, the start village OSLARS: desktop 1440x810 at NORMAL D3 and NEAR D4, phone 2556x1179 at NORMAL D5, 11:00 and
   22:00), with A3's scene_phone_zoom.png beside the same spot.

**Acceptance** (measured; the full protocol is VISION_PLAN 15.36 P2b "The slice gate"): A3 parity 0 px on the kept frames
(the 8 idles and 8 swing keys; the redrawn walk and run frames by the leg-mass gates of section 10);
heights by section 3; every world pixel a whole D x D block; contrast by section 8; walk and run registration by section
10; every slice look covered by authored parts; the craft checks of section 24 at 0 failures; the ramps generator within
delta E 6 of A3; then the judge's look review (A3's look held on every new part, the top-left light on all four facings,
no pillow shading, banding, orphans or jaggies, the hero found first in the village shot); then **the owner's sign-off**,
which also settles the two open looks: the run above 54 px/s, and sheathed weapons out of combat.

---

## 20. Don'ts

- No fractional world scale, no blur, no runtime scaling or rotation of sprites (a quarter turn of a composed lying
  figure at bake time is exact pixels, not a runtime rotation).
- No mixels: never a v1 frame scaled up, never a sprite at another pixel size than its neighbours.
- No limb, face or fold drawn by geometry code; no procedural texture except the material rules of section 7.
- No pure black; no black outer outline on people; no hand-painted lit outlines (the finishing pass makes them); no double
  lines; nothing outlined inside the face.
- No pillow shading, no banding, no dither on people, no dotted trims, no orphan pixels.
- No mirrored left frames at runtime; no left map shipped unchecked from a draft; no weapon changing hands between facings.
- No exaggerated anatomy on either body; no armour that sculpts a body; no garment shared between the man and the woman.
- No head type that moves A3's eye rows, eye columns or mouth row; no beard on a woman or a kid.
- No text in the world layer; no body-holder left at its pre-P2 size once the doll ships.

**The owner's rules (hard, from the owner's own reviews; every part that draws bodies, legs, clothing or poses obeys them):**
- No skinny legs. Keep A's CHUNKY proportions: sturdy legs and boots, the A3 silhouette (the owner rejected skinny legs
  twice).
- No walk or run frame under 90 % of the idle's leg area or narrower than the idle's hip width; no distorted, sheared,
  squashed or tool-rotated boot (side views: only MOTION_P2's five hand-drawn angled keys of the one boot, placed as
  stamps; front and back views: the idle boot moved and lifted); no mushy knee or ankle (clean joints, the bend at the
  knee); no hip bob over 1 px (0 / +1 / 0); no planted leg off the line under the hips; no splay beyond MOTION_P2's key
  poses (a down frame no wider than contact). `rpg_test --doll --gates` measures these.
- (The owner's motion reference, 2026-10-20.) No side walk or run frame with a leg hidden under a hem (long garments show
  their shoe tips), a boot that floats or reads glued to a hem, a leg that is not continuous from the hip to the boot, a
  planted foot that skates, or a flight that is a stiff inverted V of two straight legs.
- No woman's body judged only in a dress: develop and judge women's legs and bodies in trousers and bare legs too (they
  wear trousers, armour, short garments). No clothing or armour shared between the bodies: each is fitted separately for
  men and for women.
- No thin variety: many head types, lots of hair styles (the plan's 9 + 9 heads, 24 hair styles, 9 beards), garments per
  body; nothing that reads as one head recoloured.
- Nothing crappy or half baked: the bar is triple-A pixel art (Sea of Stars / Chained Echoes craft at 32 px).

---

## 21. Hair and beards

Adult hair is drawn on the head box and shared by the man, the woman and the youth (the head box is the same; the woman's
sits one row lower). Every style has 4 facings; a hanging style has hang keys; a style that rises or bounces has tuft keys;
a volume style has a capped variant for under headwear.

| # | style | notes |
|---|---|---|
| 1 | Bald | the head type's skull, a skin highlight on the crown |
| 2 | Shaved | a stubble cap: hair step 1-2 speckle-free fill, the skull shape showing |
| 3 | Cropped | close on the sides, 1-2 px on top |
| 4 | Short | neat, a side part, the fringe off the brow |
| 5 | Tousled | A3's hero: locks over the brow, the cowlick (tuft keys: stretch, squash) |
| 6 | Spiky | 3-4 spikes rising to F-33, lit tips |
| 7 | Curly | short curls, a lumpy lit outline |
| 8 | Receding | thin on top, the forehead high, full at the sides (elders) |
| 9 | Mohawk | a crest to F-33, shaved sides (tuft keys) |
| 10 | Topknot | shaved or tight sides, a knot on the crown (tuft keys) |
| 11 | Bob | chin-length, a straight fringe |
| 12 | Shag | layered to the shoulders, swept |
| 13 | Long loose | straight to the shoulder blades (hang keys) |
| 14 | Long wavy | to mid-back, waves in step 3 (hang keys) |
| 15 | Ponytail | low tail (hang keys) |
| 16 | High ponytail | crown tail (hang keys) |
| 17 | Braid | A3's woman: one braid over the near shoulder in side views, over her left shoulder in front (hang keys) |
| 18 | Twin braids | two braids (hang keys, out of phase) |
| 19 | Bun | a low chignon |
| 20 | Locs | rope locks to the shoulders (hang keys) |
| 21-24 (depth) | Crown braid, Top bun, Pigtails (hang keys), Big curls (volume) | |

Child hair (11 px head): Short, Tousled, Cropped, Curly, Bob, Long, Pigtails, Braid, Bun, Ponytail.

**Beards** (men; hair ramp): Stubble, Short boxed, Full, Long (to the chest), Braided (Fjordfolk), Goatee, Moustache,
Van Dyke, Mutton chops; Handlebar as depth. A full beard's outline covers the widest jaw, so it fits every head type;
stubble, goatee and moustaches are overlays at the mouth anchor; mutton chops follow the cheek and come per jaw class.
One pixel of mouth always shows.

**Headwear over hair**: each headwear and helm carries a mask of the hair it hides; long lengths, braids and tails below
its rim still show; volume styles switch to their capped variant.

---

## 22. Garments and armour

Every garment and armour piece is **fitted to each body**: the man's and the woman's are separate maps, never one shape.
A garment is a torso part (rigid, 4 facings) + a sleeve style (the arm regions' materials) + a hang part (what falls
below the waist, with sway and posture keys) + optional overlays.

**Civilian garments** (man 13, woman 14, youth 8, child 7):

| garment | art notes |
|---|---|
| Shirt and trousers (Tunic) | A3's tunic: lit chest plane, a fold under the strap, clean skirt folds to F-9, solid lit hem; the woman's narrows 1 px at the waist |
| Culture tunic | longer, a 2-row skirt over the trousers, belted |
| Dress (woman) | A3's dress: bodice in cloth 1, chemise puff sleeves in linen, the skirt to the ankles, an apron in cloth 3 (optional) |
| Robe | one long garment, wide sleeves (a cuff flare at the wrist), a rope belt or sash; the hood optional |
| Kaftan | long, open down the front over the inner garment, a sash, trimmed edges |
| Wrap | draped over one shoulder, asymmetric, a long skirt; the far arm bare |
| Coat | knee length, open front, buttons in metal 2, over trousers |
| Kilt | pleated to the knee, bare knees, a linen shirt, a plaid over the shoulder |
| Poncho | a wide drape over the shoulders, a fringe at the knee, the arms under it |
| Gown | elven: fitted, long and flowing (the man's: a long high-collared robe) |
| Rags | torn zig-zag hems, patches, bare arms and feet |
| Work apron | smith and shipwright: a leather apron over a work shirt, sleeves rolled |
| Smock | farmer and labourer: loose, belted, gathered at the neck, sleeves rolled |
| Jerkin | hunter: leather over a shirt, a belt with pouches, often a hood |

**Overlays**: apron (A3, cloth 3), tabard (accent: the guard's, the herald's), sash (accent), baldric (leather, worn with a
sheathed sword: A3). **Neckwear**: scarf (A3's hero; accent; its tail hangs and flies), kerchief, collar (linen), torc
(metal 2), amulet (metal 2, glints). **Belt items**: keys (the steward), trophies (the warlord), pouches. **Patterns**:
plain; tiles (stripes, checks, dots) laid over a garment's cloth-1 pixels in the pattern slot on its shading steps;
border trim (the garment's edge region in the pattern slot); motifs (embroidery, vines) stamped at the garment's motif
anchors (chest, hem, cuffs).

**Armour** (P2c; fitted to man and woman):

| weight | form | art notes |
|---|---|---|
| light | Padded | a quilted gambeson in the maker's cloth: horizontal quilt lines |
| light | Leather | a stitched jerkin; studs as an optional region |
| medium | Mail | a hauberk in the mail brick; a mail skirt to the knee |
| medium | Scale | rows of overlapping scales |
| medium | Lamellar | vertical laced plates, the lacing in the accent slot |
| medium | Brigandine | the maker's cloth with rivet dots in metal 2 |
| medium | Leaf | elven: curving rows of leaf plates, gilt edges |
| heavy | Plate | a breastplate with a lit ridge, fauld bands, tassets; vambraces, greaves and sabatons through the limb regions |

The legacy outfits map onto these: Leather > Leather, Chain > Mail, Plate > Plate, Elven > Leaf (jade and gold), Ebony >
Plate in the obsidian band with spikes, Guard > Mail with the tabard. The bands (leather, iron, steel, gilded, jade,
obsidian, emberforged), bronze, the culture alloys and their sheens, and Mythic tempering are ramps, never new shapes;
the gear ladder's tiers (section 27) add finish and grow the silhouette only through the culture's dials, helms, shields,
cloaks and plumes.
Pauldrons (dial: none, small, medium, large) come in three families (plate, leather and fur, scale and lamellar) at the
shoulder anchors. Skirts (dial) use the armour hangs (short tassets, knee mail or scale). Gloves recolour the hand
(leather, metal) and add the gauntlet flare region at steel and up; boots recolour the boot region, run tall leather up
the shin, and greaves and knee cops at iron and steel. Ornaments are optional regions inside the maps: plumes, fur trim,
gilding (the trim region in gold), horsetail, studs, etching, tassels, scallops, rivets, filigree; the M12 rank marks;
legendary and Mythic glints in the glow colour on the breathe frame.

**Equipping armour must visibly change the hero, man or woman**: every medium and heavy form changes the silhouette
(pauldrons, skirt, boots) and at least 25 % of the figure's pixels against the unarmoured body; light forms at least 15 %.

---

## 23. Headwear, helms, cloaks, shields

All adult headwear, helms and cloaks are drawn on the shared adult anchors (head box, shoulders).

- **Culture headwear** (P2b): Hood, Cap, Turban, Fur hat, Veil, Circlet, Conical straw hat, Headscarf; the legacy hood
  (its cloth from the hose, P2a's rule) and the travel cloak's hood. Cloth lit top-left, brims seen from the high 3/4
  camera. Kids: cap, hood, headscarf, circlet.
- **Helms** (P2c), 12 shapes: leather cap, nasal, kettle, great helm, spangenhelm, horned, plumed, conical with aventail,
  masked, crested (the crest dial as optional regions), winged, horned crown. Open helms show the face; the great helm
  and the masked helm hide it to an eye slit. A helm stays on a lying head.
- **Cloaks** (P2b): travel cloak (hooded, to the calves), fur mantle (short, a fur collar), long noble cloak (to the
  ankles, gilt trim), cape (to the waist). Behind the body facing down, over it facing up, a side panel in side views;
  sway keys, a run flare, sitting and riding drapes; a cloak lies under its corpse.
- **Shields** (P2c), 8 shapes: round, heater, kite, tower, crescent, oval, buckler, leaf; field, device and rim regions
  (the hero's heraldry, the maker's paint); on the off arm at rest, raised in a block, flat beside a corpse.

---

## 24. The doll: how a part is drawn

The doll (VISION_PLAN 15.36 "The production method") composes a person from hand-drawn part maps. These are the rules an
artist draws by.

- **A part** is a named set of views (D, U, R, L) and keys, in one `.pxm` text file under rpg/art/doll/maps/: one
  character per pixel, `.` leaves what is below, `-` erases what is below (an arm lifted off the torso, a hood hiding
  hair). Each character resolves through the legend to a slot (or a region) and a step 0-4.
- **Layers** come in a fixed order per facing (the layer table): back items, cloak back, hair back, far arm, legs, shoe
  tips, hang back, torso, hang front, belt and overlays, neck, near arms, head, face features, beard, hair front, ears
  (elf and half-breed), headwear, held item (in front or behind by the clip), shield, cloak front (facing up).
- **Anchors**: head parts are drawn on the head box (its top-left at pivot + (-8, -33) for the man, (-8, -32) for the
  woman); torso, belt, neck and cloak parts on the neck point; hangs on the hip point; legs on the pivot; arms on the
  shoulder points; held items on the grip; pauldrons on the shoulders. The clip moves the anchors per frame (bob, lean,
  breathe, the elder's stoop), so a rigid part is drawn once per facing, never per frame.
- **Keys**: legs and arms are drawn per key (idle, walkA, walkA2, walkB, walkC, walkC2, walkD, run1-6, the action and
  posture keys); hangs per sway key (rest, wide in side views, run flare, sit, floor, kneel, ride; the front and back sways
  are the rest key's bottom rows shifted by a pixel, the count of rows declared by the part); hair hangs per lag key;
  tufts per bob key.
- **Regions** let one drawing serve every garment: an arm map marks upper arm, forearm, cuff, hand and the gauntlet
  flare; a leg map marks thigh, knee, shin, boot and sole. The look decides each region's slot (a long sleeve, a bracer, a
  short or puff sleeve, bare skin, mail, plate; trousers, bare knees, tall boots, greaves).
- **Optional regions** (`when` overlays) carry ornaments, dials, sheaths, glints and build changes inside one part, drawn
  only when the look asks.
- **Build masks**: torsos, hangs and pauldrons mark the columns broad adds and slim removes.
- **The stretch row**: every leg drawing marks one row that doubles for elves (a straight run of both legs). (Side walk and
  run keys mark none, since a doubled row would jag a diagonal limb: their top row is a finished seat row that elves show,
  section 10.)
- **Drafts**: `tools/doll/draft.mjs` writes a mirror-and-relight draft of a right view as a left view; P2a's rasterizer and
  effect painters write first drafts of weapons and arcs. A draft is a starting point; the committed map is hand-placed
  and hand-checked (the compiler flags a left view that equals the plain mirror of its right view).
- **The finishing pass is the doll's**: draw step 0 on every silhouette edge; the doll lights the top-left ones after
  composing. The same pass runs again on a turned lying figure.
- **Craft checks** (the compiler on every map, the doll on every composed frame): every character in the legend, every
  facing and key present, no orphan pixel, no lone-colour noise outside faces and FX, no pure black, step-0 luma 70 or
  less, 48 colours or fewer per figure, the left half of each material's pixels on average at least as light as the right
  half in every facing (top-left light), heights by the grid of section 3.
- **The look-and-adjust loop**: `doll_gallery` renders any part, look or clip at 1x, 2x and 4x on grass and dirt with the
  contact shadow, and at 12x with a pixel grid and row numbers (A3's look.py contact sheet).

---

## 25. P2c: arms and armour on the doll (planner, 2026-10-23; binding on every P2c art area)

Sections 11, 12, 22 and 23 still hold; this section says how they are rendered in A3 on the shipped body and motion
(docs/MOTION_P2.md), and how actions are timed. docs/PLAN.md "## P2c" is the plan; docs/P2C_CONTRACTS.md the shapes.

### 25.1 The feel

A town guard at the gate, on a phone at arm's length: the kingdom's red tabard over grey mail, a glint on the nasal helm,
the kite shield on his arm with its gold device, a brass hilt at his hip. Across the square a plate soldier turns and her
pauldrons catch the light; her crest bobs over the crowd as she runs. In a fight every weapon has its own body. The sword
coils and sweeps through A3's three-tone arc. The spear is drawn back to the hip and darts out in a streak. The archer
brings the string to the cheek, holds it, and lets it snap. Metal reads as metal: broad lit planes, one ridge, a few
glints. Mail reads as mail, a fine brick and never noise. Leather is matte and warm, and a tabard over mail is plainly
cloth. Put on plate and the hero changes shape. Fall in it, and the helm stays on, the shield lies flat and the sword
lands beside the hand. Nothing floats, slides, clips or crosses a face, and every leg is still the short, chunky,
far-leg-whole leg the owner approved.

### 25.2 Materials in A3

- **Steel plate**: A3's steel ramp (`#201C31 #525C7A #95A2BC #D5DEEA #FFFFFF`) laid as **broad planes**, never a gradient.
  The plane facing the top-left light is step 3, the turn is step 2, the shadow side step 1, and the silhouette step 0
  (the finishing pass lights its top-left edge). Each convex plate (breastplate, cop, crown, pauldron) carries **one lit
  ridge**: a step-4 line or run where the plane turns, on the lit side. White glints (step 4 pixels) sit at the top-left
  of convex shapes only, and a figure shows **three at most** in a frame. In the walk they may move a pixel with the bob
  and in the turn they jump to the new lit edge (the delight: plate catching the light as the body turns).
- **Bands and alloys are ramps, never shapes** (a tier's finish and dials are section 27): leather, bronze (warm
  gold-brown, never the gilded yellow), iron (dark, matte, narrow spread), steel (A3's), gilded (warm, steps 3-4 toward
  yellow), jade (green-gold), obsidian (purple-black, one violet glint), emberforged (red-orange; its visor or seams take
  the glow slot from T5, 27.3), starmetal (pale blue-white, the heartland's band 7). Sheens change the spread: matte
  compresses it, bright widens it with a white step 4, dark shifts it down, banded alternates step rows on the plates,
  iridescent hue-shifts the steps, glowing adds a glow-slot rim, pale lifts it, burnished warms steps 3-4. Mythic
  tempering is gear_look's ember red, read through the same ramp.
- **Mail**: the metal ramp in a **brick**: 2x1 cells in horizontal courses, offset a pixel each row, steps 2/3 on the lit
  side and 1/2 on the shadow side. Never more than three brick rows run unbroken: a belt, a fold, a tabard hem or the
  collar breaks them. The mail's edges (hem, sleeve end, coif rim) are a **solid step-1 band** with no brick, so the edge
  reads as an edge. At 1x mail reads as a grey-blue fabric with a fine texture. If it reads as dither, it is wrong.
- **Scale**: rows of 2-3 px units, each with a step-3 top and a step-1 lower edge, courses offset by half a unit.
  **Lamellar**: vertical plates 1x3 px laced by an accent line every third row. **Brigandine**: the maker's cloth with
  metal-2 rivet dots on the lit planes only (a 3 px grid). **Leaf**: curved rows of leaf plates with a gilt (metal 2)
  edge, elven and light. **Padded**: the maker's cloth with step-1 quilt lines every two rows, all straight and never
  alternating; the sides bulge a pixel every two rows, so the coat reads as stuffed.
- **Leather** (`#2C1621 #5B3129 #8D5536 #BF8249 #E7B672`): matte and warm. Boiled plates have a step-3 top edge and a
  step-1 stitch line. Studs are metal-2 pixels on an optional region. There are no white glints on leather, only one
  step-4 shine on a convex lit plane (a shoulder, the lit toe).
- **Cloth over mail** (tabard, surcoat): the tabard is cloth in the accent slot on the chest plane, lit like A3's tunic
  (a lit plane, one fold under the belt, a solid lit hem). Its side edges are a step-0 line against the mail, and its hem
  sits two rows above the mail skirt's hem so both read. The mail shows at the shoulders, on the arms and below the hem.
  The kingdom's device (field and device colours, 3-5 px) sits at the chest centre as an optional region. The tabard's
  base luma differs from the mail's step 2 by 25 or more, so the cloth never merges into the metal. No pattern tiles on
  armour.
- **Trim** (metal 2: brass, gold, silver, bronze): 1 px edges, rivets, buckles and guards; at most 15 % of a piece's pixels
  (the gilding ornament excepted).

### 25.3 Silhouettes and fit

- **Three weights.** *Light* (padded, leather) keeps the civilian silhouette within a pixel, with small pauldrons at most.
  *Medium* (mail, scale, lamellar, brigandine, leaf) adds a skirt toward the knee by the skirt dial and a collar or coif.
  *Heavy* (plate) adds pauldrons, the tasset fan and the helm's mass. The silhouette changes only through **torsos,
  skirts, pauldrons, helms and shields**.
- **Legs never change shape.** Leg armour recolours the leg's regions: cuisses on the thigh, the cop on the knee, the
  greave on the shin, the sabaton on the boot. A hand-drawn tile may add lames inside the silhouette. The leg is always
  the shipped leg (short, full thickness, the far leg whole) and the boot is always the one boot (MOTION_P2 0). Arms are
  the same: sleeves, vambraces, bracers and gauntlets are the arm regions recoloured, with material tiles. The cuff flare
  (region X) is the only arm shape armour adds.
- **The woman's armour** is the same piece cut to her torso: a waist one pixel narrower, and the breastplate shaped by
  **one step-1 shading row** across the chest. No sculpted cups and no exaggeration. Skirts, tassets and tabards follow
  her hem rules (section 14) and her legs are judged in armour as well as in a dress. Torsos, skirts and tabards are
  separate maps per body. Pauldrons and helms are shared, because the shoulders (16 px) and the head box are the same on
  both bodies (docs/PLAN.md P2c decision D10).
- **Builds** move armour with the body's build masks: broad adds a column to the plates and pauldrons, slim removes one.
  Elves wear the leaf form most and their legs carry the stretch row as always. Elders wear what adults wear, with the
  stoop. **No armour on kids.**
- **Equipping must show** (section 22): plate changes 25 % or more of the idle figure's pixels and its outline, leather
  15 % or more, on the man and on the woman.

### 25.4 Helms

- On the adult head box, lit as a sphere: a lit crown and a highlight ring across the crown from behind, as A3's hair has.
  Steel helms carry one white glint at the top-left of the crown and leather caps carry none.
- **Open helms show the face.** The rim sits on the brow row (F-25 man, F-24 woman), so the eyes, the nose pixel and the
  mouth always show. The nasal bar covers the nose pixel only and the cheek guards cover the cheek columns, never an eye.
  **Closed helms** (great helm, masked) hide the face to an eye slit: a 1 px dark line on the eye row whose top-left end
  the finishing pass lights.
- Crests, plumes and horsetails are in the plume slot (plumeColor) and rise to F-37 at most. The crest dial (1-4) is
  optional regions inside one map. A plume bobs a pixel behind the walk's bob (the delight). Horns and wings use the
  metal or horn ramps.
- Each helm masks the hair it hides (`-` cells). Long lengths, braids and tails below the rim still show, so a woman in an
  open helm reads as a woman by her braid and her face. Volume styles switch to their capped views. A helm stays on a
  lying head, and gets a painted lying view only where the turn reads wrong.

### 25.5 Shields

- The shield is on the **off arm** (the body's left) and rides its hand's grip. Facing right it is behind the body (its
  rim and the top of a kite show past the back and over the shoulder). Facing left it is in front, covering the chest and
  belly with its rim at the shoulder line, never over the face. Facing down it shows its face at screen right. Facing up
  it shows its back (straps, the grip, wood in steps 1-2) at screen left.
- Three regions: the **field** (3 steps, lit top-left as a convex face: a lit band at the top-left and shadow at the
  bottom-right), the **device** (2-3 colours, 3-5 px, centred) and the **rim** (metal trim or leather, 1 px). The field
  defaults to the accent (the hero's arms, the kingdom's colour). A maker culture's paint is the field slot.
- Keys: at rest the shield hangs still on the arm (the off arm does not swing when it carries a shield). Guard raises it
  to the chest. Block squares it to the threat. Bash leads the roll. Lie puts it flat beside a corpse. Back slings it on
  the back for seated and kneeling postures.

### 25.6 Pauldrons, skirts, gloves and boots

- **Pauldrons** sit on the shoulder cap at the shoulder anchors. Small, medium and large reach 1, 2 and 3 px past the
  16 px shoulder line on each side. There are three families: *plate* (plate, brigandine, leaf: one lit ridge and a
  glint), *fur and leather* (padded, leather: matte, a fur edge with the fur trim ornament) and *scale* (mail, scale,
  lamellar: two rows of units). On the near side they sit over the arm and on the far side behind it. They ride the lean
  and the bob exactly and lift a pixel on overhead wind-ups.
- **Skirts and tassets** (the skirt dial: none, short, mid, to the knee) ride the hang channel like garment hangs. A mail
  skirt opens over the stride. Plate tassets are rigid plates that hinge: they shift whole by a pixel and never bend.
  The watch's skirt stops above the knee so the legs read.
- **Gloves and boots** by band: leather gloves recolour the hand, metal gauntlets add the cuff flare from steel up. Boots
  recolour the boot, tall leather runs up the shin, and greaves and knee cops come from steel up. All of this is recolour
  inside the shipped arm and leg (25.3).

### 25.7 Weapons at 32 px (the library)

- Sizes are section 11's table. Weapons are **authored maps at 16 angles** (22.5 degree steps) on the grip, drafted from
  P2a's rasterizer and cleaned by hand. Each angle is a drawing: lines step regularly (1:1 at 45 degrees, 2:1 at 22.5),
  there are no doubled pixels at corners and no L-shaped jaggies, and the lit edge is toward the top-left at every angle.
- **Blades** are 2 px (A3): a lit edge row in steps 3-4 and a back row in steps 1-2, a white tip pixel, the step-0 outline
  only at the tip and where the blade crosses the body, a 3 px brass guard and a 1-2 px pommel. **Hafts** are 2 px of
  wood (steps 2 and 3), outlined only where they cross a part of similar value. **Heads read by shape**: the axe's bearded
  head (5x6) has a steel edge line and a dark cheek; the mace's flanged head (4x5) has two glints; the spear's leaf head
  (5 px) has a white tip; the staff's 3x3 head has a glow pixel. **Bows** are a 1 px stave (wood or horn) curved by
  stepped runs of 1-3 px with a 1 px string in linen step 3; facing down and up, where a bow is seen edge-on, it is drawn
  at three-quarters, 2 px at the grip, its curve spanning 3 columns or more (the owner, 2026-10-23: "too thin to read").
  The crossbow is 12x10 with the prod across.
- **The silhouette test**: fill a held weapon black at 1x on the phone and its class must still be clear (cross guard,
  bearded head, flanged head, leaf point, long two-hand grip, curve and string).
- **The hand holds the hilt** (section 5): the fist is drawn over the grip, with the guard showing on one side and the
  pommel on the other. Two-handed weapons have both fists on the haft (greatsword 3 px apart, spear and staff 6-8 px
  apart) in every frame. The weapon is in the right hand in every facing and the bow in the left.
- **Depth**: the weapon is behind the body when its hand is the far one (facing left) and on wind-ups over the back.
- **Forms** (8 blades, 3 polearms, 5 bows) are their own maps on the base hilts and hafts. Tier and alloy are the metal
  slot. Legendary and Mythic pieces glint in the glow colour on the breathe frame, also while sheathed.
- **Carried at rest**: swords, daggers, axes and maces on the belt (the off hand on the hilt: A3). The greatsword is
  across the back. Spears and staffs are upright in the right hand, the hand at mid-shaft and the foot a pixel off the
  ground. In the run they tilt one step forward. Bows go on the back with the quiver, and arrows show in the quiver (the
  delight). The crossbow is slung by its stock.

### 25.8 Actions: timing, anticipation, impact

The sim's clocks are fixed (no balance change). The doll's frames are timed against them:

| who | windup | strike | recover | frames per hit (windup / strike / follow) |
|---|---|---|---|---|
| the hero | 70 ms x pace (unarmed 50) | 110 ms x pace | 130 ms x pace (finisher 280, unarmed 80) | 1-2 / 1-2 / 2 (A3's sword 1 / 1 / 2) |
| foes | 220-380 ms by family (dagger 0.22 ... greatsword 0.38; the bandits' mean held at 0.36) | 280 ms (heavy 400) | 350 ms (heavy 750) | the same frames, held longer |
| guards and militia | 300 ms | 250 ms | 300 ms | the same |
| the bow | the draw over the windup: nock, chest, cheek | release 80 ms | the drawn idle | 3 / 1 / - |
| block | 150 ms flash on a parry (no sim state): raise 30, impact 50, brace 70 ms | | | 3 |
| roll | 4 x 85 ms | | | 4 |
| cast | gather while the spell is held | release | | 2 |

Pace: sword 1.0, dagger 0.8, axe 1.08, spear 1.1, mace 1.25, greatsword 1.18, fists 0.9. The pick splits the windup at
45 %, the strike at 40 % and the follow at 50 %. The hero's **loaded pose** (the windup's last frame) is on screen for
about 40 ms, so it must read in one glance. A foe's loaded pose is held 120-210 ms and is the telegraph.

1. **Anticipation**: every blow starts with a coil against its direction. Weight goes on the back foot and the weapon
   goes behind (A3's sword: the hand behind the back shoulder, the blade cocked clear of the jaw). The loaded frame is
   the family's strongest silhouette. The spear's wind-up takes two frames (both fists drawn back to the hip, the body
   leaning back 2 px), never a static poke (the owner, 2026-10-23).
2. **Impact**: the strike's first frame. The weapon is fully visible at the end of its travel or its fastest point (never
   edge-on and never hidden in side views). The body commits with lunge legs and a 1-2 px lean, the face is fierce and
   the effect is at full width.
3. **Follow-through**: the weapon carries 22.5-45 degrees past the target. Hangs, tassets, the tabard, plumes and braids
   lag a frame, and the off arm counter-swings (or tucks the shield). The **recover** lands within 2 px of the guard
   stance (hands and weapon angle), so the next blow reads as one motion.
4. **Variants**: hit 2 mirrors hit 1 or changes its plane (a backhand, a rising cut). Hit 3, the finisher, is bigger: a
   stance 2 px wider, a deeper coil, the effect in the finisher tint, and a held impact frame. On heavy families the
   delight is the impact flash and a one-frame camera nudge.
5. **Families** (section 12's table, on this body):

| family | anticipation | impact | follow |
|---|---|---|---|
| unarmed | the fist at the ear | the jab at full reach | back to the guard |
| dagger | a short cock at the hip (the fastest family: small) | the stab at full reach, the off hand forward | snap back |
| sword | A3's coil | A3's sweep and arc | A3's cut and guard |
| greatsword | the blade over the back shoulder, the stance wide | the cleave at the ground line | the weight drags it on |
| axe | up behind the head | the chop at knee height | the head bites down |
| mace | overhead, both elbows up | the smash on the ground line, dust | the bounce |
| spear (glaive, halberd) | both fists drawn to the back hip, weight back | the thrust at full reach, the front foot planted (the polearms sweep) | the shaft pulled back to the guard |
| staff | drawn back across the body | the diagonal strike | it swings through |
| bow | the draw is the anticipation | the release | the bow lowers |

6. **The bow**: *nock* (the arrow to the string at the waist), *draw* (to the chest), *full draw* (to the cheek, the
   anchor: the bow arm straight, the elbow up, held for the rest of the windup), *release* (the string snaps forward, the
   bow hand recoils a pixel, the arrow is gone because the projectile is the game's), then back to the drawn idle. The
   crossbow aims with the stock at the shoulder, shoots as the prod snaps, and cranks as its follow.
7. **Block**: three beats inside the 150 ms flash (the owner, 2026-10-23: "one held frame"): raise, impact (a 1 px
   recoil and a spark, `fx spark`), brace. With a shield, the shield is square to the threat with the eyes over its rim
   and the knees bent, and the sword arm is cocked high and back, so the side view never reads as a sword held low.
   Without one (the sword's parry), the blade or haft is across the body at 45 degrees; the spear's block raises both
   arms with the haft high.
8. **Roll**: crouch, tuck (the crouch turned a quarter toward the roll), over (the other quarter), rise. The shield
   leads the tuck as the bash.
9. **Cast**: *gather* (the hands meet at the chest with the glow between them, weight back), then *release* (the arms or
   the staff thrust toward the aim, weight forward).
10. **Hurt** is the flinch (section 10). A helm snaps back with the head and the shield arm drops a pixel.
11. **Guard stance** (the drawn idle) is ready, never hanging. One-handed weapons point forward-up at 45 degrees beside
    the body with the shield at the chest. The spear is levelled at the hip, tip forward. The greatsword is angled up in
    front in both hands, the staff held across, the bow low in the left hand with an arrow nocked, the fists up. The
    breathe frame lifts weapon and shield with the upper body.
12. **Death with gear** (section 13): recoil, buckle, topple. The weapon lands flat on the ground beside the near hand
    (never level over the head, standing up or arcing in the air: the owner, 2026-10-23), the shield falls flat beside
    the body, the helm stays on, back items lie under the body, and the lying body reads as head, torso and legs (a
    painted lie view wherever the turn makes a lump).

### 25.9 Gear in motion (the shipped method holds)

Every armoured or armed look passes B11 limbs, B11 wear and B12 boots (MOTION_P2 13). Skirts, tassets and tabards ride
the hang channel (`wide` in the walk, `flare` in the run), open over the stride and trail. The run's 4 px lean carries
the torso's `runl` view, and pauldrons, the tabard, the helm and the plume ride it exactly. Shields hug the body in the
run, inside the frame box plus one column, and never over a boot. In the run the scabbard swings back (`kick`), the
spear tilts one step forward and the bow stays on the back. The drawn run keeps the guard. Nothing crosses a face. No
gear widens a near-budget look's side run frames (B10).

### 25.10 Effects

The nine family effects are re-cut to A3's three-tone construction (edge, body, core), using section 6's named colours.
They are drawn from maps in side and down orientations (mirrored and turned for the others, since FX carry no light),
with the finisher by tint (`#FFE6A0`) and P2a's sizes. A3's arc itself is re-cut smaller (the owner, 2026-10-23: the
swoosh was oversized and dropped below the feet facing down): every effect stays inside the figure's box plus the
weapon's reach, and above the sole row. An effect peaks on the impact frame and ends with the strike state
within a frame. The Thrust Streak runs from the spear tip back along the shaft's line. The Twang is the string's
vibration lines for one frame. The delight is sparks (`#FFD87A`) when a mace meets metal.

### 25.11 Colours with gear

48 colours or fewer per composed figure (B8). Gear shares slots. The tabard is the accent. The shield's field defaults to
the accent, and a separate field ramp appears only for a maker's paint. The device takes 3 steps and the plume 3-4. Every
metal piece shares metal and metal 2 (the body armour's band tints them all, gear_look's rule), and every leather piece
shares leather. The weapon in hand keeps its own metal (the blade slot: the same ramp when the metals match, so it adds
no colour then). T5-T6 looks may add up to 3 glow-slot steps beyond the 48, and touching materials part by value (27.5).

### 25.12 P2c don'ts

- No armour that widens a leg or reshapes a boot; no per-key sleeve map where a region tile does the job.
- No mail that reads as dither; no more than three glints in a frame; no glints on leather; no pillow-shaded plate.
- No new torso, helm or skirt shape for a band, an alloy or a tier (materials are ramps; tiers are finish and the
  culture's dials: 27); no torso, skirt or tabard shared between the bodies.
- No tool-rotated weapon map: every angle is hand-cleaned. No weapon changing hands between facings.
- No weapon, shield, plume or effect over a face in any idle, walk or run frame; no helm floating a row above the head.
- No hidden or edge-on weapon on an impact frame in side views; no recover that pops back to the guard.
- No pattern tiles on armour; no armour on kids.

---

## 26. The P2c style slice (wave 1; the owner signs it off)

The smallest finished piece that locks how steel, leather, a shield, a spear and a bow look and move on the shipped doll.
No production map is drawn for P2c until the owner signs it off (docs/PLAN.md "## P2c", "The style slice").

**Looks** (looks.txt lines in docs/PLAN.md):
1. **The town guard** (man): the man's P2b mail with a tabard in the kingdom's red, the nasal helm, a kite shield with a
   gold device, A3's sword sheathed at the left hip, leather gloves and boots, a full beard under the helm.
2. **The plate soldier** (woman): steel plate with medium plate pauldrons, short tassets, gauntlets and greaves, an open
   crested helm with a red crest (her braid shows below it), and a spear.
3. **The archer** (man): studded leather, a leather cap over tousled hair, a self bow, and a quiver with arrows and the bow
   on his back out of combat.
4. **The fit twins**, unarmed, so the owner sees each new form on both bodies: plate on the man, leather on the woman.
5. **The A3 hero**, as the reference: his sword re-cut onto the library, pixel-identical (B1).

**Clips, in all four facings** (the in-game read): sheathed idle, walk and hurt; the run; the drawn idle, walk and run;
hit 1 (the guard: A3's swing with the shield on his arm; the soldier: the thrust); block (guard and soldier); the bow's
draw and release (archer); the fall and lying on both sides, with gear. The twins: loco, run and the fall.

**The owner pack** (regenerated from the game by the owner-pack tool):
1. The A3 re-cut parity sheet.
2. The slice lineup at NEAR (D4): five looks in four facings, at 11:00 and 22:00.
3. The fit pairs: plate, leather and mail on both bodies.
4. Side walk and run: 8x strips with the hip-to-boot trace and 3x game-speed GIFs in both facings.
5. The guard's drawn guard, hit 1 and block at game speed and at 1/4 speed.
6. The soldier's carry, drawn stance, thrust and block.
7. The archer's draw and release.
8. The falls and the lying bodies with gear.
9. Each slice look at rig 1 (v1) beside rig 2.
10. In-world shots (seed 7, OSLARS: desktop 1440x810 at D3 and D4, phone 2556x1179 at D5, 11:00 and 22:00) with the
    guard at the gate, the soldier running across the square and the archer drawing on a bandit.
11. The budgets and gates table.

**Acceptance** (measured first, then the eye):
- B1 parity 16 / 16 at 0 px with the re-cut sword.
- B11 limbs, B11 wear and B12 boots at 0 on every slice look.
- C3 gear: no holes, the helm on the head, the shield on the arm, the hand on every grip, the lying gear.
- C4: plate 25 % or more and leather 15 % or more of the idle figure changed, both bodies.
- C5: the spear's strike 25 % or more apart from the sword's in every facing, its tip forward 8 px or more, the left
  frames painted.
- C6 on the sword, spear and bow maps.
- B8 craft at 0, with 48 colours or fewer.
- B10 within LOCO 128, RUN 64, DRAWN 256, ACTION 512 and DEATH 48 KB.
- Whole D x D blocks; the day edge 40 or more on the four grounds; heights by section 3 (crest to F-37).
- Then **the judge's look review**: steel as planes with one ridge, mail as mail, leather matte, the tabard clearly
  cloth, the woman's plate shaped and not sculpted, the crest, device and cap readable at 1x on the phone, the spear and
  bow readable by silhouette, anticipation and impact at game speed, nothing floating, sliding, clipping or crossing a
  face.
- Then **the owner's sign-off**. It also settles the open looks: the library re-cut, shields on the arm at rest, the
  spear carried upright, the tabard over mail, mail sleeves by tile or drawn, and the DRAWN atlas split.

After the sign-off this section records the slice's measured numbers, and every later P2c part matches the slice.

**The owner's verdict (2026-10-23): the direction is approved**, with these items open (all fixed in the ladder slice,
27.9): the spear crosses the knight's helm and face in the side walk and run; the spear vanishes for a frame on a hit and
is then held stiff; the thrust is a static poke (no wind-up, lean or weight); blocks are one held frame, and the guard's
side block reads as a sword held low; deaths are illegible lumps with gear floating over them (the spear level over the
head, the bow arcing in the air, the sword standing up); the archer is brown on brown and lost on dark ground; the
knight's plate is pale and flat; the guard's beard reads as a mask facing R / L; the bow is too thin facing D / U; the
sword's swoosh is oversized and drops below the feet facing D; knightm and archerw have no attack frames. The owner then
asked for the gear ladder (section 27). The slice's measured numbers are in docs/PLAN.md "Slice results" (iteration 3).

---

## 27. The gear ladder: rags to riches (planner, 2026-10-23; binding on every P2c art area)

The owner, after approving the P2c slice's direction (2026-10-23): "my personal style is a rag to riches evolution I want
beginners gear to look okay/mid and then slowly progresses throughout the game where you get cooler and cooler gear I'm
thinking too late stage gear could be animated in some way", built on the culture armour generator and the game's rules.
This section is how gear looks at each step and how it moves at the top. docs/PLAN.md "## P2c" ("The gear ladder") maps
the steps onto items, ranks and hours; docs/P2C_CONTRACTS.md 3b and 6b hold the data and the runtime. Sections 22-25
still hold: a piece's **material** is its ramp (25.2), its **maker culture** its shapes (25.3-25.7), and its **tier** its
finish, how far the culture's silhouette dials grow, and, at the top, how it moves.

### 27.1 The feel

The hero leaves home in a mended gambeson, a borrowed iron sword and a plain wooden shield: dull, patched at the
shoulder, a frayed hem, but whole and honest; you could walk into any town in it. A few hours later the patches are gone
and a buckle shines. By the first city the hero wears a soldier's steel with a thin brass edge, a nasal helm and a
painted device. Far from home the metal takes on a people's strange colour, the plates are etched, the shoulders broaden
and a cape falls behind. Near the end the trim is gold and filigreed, a gem breathes at the chest, the cloak stirs when
the hero stands still, and a legendary blade carries runes that glow with the hero's breath while a spark runs up its
edge now and then. Every step reads at arm's length on a phone, the way a 16-bit party shows its strength at a glance.
Nobody ever looks broken: the cheapest kit is plain, never shabby.

### 27.2 The six tiers

| | T1 Scrap | T2 Common | T3 Sturdy | T4 Fine | T5 Masterwork | T6 Legendary |
|---|---|---|---|---|---|---|
| reads as | worn, mended, honest | clean, serviceable | a soldier's kit | an officer's kit | a champion's kit | a legend: it lives |
| metal | its material's ramp compressed (steps 1-3), matte | the full ramp | bright, one lit ridge a plate | the people's alloy: its hue and sheen | the masterwork alloy, broad bright planes | as T5; Mythic ember tempered |
| leather and cloth | steps 1-3, one shine at most | the full ramp, one shine | stitched edges | dyed panels | tooled gilt | as T5 |
| glints (white step 4, convex metal only) | 0 | 1 | 2 | 3 | 3 | 3, one may travel |
| wear (W) | a patch or tarnish a piece, a frayed hem | none | none | none | none | none |
| trim (R) | none: edges in the material | one fitting (a buckle, a rivet pair) | brass edges, 3-8 % of the piece | edges and rivet rows, 6-12 % | gold edges (gilding), 10-20 % | as T5 |
| panel (P) | plain | plain | plain | etched | filigree | filigree and a glyph |
| gem (G) | a stud in the material | a stud | a metal-2 boss | a metal-2 boss | a gem (glow slot), a faint breath | a gem, breathing |
| seams (N) | drawn | drawn | drawn | drawn | drawn | runes (glow slot), breathing |
| dials | no pauldrons, skirt short; no crest, plume or cape | pauldrons small, skirt mid, crest small | the people's own | the people's + its alloy's step | as T4, + a plume on a helm made for one | as T5 |
| helm | none, a hood or a soft cap | the soft form, or a plain metal one | the people's metal form | + crest or plume | + a gilt rim, a brow gem | + a glowing eye slit or brow rune |
| shield | plain wood: an unpainted field, a leather rim | the field painted (no device; livery keeps the kingdom's) | field, device (2 colours), metal rim | a 3-colour device, a metal-2 rim, a boss | a gilt rim, a gem boss | the device or boss glowing |
| weapon | its material, matte; an iron guard, a leather grip | clean; an iron guard | steel, a brass guard | an etched fuller, a shaped guard | a gold guard, a gem pommel | rune inlay, the edge spark |
| cloak | none | none | the people's cape | the people's cape or a travel cloak | the long noble cloak | the cloak stirs |
| moves | | | | | the gem breathes | runes breathe, the spark, motes; Mythic: the flame |

Each worn piece keeps its own tier: a Legendary sword glows on a common gambeson. From T1 to T3 the steps are mostly
material and silhouette (soft to steel, bare shoulders to the people's own); from T3 to T6 mostly finish and light, the
alloy's colour and the cape. The ladder spends hours on each step (PLAN), so every step up is felt. A blade keeps its
white tip pixel at every tier (C6): T1's dullness is in its flats and guard, never in its point.

### 27.3 The finish language

Finish lives in five regions every armour, helm, shield and weapon map marks once in its base views (contracts 3b):
**R trim** (edges, hems, rims, the guard and pommel), **P panel** (the decorated plane: the breastplate's centre, a
pauldron's face, a helm's brow band, the field round the device, a blade's flat), **G gem** (1-4 px sockets: chest,
brow, pommel, boss, clasp), **N seam** (the lines between plates, lames and scales, the fuller, a shield's boards) and
**W wear** (one or two 2x2-3x3 spots a piece). The doll recolours them and lays the finish tiles over them by the piece's
own tier, so one drawing serves all six tiers. One hand (the FINISH area) draws the tiles, so the language matches on
every piece.

- **Wear (T1): mended, never broken.** A patch is a 2x2 to 3x3 block a cloth or leather step off the piece, edged on its
  lit side by a 1 px stitch line (linen step 2-3): it reads as a repair. Tarnish is a 2-3 px cluster of a warm rust-brown
  step 1 on the metal's shadow side, never on the lit plane. A frayed hem loses one hem pixel every 4-6 px, but the
  outline stays closed and the step-0 ring whole. No holes through the body, no rips, no smears; wear covers 6 % of a
  piece at most. At 1x the T1 hero reads as plain and used; at 3x the patches are plainly stitched.
- **Trim (T2-T6)**: 1 px, on edges only, lit on the top-left of each edge: brass at T3, gold at T5. Inside a plane only
  rivet rows (a 3 px grid on lit planes).
- **Etching (T4)**: on P, 1 px step-1 lines on step-3 planes in a motif (a scroll, a chevron, a knot) that repeats no
  tighter than 5 px; never a checker, never dither. At 1x it reads as finer metal, at 3x as engraving.
- **Filigree (T5-T6)**: on P, metal-2 scrolls 1 px wide with a step-4 point at each scroll's lit end, in the maker
  people's motif (PLAN's table: the Heartland's oak leaves, knotwork, laurel, cloud scrolls, sun rays, leaf veins,
  stars...).
- **Gems (T5-T6)**: G in the glow slot, 2 steps (rim 2, core 4) with a step-4 point at the top-left: 1x1 on a pommel,
  2x2 on a brow or a clasp, 3x2 on a chest or a boss.
- **Runes (T6)**: N in the glow slot, broken by the rune tile into 2-3 px dashes and ticks so the light reads as writing,
  not as a neon outline; step 2 at rest, lit by the living finish (27.7).
- **The glow colour** is the piece's own: a people's alloy's light key (Jade's jadesilver green, Starspire's iridescent),
  warm gold on gilded metal, cold blue-white on steel and starmetal, violet on obsidian, ember red-orange on emberforged
  and Mythic pieces.

### 27.4 Silhouettes grow inside the people's own style

The culture generator (ArmsStyle: forms, dials, ornament, alloys) decides what a people's gear is; the ladder decides how
much of it shows. A tier never borrows another people's form: Highlanders never sprout plate pauldrons, their finery is a
longer sash, a plume and a gold-studded targe.
- **Forms by material** (the generator's rule, unchanged): soft pieces (leather, cloth) take the people's everyday helm and
  body (helm[1], body[1]), metal pieces their signature forms (helm[0], body[0]), alloys a finer cut (mail to scale,
  padded or leather to lamellar, brigandine to plate); an item's own form wins. So T1-T2 are mostly soft, T3 the
  signature metal, and T4 and up the finer cut.
- **Dials by tier**: T1 hides the pauldrons, crest, plume and cape and caps the skirt at short; T2 caps pauldrons at
  small, the skirt at mid and the crest at small; T3 shows the people's dials; T4 adds the alloy's step (+1 pauldron and
  crest, the generator's rule); T5 adds a plume on a helm drawn for one, and the noble cloak where it is worn. Ranks lift
  people's dials as the census does (the capital +1, royals and kings +2); the ladder never lifts twice. So the
  Heartland's shoulders go none, small, medium, then large from its first alloy.
  (alloys i3) A T5-T6 body with shoulder plates always wears the large ones (the resolver's one lift), whatever its metal.
- **Alloys by name** (alloys i3): an alloy draws the metal its NAME says (BRONZE warm orange-brown, BRASS yellow, IRON dark
  and cool, STEEL mid and cool, SILVER pale, GLASS a jewel), in its people's own family of that metal (96 families: 8
  slots x 12 peoples). Metal, never cloth: a near-black core, a saturated body (step 2 at 1.2x chroma), a hard pale lit
  plane 28 L above it at half the chroma, a near-white glint; no lit step pink, brown or skin. At T5-T6 a coloured or dark
  alloy's trim is its own metal polished (one ramp step up); a pale neutral steel or silver keeps the gold trim.
  The Heartland's T5 is gilded (its band-4 steel), its masterwork alloy drawn as the steel it is named.
- **Heartland pieces** (no maker culture) climb the heartland's own line: leather at T1, mail at T2, plate from T3;
  pauldrons none, small, medium, medium, large, large; skirts short, short, mid, mid, knee, knee.
- **Flavour at every tier**: the same tier reads differently in each people because forms, dials, ornament, palette and
  heraldry are theirs, while the finish (27.3) is shared, so two peoples' T3 kits read as the same grade. At T3: a
  Heartland man-at-arms in steel mail with a nasal helm and a kite bearing his device; a Jade lancer in a masked helm,
  lamellar with a mid-length skirt, scalloped and tasselled hems, broad lamellar shoulders and a glaive, no shield; an
  Imperial swordsman in a crested helm with a tall crest in his people's plume colour, scale with tassets, a tower shield
  and a long cloak; a Fjordfolk warrior in a spangenhelm, mail with fur trim, small scale shoulders, a round shield, a
  broadsword and a long cloak.

### 27.5 Colour and value

- T1 uses the fewest colours (a piece's material in 3 steps, a patch in 2): about 34-38 a figure. T6 uses up to 48 plus
  3 glow steps (B8 counts the glow slot apart, 3 at most).
- **Value first** (C14): touching materials part by 18 luma or more at step 2 (cloth over leather, leather over trousers,
  hair against a helm, a beard under a cheek guard), so no kit reads as one brown or grey lump; on the darkest day ground
  the figure's lit top-left edge stands 40 or more over it. The style slice's archer, brown on brown, is the
  counter-example.
- Metals stay themselves (25.2). A heartland IRON piece looks iron, STEEL steel, GILDED gilded, and so on to STARMETAL:
  the band the player reads on the item.
- Plate is never pale and flat: the lit plane (step 3) and the shadow side (step 1) stand 60 luma or more apart, with one
  ridge (the style slice's knight).

### 27.6 People on the ladder

Rank is a tier: bandits, militia and pirates T1 (the local poor kit); bandit chiefs, the village watch and soldiers T2;
the capital watch, heralds, envoys and warlords T3; captains, the royal guard, jarls and story knights T4; kings T5.
Guards and soldiers wear their kingdom's livery and device from T2. A late hero (T5-T6) outshines any king of any people.
Far, dangerous lands field finer soldiers (depth: a tier up from band 4).

### 27.7 The living finish (T5-T6)

Late gear moves quietly, as light over the drawn frame (no extra frames, no new atlas per phase), on the body's own clock:
- **Breath**: gems and runes brighten and dim together on a 2.2 s cycle (two of the idle's breaths), peaking on the
  breathe frame while idle and free-running in motion: additive light from 0.30 to 0.85 at T6, 0.15 to 0.45 for a T5
  gem. Every wearer has its own phase. By day a shimmer, at night a soft glow.
- **The edge spark** (a T6 weapon): a small four-point star runs from the grip to the tip in 0.33 s once every 3.3 s, and
  flashes once at the tip on the strike's impact frame. It is one of the three glints.
- **Motes** (T6): 1 px sparks in the glow colour, three alive at most, born on the glow pixels every 0.45 s, rising 8 px
  over 1.0 s as they fade.
- **The cloak and plume stir** (T5-T6): on the idle's breathe frame the cloak's hem swings out a pixel and the plume
  lifts; in the run the cloak's flare is a pixel wider.
- **Mythic**: the ember-tempered metal, the white-hot blade and the flame that hugs the silhouette (M8b's aura on doll
  frames: light only within 3 px of the outline).
- **Readable, never noisy**: 32 animated pixels a figure at most (ladder i2: was 24); no pixel's light jumps by more than a quarter between
  two 60 Hz frames; nothing blinks faster than twice a second (the spark's run aside); two wearers never pulse in step.
  If it reads as noise at 1x, it is wrong.

### 27.8 Ladder don'ts

- No broken, torn or ugly T1 gear: worn means mended and whole; no holes in a silhouette.
- No new torso, helm or skirt shape for a tier: tiers are finish and dials, materials are ramps.
- No people borrowing another people's forms at any tier.
- No trim inside a plane (but rivet rows); no etching or filigree that reads as dither; no glow below T5 (but white
  glints).
- No animation that blinks, strobes or moves the silhouette; no light wider than the figure plus 3 px (Mythic's flame
  aside).
- No kit whose touching materials share a value.

### 27.9 The ladder slice (wave 1b; the owner signs it off)

The smallest finished piece that locks the ladder before any production map (docs/PLAN.md "The ladder slice"):
1. **The hero's road**: the A3 hero in the Heartland kit at T1 to T6 (27.2 as one man's story), sword and shield; T6
   alive; the Mythic flame as depth.
2. **The heroine at the top**: the same T6 on the woman, in the open crested helm, with a runed spear.
3. **Flavour at T3**: the Jade lancer, the Imperial swordsman and the Fjordfolk warrior of 27.4, beside the hero's T3.
4. **Rank to tier**: a bandit (T1), the village watch (T2: the style slice's guard), the royal guard (T4), and a king (T5)
   as depth.
5. **The style slice's looks**, every open item of section 26 fixed.

**Clips, in all four facings**: idle, walk, run, hurt, the drawn idle, walk and run, hit 1, the fall and lying; T6 with
the living finish.

**The owner pack** (every image 8000 px or less on its long side):
1. The ladder strip: T1 to T6 side by side, idle facing down and right, at 1x and 3x, on grass and on dark ground, by
   day and by night.
2. Each tier in four facings at 3x.
3. Walk and run strips (8x, the hip-to-boot trace) and 3x game-speed GIFs, both side facings, per tier.
4. Hit 1 per tier at game speed and at quarter speed.
5. The living finish: the T6 man and woman idle, walking, running and striking as 3x GIFs at game speed (4.4 s), a
   one-cycle frame strip, and night shots; the Mythic flame if built.
6. Flavour: the Heartland, Jade, Imperial and Fjordfolk T3 kits in four facings, side by side.
7. Rank to tier: the bandit, the watch, the royal guard (and the king).
8. The open items, before and after.
9. In the world (seed 7, the start village; desktop 1440x810 at D3 and D4, phone 2556x1179 at D5; 11:00 and 22:00): the
   T1 hero at the gate beside the watch; the T6 hero in the square at night; the flavour row in the market.
10. The gates and budgets table, with the ladder's numbers per tier.

**Acceptance**: measured first (PLAN's ladder slice gate), then the judge's look review: each tier a clear step at 1x;
T1 okay / mid (plain, mended, whole, never ugly); the finish reads as craft, never noise; the living finish quiet and
readable; the peoples distinct at the same tier; every open item closed. Then the owner's sign-off, which also settles
PLAN D13-D23. After it, this section records the measured numbers and every later part matches the ladder slice.

---

## Appendix: the P2a slice record (history; its look is superseded)

The P2a procedural slice passed its judge on 2026-10-18 (VISION_PLAN 15.36 P2a gates, R1). Its measured numbers still
serve as baselines: idle heights man 32, woman 31; every `ui 0` world shot whole D x D blocks; night figure medians over
the ground +18 to +37 and day edges 72.6-97.1; the ring 45.8-78.4; LOCO atlases 79-106 KB, DEATH 40-46 KB, the sword's
ACTION 206-212 KB; one facing's LOCO bake p50 0.09 ms; the walk measured by .tmp/judge/motion.py and motion_reg.py on the
frame-exact walk60 run (registration 1 px, slip 4 px). The rejected parts were the body's proportions, shading and faces.
