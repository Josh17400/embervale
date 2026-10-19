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
| walk: 6 frames over a 28 px cycle | the cycle comes from the drawings' planted feet: man 19 px, woman 20 px; a RUN (about 36 px) above 54 px/s | 10 |
| weapons rasterized procedurally at 16 angles | weapons are authored maps at 16 angles (drafted from P2a's rasterizer, then cleaned by hand) | 11 |
| fighters hold the weapon in every clip | out of combat the blade rides sheathed (A3: the sword at the hip, the hand on the hilt); in hand while fighting (the owner confirms at the slice) | 11 |
| fall: recoil, buckle, a painted topple | recoil, buckle, then the lying figure itself lifted for one frame; lying is the composed side view turned a quarter | 13 |
| the P2a style slice (procedural; passed 2026-10-18) | the A3 doll slice, P2b wave 1, signed off by the owner; P2a's record kept in the appendix | 19 |

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
0.31-0.37, tables 0.44-0.50, horse withers 0.80-0.90, gunwale 0.35-0.45, rails 0.45-0.55, gate clearance 1.25 or more,
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

**The walk fits A3's stride.** Measured on A3's side walks (docs/ref/style_a3, the soles' Ink spans): the man's planted
foot travels 9 px from heel strike to toe-off (centre 18 > 15 > 9 on the near leg; 18.5 > 14 > 9 on the far leg), the
woman's 10 px (16.5 > 13.5 > 6.5). A walk cycle is two steps, so **the man's cycle is 19 px and the woman's 20 px**.
(A2's notes asked for a longer cycle for A2's longer legs; A3 keeps A's shorter legs. On the 28 px cycle A3's feet skate
forward about 4.5 px a step, and A3's contact and down frames share one leg drawing, so the planted foot cannot move
between them.) The fix, painted in the slice: each side view gets 6 leg drawings. A3's contact drawings stay pixel for
pixel; the pass drawings are re-registered with the planted foot under the hips (1-3 px further back than A3's); and two
new down drawings bend the knees with the planted foot 3 px behind contact. The planted foot then steps 3 / 3 / 3.5 px
(man) and 3 / 3.5 / 3.5 px (woman). The frame table is not a constant: it comes from the clip's plant anchors
(section 24). At 40 px/s the man's walk runs at 12.6 fps (2.1 cycles a second); a 20 px/s stroll at 6.3 fps. Front and
back views walk in place (the 16-bit convention, A3's drawings).

**The run (new; the owner confirms at the slice).** A3's walk at the hero's 74 px/s would be 3.9 cycles a second, a
scurry. Above 54 px/s people run: 6 frames, the torso leaning 1 px forward in side views, a 2 px bob (low on push, high
in flight), arms bent and pumping, every hanging part lagging 2 px. The planted foot travels about 10 px in the two stance
frames and the flight adds about 8 px a half-cycle, so the cycle is **about 36 px** (the drawings' anchors decide): at
74 px/s, 2.06 cycles a second and 12.3 fps, close to the cadence of A3's approved walk GIF (2.6 cycles a second). Kids
run with the Play posture's frames; elders never run.

**Registration** (P2a, kept): at every frame change of a side walk or a run's stance frames, the planted foot lands within
1 px of where it stood; within a held frame it slips one frame step or less; a walk starts on the pass frame; the body
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
   (idles, front and back walks, side contact frames, swings) is pixel-identical to docs/ref/style_a3/frames.
2. **The side walks re-registered** (section 10): the pass drawings moved, the down drawings new.
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

**Acceptance** (measured; the full protocol is VISION_PLAN 15.36 P2b "The slice gate"): A3 parity 0 px on the kept frames;
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
obsidian, emberforged), bronze, the culture alloys and their sheens, and Mythic tempering are ramps, never new shapes.
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
- **The stretch row**: every leg drawing marks one row that doubles for elves (a straight run of both legs).
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

## Appendix: the P2a slice record (history; its look is superseded)

The P2a procedural slice passed its judge on 2026-10-18 (VISION_PLAN 15.36 P2a gates, R1). Its measured numbers still
serve as baselines: idle heights man 32, woman 31; every `ui 0` world shot whole D x D blocks; night figure medians over
the ground +18 to +37 and day edges 72.6-97.1; the ring 45.8-78.4; LOCO atlases 79-106 KB, DEATH 40-46 KB, the sword's
ACTION 206-212 KB; one facing's LOCO bake p50 0.09 ms; the walk measured by .tmp/judge/motion.py and motion_reg.py on the
frame-exact walk60 run (registration 1 px, slip 4 px). The rejected parts were the body's proportions, shading and faces.
