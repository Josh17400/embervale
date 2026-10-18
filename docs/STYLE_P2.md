# STYLE_P2: the Clear Sight rig (art direction for P2a, P2b and P2c)

Written 2026-10-18 by the planner for VISION_PLAN 15.36. This binds every P2 art area: the new human rig (v2), its
animations and effects, the things a body uses (doors, seats, beds, mounts, boats), and the world scale. The plan,
the areas and the gates live in VISION_PLAN 15.36. The P2a style slice (section 19) locks this direction; after the
slice passes, this file is changed only to record what the slice measured, never to restart the direction.

Numbers marked "today" were measured in the code on 2026-10-18 (file:line in 15.36) or by the 2026-10-17 survey.

---

## 0. The feel

On a phone at arm's length you can tell who everyone is. The hero stands out at once: a little taller than the
fences, a warm gold ring at the feet, a dark crisp edge against any ground. Faces read: a frown under a fringe, a
beard, the glint of a lash. Hands hold things: a mug, a hoe, a sword hilt with the guard showing on one side and the
pommel on the other. Every weapon has its own weight. A dagger darts, a sword sweeps, a greatsword comes down like a
falling tree and hauls round, an axe bites down, a mace lands on the ground with dust, a spear snaps straight out.
When someone dies they buckle and fall and lie there. The world around them is unchanged except that its doors,
chairs, beds and horses fit them. Everything stays sharp-edged, painted, 16-bit: no blur, no smears, no mixed pixel
sizes.

---

## 1. Scale: one pixel grid, the zoom, and the rig

**One pixel grid.** Tiles stay 16 px. Every world sprite (terrain, props, buildings, people, creatures, FX) is drawn
1 art px = 1 world px. The whole world layer is then scaled by a whole number D (device px per world px). Nothing in
the world is ever drawn at another scale: no 2x people on a 1x world, no runtime rotation or scaling of sprites, no
upscaled v1 frames dressed up as v2. UI art (font, icons, panels) lives only in the UI layer at the UI scale U.

**Footprints never change.** A building, a bed, a stall or a boat covers the same tiles as today. Sprites overhang
their footprint upward, as the 3/4 view already allows (BLDG_PAD and the prop canvases). So world generation,
collision, pathing and saves are untouched by P2.

**The zoom (world layer only).** D0 = round(device px height / 240), ties up, kept inside V = h / D in [150, 300]
world rows. ZOOM offers FAR (D0 - 1), NORMAL (D0), NEAR (D0 + 1) and CLOSE (D0 + 2), each only where V stays in
[150, 300]. FAR never shows more than about today's phone canvas (295 rows), so it never costs more to draw.

| reference screen (device px) | UI today (kept) | world D0 | world view | v1 22 px figure | v2 32 px figure | zoom levels |
|---|---|---|---|---|---|---|
| desktop 1440x810 (the default window) | 480x270 @3 | 3 | 480x270 | 8.1 % | 11.9 % | N3 E4 C5 |
| desktop 1920x1080 | 480x270 @4 | 5 | 384x216 | 10.2 % | 14.8 % | F4 N5 E6 C7 |
| laptop 1366x768 | 683x384 @2 | 3 | 456x256 | 8.6 % | 12.5 % | N3 E4 C5 |
| desktop 2560x1440 | 512x288 @5 | 6 | 427x240 | 9.2 % | 13.3 % | F5 N6 E7 C8 |
| iPhone 15 Pro, Home Screen app 2556x1179 | 639x295 @4 | 5 | 512x236 | 9.3 % | 13.6 % | F4 N5 E6 C7 |
| iPhone 15 Pro, Safari 2556x1020 | 721x287 @3.54 | 4 | 639x255 | 8.6 % | 12.5 % | N4 E5 C6 |
| iPhone 11, Home Screen 1792x828 | 615x284 @2.91 | 3 | 598x276 | 8.0 % | 11.6 % | N3 E4 C5 |
| iPhone SE 1334x750 | 480x270 @2.78 | 3 | 445x250 | 8.8 % | 12.8 % | N3 E4 C5 |
| iPad landscape 2360x1640 | 480x333 @4.92 | 7 | 338x235 | 9.4 % | 13.7 % | F6 N7 E8 C9 |
| Witness harness window 960x540 | 480x270 @2 | 2 | 480x270 | 8.1 % | 11.9 % | N2 E3 |

(F FAR, N NORMAL, E NEAR, C CLOSE with their D. Figure = share of the screen height at NORMAL; today's phone app is
7.5 %, the desktop 8.1 %.) The UI keeps today's fit on every screen, fractional where it is fractional today; the
world is whole-number everywhere.

**The rig.** An adult man is 32 px from the sole's outline row to the crown's outline row: 2 tiles, the classic 16-bit
proportion, against 22 px today (x 1.45). That is what buys a real face: a 12 px head with a 10 px wide face.

**What scales with the body, and what does not.** Anything a body passes through, sits on, lies in, rides, rows,
climbs or pushes scales by k = 32 / 22 = 1.45 against today, rounded to a clean pixel and held to the ratios below.
Everything else keeps its size: trees, rocks, flora, crops, ground decals, terrain, water, roofs' pitch, wall
materials and their pattern density (a stone course stays 4 px; there are just more courses).

| body-holder | today (px) | ratio to a 22 px figure | P2 target at 32 px | rule |
|---|---|---|---|---|
| house door (h x w) | 16 x 10 | 0.73 / 0.45 | 23 x 14 | door height 0.70-0.76 of the figure |
| small / temple / grand / palace door | 13x8, 20x12, 22x12, 26x16 | | 19x12, 29x17, 32x17, 38x23 | same ratios |
| one-storey wall | 24 | 1.09 | 35 (never under door + 6) | 1.00-1.12 of the figure |
| chair / stool / bench seat surface | seat line row 19 of 24 | | hips land on the seat; seat at 0.31-0.37 of the figure above the floor | sitter's hips on the seat, feet on the floor |
| table / counter top | | | 0.44-0.50 of the figure | a seated eater's forearms rest on it |
| bed (2-tile footprint) | 20 x 32 sprite | | footprint kept; sprite may widen to 24 and rise for the headboard | a body lying north-south is drawn at 0.8 of its length (3/4 ground foreshortening) |
| horse (ridden and loose) | 36 x 36 cell | | about 50 x 50; withers 0.80-0.90 of the figure | the rider's hips on the saddle, feet below the belly line, hands at the reins |
| rowboat / deck rail | | | gunwale 0.35-0.45, rail 0.45-0.55 of the figure | a seated rower's hips on the thwart |
| ladder (cave exit) / stair flight | 16 x 24 / 16 x 40 | | 16 x 34 / height x 1.45 | rungs every 6 px |
| gate passage, arch, city gate | | | clear height 1.25 of the figure or more | the hero walks under, never through |
| hand cart / market cart wheel | | | wheel diameter 0.45-0.55 of the figure | the pusher's hands at the handles |
| fence, hedge, garden wall | | | 0.45-0.60 of the figure | a fence reaches the hip |
| grave marker | 24 x 32 | | x 1.2 at most | a kneeling mourner's head clears it |

---

## 2. Shape language (people)

Sturdy, readable, slightly heroic 16-bit figures. Big readable head, compact torso, legs that carry weight. Every
silhouette is one clear shape with a dark edge; details live inside it. Round heads and hands, squared shoulders on
men, softer shoulder-to-hip line on women, straight legs. Clothes hang: hems are lit, folds are single-pixel shadow
lines at elbows, waist and knees. Gear (P2b) sits on the body's planes and follows the same anchors, never floating.

---

## 3. Proportions

Rows counted up from F, the sole's outline row (the lowest opaque row; it lands 1 px below the actor's p.y, as v1's
feet do today). All heights include the 1 px outline.

| row band (adult man) | rows | content |
|---|---|---|
| F-31 | 1 | crown outline |
| F-30 .. F-28 | 3 | hair mass (a voluminous style may rise 2 px above F-31) |
| F-27 | 1 | hairline / forehead |
| F-26 | 1 | brows |
| F-25 .. F-24 | 2 | eyes, ears |
| F-23 | 1 | nose shadow, cheeks |
| F-22 | 1 | mouth |
| F-21 | 1 | chin |
| F-20 | 1 | jaw shadow onto the neck |
| F-19 | 1 | neck and collar |
| F-18 .. F-11 | 8 | shoulders F-18, chest F-17..F-15, waist F-14..F-13, belt F-12, hips F-11 |
| F-10 .. F | 11 | thighs F-10..F-7, knee F-6, shins F-5..F-2, ankle F-1, sole F |

Head 12 : body 20, about 2.7 heads: big enough for a face, enough body for armour to read.

| body | height | head | shoulders (front, incl. arms) slim / average / broad | waist | hips |
|---|---|---|---|---|---|
| man | 32 | 12 x 12 (10 px face) | 15 / 16 / 18 | 12 | 13 |
| woman | 31 (legs 10) | 12 x 12 | 13 / 14 / 16 | 11 | 13 |
| elf (either body) | +1 (longer legs) | 12, finer features | one step slimmer | | |
| half-breed | as human | 12 | a touch slimmer (one step at average) | | |
| youth (age 2) | 26 (torso 6, legs 7) | 12 | 12 | | |
| child (age 1) | 21 (torso 5, legs 5) | 11, rounder | 10 | | |

The cells are painted in scratch canvases (48 x 48 for locomotion and postures, feet at row 44; 80 x 72 for actions,
feet at row 60) and trimmed into atlases with a pivot per frame (section 10), so the sizes above are the only fixed
numbers. Side view: chest depth 9, head depth 11 with the nose 1 px proud of the face.

---

## 4. Faces (12 px head, 10 x 9 px of face)

- **Eyes** sit just below the middle of the head (F-25..F-24), the 3/4 view convention. Each eye is 1 px wide and
  2 px tall, columns 3 and 6 of the 10 px face (2 px apart, 3 px from each side). The upper pixel is the dark lash
  and pupil (the eye colour darkened 55 % toward Ink); the lower pixel is the iris colour (Eye Dusk #241A30 when none
  is chosen). No whites: no sclera, no anime eyes.
- **Brows**: 2 px over each eye on F-26, the hair colour darkened 30 %. Men: straight, inner end 1 px lower. Women:
  1-2 px, outer end 1 px higher (a gentle arch).
- **Nose**: one skin shadow pixel (ramp 1) right of centre on F-23; the light is top-left, so the nose shades
  down-right.
- **Mouth**: 2 px on F-22, skin ramp 0 mixed 30 % with #7A3B33. Women add one lip-tint pixel below it (Rosehip).
  Never bright red.
- **Chin and jaw**: F-21 skin ramp 1; F-20 skin ramp 0 (the shadow under the chin). Men: the chin row 6 px wide.
  Women and children: 4 px.
- **Ears**: 1 px each side at eye height (ramp 2, with a 1 px inner shadow). Half-breed: 1 px tip up. Elf: 2-3 px
  swept up and back, ears break the head's outline.
- **Cheeks**: optional 1 px blush (skin + 8 % red) under the outer eye for women and children; never on men.
- **Beard** (men only): covers the chin and jaw rows and the mouth corners; 1 px of mouth stays visible.
- **Side view**: one eye in the front third, its brow above, the nose 1 px proud with the outline wrapped round it, a
  1 px mouth, the ear 2 x 2 at the middle of the head, the jaw line.
- **Back view**: no face. Hair covers the head; 1 px ears; 1-2 px of nape skin at F-20..F-19.
- **Expressions** (the only face changes): idle neutral; blink (both eyes become a 1 px lash line for one frame); hurt
  (eyes squeezed to 2 px dark lines, mouth a 2 x 2 open dark); effort on a strike's second frame (mouth a set 3 px
  line); fall and lying (eyes closed lines).
- The hero gets nothing extra in the face. The ring, not the face, marks the hero.

---

## 5. Hands

- 2 x 2 px fist plus a 1 px wrist; an open hand is 2 x 3 (waving, casting, praying joined as 3 x 2).
- Skin ramp: top-left pixel ramp 3, the rest ramp 2, the bottom-right pixel ramp 1. Gloves (P2b) recolour the same
  pixels with the glove band.
- Outside the body the hand is inside the outer ink outline. Over the body (a hand in front of the chest) a 1 px
  selective contour separates it from the cloth (the existing `layered` rule).
- Front and back views: both hands show in idle and walk at hip level (F-11..F-9), swinging 3 px opposite the legs.
  Side views: the near hand always shows; the far hand shows when it holds something.
- A hand is drawn OVER what it grips: the grip vanishes under the fist, the guard shows on one side and the pommel on
  the other. Two-handed weapons: the lead hand at the guard, the rear hand 3 px down the grip (greatsword) or 6-8 px
  down the shaft (spear, staff).

---

## 6. Palette (named; the existing art_internal.h ramps carry everything else)

| name | hex | use |
|---|---|---|
| Ink | #1E162A | the outer outline of every character (kInk); the darkest colour in the game. Never #000000 |
| Lit Ink | Ink mixed 30 % toward the neighbour (on skin #ECBC92: #5C4849) | the outer outline on top- and left-facing edges |
| Eye Dusk | #241A30 | the default iris |
| Skin S1-S8 | #FADCBE #F2CCA6 #ECBC92 #D6A074 #BE865E #A06846 #7C5038 #5C3C2E | the creator's eight tones; ramp spread 0.8 |
| Hair | Raven #241E22, Umber #462C1E, Chestnut #784624, Auburn #964024, Copper #C8682C, Wheat #DCB464, Flax #E8DCB6, Ash #969496, Silver #E4E2DA, Plum #78326E | the creator's ten |
| Eyes | Brown #603A20, Blue #3C70C4, Green #388C50, Grey #788492, Amber #CE9228, Violet #8850B0 | eyeColor |
| Mouth Line | skin ramp 0 + 30 % #7A3B33 | mouths |
| Rosehip | skin ramp 1 + 10 % #A0403C | women's lip tint |
| Ember Gold | #F2C14E | the hero's ring rim |
| Ring Shadow | Ink at 45 % | the line under the ring's front arc |
| Moonwash | #8FA6D8 | the night lift on figures (additive) |
| Warning Ember | #E8603C | off-screen danger pips (Ink outline) |
| Steel Arc | core #F4F1E6, edge #B8C4D8 | sword and dagger arcs |
| Heavy Arc | core #FFF2D0, edge #C8A878 | greatsword arc and ghost blade |
| Chip | wood #B07A4A, bone #E6DCC0 | axe chips |
| Smash Dust | #C8B496, sparks #FFD87A | mace and hammer impact |
| Glint | #FFFFFF core, #FFE9A8 halo | dagger stab, blade tips |
| Thrust Streak | #E8F0FF to #9FB4D8 | spear thrust |
| Swish | #D8CDB8 at 50 % | staff and walking stick arcs |
| Punch Star | #FFF0C0 core, #F08A3C rim | fists |
| Twang | #E8E0C8 | bow release |
| Finisher | #FFE6A0 tint, 1.2x the family's FX | the third hit of a combo (as today's warm tint) |

Clothes keep the creator's shirt and trouser palettes and the culture engine's dress colours. Metals (P2b) keep the
M6 band ramps and alloy keys (metalRampKeys), so the doll, the icons and the world agree.

---

## 7. Light, shading and materials

- **Light from the top-left**, a little toward the viewer (art_internal.h kLx -0.56, kLy -0.62, kLz 0.55), on every
  facing. 5-step ramps: 0 deepest shadow (drifts blue-violet), 1 shadow, 2 base, 3 light, 4 highlight (drifts warm).
- **Volumes**: the head is a sphere (the hair mass lit top-left with a 1-2 px sheen), limbs are cylinders (lit left
  column, shadow right column), the torso a box whose left plane is lit.
- **Contact shadows**: hair onto the forehead (one ramp-1 row), chin onto the neck (ramp 0), arms onto the torso sides
  (1 px), under the belt, inside the elbow and behind the knee.
- **No pillow shading** (no shading that follows the outline evenly). No banding: neighbouring ramp steps never form
  parallel stripes. Dither only at light-band edges on wide cloth (robes, cloaks, skirts), never on faces or hands.
- **Materials**: skin warm and soft (no specular); cloth matte, folds as 1 px ramp-1 lines, lit hems; leather ramp
  spread 1.0, highlights stop at ramp 3; hair streaks of ramp 3 follow the style's flow; metal (P2b) spread 1.1-1.3
  with a 1 px specular on the top-left edge and a bright tip pixel on blades.
- Ground shadows are never painted into a sprite: the renderer draws them (the soft contact ellipse at the feet).

---

## 8. Outlines and contrast (characters against the world)

- **Characters** (people, creatures, mounts, the hound) get an OUTER ink outline: 1 px round the whole silhouette,
  Ink on bottom- and right-facing edges, Lit Ink on top- and left-facing edges.
- **Inner contours** (a part over another part of the same figure: a hand over the chest, an arm over the body, a
  blade over cloth) use today's selective outline (`outlineOf`: the neighbour darkened, mixed 45 % with ink), 1 px,
  only where two shapes of similar value meet. No double lines; nothing outlined inside the face.
- **The world** (terrain, props, buildings, flora) keeps its selective outlines. That difference is the figure-ground
  rule: people always pop.
- P2a applies the ink outer outline to today's v1 sheets as well (people, monsters, critters, horses, the hound), so the
  contrast ships before the new rig.
- **Night**: after the light map, each visible human figure is added again in Moonwash at 0.10-0.18 of the darkness, so
  people stay a step brighter than the ground they stand on. Lamps and the hero's torch still win.
- **The hero's ring**: a 1 px Ember Gold ellipse at the feet (24 x 9 on the v2 rig, 18 x 7 on v1), its front arc 1 px
  brighter, a 1 px Ring Shadow line under the front arc so it reads on snow and sand. Drawn after the contact shadow
  and before the body, 80 % alpha; it flares to 95 % for one second when the hero is hit or walks into a crowd (4 or
  more people within 3 tiles). PLAYER RING: ON / OFF in SETTINGS (default ON).
- **Measured targets** (VISION_PLAN 15.36): day edge contrast of a figure's outline ring against the ground ring at
  least 40 luma on grass, road, sand and snow; at night at least 25, and the figure's median luma at least 10 above
  the ground's.

---

## 9. Facings

v2 paints four facings natively: down, up, right and LEFT. The left row is painted, never mirrored, so the light stays
top-left and the weapon stays in the right hand. Facing right the weapon hand is the near hand; facing left it is the
far hand, and the blade must still read: in idle and walk at least 4 px of blade or haft shows beyond the body, and
every strike's arc passes in front of the body. The renderer never flips a v2 people frame. FX sprites (arcs, streaks,
stars) may be mirrored; they carry no light.

---

## 10. Animation: clips, frames, timing

Every frame lives in a trimmed atlas with a pivot (the point between the feet on the sole row); the renderer places
the pivot on the actor's position. Atlases per look: LOCO (all humans), ACTION (fighters: the look's weapon family),
DEATH (baked when someone falls), POSTURE (per posture in use).

| clip | frames per facing | timing | reads as |
|---|---|---|---|
| Idle | 3: neutral, breathe, blink | neutral and breathe alternate 0.55 s each (shoulders and chest rise 1 px); a 0.12 s blink every 3-6 s, phase per actor | alive while standing |
| Walk | 6: contact, down, pass, contact, down, pass | one frame per 4.5 px travelled (a 27 px stride cycle): 16 fps at the hero's 74 px/s, 9 fps at 40 px/s, never sliding | feet planted at any speed |
| Hurt | 1 | held through the Hurt state (0.18 s for the hero) | flinch: head back, shoulders up, eyes squeezed |
| Windup | 2 | 45 % / 55 % of the windup state (the hero: 0.07 x pace s; enemies: their family's windup) | anticipation |
| Strike | 2 | 40 % / 60 % of the strike state (0.11 x pace s); the FX starts on the first | the blow |
| Follow | 2: follow-through, recover | 50 % / 50 % of the recover state (0.13 x pace s) | weight after the blow |
| Combo variants | Windup + Strike + Follow for hit 1 (forehand), hit 2 (backhand), hit 3 (finisher) | as above per hit | three different blows |
| Cast | 2: gather, release | gather over the cast windup, release after | magic, distinct from a swing |
| Bow | Draw 3 (nock, chest, cheek) + Release 1 | draw over the windup, held at the cheek while aiming; release 0.08 s | a real draw in every facing |
| Block (depth) | 1 | while a parry window is open | weapon across the body |
| Roll (depth) | 4: crouch, tuck, over, rise | 0.085 s each = the sim's 0.34 s roll | a real tumble |
| Fall | 3: recoil, buckle, topple | 0.10, 0.12, 0.12 s from the killing blow | the body gives way |
| Lying | 2 (head west, head east) | held 60 s, fading over the last 5 s | the dead lie on the ground |

The hero's windup is only 4 sim frames: each family must read in its strike and follow-through frames and its FX, not
in the windup. Enemies wind up for 0.22-0.38 s, so their windups carry the telegraph.

---

## 11. Weapons: size, grip and depth

| weapon (art id) | length on a 32 px figure | width | grip |
|---|---|---|---|
| dagger (5) | blade 5 + hilt 3 = 8 | 1-2 | one hand |
| sword (1) | blade 11 + hilt 4 = 15 | 2 (lit edge, shadow edge, a 1 px fuller on steel) | one hand; the guard and pommel show either side of the fist |
| greatsword (7, new) | blade 18 + hilt 6 = 24 | 2-3 | two hands, 3 px apart |
| axe (2) | haft 13, bearded head 5 x 6 | | one hand |
| mace (6) / hammer and pick (6) | haft 11, flanged head 4 x 5 / haft 13, head 6 x 3 | | one hand |
| spear (8) / glaive / halberd forms | shaft 34 (taller than the bearer), leaf head 5; glaive blade 7; halberd axe and spike | 1 px shaft, 2 px head | two hands, 6-8 px apart |
| staff (4) | 30 with a 3 x 3 head | | two hands, or one hand and planted |
| walking stick (9) | 24 with a crook | | one hand |
| bow (3) classic / longbow / recurve / crossbow | 22 / 28 / 20 with curled tips / 12 x 10 | | the off hand holds the bow, the right hand draws |

- Every weapon is rasterized procedurally at the angle its pose gives (16 directions, 22.5 degree steps) with clean
  pixel lines: no doubled pixels, no L-shaped jaggies on diagonals. Never a rotated bitmap.
- **Depth**: the pose data says per frame whether the weapon is behind the body (the weapon hand is the far hand and
  the blade points away) or in front (everything else). Shields (P2b) sit on the off arm and follow the same rule.
- Tier tint stays the weaponColor metal ramp; P2b adds the culture blade, polearm and bow forms on these bases.
- Fighters hold their weapon in hand in every clip (as today). Civilians hold only what their posture gives them.

---

## 12. Swing families

Each melee family has three variants (hit 1 forehand, hit 2 backhand, hit 3 finisher), each Windup 2 + Strike 2 +
Follow 2 frames, in four facings. The tip path below is measured from the pose data (rpg_test --rig).

| family | hit 1 | hit 2 | hit 3 (finisher) | tip path rule | FX |
|---|---|---|---|---|---|
| Unarmed | jab: the lead fist straight out | cross: the rear fist, shoulder turned | rising hook | fist travels 7 px or more along the aim; hands alternate | Punch Star 9 x 9, 2 frames |
| Dagger | stab: hand from the hip, arm locked straight, body leaning in | short slash across | two quick stabs | tip travels 8 px or more along the aim, drifting 2 px or less sideways | Glint 12 x 6, 2 frames at the tip |
| Sword | horizontal arc right to left across the front at chest height | the mirror path left to right, same hand | overhead vertical cut to the low front | arc of 120 degrees or more | Steel Arc crescent 32 x 32, 4 frames |
| Greatsword | heavy overhead cleave: both hands raised high behind the head, body leaning back 1 px, the blade brought down to knee height in front, stance 2 px wider, 1 px crouch | wide waist-high sweep: loaded low behind the right hip, swung round to the far left | a rising sweep into an overhead slam that strikes the ground, with dust | hit 1: tip travels 24 px or more down through 100 degrees or more; hit 2: arc of 160 degrees or more, the blade spanning most of the cell | Heavy Arc 48 x 40, 5 frames with a trailing ghost blade |
| Axe | chop: head raised behind the head, bites down to knee height | diagonal from the other side | two-handed overhead chop | head travels 20 px or more downward through 60 degrees or more, ends low | short heavy arc 28 x 28, 4 frames, plus Chips on a hit |
| Mace / hammer | smash: raised overhead, rising 1 px onto the toes, lands on the ground in front, knees bent 2 px | heavy horizontal hook | overhead smash, bigger ring | head travels 24 px or more down; the impact frame touches the ground line within 2 px | Smash ring 32 x 16, 5 frames, dust, 3 sparks on metal |
| Spear (and glaive / halberd) | thrust: shaft slides back 6 px, then forward 10, then 4 more, front foot forward | a second thrust aimed higher | lunge with a step (spear); a sweeping cut (glaive, halberd) | tip travels 12 px or more straight (2 px or less sideways); reach 30 px or more from the body centre side-on | Thrust Streak 24 x 5, 3 frames along the aim |
| Staff / walking stick | diagonal two-handed strike, far end leading | rising strike from low | overhead two-handed smash | arc of 90 degrees or more | Swish 28 x 28, 3 frames, no glint |
| Bow | draw and release (section 10) in every facing; the arrow leaves from the string hand | | | draw hand travels 6 px or more back from the bow | Twang 8 x 8, 2 frames at the bow |

- How each view reads: facing down, the arc crosses in front of the body (over it); facing up, it passes over the head
  and shoulders, visible above them; side-on, it sweeps in front of the face side.
- A finisher lands with a one-frame impact flash on the foe and, for the greatsword and the mace or hammer, a one-frame
  camera nudge.
- Two families never share a strike frame: any two families' Strike frames differ in 25 % or more of the union of
  their opaque pixels, per facing.
- The FX is timed by the family's pace (it ends with the strike state, within one frame), never by a fixed 0.11 s.

---

## 13. Hurt, death, fall, kneel, lying

- **Fall** (every human, every facing, 0.34 s): recoil (head snapped back, arms flung, weapon loose), buckle (both
  knees bent, torso slumped forward, head down), topple (body tipped toward the fall side, painted from the side-view
  figure, not rotated by 45 degrees).
- **Lying**: flat on the ground, horizontal, head toward the fall side (painted head west and head east), arms loose,
  the weapon dropped beside the hand, a shield flat beside the body. Eyes closed. Opaque height 16 px or less (half the
  standing figure or less). Built from the side-view figure turned onto the ground plus hand-tuned limbs, so every gear
  layer (P2b) lies with the body.
- **Corpse**: lies 60 s, fades over the last 5 s, blocks nothing, at most 24 at once (the oldest fade first). A small
  dark ground stain under it (Ink at 25 %), no gore.
- **The hero's death**: the same fall, played under the death screen's fade (view time), lying before the words are
  fully up.
- **Kneel** (a posture): one knee on the ground, torso upright, head at about 22 px. **Downed** (a companion at 0 HP):
  slumped on one knee, a hand to the wound, head low, two slow breaths.

---

## 14. Female and male bodies

Both bodies share one skeleton: head, hands, hips and feet sit within 1 px of each other, so every pose and every gear
layer fits both. The difference is read through shoulders, waist, hips, stance, face and hair, never through
exaggeration.

| aspect | man | woman |
|---|---|---|
| height | 32 | 31 (legs 1 px shorter) |
| shoulders (average build) | 16 | 14 |
| waist / hips | 12 / 13 | 11 / 13 (the belt row 1 px in) |
| neck (side view) | broader | 1 px slimmer |
| jaw | chin row 6 px | chin row 4 px |
| brows | straighter, inner end lower | finer, gentle arch |
| eyes | 1 x 2 | 1 x 2 plus one lash pixel at the outer top |
| mouth | 2 px line | 2 px line plus one Rosehip pixel |
| chest | flat shading | ONE ramp-1 shadow row on the clothing under the bust; the silhouette changes by 1 px at most |
| stance (front view) | feet 4 px apart | 2 px apart |
| walk | hands swing 3 px | 2 px, a 1 px hip sway front and back views only |
| idle | weight centred | weight on one leg (1 px hip shift) |
| beard | allowed | never |
| hair | any style | any style (the look decides) |
| armour (P2b) | the same pieces | the same pieces fitted: breastplate shaped by 1 px, pauldrons 1 px narrower; no sculpted cups |

Children and youths are painted natively at their own proportions (section 3); today's row-dropping childBody() is
retired with v1.

---

## 15. Postures and things in hand

All 31 postures (art_life.h: sitting, eating, drinking, working with hammer, hoe, broom, axe, ladle, carrying,
fishing, sleeping, waving, playing, dancing, the bard's instruments by culture, praying, begging, the lamplighter,
reading, cross-legged, riding, downed, kneeling) are poses of the same skeleton in four facings, with today's frame
counts and paces (postureInfo). Props in hand (mug, spoon, bread, tools, instruments, ledger, lamp pole, bowl) are
painted at the hand anchor in their own ramps, hand drawn over the grip. A chair-height sitter's hips sit on the seat
surface and the feet on the floor; a cross-legged sitter's hips sit 2 px above the floor or cushion; a sleeper is the
sleeper sprite fitted to its berth (head on the pillow, the body a soft mound under the berth's own blanket, eyes shut,
hats off), drawn at 0.8 length in north-south beds. Skeletons, draugr and goblins are painted with this skeleton too,
in the monster sheet layout: skeletons and draugr at the adult's 32 px, goblins about 24 px (0.7-0.8 of the adult)
with a youth's big head and short legs.

---

## 16. How bodies meet the world

- Feet are on the ground line in every contact frame: no hovering, no sinking (0 px).
- A seated body's hips are on the seat (0 px), its forearms on the table when eating; a rider's hips on the saddle,
  hands at the reins, on every horse frame; a rower's hips on the thwart; a sleeper's head on the pillow.
- Doors: the opening is 0.70-0.76 of the figure. In the 3/4 view a walker's head may overlap the lintel by up to
  9 px as they step through; the hero never looks taller than the eaves of a one-storey house (wall 1.00-1.12 of the
  figure).
- Sorting stays by the feet (y). A lying body sorts by its own baseline; a stall's roof, a tree crown or a gatehouse
  vault still turns the hero into today's silhouette when it hides them.

---

## 17. Composition in the world

- People are the focal points. Nothing person-sized crowds a path: ground clutter by roads and doors stays under 0.6 of
  the figure (the bigger rig already turns today's person-sized wheat clumps, 15-20 px, into hip-high crops).
- Markers float 4 px above the hair (rig metrics markerY), never touching it; bubbles sit above markers.
- In a crowd the hero is found by the ring first, then by the outline and the face.
- Nothing floats: every body, cart, boat and mount has its contact shadow or its seat.

---

## 18. UI for P2

- **SETTINGS** gains ZOOM (FAR / NORMAL / NEAR / CLOSE, only the levels the screen allows; help: "HOW CLOSE THE
  CAMERA SITS. EVERY PIXEL STAYS WHOLE.") and PLAYER RING (ON / OFF; help: "A GOLD RING AT YOUR FEET, SO YOU ARE EASY
  TO FIND IN A CROWD."). Desktop keys: - and = (and the keypad), and the mouse wheel in play; touch: the SETTINGS row
  (a two-finger pinch is depth).
- **Text never lives in the world layer.** Speech plates, name tags, "E: TALK", combat numbers, banter and elite name
  plates are drawn in the UI layer at the UI scale, anchored at the projected world point, so their glyphs are the
  same size at every zoom. Pixel icons over heads (markers, bubble icons), bars and the ring stay in the world layer.
- **Off-screen pips**: a 7 x 5 Warning Ember chevron with an Ink edge at the safe-area edge, pointing at each
  aggro'd hostile outside the view within its aggro radius, pulsing slowly.
- Menu figures (the paper doll, the creator preview, the HUD's hero) show v2 frames at whole multiples (P2b).

---

## 19. The style slice (P2a wave 1, artist-xhigh)

The smallest finished piece that locks the look. Built on rig v2 behind `rig 2`, judged before any other v2 art starts.

**Contents**
1. The hero, man and woman, same look (Chestnut short hair for him, Chestnut braids for her, S3 skin, the default blue
   tunic and brown trousers, plain shoes, a sword): Idle (3), Walk (6), Hurt, the sword's three swing variants, Fall
   (3) and Lying, in all four facings, with the Steel Arc FX.
2. One villager woman in a Dress with a culture cut and a pattern, Wheat hair, S6 skin: Idle and Walk in four facings.
3. Their surroundings at the target scale: one start-village cottage with its door and wall at the body-holder ratios
   (bodyScale plumbing from phase A), a fence run beside it, the contact shadow and the hero's ring.
4. Shots (script p2a_slice.txt and p2a_slice_phone.txt, seed 7, the start village OSLARS): desktop 1440x810 at NORMAL
   (D3) and NEAR (D4); phone 2556x1179 at NORMAL (D5); 11:00 and 22:00; the hero idle beside the villager by the
   cottage door; each swing variant mid-strike; the fall at 0.1, 0.2 and 0.34 s and lying; a comparison shot with a v1
   villager (rig 1) at the same zoom; the rig gallery page (every slice frame at 1x and 4x).

**Acceptance** (measured by the judge with tools/p2_measure.mjs; the full list is VISION_PLAN 15.36 "The style slice"):
figure 32 / 31 px (96 / 93 device px at D3 on the desktop, 160 / 155 at D5 on the phone: 11.5-14 % of the screen
height); every world pixel in the noon shots an exact D x D block; the face shows brows, two 1 x 2 eyes, a nose pixel
and a mouth with eye-to-skin contrast of 60 luma or more; two hands in front and back idle frames; outer outline
pixels at luma 60 or less (100 or less on Lit Ink edges); day edge contrast 40 or more and night 25 or more with the
figure 10 above the ground; the man and the woman differ by 2 px in shoulder width or more and read as man and woman
without exaggeration; the walk keeps the feet planted (1 px slide or less per contact frame); the sword's tip path
spans 120 degrees or more; lying within 0.45 s of the kill at 16 px tall or less; the cottage door at 0.70-0.76 of the
figure and the wall at 1.00-1.12; then the look review: palette, top-left light, no pillow shading or banding, no
orphan pixels or jaggies, the hero found first in the shot.

---

## 20. Don'ts

- No fractional world scale, no blur filter on world art, no runtime scaling or rotation of sprites.
- No mixels: never a v1 frame scaled up, never a sprite at another pixel size than its neighbours.
- No pure black; no double outlines; no pillow shading; no banding.
- No sclera whites, no faces in back views, no expressions beyond section 4.
- No exaggerated anatomy on either body; no armour that sculpts a body.
- No mirrored people frames; no weapon changing hands between facings.
- No text in the world layer.
- No body-holder left at today's size once v2 ships, and no non-body prop scaled with it.
