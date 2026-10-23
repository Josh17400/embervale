# Pixel-art walk and run cycles for short-legged (chibi) sprites: research for EMBERVALE

Date: 2026-10-21. Scope: 30-32 px tall people, about 10 px crotch to sole, 6-row boot, shown at 3-4x, 6-frame (maybe 8) cycles advanced by distance.

## 0. Read this first: evidence quality

The web tools available for this research were weak. Several primary sources returned 403 or no text (Saint11's blog pages, Pixel Logic, Pedro Medeiros's Miniboss blog, the Game Developer Dead Cells piece). What I could get, I got from the artifact itself.

- VERIFIED from primary data: Saint11's own Walk Cycle and Run Cycle 1 infographics (I downloaded the GIFs from saint11.art and read them frame by frame); the pret/pokefirered animation tables (actual decompiled source); Slynyrd Pixelblog 8 and 50 text; Stardew Valley wiki frame lists (via search excerpt); several Aseprite and PixelJoint forum threads.
- NOT FOUND despite repeated searches: frame-by-frame breakdowns of Chrono Trigger, Secret of Mana, FF6, EarthBound/Mother 3, Sea of Stars, Chained Echoes, Eastward, CrossCode, Moonlighter, Hyper Light Drifter, Celeste's run, Owlboy, Children of Morta, Octopath. No page I could open gave their leg-pixel data. Anything said about those games below is labelled MEMORY (my prior knowledge, unverified). Check by ripping the sheets (The Spriters Resource) and stepping through them in Aseprite.
- Section 5 (the recipe) is a synthesis. The numbers are starting values derived from the verified principles and simple geometry, not copied from a shipped game. They must be judged by eye (section 7).

## 1. How pros structure walk and run cycles (verified)

### 1.1 Saint11 (Pedro Medeiros, Celeste art director): Walk Cycle infographic
Index: https://saint11.art/blog/pixel-art-tutorials/ . GIF: https://saint11.art/img/pixel-tutorials/Walk.gif . I read it frame by frame. It states:

- Two versions of the same walk: 12 frames ("fluid animation, 6 frames for each leg") and 6 frames ("useful in low resolution, 3 frames for each leg"). The 6-frame version is frames 1, 3, 5 of the 12, the "important frames".
- The six named poses: 1 CONTACT (first frame where the front leg touches the ground); 2 DOWN (the impact makes torso and head go down); 3 DOWN+ (lowest frame, widest arm position); 4 PREPARE PASSING (get ready to move up; "I added a head delay in this one"); 5 PASSING (moving up, "lots of potential for variation here"); 6 UP (highest point).
- Rules at the bottom: "Arms always move with the opposite leg." "Leg on ground always moves towards the back."
- The GIF plays at 100 ms per frame (12 frames, 1.2 s per cycle of two steps, so the 6-frame equivalent is about 200 ms/frame at that speed).
- In the drawing the far leg is a much darker value than the near leg, the near arm is the lightest shape, and the head is allowed to lag the body.

### 1.2 Saint11: Run Cycle 1 ("Simple Run")
GIF: https://saint11.art/img/pixel-tutorials/RunCycleSimple.gif (read frame by frame; 80 ms/frame in the file).

- "A simple cycle can be understood in 4 frames, then you repeat everything with the other leg. You can make the animation more fluid by making inbetweens." Shown as 8 and 16 frame versions.
- Four keys: JUMP (both legs in the air, arms always alternated with legs; HIGHEST position); FALL DOWN (fall just a little, open arms/legs a little; WIDEST); CONTACT (hit the ground, forward leg is STRAIGHT; LOWEST position); RECOVER (get ready for leg/arm to cross; forward leg moves back, back leg moves forward; labelled THINNEST).
- Even Saint11's run has a narrow "thinnest" frame (recover). He accepts it on one frame only; the highest and widest frame is the flight pose right before it.
- Back leg and back arm are a darker blue than the near ones.

### 1.3 Slynyrd (Raymond Schlitter)
- Pixelblog 8, Intro to Animation (2018): https://slynyrd.com/blog/2018/8/19/pixelblog-8-intro-to-animation . Mega Man's 3-frame run (4 slots, the pass frame used twice) "captures more kinetic energy than most modern run cycles". Advice: "make powerful stride poses with limbs extended and the character tilted forward"; "the pass frame should have all the limbs close to the body in neutral positions so it connects with both stride poses"; include vertical movement for the bounce. His 8-frame run was cut to 6 by removing the recoil and to 4 by removing the high point; he prefers 6 "in most cases". Contact and pass are never cut. Timing: 8 frames at 80 ms, 4 frames at 160 ms (same 640 ms cycle); timing has a large effect on energy, so test several speeds.
- Pixelblog 50, Human Walk Cycle (2024): https://www.slynyrd.com/blog/2024/5/24/pixelblog-50-human-walk-cycle . Built from 60 fps video reduced to 8 frames: contact (heel meets ground, limbs at extremes, figure lowest), down (foot flat, weight shifts, other foot lifts and swings), pass (legs cross, figure tallest), swing (other leg's biggest forward move, still off the ground). 8 frames "strike a balance between economy and fluidity"; 4 "feel lacking alone"; 6 drops the least important; beyond 8 is aesthetic. Head path is a triangle wave, not a sine ("more organic"). Opposite limbs share momentum. One pixel of chest movement on contact "makes all the difference".

### 1.4 Classical vocabulary (secondary sources)
Williams' contact method: walk = contact, down, passing, up per step, 8 poses per cycle (course notes e.g. https://courses.cs.washington.edu/courses/cse464/11wi/assignments/assignment4 ). A run swaps passing/up for push and flight (both feet off the ground); AnimSchool and Ulster course blogs (via search) put a film-rate walk at about 24 frames and a run at 12-16. Aseprite forum: "Without both feet in the air for a bit, a motion is not a run."

### 1.5 Forum-level consensus (verified text)
- Aseprite, 16x16 walk/run thread https://community.aseprite.org/t/does-anyone-have-a-16x16-animation-walk-run-cycle/10479 : cycles are categorised by frame count not canvas size; small sprites use fewer frames because they cannot show the difference between some frames, so merge the frames you cannot tell apart; draw big first; "block in the whole leg instead of 1px lines. Even at 16x16 this is usually thicker than 1px"; use different values for front and back legs; many 8-bit games use 2 or 4 frames (often 3 distinct).
- Aseprite walking critique https://community.aseprite.org/t/still-learning-or-trying-to-learn-to-do-walking-animation/8291/26 : four frames suffice (contact, passing, contact, passing); body and head lowest on contacts, higher on passing; both contacts must be the same height or "it looks like the character has a limp"; the back leg should push off further back than you think.
- PixelJoint https://pixeljoint.com/forum/printer_friendly_posts.asp?TID=25611 : 3 frames is too few, 4 the minimum, 6 suits a small sprite; about 8 for a full loop; no neutral standing pose in a walk; torso dips when legs are extended and rises when they come together; torso twists against the arms; a 2-frame walk works only if both frames show fully extended legs.
- PixelJoint run thread https://pixeljoint.com/forum/printer_friendly_posts.asp?TID=6196 : a cycle missing its passing frame looks off; small leg size makes the full range hard to fit.
- Flowlab thread (403 on fetch, summary via search): trouble with diagonal legs; advice was to make the back leg darker as in Saint11's tutorial. https://community.flowlab.io/t/i-can-not-make-good-leg-movement-for-my-walk-cycle/41890
- imonk's run tutorial https://itch.io/t/2491630/pixel-tutorial-run-cycle : 8 frames; keys are a crouch with one leg grounded then a kick-out and small hop; hair and the dangling sword arm bob as secondary motion; tween then tune playback speed.
- Polycount (3D, same physics) https://polycount.com/discussion/comment/2379992 : both feet must travel the same distance over the same number of frames while planted, and the planted foot's ground position must be linear in time, otherwise it slides.

### 1.6 Time-keyed vs distance-keyed: what shipped code does
pret/pokefirered, https://raw.githubusercontent.com/pret/pokefirered/master/src/data/object_events/object_event_anims.h :
- Walk (side): frames 7, 2, 8, 2, each held 8 ticks. Frame 2 is also the standing pose; 7 and 8 are the two step poses. East is west with hFlip. Four slots, three images.
- Run (side): frames 15, 16, 15, 17 held 5, 3, 5, 3 ticks. Separate run art, and uneven holds.
- INFERENCE (mine, not in the source): walking moves 1 px/tick and running 2 px/tick, so each loop covers 32 px (two 16 px tiles): one foot per tile, effectively distance-locked.
- Stardew Valley (https://stardewvalleywiki.com/Modding:Farmer_sprite , via search excerpt): walk frame list 0, 1, 0, 2 (stand, step A, stand, step B) at 200 ms each; left is the mirror of right; frames are 16x32; arms are a separate layer from body and legs.

Shipped 16-bit and indie RPG default: 4 slots, 3 images, the middle image doubles as idle, mirror for the other side, 1 px body shift. Your 6-frame cycle (contact, down, pass x2) is richer. The 4-slot version works because overworld sprites are tiny and the whole sprite bobs 1 px.

## 2. Why legs read as tilting sticks, clunky, or skinny

Verified sources plus standard animation theory. Items marked (mine) are inference.

1. Rigid pendulum (mine): each leg is the same shape every frame, just rotated, so the soles trace an arc about the hip. Real walking differs in two ways. (a) The planted foot is flat on the ground and slides backward at exactly ground speed, so hip-to-foot distance changes (Saint11: "leg on ground always moves towards the back"). (b) The swing leg folds: knee leads, shin trails, so its shape is bent at pass and nearly straight at contact.
2. Cure: animate foot positions and body height, and redraw the leg from a row-offset table every frame instead of rotating a shape.
3. Clunky comes from timing and missing keys: no pass frame (PixelJoint), unmatched contact heights (Aseprite: "limp"), a sine bob (Slynyrd wants a triangle wave), equal holds in a run (Pokemon uses 5/3), and head, torso and hem all moving on the same frame as the legs (Saint11 delays the head).
4. Skinny run legs: a diagonal drawn as a 1-2 px staircase is 1-2 px thick. At 45 degrees a leg W px wide per row has perpendicular thickness of about 0.7 W. Fix: keep every row W >= 4 wide and shift whole rows (shear) instead of drawing a line; keep the near leg two or three value steps lighter than the far leg; widen knee and boot where the leg is most horizontal; keep staircase steps regular (2-2-2, not 1-3-1). Sources: Aseprite "block in the whole leg", Flowlab/Saint11 darker back leg, PixelJoint stair-step advice.
5. Detached feet: the boot is a separate piece that does not follow the shaft. Cure: shaft and foot are one shape per frame; tilt a boot by changing which foot pixels touch the ground (heel-only at contact, toe-only at push-off), not by rotating it.

### Near leg vs far leg
Paint the far leg first in a darker ramp, then the near leg, then the tunic hem. At pass the far leg is mostly behind the near leg, so draw its lifted boot trailing behind the stance leg where it stays visible. Never let two same-value legs touch. Near arm lightest, far arm dark and half hidden behind the torso.

### Knees at 10 px (mine)
A 10 px leg cannot hold a real thigh and shin. The "knee" is where the shear offset changes direction in the four trouser rows above the 6-row boot. The lifted leg is simply shorter (fewer rows), not rotated.

## 3. What shipped short-legged sprites do

Verified: Pokemon Gen 3 and Stardew (above): 4 slots, 3 images, middle image is the stand pose, mirror, foot lifts a few pixels, body shifts 1 px. Mega Man (Slynyrd): 3 frames, strong lean, neutral pass.

MEMORY (unverified, check by ripping sheets): Chrono Trigger and Secret of Mana walk and run with thick short legs where the pass frame lifts the back boot with a 1 px bob and the hem lags a frame; EarthBound/Mother 3 use a 3-image shuffle with a 1 px head bob; Sea of Stars and Chained Echoes use more frames and a stronger torso bob; Octopath HD-2D uses 6-8 frames. Impressions only. The Nintendo Life Sea of Stars interview (https://www.nintendolife.com/features/i-dont-want-to-copy-paying-homage-to-childhood-rpgs-with-sea-of-stars) says nothing about frame counts.

The pattern the verified sources agree on:
- Most of a walk is torso and head: a 1 px bob and a 1-2 px arm swing carry it (Pokemon and Stardew are almost nothing else).
- The legs show two things: one boot lifted a few pixels, one boot planted, with a short stride.
- A run differs from a walk by a longer stride, a forward lean, an airborne frame, a bigger bob (2 px), and shorter holds.

## 4. Pixel-specific techniques

- Sub-pixel animation: https://tinywarriorgames.com/2019/01/04/game-development-pixel-art-sub-pixel-animation/ and https://someanimatorwannabe.substack.com/p/animator-techniques-sub-pixel-animation . Recolor edge pixels instead of moving pixels for sub-1 px motion (Metal Slug's Fio, about 34 px tall: breathing and head motion are colour shifts, the ponytail is four toggling pixels). Use it for hem and hair. Legs at 10 px should move in whole pixels.
- Smear frames (Tiny Warrior): an in-between where the shape is smeared across two positions. For a run this is mainly a 1 px longer boot on the flight frame; optional.
- Scaling a cycle: Slynyrd's equal-cycle rule (8 frames at 80 ms equals 4 frames at 160 ms).
- Not retrievable, but cover this: Pixel Logic by Michael Azzi (ch. 8 Subpixeling, ch. 9 Animation; https://80.lv/articles/pixel-logic-a-pixel-art-tutorial-book/ ), Saint11's Walk cycle tutorial, Brandon James Greer, AdamCYounis, MortMort.

## 5. Recipe for EMBERVALE

Assumptions: faces right; legs 4 px wide; leg column is 10 rows (row 0 at crotch, row 9 sole); rows 0-3 trouser, rows 4-9 boot (shaft 4 rows, foot 2 rows, toe 2 px ahead of the shaft); the tunic hem covers the rows above the crotch.

Core idea: each leg is a 10-row column with an x offset per row (a shear table) plus a lift. Do not rotate. Pose = (offsets, foot type, lift). When dy (body drop) or lift is nonzero the leg has 10 - dy - lift rows, so the sole stays on the ground line (planted) or above it (lifted).

### 5.1 Distance keying, no sliding
- Walk: planted foot goes from +4 px (heel at contact) to -4 px (toe at the next contact) over 3 frame intervals: v = 8/3 = 2.7 px per frame, D = 16 px per cycle (one 16 px tile per cycle, 8 px per step). Advance one frame every 2.67 px moved. Planted foot positions relative to the hip: +4, +1, -1, -4 (steps 3, 2, 3, under 1 px of unevenness).
- Run: the foot is down for only about 2 intervals: contact +5, toe-off -5, so v = 5 px per frame and D = 30 px per cycle. For a slower game run, cut the swing to +/-4 (D = 24).
- General rule (mine): v = (contact offset minus lift-off offset) / grounded intervals; frame advance comes from a distance accumulator, so changing speed changes playback rate automatically with no sliding. If walk speed is very different from 16 px per cycle, change the stride (swing), not the frame count.
- Optional: uneven holds in the run, Pokemon style (long on recover, short on contact and flight).

### 5.2 Walk, 6 frames (A = near leg, B = far leg)
Bob: hips dy 0 at pass, +1 at contact and down (the spread legs genuinely lower a 10 px leg by about 1 px: sqrt(100-16)=9.2). Head drops 1 more px at "down" and lags one frame (Saint11 head delay). Arms: near arm opposite near leg, hand 2 px forward and 2 px back, hand 1 px higher at pass. Hem: one frame behind the legs, 1 px sway.

| F | Pose | dy | A (near) | B (far) |
|---|------|----|----------|---------|
| 0 | contact A | +1 | front, straight, heel down with toe pixels one row up, foot at +4 | back, still on ground, toe only (heel one row up) at -4 |
| 1 | down A | +1 | foot flat, planted at about +1 | lifting: sole 1 px off the ground, toe down, at -3 |
| 2 | pass A | 0 | stance leg vertical, foot flat at 0 | swing: sole 3 px up, knee forward (+1), shin trailing back to -3, boot visible behind the stance leg |
| 3 | contact B | +1 | A back (as B at F0) | B front (as A at F0) |
| 4 | down B | +1 | A lifting | B planted |
| 5 | pass B | 0 | A swing | B stance |

Per-row x offsets used for the diagrams (hip = 0): front contact [0,0,1,1,2,2,3,3,4,4]; back contact [0,0,-1,-1,-2,-2,-3,-3,-4,-4]; planted at down [0,0,1,1,1,1,2,2,2,2]; swing at pass [0,1,1,0,-1,-2,-3] (7 rows, lift 3); stance [0]*10.

Timing: distance-keyed (frame every 2.67 px). For a visual sanity check, Saint11's GIF equivalent is about 200 ms per 6-frame step; if your ground speed makes frames flash by much faster than about 100 ms per frame, lengthen the cycle (D = 20, swing +/-5) rather than the speed of playback.

Eight-frame variant: insert an "up/swing" frame between pass and the next contact (Slynyrd's frame 4): swing boot at +2, 2 px up, hip dy 0 (or -1 if you allow a 2 px bob).

### 5.3 Run, 6 frames
Bob 2 px: contact dy 0, recover dy +1, flight dy -1. Lean: head x +1, hips x 0. Forward leg straight at contact (Saint11). Arms bent, 3 px swing, near arm opposite near leg. Hair and hem stream back 1-2 px and lag one frame. Leg shear 1 column per row (45 degrees) with width held at 4. Cap the angle at about 45-50 degrees; short legs spread wider look like scissors.

| F | Pose | dy | A (near) | B (far) |
|---|------|----|----------|---------|
| 0 | contact A | 0 | front, straight, heel strike at +5 | trailing, sole 2 px up, toe down, at -6 |
| 1 | recover | +1 | planted flat under the body at +1, slightly bent | knee driving forward, sole 3 px up, shin folded back |
| 2 | flight | -1 | trailing, extended back, sole 3 px up, at -6 | reaching, sole 2 px up, heel first, at +5 |
| 3-5 | same, A and B swapped | | | |

Timing: Pokemon-style 5:3 holds (long on recover, short on contact/flight); about 100 ms per frame average (cycle 600-650 ms; Slynyrd's 640 ms) as a start.

### 5.4 Diagrams (generated by a script from the row-offset tables; schematic)
Legend: # tunic hem, N near leg, f far leg (draw it darker), = ground. Walk diagrams show the table. Run diagrams are rougher: they illustrate a constant 4 px width on a diagonal; tune by eye.

```
W0 contact dy+1      W1 down dy+1         W2 pass dy0
...................  ...................  ...................
...................  ...................  ...................
...................  ...................  .....#######.......
.....#######.......  .....#######.......  .....#######.......
.....#######.......  .....#######.......  ......NNNN.........
......NNNN.........  ......NNNN.........  ......NNNNf........
......NNNN.........  ......NNNN.........  ......NNNNf........
.....ffNNNN........  .....ffNNNN........  ......NNNN.........
.....ffNNNN........  ....fffNNNN........  .....fNNNN.........
....ffffNNNN.......  ....fffNNNN........  ....ffNNNN.........
...ffff..NNNN......  ...ffff.NNNN.......  .....fNNNN.........
...ffff..NNNN......  ...ffff.NNNN.......  ......NNNN.........
..ffff....NNNNNN...  .....fffNNNN.......  ......NNNN.........
....ffff..NNNN.....  ........NNNNNN.....  ......NNNNNN.......
===================  ===================  ===================

R0 contact dy0       R1 recover dy+1      R2 flight dy-1
...................  ...................  ...................
...................  ...................  .....#######.......
.....#######.......  ...................  .....#######.......
.....#######.......  .....#######.......  ......NNNN.........
......NNNN.........  .....#######.......  .....ffNNNN........
.....ffNNNN........  ......NNNN.........  ....ffffNNNN.......
....fffNNNN........  ......NNNNf........  ...ffff..NNNN......
...ffff.NNNN.......  ......NNNNff.......  ...ffff..NNNN......
..ffff..NNNN.......  .......NNNNff......  ..ffff...NNNN......
.ffff....NNNN......  .......NNNNff......  .ffff.....NNNN.....
ffff......NNNN.....  .......NNNNf.......  ..ffff....NNNNNN...
..ffff.....NNNN....  .......NNNNff......  ...........NNNN....
...........NNNNNN..  .......NNNN........  ...................
...........NNNN....  .......NNNNNN......  ...................
===================  ===================  ===================
```

Every leg row is 4 px wide at every angle; feet are 6 px (4 shaft + 2 toe); the swing leg is shorter at pass instead of rotated; the planted foot stays on the ground line. (The generator's near leg hides most of the far leg in R1, which is why a real drawing should lift the far boot clear and give it a lighter outline; fix by eye.)

## 6. Checklist

TILTING LEGS
- [ ] Leg shape differs per frame (not just rotated); the lifted leg is shorter.
- [ ] Planted soles stay on the ground row and advance equally (3,2,3) with no slide.
- [ ] At pass the swing knee leads, the shin trails, and its boot is visible.
- [ ] Body bobs (1 px walk, 2 px run), head lags a frame; a static body makes legs look like a pendulum.
- [ ] Every cycle has a pass frame; no neutral stand pose inside a walk.

CLUNKY
- [ ] Both contacts are the same height.
- [ ] Bob is a triangle wave: lowest at contact/down, highest at pass.
- [ ] The run has a flight frame with both boots off the ground.
- [ ] Arms oppose legs; at least one secondary part (hem, hair) lags a frame.
- [ ] Run holds are uneven; frame advance is by distance and the stride changes with speed.

SKINNY
- [ ] No leg row narrower than 4 px; diagonals are sheared, not a line.
- [ ] Steps are even (2-2-2), no single-pixel steps, no orphan pixels.
- [ ] The 45 degree flight pose is checked at 1x.
- [ ] Near leg 2-3 value steps lighter than far leg, with a dark gap or outline between.
- [ ] Only one run frame (recover) may be narrow (Saint11 labels it "thinnest").

## 7. How to verify
1. Onion-skin 2 frames either side; check that each planted foot's pixels sit on the same ground row and move 2-3 px per frame.
2. Play at 1x on the real device at real ground speed over a scrolling tile background and watch a planted boot against a ground tile; sliding shows instantly. Never judge from a still sheet.
3. Play at half and double speed (Slynyrd: test several speeds).
4. Squint at 25% size to check leg thickness in the run.
5. Fix one frame at a time by editing row offsets and foot type, not a joint-angle table.
6. Keep a GIF of the old cycle beside the new one and compare.

## 8. Where sources disagree
- Frame count: Mega Man 3 distinct (Slynyrd); Saint11 6 for low-res walk and 4 keys for a run; Slynyrd 8 but prefers 6; forum 4 minimum and 6 for small; Pokemon and Stardew 3 images in 4 slots.
- Lowest point: Slynyrd contact; Saint11 DOWN+ (after contact); Aseprite on the contacts. Same pixel at a 1 px bob; with a 2 px bob follow Saint11.
- Whether to raise the sprite on straight-leg frames: one community comment says yes, others do not.
- More frames vs fewer, stronger poses: Slynyrd's Mega Man argument favours fewer.

## 9. Sources
- Saint11: https://saint11.art/blog/pixel-art-tutorials/ ; https://saint11.art/img/pixel-tutorials/Walk.gif ; https://saint11.art/img/pixel-tutorials/RunCycleSimple.gif
- Slynyrd: https://slynyrd.com/blog/2018/8/19/pixelblog-8-intro-to-animation ; https://www.slynyrd.com/blog/2024/5/24/pixelblog-50-human-walk-cycle
- pret/pokefirered: https://raw.githubusercontent.com/pret/pokefirered/master/src/data/object_events/object_event_anims.h
- Stardew wiki: https://stardewvalleywiki.com/Modding:Farmer_sprite
- Aseprite: https://community.aseprite.org/t/does-anyone-have-a-16x16-animation-walk-run-cycle/10479 ; https://community.aseprite.org/t/still-learning-or-trying-to-learn-to-do-walking-animation/8291/26 ; https://community.aseprite.org/t/running-animation-progress/10995
- PixelJoint: https://pixeljoint.com/forum/printer_friendly_posts.asp?TID=25611 ; https://pixeljoint.com/forum/printer_friendly_posts.asp?TID=6196
- itch.io: https://itch.io/t/2491630/pixel-tutorial-run-cycle ; https://itch.io/blog/526535/pixel-art-walk-cycle-animation
- Sub-pixel: https://tinywarriorgames.com/2019/01/04/game-development-pixel-art-sub-pixel-animation/ ; https://someanimatorwannabe.substack.com/p/animator-techniques-sub-pixel-animation
- Polycount foot sliding: https://polycount.com/discussion/comment/2379992
- Pixel Logic: https://80.lv/articles/pixel-logic-a-pixel-art-tutorial-book/ ; Game Anim 2D list: https://www.gameanim.com/tag/2d-animation
- Williams contact method (course notes): https://courses.cs.washington.edu/courses/cse464/11wi/assignments/assignment4
- Not retrieved: Game Developer, "Art Design Deep Dive: Using a 3D pipeline for 2D animation in Dead Cells"
